#pragma once

#include <cstdint>

#pragma pack(push)
#pragma pack(1)
typedef struct _SMBIOSDataHeader
{
        uint8_t	_unused;
        uint8_t	major_version;
        uint8_t	minor_version;
        uint8_t	dmi_revision;
        uint32_t length;

        uint8_t table_data;
} SMBIOSDataHeader;

static constexpr size_t SMBIOSDataHeaderLength = 8;
static_assert(sizeof(SMBIOSDataHeader) == SMBIOSDataHeaderLength + 1, "invalid struct size");


typedef struct _SMBIOSEntryHeader
{
        uint8_t type;
        uint8_t length;
        uint16_t header;

        uint8_t data;
} SMBIOSEntryHeader;

static constexpr size_t SMBIOSEntryHeaderLength = 4;
static_assert(sizeof(SMBIOSEntryHeader) == SMBIOSEntryHeaderLength + 1, "invalid struct size");

#pragma pack(pop)