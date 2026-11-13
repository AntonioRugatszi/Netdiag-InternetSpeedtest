#!/bin/bash
# Build script for netdiag on Termux (Android)

set -e

echo "=== netdiag Termux Build Script ==="
echo

# Check if running in Termux
if [ -z "$TERMUX_VERSION" ]; then
    echo "Warning: Not running in Termux environment"
    echo "TERMUX_VERSION not set. Continue anyway? (y/N)"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo "Installing dependencies..."
pkg update && pkg install -y git clang libcurl openssl-tool termux-api

echo
echo "Building netdiag..."
# Use clang (Termux default) or g++ if available
if command -v clang++ &> /dev/null; then
    CXX=clang++
else
    CXX=g++
fi

$CXX -std=c++17 -O2 -o netdiag netdiag.cpp -lcurl

echo
echo "Build successful! Run with: ./netdiag"
echo
echo "=== Optional: Install Termux:API for enhanced Wi-Fi info ==="
echo "1. Install Termux:API app from F-Droid or Play Store"
echo "2. Run: pkg install termux-api"
echo "3. Grant location permission for Wi-Fi scanning:"
echo "   termux-wifi-connectioninfo"
echo
echo "Features enabled with Termux:API:"
echo "  - Wi-Fi SSID, BSSID, signal strength (RSSI)"
echo "  - Frequency, link speed, MAC address"
echo "  - Android device ID (instead of machine-id)"