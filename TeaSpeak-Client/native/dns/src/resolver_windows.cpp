//
// Created by WolverinDEV on 25/10/2019.
//

#include <iostream>
#include <cassert>
#include "./resolver.h"
#include "./response.h"
#include "resolver.h"

using namespace std;
using namespace tc::dns;

bool Resolver::initialize_platform(std::string &error, bool hosts, bool resolvconf) {
    this->dnsapi.libhandle = LoadLibraryA("dnsapi.dll");

    /* windows 8 or newer */
    if(this->dnsapi.libhandle && false) {
        this->dnsapi.DnsCancelQuery = (decltype(this->dnsapi.DnsCancelQuery)) GetProcAddress(this->dnsapi.libhandle, "DnsCancelQuery");
        this->dnsapi.DnsQueryEx = (decltype(this->dnsapi.DnsQueryEx)) GetProcAddress(this->dnsapi.libhandle, "DnsQueryEx");

        if(!this->dnsapi.DnsCancelQuery || !this->dnsapi.DnsQueryEx) {
            this->dnsapi.DnsQueryEx = nullptr;
            this->dnsapi.DnsCancelQuery = nullptr;

            FreeLibrary(this->dnsapi.libhandle);
            this->dnsapi.libhandle = nullptr;
        }
    }

    return true;
}

void Resolver::finalize_platform() {
    if(this->dnsapi.libhandle)
        FreeLibrary(this->dnsapi.libhandle);
    this->dnsapi.libhandle = nullptr;
    this->dnsapi.DnsQueryEx = nullptr;
    this->dnsapi.DnsCancelQuery = nullptr;
}

DWORD CreateDnsServerList(
        _In_ PWSTR ServerIp,
        _Out_ PDNS_ADDR_ARRAY DnsServerList
)
{
    DWORD  Error = ERROR_SUCCESS;
    SOCKADDR_STORAGE SockAddr;
    INT AddressLength;
    WSADATA wsaData;

    ZeroMemory(DnsServerList, sizeof(*DnsServerList));

    Error = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (Error != 0)
    {
        wprintf(L"WSAStartup failed with %d\n", Error);
        return Error;
    }

    AddressLength = sizeof(SockAddr);
    Error = WSAStringToAddressW(ServerIp,
                               AF_INET,
                               NULL,
                               (LPSOCKADDR)&SockAddr,
                               &AddressLength);
    if (Error != ERROR_SUCCESS)
    {

        AddressLength = sizeof(SockAddr);
        Error = WSAStringToAddressW(ServerIp,
                                   AF_INET6,
                                   NULL,
                                   (LPSOCKADDR)&SockAddr,
                                   &AddressLength);
    }

    if (Error != ERROR_SUCCESS)
    {
        wprintf(L"WSAStringToAddress for %s failed with error %d\n",
                ServerIp,
                Error);
        goto exit;
    }

    DnsServerList->MaxCount = 1;
    DnsServerList->AddrCount = 1;
    CopyMemory(DnsServerList->AddrArray[0].MaxSa, &SockAddr, DNS_ADDR_MAX_SOCKADDR_LENGTH);

    exit:

    WSACleanup();
    return Error;
}

void Resolver::destroy_dns_request(Resolver::dns_request *request) {
    assert(this_thread::get_id() == this->event.loop.get_id() || !this->event.loop_active);

    if(request->threaded_lock) {
        unique_lock lock{*request->threaded_lock};
        if(request->thread_data) {
            request->thread_data->request = nullptr;
            request->thread_data = nullptr;
        } else {
            //Threaded data has been deleted now delete the lock
            lock.unlock();

            delete request->threaded_lock;
            request->threaded_lock = nullptr;
        }
    }

    {
        lock_guard lock{this->request_lock};
        this->dns_requests.erase(std::find(this->dns_requests.begin(), this->dns_requests.end(), request), this->dns_requests.end());
    }

    if(!this->event.loop_active && this->dnsapi.DnsQueryEx)
        this->dnsapi.DnsCancelQuery(&request->dns_cancel);

    if(request->dns_result.pQueryRecords)
        DnsRecordListFree(request->dns_result.pQueryRecords, DnsFreeRecordList);

    if(request->processed_event) {
        event_del_noblock(request->processed_event);
        event_free(request->processed_event);
        request->processed_event = nullptr;
    }

    if(request->timeout_event) {
        event_del_noblock(request->timeout_event);
        event_free(request->timeout_event);
        request->timeout_event = nullptr;
    }

    delete request;
}

void Resolver::resolve_dns(const char *name, const rrtype::value &type, const rrclass::value &klass, const chrono::microseconds &timeout, const tc::dns::Resolver::dns_callback_t &callback) {
    if(!this->event.loop_active) {
        callback(ResultState::INITIALISATION_FAILED, 3, nullptr);
        return;
    }

    auto request = new dns_request{};
    request->resolver = this;

    request->host = name;
    request->whost = std::wstring{request->host.begin(), request->host.end()};
    request->rrtype = type;
    request->rrclass = klass;
    request->callback = callback;

    memset(&request->dns_cancel, 0, sizeof(request->dns_cancel));
    memset(&request->dns_query, 0, sizeof(request->dns_query));
    memset(&request->dns_result, 0, sizeof(request->dns_result));
    request->dns_result.Version = DNS_QUERY_REQUEST_VERSION1;

    /* newer method */
    if(this->dnsapi.DnsQueryEx) {
        request->dns_query.Version = DNS_QUERY_REQUEST_VERSION1;
        request->dns_query.pQueryContext = request;
        request->dns_query.QueryName = request->whost.c_str();
        request->dns_query.QueryType = type;
        request->dns_query.pQueryCompletionCallback = [](void* _request, PDNS_QUERY_RESULT result) {
            auto request = static_cast<dns_request*>(_request);
            assert(result == &request->dns_result);
            evuser_trigger(request->processed_event);
            request->resolver->event.condition.notify_one();
        };

        /*
            Error = CreateDnsServerList(ServerIp, &DnsServerList);

            if (Error != ERROR_SUCCESS)
            {
                wprintf(L"CreateDnsServerList failed with error %d", Error);
                goto exit;
            }

            DnsQueryRequest.pDnsServerList = &DnsServerList;
         */
    } else {
        request->threaded_lock = new std::mutex{};
        request->thread_data = new dns_old_request_data{};
        request->thread_data->request = request;
        request->thread_data->lock = request->threaded_lock;
    }

    request->timeout_event = evtimer_new(this->event.base, [](evutil_socket_t, short, void *_request) {
        auto request = static_cast<dns_request*>(_request);
        request->resolver->evtimer_dns_callback(request);
    }, request);

    request->processed_event = evuser_new(this->event.base, [](evutil_socket_t, short, void *_request) {
        auto request = static_cast<dns_request*>(_request);
        request->resolver->dns_callback(request);
    }, request);

    if(!request->timeout_event || !request->processed_event) {
        if(request->timeout_event)
            event_free(request->timeout_event);

        if(request->processed_event)
            event_free(request->processed_event);

        callback(ResultState::INITIALISATION_FAILED, 2, nullptr);
        return;
    }

    unique_lock rlock{this->request_lock};
    if(this->dnsapi.DnsQueryEx) {
        auto error = this->dnsapi.DnsQueryEx(&request->dns_query, &request->dns_result, &request->dns_cancel);
        if (error != DNS_REQUEST_PENDING) {
            rlock.unlock();

            evuser_trigger(request->processed_event);
            return;
        }
    } else {
        auto data = request->thread_data;

        std::string host_str{name};
        auto t = std::thread([data, host_str, type]{
            ULONG64 query_options{0};

            PDNS_RECORD query_results{nullptr};
            auto error = DnsQuery_A(host_str.c_str(), type, (DWORD) query_options, nullptr, &query_results, nullptr);

            unique_lock lock{*data->lock};
            if(!data->request) {
                //We timed out, and we should do the lock delete stuff
                lock.unlock();

                delete data->lock;
                delete data;
                return;
            }

            data->request->dns_result.pQueryRecords = query_results;
            data->request->dns_result.QueryOptions = query_options;
            data->request->dns_result.QueryStatus = error;
            data->request->thread_data = nullptr;
            evuser_trigger(data->request->processed_event);
            delete data;
        });
        t.detach();
    }

    {
        auto seconds = chrono::floor<chrono::seconds>(timeout);
        auto microseconds = chrono::ceil<chrono::microseconds>(timeout - seconds);

        timeval tv{(long) seconds.count(), (long) microseconds.count()};
        auto errc = event_add(request->timeout_event, &tv);

        //TODO: Check for error
    }

    this->dns_requests.push_back(request);
    this->event.condition.notify_one();
}

void Resolver::evtimer_dns_callback(Resolver::dns_request *request) {
    if(this->dnsapi.DnsQueryEx) {
        auto err = this->dnsapi.DnsCancelQuery(&request->dns_cancel);
        if(err != 0)
            std::cerr << "Failed to cancel timeouted DNS request" << std::endl;
    } else {
        request->callback(ResultState::DNS_TIMEOUT, 0, nullptr);
        this->destroy_dns_request(request);
    }
}

void Resolver::dns_callback(Resolver::dns_request *request) {
    if(request->dns_result.QueryStatus != ERROR_SUCCESS) {
        auto status = request->dns_result.QueryStatus;
        if(status >= DNS_ERROR_RESPONSE_CODES_BASE && status <= DNS_ERROR_RCODE_LAST)
            request->callback(ResultState::DNS_FAIL, request->dns_result.QueryStatus - DNS_ERROR_RESPONSE_CODES_BASE, nullptr);
        else if(status == ERROR_CANCELLED)
            request->callback(ResultState::DNS_TIMEOUT, 0, nullptr);
        else
            request->callback(ResultState::DNS_API_FAIL, request->dns_result.QueryStatus, nullptr);
    } else {
        auto callback = request->callback;
        auto data = make_shared<DNSResponseData>();
        data->wide_string = this->dnsapi.DnsQueryEx != nullptr;
        memcpy(&data->data, &request->dns_result, sizeof(request->dns_result));
        request->dns_result.pQueryRecords = nullptr;
        callback(ResultState::SUCCESS, 0, std::unique_ptr<DNSResponse>(new DNSResponse{data}));
    }
    this->destroy_dns_request(request);
}

DNSResponseData::~DNSResponseData() {
    if(this->data.pQueryRecords)
        DnsRecordListFree(this->data.pQueryRecords, DnsFreeRecordList);
}

DNSResponse::DNSResponse(std::shared_ptr<tc::dns::DNSResponseData> data) : data{std::move(data)} {}

bool DNSResponse::parse(std::string &error) {
    auto head = this->data->data.pQueryRecords;
    if(!head) {
        error = "empty response";
        return false;
    }

    do {
        this->parsed_answers.emplace_back(new response::DNSResourceRecords{this->data, head});
    } while((head = head->pNext));
    return true;
}
