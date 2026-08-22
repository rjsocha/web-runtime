#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/version.hpp>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <winternl.h>
#else
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

using Row = std::pair<std::string, std::string>;
using Rows = std::vector<Row>;

static const auto started = std::chrono::steady_clock::now();

static std::string env(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
}

static std::string mb(double bytes) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << bytes / 1024.0 / 1024.0 << " MB";
    return out.str();
}

static std::string section(const std::string& title, const Rows& rows) {
    std::size_t width = 0;
    for (const auto& row : rows) {
        width = std::max(width, std::min<std::size_t>(row.first.size(), 30));
    }

    std::ostringstream out;
    out << '[' << title << "]\n";
    for (const auto& row : rows) {
        std::string value = row.second;
        std::replace(value.begin(), value.end(), '\n', ' ');
        std::replace(value.begin(), value.end(), '\r', ' ');
        out << std::left << std::setw(static_cast<int>(width)) << row.first << "  " << value << '\n';
    }

    out << '\n';
    return out.str();
}

static std::string uptime() {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - started).count();

    std::ostringstream out;
    out << seconds / 86400 << '.'
        << std::setfill('0') << std::setw(2) << seconds % 86400 / 3600 << ':'
        << std::setw(2) << seconds % 3600 / 60 << ':'
        << std::setw(2) << seconds % 60;
    return out.str();
}

static std::string hostname() {
    char name[256] = {};
#ifdef _WIN32
    DWORD size = sizeof(name);
    if (GetComputerNameA(name, &size)) {
        return name;
    }
#else
    if (gethostname(name, sizeof(name) - 1) == 0) {
        return name;
    }
#endif
    return "-";
}

static std::string architecture() {
#if defined(__x86_64__) || defined(_M_X64)
    return "amd64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#else
    return "unknown";
#endif
}

static std::string osName() {
#ifdef _WIN32
    return "windows";
#else
    return "linux";
#endif
}

static std::string osDescription() {
#ifdef _WIN32
    RTL_OSVERSIONINFOW info = {};
    info.dwOSVersionInfoSize = sizeof(info);

    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
        using RtlGetVersion = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
        auto version = reinterpret_cast<RtlGetVersion>(
                reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));

        if (version != nullptr && version(&info) == 0) {
            return "Windows " + std::to_string(info.dwMajorVersion) + "."
                   + std::to_string(info.dwMinorVersion) + " build "
                   + std::to_string(info.dwBuildNumber);
        }
    }

    return "Windows";
#else
    std::ifstream release("/etc/os-release");
    std::string line;
    while (std::getline(release, line)) {
        if (line.rfind("PRETTY_NAME=", 0) == 0) {
            std::string name = line.substr(12);
            if (!name.empty() && name.front() == '"') {
                name = name.substr(1, name.size() - 2);
            }
            return name;
        }
    }
    return "linux";
#endif
}

static std::string crt() {
#ifdef _WIN32
    std::string loaded = "unknown";
    if (GetModuleHandleA("ucrtbase.dll") != nullptr) {
        loaded = "ucrtbase.dll";
    } else if (GetModuleHandleA("msvcrt.dll") != nullptr) {
        loaded = "msvcrt.dll";
    }

#ifdef _UCRT
    return loaded + ", built for ucrt";
#else
    return loaded + ", built for msvcrt";
#endif
#else
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        if (line.find("/libc.so") != std::string::npos) {
            return "glibc, dynamic";
        }
        if (line.find("ld-musl") != std::string::npos || line.find("libc.musl") != std::string::npos) {
            return "musl, dynamic";
        }
    }

#ifdef __GLIBC__
    return "glibc, static";
#else
    return "musl, static";
#endif
#endif
}

static std::string totalMemory() {
#ifdef _WIN32
    MEMORYSTATUSEX status = {};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return mb(static_cast<double>(status.ullTotalPhys));
    }
    return "-";
#else
    for (const char* path : {"/sys/fs/cgroup/memory.max", "/sys/fs/cgroup/memory/limit_in_bytes"}) {
        std::ifstream file(path);
        std::string value;
        if (file >> value && value != "max") {
            double limit = std::strtod(value.c_str(), nullptr);
            if (limit > 0 && limit < 4.6e18) {
                return mb(limit);
            }
        }
    }

    std::ifstream meminfo("/proc/meminfo");
    std::string key;
    double kilobytes = 0;
    while (meminfo >> key >> kilobytes) {
        if (key == "MemTotal:") {
            return mb(kilobytes * 1024);
        }
        meminfo.ignore(1024, '\n');
    }

    return "-";
#endif
}

static std::string residentMemory() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters = {};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return mb(static_cast<double>(counters.WorkingSetSize));
    }
    return "-";
#else
    std::ifstream status("/proc/self/status");
    std::string key;
    double kilobytes = 0;
    while (status >> key) {
        if (key == "VmRSS:" && status >> kilobytes) {
            return mb(kilobytes * 1024);
        }
        status.ignore(1024, '\n');
    }
    return "-";
#endif
}

#ifdef _WIN32
static std::string narrow(PWCHAR text) {
    if (text == nullptr) {
        return {};
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }

    std::string out(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
    return out;
}
#endif

static Rows interfaces() {
    Rows rows;

#ifdef _WIN32
    ULONG size = 0;
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                             GAA_FLAG_SKIP_DNS_SERVER, nullptr, nullptr, &size) != ERROR_BUFFER_OVERFLOW) {
        return rows;
    }

    std::vector<char> buffer(size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                             GAA_FLAG_SKIP_DNS_SERVER, nullptr, adapters, &size) != NO_ERROR) {
        return rows;
    }

    for (auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) {
            continue;
        }

        std::string addresses;
        for (auto* address = adapter->FirstUnicastAddress; address != nullptr; address = address->Next) {
            char text[INET6_ADDRSTRLEN] = {};
            void* raw = address->Address.lpSockaddr->sa_family == AF_INET
                    ? static_cast<void*>(&reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr)->sin_addr)
                    : static_cast<void*>(&reinterpret_cast<sockaddr_in6*>(address->Address.lpSockaddr)->sin6_addr);

            if (inet_ntop(address->Address.lpSockaddr->sa_family, raw, text, sizeof(text)) == nullptr) {
                continue;
            }

            if (!addresses.empty()) {
                addresses += ", ";
            }
            addresses += std::string(text) + "/" + std::to_string(address->OnLinkPrefixLength);
        }

        if (!addresses.empty()) {
            std::string name = narrow(adapter->FriendlyName);
            if (name.empty()) {
                name = narrow(adapter->Description);
            }
            if (name.empty()) {
                name = adapter->AdapterName;
            }

            rows.emplace_back(name, addresses);
        }
    }
#else
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) {
        return rows;
    }

    std::map<std::string, std::string> byName;
    for (auto* entry = list; entry != nullptr; entry = entry->ifa_next) {
        if (entry->ifa_addr == nullptr || (entry->ifa_flags & IFF_UP) == 0) {
            continue;
        }

        char text[INET6_ADDRSTRLEN] = {};
        int prefix = 0;

        if (entry->ifa_addr->sa_family == AF_INET) {
            auto* addr = reinterpret_cast<sockaddr_in*>(entry->ifa_addr);
            inet_ntop(AF_INET, &addr->sin_addr, text, sizeof(text));
            auto mask = ntohl(reinterpret_cast<sockaddr_in*>(entry->ifa_netmask)->sin_addr.s_addr);
            while (mask & 0x80000000u) { prefix++; mask <<= 1; }
        } else if (entry->ifa_addr->sa_family == AF_INET6) {
            auto* addr = reinterpret_cast<sockaddr_in6*>(entry->ifa_addr);
            inet_ntop(AF_INET6, &addr->sin6_addr, text, sizeof(text));
            auto* mask = reinterpret_cast<sockaddr_in6*>(entry->ifa_netmask);
            for (int i = 0; i < 16; i++) {
                unsigned char byte = mask->sin6_addr.s6_addr[i];
                while (byte & 0x80) { prefix++; byte <<= 1; }
            }
        } else {
            continue;
        }

        auto& addresses = byName[entry->ifa_name];
        if (!addresses.empty()) {
            addresses += ", ";
        }
        addresses += std::string(text) + "/" + std::to_string(prefix);
    }

    freeifaddrs(list);
    rows.assign(byName.begin(), byName.end());
#endif

    return rows;
}

static std::string report(const http::request<http::string_body>& request,
                          const tcp::socket& socket, bool verbose, bool withEnvironment) {
    Rows runtime = {
            {"hostname", hostname()},
            {"uptime", uptime()},
            {"framework", std::string("Boost.Beast ") + std::to_string(BOOST_VERSION / 100000) + "."
                          + std::to_string(BOOST_VERSION / 100 % 1000) + "."
                          + std::to_string(BOOST_VERSION % 100)},
            {"runtime identifier", osName() + "-" + architecture()},
            {"os", osDescription()},
            {"architecture", architecture()},
            {"processors", std::to_string(std::thread::hardware_concurrency())},
            {"crt", crt()},
    };

    if (verbose) {
#ifdef __clang__
        runtime.emplace_back("compiler", std::string("clang ") + __clang_version__);
#elif defined(__GNUC__)
        runtime.emplace_back("compiler", std::string("gcc ") + __VERSION__);
#else
        runtime.emplace_back("compiler", "unknown");
#endif
        runtime.emplace_back("standard", std::to_string(__cplusplus));
    } else {
        runtime.emplace_back("ram", totalMemory());
    }

    std::string node = env("RUNTIME_NODE");
    if (!node.empty()) {
        std::string details;
        for (const char* name : {"RUNTIME_NODE_ID", "RUNTIME_TASK", "RUNTIME_SLOT"}) {
            std::string value = env(name);
            if (!value.empty()) {
                if (!details.empty()) {
                    details += " / ";
                }
                details += value;
            }
        }

        runtime.insert(runtime.begin(), {"node", details.empty() ? node : node + " (" + details + ")"});
    }

    std::string out = section("runtime", runtime);

    if (verbose) {
        out += section("process", {
                {"pid", std::to_string(
#ifdef _WIN32
                        GetCurrentProcessId()
#else
                        getpid()
#endif
                )},
                {"resident memory", residentMemory()},
                {"threads", std::to_string(std::thread::hardware_concurrency())},
        });
    }

    out += section("network", interfaces());

    boost::system::error_code failed;
    auto remote = socket.remote_endpoint(failed);
    auto local = socket.local_endpoint(failed);

    auto endpoint = [](const tcp::endpoint& value) {
        auto address = value.address();
        if (address.is_v6() && address.to_v6().is_v4_mapped()) {
            address = net::ip::make_address_v4(net::ip::v4_mapped, address.to_v6());
        }
        return address.is_v6()
                ? "[" + address.to_string() + "]:" + std::to_string(value.port())
                : address.to_string() + ":" + std::to_string(value.port());
    };

    out += section("request", {
            {"method", std::string(request.method_string())},
            {"path", std::string(request.target())},
            {"protocol", "HTTP/" + std::to_string(request.version() / 10) + "."
                         + std::to_string(request.version() % 10)},
            {"scheme", "http"},
            {"host header", std::string(request[http::field::host])},
            {"remote", endpoint(remote)},
            {"local", endpoint(local)},
    });

    if (verbose) {
        Rows headers;
        for (const auto& field : request) {
            headers.emplace_back(std::string(field.name_string()), std::string(field.value()));
        }
        std::sort(headers.begin(), headers.end());
        out += section("headers", headers);
    }

    if (withEnvironment) {
        Rows rows;
#ifdef _WIN32
        LPCH block = GetEnvironmentStrings();
        for (LPCH entry = block; entry != nullptr && *entry != '\0'; entry += std::strlen(entry) + 1) {
            std::string text(entry);
            auto split = text.find('=', 1);
            if (split != std::string::npos) {
                rows.emplace_back(text.substr(0, split), text.substr(split + 1));
            }
        }
        FreeEnvironmentStrings(block);
#else
        extern char** environ;
        for (char** entry = environ; *entry != nullptr; entry++) {
            std::string text(*entry);
            auto split = text.find('=');
            if (split != std::string::npos) {
                rows.emplace_back(text.substr(0, split), text.substr(split + 1));
            }
        }
#endif
        std::sort(rows.begin(), rows.end());
        out += section("environment", rows);
    }

    return out;
}

static void serve(tcp::socket socket) {
    try {
        beast::flat_buffer buffer;
        http::request<http::string_body> request;
        http::read(socket, buffer, request);

        std::string target(request.target());
        bool environment = target.rfind("/env", 0) == 0;
        bool verbose = environment || target.rfind("/verbose", 0) == 0;

        http::response<http::string_body> response{http::status::ok, request.version()};
        response.set(http::field::content_type, "text/plain; charset=utf-8");
        response.keep_alive(false);
        response.body() = report(request, socket, verbose, environment);
        response.prepare_payload();

        http::write(socket, response);

        boost::system::error_code ignored;
        socket.shutdown(tcp::socket::shutdown_send, ignored);
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
    }
}

int main() {
    // As PID 1 nothing terminates this process by default: the kernel drops
    // signals that have no handler installed, so docker stop would wait for
    // its timeout and then kill the container.
    std::signal(SIGINT, [](int) { std::_Exit(0); });
    std::signal(SIGTERM, [](int) { std::_Exit(0); });

    unsigned short port = 8080;
    std::string configured = env("PORT");
    if (!configured.empty()) {
        port = static_cast<unsigned short>(std::stoi(configured));
    }

    try {
        net::io_context context{1};
        tcp::acceptor acceptor{context};

        tcp::endpoint endpoint{tcp::v6(), port};
        acceptor.open(endpoint.protocol());
        acceptor.set_option(net::socket_base::reuse_address(true));
        acceptor.set_option(net::ip::v6_only(false));
        acceptor.bind(endpoint);
        acceptor.listen(net::socket_base::max_listen_connections);

        for (;;) {
            tcp::socket socket{context};
            acceptor.accept(socket);
            std::thread(serve, std::move(socket)).detach();
        }
    } catch (const std::exception& failure) {
        std::cerr << "cannot listen on :" << port << ": " << failure.what() << '\n';
        return 1;
    }
}
