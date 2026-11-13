# netdiag - Compilation Instructions

## Source File
`netdiag.cpp` - Single-file cross-platform C++17 network diagnostics tool

---

## Linux (Debian/Ubuntu)

### Install Dependencies
```bash
sudo apt update && sudo apt install -y libcurl4-openssl-dev build-essential
```

### Compile
```bash
g++ -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl
```

### Run
```bash
./netdiag
```

---

## Linux (Arch Linux)

### Install Dependencies
```bash
sudo pacman -S curl base-devel
```

### Compile
```bash
g++ -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl
```

---

## Linux (Fedora/RHEL/CentOS)

### Install Dependencies
```bash
sudo dnf install libcurl-devel gcc-c++ make
```

### Compile
```bash
g++ -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl
```

---

## macOS

### Install Dependencies (Homebrew)
```bash
brew install curl
```

### Compile
```bash
# With Homebrew curl (recommended)
g++ -std=c++17 -O2 -o netdiag netdiag.cpp -I/opt/homebrew/opt/curl/include -L/opt/homebrew/opt/curl/lib -lcurl

# Or with system curl (if available)
g++ -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl
```

### Run
```bash
./netdiag
```

---

## Windows (MinGW-w64 / MSYS2)

### Install Dependencies (MSYS2)
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-curl make
```

### Compile
```bash
g++ -std=c++17 -O2 -o netdiag.exe netdiag.cpp -lcurl -lws2_32 -lwlanapi -lole32
```

### Run
```bash
./netdiag.exe
```

---

<<<<<<< HEAD
=======
## Termux (Android)

### Install Dependencies
```bash
pkg update && pkg install -y clang libcurl termux-api
```

### Compile
```bash
# Using clang (Termux default)
clang++ -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl

# Or with the build script
./build-termux.sh
```

### Run
```bash
./netdiag
```

### Enhanced Wi-Fi Info (requires Termux:API)
1. Install **Termux:API** app from [F-Droid](https://f-droid.org/packages/com.termux.api/) or Play Store
2. Run: `pkg install termux-api`
3. Grant location permission when prompted

With Termux:API, netdiag shows:
- Wi-Fi SSID, BSSID
- Signal strength (RSSI in dBm)
- Frequency (MHz) and link speed (Mbps)
- Android device ID (instead of machine-id)

---

>>>>>>> origin/termux-support
## Windows (MSVC / Visual Studio)

### Using vcpkg
```bash
vcpkg install curl:x64-windows
```

### Compile (Developer Command Prompt)
```cmd
cl /std:c++17 /O2 /EHsc netdiag.cpp /link libcurl.lib ws2_32.lib wlanapi.lib ole32.lib
```

Or with CMake:
```cmake
cmake_minimum_required(VERSION 3.16)
project(netdiag LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(CURL REQUIRED)
add_executable(netdiag netdiag.cpp)
target_link_libraries(netdiag PRIVATE CURL::libcurl ws2_32 wlanapi ole32)
```

---

## Cross-Platform CMake (Recommended)

Create `CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.16)
project(netdiag LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find libcurl
find_package(CURL REQUIRED)

add_executable(netdiag netdiag.cpp)

target_link_libraries(netdiag PRIVATE CURL::libcurl)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(netdiag PRIVATE ws2_32 wlanapi ole32)
elseif(APPLE)
    # macOS - nothing extra needed typically
elseif(UNIX)
    # Linux - pthread may be needed
    find_package(Threads REQUIRED)
    target_link_libraries(netdiag PRIVATE Threads::Threads)
endif()

# Install target
install(TARGETS netdiag DESTINATION bin)
```

### Build with CMake
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

---

## Verification

After compilation, run:
```bash
./netdiag
```

Expected output shows:
- Local network interface, IP, MAC, SSID (if Wi-Fi)
- Public IP, ISP, location
- Download/upload speed in Mbps
- Latency in ms

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `curl/curl.h not found` | Install libcurl dev package (see above) |
| Linker error: `-lcurl` not found | Ensure libcurl is installed and in library path |
| Windows: `wlanapi.h` not found | Install Windows SDK or use MSYS2/MinGW-w64 |
| Speed test fails | Check firewall, try different endpoints in code |
| Permission denied on Linux | Some interfaces need root; run with `sudo` for full info |

---

## Customization

Edit `netdiag.cpp` to change:
- Speed test endpoints (lines ~480-520)
- Download test file size (Cloudflare `bytes=` parameter)
- Upload payload size (`UPLOAD_SIZE` constant)
- Number of ping samples (default: 5)
- Timeout values in `http_get`/`http_post`/`http_ping`

Popular alternative speed test endpoints:
- **Download**: `https://speed.hetzner.de/100MB.bin`, `https://proof.ovh.net/files/100Mb.dat`
- **Upload**: `https://httpbin.org/post`, `https://speed.cloudflare.com/__up`
- **Ping**: `https://1.1.1.1/cdn-cgi/trace`, `https://cloudflare.com/cdn-cgi/trace`