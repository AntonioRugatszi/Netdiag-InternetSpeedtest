/**
 * netdiag - Cross-platform Network Diagnostics & Internet Speed Test
 *
 * A C++17 program that retrieves local network details, public IP/ISP info,
 * and performs download/upload/ping speed tests.
 *
 * Compilation:
 *   Linux:   g++ -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl
 *   macOS:   g++ -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl
 *   Windows: g++ -std=c++17 -O2 -o netdiag.exe netdiag.cpp -lcurl -lws2_32 -lwlanapi -lole32
 *
 * Dependencies:
 *   - libcurl (install: Ubuntu/Debian: sudo apt install libcurl4-openssl-dev
 *                       Arch: sudo pacman -S curl
 *                       macOS: brew install curl
 *                       Windows: vcpkg install curl or use MinGW with curl)
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// libcurl for HTTP requests
#include <curl/curl.h>

// Platform-specific headers
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #include <wlanapi.h>
    #include <objbase.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "wlanapi.lib")
    #pragma comment(lib, "ole32.lib")
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <net/if.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <unistd.h>
    #include <netdb.h>
    #ifdef __linux__
    #include <linux/if_packet.h>
    #endif
#endif

// JSON parsing (simple manual parsing for no external deps)
#include <regex>

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Trim whitespace from both ends of a string
 */
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

/**
 * Execute a shell command and return its output
 */
static std::string exec_command(const char* cmd) {
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return result;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return trim(result);
}

/**
 * Format bytes as human-readable string (KB, MB, GB)
 */
static std::string format_bytes(double bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    while (bytes >= 1024 && unit < 4) {
        bytes /= 1024;
        unit++;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << bytes << " " << units[unit];
    return oss.str();
}

/**
 * Format speed in bits per second as Mbps/Gbps
 */
static std::string format_speed_bps(double bits_per_sec) {
    if (bits_per_sec >= 1e9) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << (bits_per_sec / 1e9) << " Gbps";
        return oss.str();
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (bits_per_sec / 1e6) << " Mbps";
    return oss.str();
}

/**
 * Format milliseconds with appropriate precision
 */
static std::string format_ms(double ms) {
    std::ostringstream oss;
    if (ms < 1.0) {
        oss << std::fixed << std::setprecision(2) << ms << " ms";
    } else if (ms < 10.0) {
        oss << std::fixed << std::setprecision(1) << ms << " ms";
    } else {
        oss << std::fixed << std::setprecision(0) << ms << " ms";
    }
    return oss.str();
}

// ============================================================================
// libcurl Callback & HTTP Helpers
// ============================================================================

/**
 * Write callback for libcurl - stores response in a string
 */
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total = size * nmemb;
    userp->append(static_cast<char*>(contents), total);
    return total;
}

/**
 * Progress callback for libcurl - used for speed measurement
 */
static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                             curl_off_t ultotal, curl_off_t ulnow) {
    // We don't use this for speed calculation; we use our own timing
    return 0;
}

/**
 * Perform HTTP GET request and return response body
 */
static bool http_get(const std::string& url, std::string& response, long& http_code,
                     double& elapsed_seconds, size_t* bytes_received = nullptr) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "netdiag/1.0 (C++17; libcurl)");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  // For speed test endpoints
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    auto start = std::chrono::high_resolution_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end = std::chrono::high_resolution_clock::now();
    elapsed_seconds = std::chrono::duration<double>(end - start).count();

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (bytes_received) {
        curl_off_t size = 0;
        curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &size);
        *bytes_received = static_cast<size_t>(size);
    }

    curl_easy_cleanup(curl);
    return res == CURLE_OK && http_code == 200;
}

/**
 * Perform HTTP POST request with payload for upload test
 */
static bool http_post(const std::string& url, const std::string& payload,
                      long& http_code, double& elapsed_seconds,
                      size_t* bytes_sent = nullptr) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    headers = curl_slist_append(headers, "Expect:");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    std::string dummy_response;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dummy_response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "netdiag/1.0 (C++17; libcurl)");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    auto start = std::chrono::high_resolution_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end = std::chrono::high_resolution_clock::now();
    elapsed_seconds = std::chrono::duration<double>(end - start).count();

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (bytes_sent) {
        curl_off_t size = 0;
        curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD_T, &size);
        *bytes_sent = static_cast<size_t>(size);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK && http_code == 200;
}

/**
 * Simple ping using HTTP HEAD request to measure latency
 */
static bool http_ping(const std::string& url, double& latency_ms) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  // HEAD request
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    auto start = std::chrono::high_resolution_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end = std::chrono::high_resolution_clock::now();
    latency_ms = std::chrono::duration<double, std::milli>(end - start).count();

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    return res == CURLE_OK && (http_code == 200 || http_code == 404 || http_code == 301 || http_code == 302);
}

// ============================================================================
// JSON Parsing Helpers (Simple, no external dependencies)
// ============================================================================

static std::string json_extract(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"\\s*:\\s*\"([^\"]*)\"";
    std::regex re(pattern);
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        return match[1].str();
    }
    // Try without quotes (for numbers)
    pattern = "\"" + key + "\"\\s*:\\s*([^,}\\]]+)";
    re = std::regex(pattern);
    if (std::regex_search(json, match, re) && match.size() > 1) {
        return trim(match[1].str());
    }
    return "";
}

// ============================================================================
// Platform-Specific Network Info Retrieval
// ============================================================================

struct NetworkInfo {
    std::string ssid = "N/A (Ethernet or unknown)";
    std::string local_ip = "N/A";
    std::string mac_address = "N/A";
    std::string interface_name = "N/A";
    bool is_wifi = false;
};

#ifdef _WIN32
// Windows implementation using Native Wifi API and GetAdaptersAddresses

static std::string wlan_get_ssid() {
    HANDLE hClient = nullptr;
    DWORD dwVersion = 0;
    DWORD res = WlanOpenHandle(2, nullptr, &dwVersion, &hClient);
    if (res != ERROR_SUCCESS || !hClient) return "";

    PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
    res = WlanEnumInterfaces(hClient, nullptr, &pIfList);
    if (res != ERROR_SUCCESS || !pIfList) {
        WlanCloseHandle(hClient, nullptr);
        return "";
    }

    std::string ssid;
    for (DWORD i = 0; i < pIfList->dwNumberOfItems; ++i) {
        PWLAN_INTERFACE_INFO pIfInfo = &pIfList->InterfaceInfo[i];
        if (pIfInfo->isState == wlan_interface_state_connected) {
            PWLAN_AVAILABLE_NETWORK_LIST pNetworkList = nullptr;
            res = WlanGetAvailableNetworkList(hClient, &pIfInfo->InterfaceGuid, 0, nullptr, &pNetworkList);
            if (res == ERROR_SUCCESS && pNetworkList) {
                for (DWORD j = 0; j < pNetworkList->dwNumberOfItems; ++j) {
                    PWLAN_AVAILABLE_NETWORK pNetwork = &pNetworkList->Network[j];
                    if (pNetwork->dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED) {
                        ssid.assign(reinterpret_cast<char*>(pNetwork->dot11Ssid.ucSSID),
                                   pNetwork->dot11Ssid.uSSIDLength);
                        break;
                    }
                }
                WlanFreeMemory(pNetworkList);
            }
            if (!ssid.empty()) break;
        }
    }

    WlanFreeMemory(pIfList);
    WlanCloseHandle(hClient, nullptr);
    return ssid;
}

static NetworkInfo get_network_info_windows() {
    NetworkInfo info;

    // Get SSID via WLAN API
    std::string ssid = wlan_get_ssid();
    if (!ssid.empty()) {
        info.ssid = ssid;
        info.is_wifi = true;
    }

    // Get adapter addresses for IP and MAC
    ULONG outBufLen = 15000;
    std::unique_ptr<IP_ADAPTER_ADDRESSES, void(*)(void*)> pAddresses(
        malloc(outBufLen), free);
    if (!pAddresses) return info;

    DWORD ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX,
                                      nullptr, pAddresses.get(), &outBufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        pAddresses.reset(malloc(outBufLen));
        if (!pAddresses) return info;
        ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX,
                                    nullptr, pAddresses.get(), &outBufLen);
    }

    if (ret == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses.get(); pCurr != nullptr; pCurr = pCurr->Next) {
            // Skip loopback, tunnel, and disconnected interfaces
            if (pCurr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (pCurr->OperStatus != IfOperStatusUp) continue;

            // Convert interface name
            int len = WideCharToMultiByte(CP_UTF8, 0, pCurr->FriendlyName, -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                std::string name(len, 0);
                WideCharToMultiByte(CP_UTF8, 0, pCurr->FriendlyName, -1, &name[0], len, nullptr, nullptr);
                if (!name.empty() && name.back() == '\0') name.pop_back();
                info.interface_name = name;
            }

            // Get IPv4 address
            for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurr->FirstUnicastAddress;
                 pUnicast != nullptr; pUnicast = pUnicast->Next) {
                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                    struct sockaddr_in* sa = (struct sockaddr_in*)pUnicast->Address.lpSockaddr;
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sa->sin_addr, ip, INET_ADDRSTRLEN);
                    if (std::string(ip) != "127.0.0.1") {
                        info.local_ip = ip;
                    }
                }
            }

            // Get MAC address
            if (pCurr->PhysicalAddressLength > 0) {
                std::ostringstream mac;
                for (UINT i = 0; i < pCurr->PhysicalAddressLength; ++i) {
                    if (i > 0) mac << ":";
                    mac << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<int>(pCurr->PhysicalAddress[i]);
                }
                info.mac_address = mac.str();
            }

            // If we got both IP and MAC, we're done
            if (info.local_ip != "N/A" && info.mac_address != "N/A") break;
        }
    }

    return info;
}

#else
// Linux/macOS implementation using getifaddrs and system commands

static std::string get_ssid_linux() {
    // Try iwgetid first (most common)
    std::string ssid = exec_command("iwgetid -r 2>/dev/null");
    if (!ssid.empty()) return ssid;

    // Try nmcli
    ssid = exec_command("nmcli -t -f active,ssid dev wifi 2>/dev/null | grep '^yes:' | cut -d: -f2");
    if (!ssid.empty()) return ssid;

    // Try iw dev
    ssid = exec_command("iw dev 2>/dev/null | grep -A1 'Interface' | grep ssid | head -1 | awk '{print $2}'");
    if (!ssid.empty()) return ssid;

    return "";
}

static std::string get_ssid_macos() {
    // Try networksetup
    std::string ssid = exec_command("networksetup -getairportnetwork en0 2>/dev/null | cut -d: -f2 | xargs");
    if (!ssid.empty() && ssid != "Off") return ssid;

    // Try en1, en2, etc.
    for (int i = 1; i < 5; ++i) {
        std::string cmd = "networksetup -getairportnetwork en" + std::to_string(i) + " 2>/dev/null | cut -d: -f2 | xargs";
        ssid = exec_command(cmd.c_str());
        if (!ssid.empty() && ssid != "Off") return ssid;
    }

    // Try airport utility (older macOS)
    ssid = exec_command("/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport -I 2>/dev/null | grep ' SSID:' | cut -d: -f2 | xargs");
    if (!ssid.empty()) return ssid;

    return "";
}

static NetworkInfo get_network_info_unix() {
    NetworkInfo info;

    // Get SSID
#ifdef __linux__
    std::string ssid = get_ssid_linux();
#elif __APPLE__
    std::string ssid = get_ssid_macos();
#endif
    if (!ssid.empty()) {
        info.ssid = ssid;
        info.is_wifi = true;
    } else {
        info.ssid = "N/A (Ethernet or unknown)";
    }

    // Get interface info via getifaddrs
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1) return info;

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        // Skip loopback
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        // Skip down interfaces
        if (!(ifa->ifa_flags & IFF_UP)) continue;
        // Skip non-running
        if (!(ifa->ifa_flags & IFF_RUNNING)) continue;

        // IPv4 address
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sa->sin_addr, ip, INET_ADDRSTRLEN);
            if (std::string(ip) != "127.0.0.1") {
                info.local_ip = ip;
                info.interface_name = ifa->ifa_name;
            }
        }

        // MAC address (AF_PACKET on Linux, AF_LINK on macOS)
        #ifdef __linux__
        if (ifa->ifa_addr->sa_family == AF_PACKET) {
            struct sockaddr_ll* sll = (struct sockaddr_ll*)ifa->ifa_addr;
            if (sll->sll_halen == 6) {
                std::ostringstream mac;
                for (int i = 0; i < 6; ++i) {
                    if (i > 0) mac << ":";
                    mac << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<int>(sll->sll_addr[i]);
                }
                info.mac_address = mac.str();
            }
        }
        #elif __APPLE__
        if (ifa->ifa_addr->sa_family == AF_LINK) {
            struct sockaddr_dl* sdl = (struct sockaddr_dl*)ifa->ifa_addr;
            if (sdl->sdl_alen == 6) {
                unsigned char* mac_bytes = (unsigned char*)LLADDR(sdl);
                std::ostringstream mac;
                for (int i = 0; i < 6; ++i) {
                    if (i > 0) mac << ":";
                    mac << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<int>(mac_bytes[i]);
                }
                info.mac_address = mac.str();
            }
        }
        #endif
    }

    freeifaddrs(ifaddr);
    return info;
}
#endif

// Cross-platform wrapper
static NetworkInfo get_network_info() {
#ifdef _WIN32
    return get_network_info_windows();
#else
    return get_network_info_unix();
#endif
}

// ============================================================================
// Public IP / ISP / Geolocation via ip-api.com
// ============================================================================

struct PublicInfo {
    std::string public_ip = "N/A";
    std::string isp = "N/A";
    std::string city = "N/A";
    std::string country = "N/A";
    std::string region = "N/A";
    std::string org = "N/A";
    std::string as = "N/A";
    double lat = 0.0;
    double lon = 0.0;
    bool success = false;
};

static PublicInfo get_public_info() {
    PublicInfo info;
    std::string response;
    long http_code = 0;
    double elapsed = 0;

    // Use ip-api.com (free, no API key needed, 45 req/min limit)
    const std::string url = "http://ip-api.com/json/?fields=status,message,query,isp,org,as,country,regionName,city,lat,lon";

    if (http_get(url, response, http_code, elapsed)) {
        if (json_extract(response, "status") == "success") {
            info.public_ip = json_extract(response, "query");
            info.isp = json_extract(response, "isp");
            info.org = json_extract(response, "org");
            info.as = json_extract(response, "as");
            info.country = json_extract(response, "country");
            info.region = json_extract(response, "regionName");
            info.city = json_extract(response, "city");
            info.lat = std::stod(json_extract(response, "lat"));
            info.lon = std::stod(json_extract(response, "lon"));
            info.success = true;
        }
    }
    return info;
}

// ============================================================================
// Machine ID (Linux only - /etc/machine-id)
// ============================================================================

static std::string get_machine_id() {
#ifdef __linux__
    std::string id = exec_command("cat /etc/machine-id 2>/dev/null | head -c 32");
    if (!id.empty()) return id;
    // Fallback to dbus machine-id
    id = exec_command("cat /var/lib/dbus/machine-id 2>/dev/null | head -c 32");
    return id.empty() ? "N/A" : id;
#else
    return "N/A (Linux only)";
#endif
}

// ============================================================================
// Speed Test Implementation
// ============================================================================

struct SpeedTestResult {
    double download_mbps = 0.0;
    double upload_mbps = 0.0;
    double latency_ms = 0.0;
    bool download_ok = false;
    bool upload_ok = false;
    bool ping_ok = false;
    std::string error_msg;
};

static SpeedTestResult run_speed_test() {
    SpeedTestResult result;

    // Generate test payload for upload (10 MB of random data)
    const size_t UPLOAD_SIZE = 10 * 1024 * 1024;  // 10 MB
    std::string upload_payload(UPLOAD_SIZE, 'x');
    // Fill with pseudo-random data to avoid compression
    for (size_t i = 0; i < UPLOAD_SIZE; ++i) {
        upload_payload[i] = static_cast<char>('A' + (i * 7 + 13) % 26);
    }

    // Download test - Cloudflare speed test endpoint (25 MB)
    std::cout << "  Testing download speed..." << std::flush;
    {
        std::string response;
        long http_code = 0;
        double elapsed = 0;
        size_t bytes_received = 0;

        // Cloudflare's speed test: 25 MB file
        const std::string dl_url = "https://speed.cloudflare.com/__down?bytes=25000000";

        if (http_get(dl_url, response, http_code, elapsed, &bytes_received)) {
            if (elapsed > 0 && bytes_received > 0) {
                double bits = bytes_received * 8.0;
                result.download_mbps = (bits / elapsed) / 1e6;
                result.download_ok = true;
            }
        } else {
            // Fallback to a smaller test file
            const std::string fallback_url = "https://speed.cloudflare.com/__down?bytes=5000000";
            if (http_get(fallback_url, response, http_code, elapsed, &bytes_received)) {
                if (elapsed > 0 && bytes_received > 0) {
                    double bits = bytes_received * 8.0;
                    result.download_mbps = (bits / elapsed) / 1e6;
                    result.download_ok = true;
                }
            } else {
                result.error_msg = "Download test failed";
            }
        }
    }
    std::cout << (result.download_ok ? " OK" : " FAILED") << std::endl;

    // Upload test - Cloudflare speed test endpoint
    std::cout << "  Testing upload speed..." << std::flush;
    {
        long http_code = 0;
        double elapsed = 0;
        size_t bytes_sent = 0;

        const std::string ul_url = "https://speed.cloudflare.com/__up";

        if (http_post(ul_url, upload_payload, http_code, elapsed, &bytes_sent)) {
            if (elapsed > 0 && bytes_sent > 0) {
                double bits = bytes_sent * 8.0;
                result.upload_mbps = (bits / elapsed) / 1e6;
                result.upload_ok = true;
            }
        } else {
            result.error_msg = "Upload test failed";
        }
    }
    std::cout << (result.upload_ok ? " OK" : " FAILED") << std::endl;

    // Ping test - multiple samples to Cloudflare
    std::cout << "  Testing latency..." << std::flush;
    {
        const std::string ping_url = "https://speed.cloudflare.com/__ping";
        std::vector<double> latencies;

        for (int i = 0; i < 5; ++i) {
            double latency = 0;
            if (http_ping(ping_url, latency)) {
                latencies.push_back(latency);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!latencies.empty()) {
            // Use median for robustness
            std::sort(latencies.begin(), latencies.end());
            result.latency_ms = latencies[latencies.size() / 2];
            result.ping_ok = true;
        } else {
            result.error_msg = "Ping test failed";
        }
    }
    std::cout << (result.ping_ok ? " OK" : " FAILED") << std::endl;

    return result;
}

// ============================================================================
// Output Formatting
// ============================================================================

// UTF-8 box drawing characters (multi-byte, use as string literals)
static constexpr const char* BOX_H = "─";   // U+2500
static constexpr const char* BOX_V = "│";   // U+2502
static constexpr const char* BOX_TL = "┌";  // U+250C
static constexpr const char* BOX_TR = "┐";  // U+2510
static constexpr const char* BOX_BL = "└";  // U+2514
static constexpr const char* BOX_BR = "┘";  // U+2518
static constexpr const char* BOX_T = "┬";   // U+252C
static constexpr const char* BOX_B = "┴";   // U+2534
static constexpr const char* BOX_L = "├";   // U+251C
static constexpr const char* BOX_R = "┤";   // U+2524
static constexpr const char* BOX_X = "┼";   // U+253C

// Helper to create a string of repeated UTF-8 characters
static std::string repeat_utf8(const char* ch, size_t count) {
    std::string result;
    result.reserve(count * 3); // UTF-8 box chars are 3 bytes each
    for (size_t i = 0; i < count; ++i) {
        result += ch;
    }
    return result;
}

static void print_header(const std::string& title) {
    std::cout << "\n" << BOX_TL << repeat_utf8(BOX_H, 78) << BOX_TR << "\n";
    std::cout << BOX_V << " " << std::left << std::setw(76) << title << " " << BOX_V << "\n";
    std::cout << BOX_BL << repeat_utf8(BOX_H, 78) << BOX_BR << "\n";
}

static void print_section(const std::string& title) {
    size_t padding = (title.length() < 74) ? 74 - title.length() : 0;
    std::cout << "\n" << BOX_TL << BOX_H << BOX_H << " " << title << " " << repeat_utf8(BOX_H, padding) << BOX_TR << "\n";
}

static void print_kv(const std::string& key, const std::string& value) {
    std::cout << BOX_V << " " << std::left << std::setw(28) << key << " " << BOX_V << " " << value << std::endl;
}

static void print_footer() {
    std::cout << BOX_BL << repeat_utf8(BOX_H, 78) << BOX_BR << "\n";
}

static void print_speed_result(const std::string& label, double value, bool ok, const std::string& unit = "Mbps") {
    constexpr const char* CHECK = "✓";   // U+2713
    constexpr const char* CROSS = "✗";   // U+2717
    std::string status = ok ? CHECK : CROSS;
    std::string formatted = ok ? format_speed_bps(value * 1e6) : "N/A";
    std::cout << BOX_V << " " << std::left << std::setw(28) << label << " " << BOX_V << " "
              << std::right << std::setw(12) << formatted << " " << status << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // UTF-8 box drawing characters for main header (double line style)
    constexpr const char* DB_H = "═";   // U+2550
    constexpr const char* DB_TL = "╔";  // U+2554
    constexpr const char* DB_TR = "╗";  // U+2557
    constexpr const char* DB_BL = "╚";  // U+255A
    constexpr const char* DB_BR = "╝";  // U+255D
    constexpr const char* DB_V = "║";   // U+2551

    std::cout << "\n" << DB_TL << repeat_utf8(DB_H, 78) << DB_TR << "\n";
    std::cout << DB_V << "                        NETDIAG - Network Diagnostics                        " << DB_V << "\n";
    std::cout << DB_V << "                    Cross-platform C++17 Network Tool                        " << DB_V << "\n";
    std::cout << DB_BL << repeat_utf8(DB_H, 78) << DB_BR << "\n";

    // -------------------------------------------------------------------------
    // Local Network Information
    // -------------------------------------------------------------------------
    print_section("LOCAL NETWORK INFORMATION");
    NetworkInfo net = get_network_info();
    print_kv("Interface", net.interface_name);
    print_kv("Connection Type", net.is_wifi ? "Wi-Fi" : "Ethernet / Unknown");
    print_kv("Wi-Fi SSID", net.ssid);
    print_kv("Local IPv4 Address", net.local_ip);
    print_kv("MAC Address", net.mac_address);

    std::string machine_id = get_machine_id();
    print_kv("Machine ID", machine_id);
    print_footer();

    // -------------------------------------------------------------------------
    // Public IP & ISP Information
    // -------------------------------------------------------------------------
    print_section("PUBLIC NETWORK & ISP INFORMATION");
    std::cout << "│ Fetching public IP and geolocation..." << std::string(43, ' ') << "│\n";
    PublicInfo pub = get_public_info();
    if (pub.success) {
        print_kv("Public IP", pub.public_ip);
        print_kv("ISP / Provider", pub.isp);
        print_kv("Organization", pub.org);
        print_kv("AS Number", pub.as);
        print_kv("Country", pub.country);
        print_kv("Region / State", pub.region);
        print_kv("City", pub.city);
        if (pub.lat != 0.0 || pub.lon != 0.0) {
            std::ostringstream coords;
            coords << std::fixed << std::setprecision(4) << pub.lat << ", " << pub.lon;
            print_kv("Coordinates (lat,lon)", coords.str());
        }
    } else {
        print_kv("Status", "Failed to retrieve public info (check internet connection)");
    }
    print_footer();

    // -------------------------------------------------------------------------
    // Internet Speed Test
    // -------------------------------------------------------------------------
    print_section("INTERNET SPEED TEST");
    std::cout << "│ Running speed tests (this may take 15-30 seconds)..." << std::string(26, ' ') << "│\n";
    SpeedTestResult speed = run_speed_test();

    if (speed.download_ok || speed.upload_ok || speed.ping_ok) {
        print_speed_result("Download Speed", speed.download_mbps, speed.download_ok);
        print_speed_result("Upload Speed", speed.upload_mbps, speed.upload_ok);

        constexpr const char* CHECK = "✓";   // U+2713
        constexpr const char* CROSS = "✗";   // U+2717
        std::string latency_str = speed.ping_ok ? format_ms(speed.latency_ms) : "N/A";
        std::string status = speed.ping_ok ? CHECK : CROSS;
        std::cout << BOX_V << " " << std::left << std::setw(28) << "Latency (Ping)" << " " << BOX_V << " "
                  << std::right << std::setw(12) << latency_str << " " << status << std::endl;
    } else {
        print_kv("Status", "All speed tests failed: " + speed.error_msg);
    }
    print_footer();

    // -------------------------------------------------------------------------
    // Summary
    // -------------------------------------------------------------------------
    print_header("SUMMARY");
    std::cout << "  Local IP:     " << net.local_ip << "\n";
    std::cout << "  Public IP:    " << pub.public_ip << "\n";
    std::cout << "  ISP:          " << pub.isp << "\n";
    std::cout << "  Connection:   " << (net.is_wifi ? "Wi-Fi (" + net.ssid + ")" : "Ethernet") << "\n";
    if (speed.download_ok) std::cout << "  Download:     " << format_speed_bps(speed.download_mbps * 1e6) << "\n";
    if (speed.upload_ok) std::cout << "  Upload:       " << format_speed_bps(speed.upload_mbps * 1e6) << "\n";
    if (speed.ping_ok) std::cout << "  Latency:      " << format_ms(speed.latency_ms) << "\n";
    std::cout << "\n";

    curl_global_cleanup();
    return 0;
}