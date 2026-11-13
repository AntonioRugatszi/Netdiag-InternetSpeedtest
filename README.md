# netdiag

Cross-platform C++17 network diagnostics & internet speed test.

## Features

- **Local Network**: Wi-Fi SSID, IPv4, MAC address, Machine ID (Linux)
- **Public IP & ISP**: IP, provider, organization, AS number, geolocation (city/country/coordinates)
- **Speed Test**: Download, upload, and latency (ping) via Cloudflare endpoints

## Quick Start

```bash
./netdiag
```

### Termux (Android)

```bash
# One-line install & run
pkg install git clang libcurl && git clone https://github.com/yourusername/netdiag && cd netdiag && ./build-termux.sh && ./netdiag

# Or with the build script
./build-termux.sh
./netdiag
```

**For enhanced Wi-Fi info (SSID, signal strength, frequency, etc.):**
1. Install **Termux:API** app from [F-Droid](https://f-droid.org/packages/com.termux.api/) or Play Store
2. Run: `pkg install termux-api`
3. Grant location permission when prompted by `termux-wifi-connectioninfo`

## Build from Source

### Linux (Debian/Ubuntu)
```bash
sudo apt install libcurl4-openssl-dev
g++ -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl
```

### Linux (Arch)
```bash
sudo pacman -S curl
g++ -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl
```

### macOS
```bash
brew install curl
g++ -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl
```

### Windows (MinGW/MSYS2)
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-curl
g++ -std=c++17 -O2 -o netdiag.exe netdiag.cpp -lcurl -lws2_32 -lwlanapi -lole32
```

### Cross-platform (CMake)
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Output Example

```
┌── LOCAL NETWORK INFORMATION ─────────────────────────────────────────────────┐
│ Interface                    │ wlan0                                          │
│ Connection Type              │ Wi-Fi                                          │
│ Wi-Fi SSID                   │ MyNetwork                                      │
│ Local IPv4 Address           │ 192.168.1.42                                   │
│ MAC Address                  │ aa:bb:cc:dd:ee:ff                              │
│ Machine ID                   │ 1234567890abcdef...                            │
└──────────────────────────────────────────────────────────────────────────────┘

┌── PUBLIC NETWORK & ISP INFORMATION ──────────────────────────────────────────┐
│ Public IP                    │ 203.0.113.42                                   │
│ ISP / Provider               │ Example ISP                                    │
│ Country                      │ United States                                  │
│ City                         │ San Francisco                                  │
└──────────────────────────────────────────────────────────────────────────────┘

┌── INTERNET SPEED TEST ───────────────────────────────────────────────────────┐
│ Download Speed               │  125.42 Mbps ✓                                 │
│ Upload Speed                 │   42.18 Mbps ✓                                 │
│ Latency (Ping)               │       12 ms ✓                                  │
└──────────────────────────────────────────────────────────────────────────────┘
```

## Requirements

- C++17 compiler (GCC 7+, Clang 5+, MSVC 19.1+)
- libcurl development headers

## License

MIT