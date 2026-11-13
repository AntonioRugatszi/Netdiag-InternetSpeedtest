# netdiag

Cross-platform C++17 network diagnostics & internet speed test.

 Android support via [Termux](https://termux.dev/) — run the same binary on your phone with enhanced Wi-Fi details (signal strength, frequency, link speed) when using [Termux:API](https://github.com/termux/termux-api).

## Features

- **Local Network**: Wi-Fi SSID, IPv4, MAC address, Machine ID (Linux/Android)
- **Public IP & ISP**: IP, provider, organization, AS number, geolocation (city/country/coordinates)
- **Speed Test**: Download, upload, and latency (ping) via Cloudflare endpoints
- **Android/Termux extras**: BSSID, RSSI (dBm), frequency (MHz), link speed (Mbps), Android device ID

## Quick Start

### Linux / macOS / Windows
```bash
g++ -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl
./netdiag
```

### Termux (Android)
```bash
# Install dependencies
pkg update && pkg upgrade
pkg install git clang libcurl termux-api

# Clone and build
git clone https://github.com/AntonioRugatszi/Netdiag-InternetSpeedtest.git
cd Netdiag-InternetSpeedtest
./build-termux.sh
./netdiag
```

**For enhanced Wi-Fi info (SSID, signal strength, frequency, link speed, BSSID):**
1. Install **Termux:API** app from [F-Droid](https://f-droid.org/packages/com.termux.api/) or Play Store
2. Run in Termux: `pkg install termux-api`
3. Grant location permission when prompted by `termux-wifi-connectioninfo`

> **Note:** Without Termux:API, the tool still works but falls back to standard Linux methods (limited Wi-Fi info on Android).

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
