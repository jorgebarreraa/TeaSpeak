#include "./hwuid.h"
#include <tomcrypt.h>
#include <fstream>
#include <misc/base64.h>
#include <misc/strobf.h>

#ifndef WIN32
#ifdef DARWIN
/*
#include <mach-o/arch.h>
unsigned short getCpuHash() {
	const NXArchInfo* info = NXGetLocalArchInfo();
	unsigned short val = 0;
	val += (unsigned short)info->cputype;
	val += (unsigned short)info->cpusubtype;
	return val;
}
 */
#else // !DARWIN
static inline void native_cpuid(unsigned int *eax, unsigned int *ebx,
                                unsigned int *ecx, unsigned int *edx)
{
	/* ecx is often an input as well as an output. */
	asm volatile("cpuid"
	: "=a" (*eax),
	"=b" (*ebx),
	"=c" (*ecx),
	"=d" (*edx)
	: "0" (*eax), "2" (*ecx));
}

uint32_t calculate_cpu_hash() {
	uint32_t cpuinfo[4] = { 0, 0, 0, 0 };
	native_cpuid(cpuinfo, cpuinfo + 1, cpuinfo + 2, cpuinfo + 3);

	uint32_t hash = 0;
	uint32_t* ptr = (&cpuinfo[0]);
	for (uint32_t i = 0; i < 4; i++)
		hash += ptr[i];

	return hash;
}
#endif // !DARWIN

constexpr uint8_t hex_value(char c) {
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'A' + 10;
	if(c >= 'A' && c <= 'F')
		return c - '0' + 10;
	return 0;
}

bool read_machine_id(uint8_t(&uuid)[16]) {
	memset(uuid, 0, 16);
	std::ifstream in(strobf("/etc/machine-id").string());
	if(in) {
		char buffer[32];
		if(!in.read(buffer, 32)) return false;

		auto it = buffer;
		auto index = 0;
		while(index < 16) {
			uuid[index] = hex_value(*it) << 4UL;
			uuid[index] = hex_value(*it);
			index++;
		}
	}
	return false;
}

inline bool generate_uuid(std::string& result, uint32_t& check_sum) {
	uint8_t buffer[16];
	if(!read_machine_id(buffer))
		memcpy(buffer, "AAAABBBBCCCCDDDD", 16);

	auto cpu_hash = calculate_cpu_hash();
	auto it = (uint32_t*) buffer;
	for(int i = 0; i < 4; i++)
		*it++ = cpu_hash;

	result = base64::encode((char*) buffer, 16);

	{
		crc32_state state;
		crc32_init(&state);
		crc32_update(&state, (u_char*) result.data(), result.length());

		crc32_finish(&state, &check_sum, sizeof(check_sum));
	}
	return true;
}
#else
#include "./smbios.h"
#include <Windows.h>
#include <sysinfoapi.h>
#include <intrin.h>

using namespace std;

//https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_2.7.1.pdf
int get_uuid(uint8_t* uuid) {
#ifdef WIN32
    typedef DWORD api_size_t;
#else
    typedef size_t api_size_t;
#endif
    unique_ptr<SMBIOSDataHeader, decltype(::free)*> smbios_data{ nullptr, ::free };
    api_size_t smbios_size = 0;
    api_size_t bytes_written = 0;

    // Query size of SMBIOS data.
    smbios_size = GetSystemFirmwareTable('RSMB', 0, nullptr, 0);
    smbios_data.reset((SMBIOSDataHeader*) malloc(smbios_size));
    if (!smbios_data) return -1; //failed to allocate memory

    bytes_written = GetSystemFirmwareTable('RSMB', 0, &*smbios_data, smbios_size);

    if (smbios_size != bytes_written) return -2; //failed to read the firmware table

    if (smbios_data->length > bytes_written - SMBIOSDataHeaderLength) return -3; //invalid length

    //Lets iterate over the headers:
    uint8_t* ptr = &smbios_data->table_data;
    SMBIOSEntryHeader* header;
    while (true) {
        header = (SMBIOSEntryHeader*) ptr;
        if (header->type == 127 && header->length == 4) {
            break; //End marker
        }

        if (header->type == 1) {
            //Type 1: System Information
            if (header->length < 0x18) {
                //If version is under 2.1 no uuid is set!
                //TODO: Check before for the version?
                return -4; //system information table is too short
            }

            memcpy(uuid, ptr + 0x08, 16);
            return 0;
        }

        ptr = ptr + header->length; // point to struct end
        while (0 != (*ptr | *(ptr + 1)) && ptr + 1 < (uint8_t*) &*smbios_data + smbios_data->length)
            ptr++; // skip string area

        ptr += 2;
        if (ptr >= (uint8_t*) &*smbios_data + smbios_data->length - 2)
            break;
    }
    return -6;
}

uint32_t calculate_cpu_hash() {
    int cpuinfo[4] = { 0, 0, 0, 0 };
    __cpuid(cpuinfo, 0);

    uint32_t hash = 0;
    auto ptr = (uint32_t*) (&cpuinfo[0]);
    for (uint32_t i = 0; i < 4; i++)
        hash += ptr[i];

    return hash;
}

inline bool generate_uuid(std::string& result, uint32_t& check_sum) {
    uint8_t buffer[16];
    auto code = get_uuid(buffer);
    if(code != 0) {
        memcpy(buffer, "DDDDCCCCBBBBAAAA", 16); /* Reversed to linux (just for the foxic) */
        buffer[15] = code; /* So we could get the error code */
    }

    auto cpu_hash = calculate_cpu_hash();
    auto it = (uint32_t*) buffer;
    for(int i = 0; i < 4; i++)
        *it++ = cpu_hash;

    result = base64::encode((char*) buffer, 16);

    {
        crc32_state state;
        crc32_init(&state);
        crc32_update(&state, (uint8_t*) result.data(), (unsigned long) result.length());

        crc32_finish(&state, &check_sum, sizeof(check_sum));
    }
    return true;
}
#endif

inline bool check_uuid(std::string& uuid, uint32_t check_sum) {
	crc32_state state;
	crc32_init(&state);
	crc32_update(&state, (uint8_t*) uuid.data(), (unsigned long) uuid.length());

	uint32_t result;
	crc32_finish(&state, &result, sizeof(result));

	return result == check_sum;
}

static std::string cached_system_uuid_{};
static uint32_t cached_system_uuid_cksm_ = 0;
std::string system_uuid() {
	if(!cached_system_uuid_.empty() && check_uuid(cached_system_uuid_, cached_system_uuid_cksm_))
		return cached_system_uuid_;

	if(!generate_uuid(cached_system_uuid_, cached_system_uuid_cksm_))
        cached_system_uuid_ = "";
	return cached_system_uuid_;
}