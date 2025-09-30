#!/bin/bash

# POWER-SUPPLaY Project Setup Script
# Creates proper directory structure and builds the project

set -e  # Exit on error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Banner
echo "=================================================="
echo "    POWER-SUPPLaY Project Setup"
echo "    Acoustic Data Transmission Research Tool"
echo "    FOR EDUCATIONAL PURPOSES ONLY"
echo "=================================================="
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then 
   echo -e "${YELLOW}Warning: Running as root is not recommended${NC}"
   echo "The POWER-SUPPLaY transmitter doesn't require root privileges"
   read -p "Continue anyway? (y/N): " -n 1 -r
   echo
   if [[ ! $REPLY =~ ^[Yy]$ ]]; then
       exit 1
   fi
fi

# Create directory structure
echo -e "${GREEN}Creating project directory structure...${NC}"

mkdir -p src/transmitter
mkdir -p src/receiver
mkdir -p build
mkdir -p docs
mkdir -p experiments
mkdir -p samples
mkdir -p tests

echo "✓ Directories created"

# Check for required tools
echo -e "\n${GREEN}Checking dependencies...${NC}"

check_command() {
    if command -v $1 &> /dev/null; then
        echo "✓ $1 found"
        return 0
    else
        echo -e "${RED}✗ $1 not found${NC}"
        return 1
    fi
}

MISSING_DEPS=0

check_command gcc || MISSING_DEPS=1
check_command make || MISSING_DEPS=1
check_command pthread || true  # pthread is usually part of gcc

if [ $MISSING_DEPS -eq 1 ]; then
    echo -e "\n${RED}Missing dependencies detected!${NC}"
    echo "Please install the required packages:"
    echo ""
    echo "Ubuntu/Debian:"
    echo "  sudo apt-get install build-essential"
    echo ""
    echo "RHEL/CentOS/Fedora:"
    echo "  sudo yum groupinstall 'Development Tools'"
    echo ""
    echo "Arch Linux:"
    echo "  sudo pacman -S base-devel"
    exit 1
fi

# Check CPU information
echo -e "\n${GREEN}System Information:${NC}"
echo "CPU Model: $(grep -m 1 'model name' /proc/cpuinfo | cut -d: -f2 | xargs)"
echo "CPU Cores: $(nproc)"
echo "Architecture: $(uname -m)"
echo "Kernel: $(uname -r)"
echo "OS: $(lsb_release -ds 2>/dev/null || cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2 | tr -d '"')"

# Create source files if they don't exist
echo -e "\n${GREEN}Creating source files...${NC}"

# Create placeholder files if main files weren't copied
if [ ! -f "src/transmitter/main.c" ]; then
    echo "Creating placeholder for main.c..."
    cat > src/transmitter/main.c << 'EOF'
// Placeholder - Replace with actual main.c content
#include <stdio.h>
int main() {
    printf("POWER-SUPPLaY Transmitter\n");
    printf("Please replace this file with the actual implementation\n");
    return 0;
}
EOF
fi

if [ ! -f "src/transmitter/modulation.c" ]; then
    echo "Creating placeholder for modulation.c..."
    cat > src/transmitter/modulation.c << 'EOF'
// Placeholder - Replace with actual modulation.c content
#include <stdint.h>
uint8_t calculate_crc8(const uint8_t* data, size_t length) {
    return 0; // Placeholder
}
EOF
fi

# Create a simple Makefile if it doesn't exist
if [ ! -f "Makefile" ]; then
    echo "Creating basic Makefile..."
    cat > Makefile << 'EOF'
CC = gcc
CFLAGS = -Wall -O2 -pthread -lm
all: transmitter
transmitter: src/transmitter/main.c
	$(CC) src/transmitter/main.c -o build/transmitter $(CFLAGS)
clean:
	rm -f build/*
EOF
fi

# Build the project
echo -e "\n${GREEN}Building project...${NC}"
make clean 2>/dev/null || true
make all

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful!${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    echo "Please check the error messages above"
    exit 1
fi

# Create test samples
echo -e "\n${GREEN}Creating test samples...${NC}"

# Create a simple test WAV file (1 second 440Hz tone)
if command -v python3 &> /dev/null; then
    python3 << 'EOF'
import wave
import math
import struct

# Generate 1 second 440Hz tone
sample_rate = 44100
duration = 1
frequency = 440
amplitude = 0.5

# Generate samples
samples = []
for i in range(sample_rate * duration):
    sample = amplitude * math.sin(2 * math.pi * frequency * i / sample_rate)
    samples.append(int(sample * 32767))

# Write WAV file
with wave.open('samples/test_tone.wav', 'w') as wav:
    wav.setnchannels(1)  # Mono
    wav.setsampwidth(2)  # 16-bit
    wav.setframerate(sample_rate)
    for sample in samples:
        wav.writeframes(struct.pack('<h', sample))

print("✓ Created test_tone.wav")
EOF
else
    echo "Python3 not found - skipping WAV generation"
fi

# Create test data files
echo "POWER-SUPPLaY TEST DATA" > samples/test_data.txt
echo "0123456789ABCDEF" > samples/test_hex.txt
echo "✓ Created test data files"

# Create run script
echo -e "\n${GREEN}Creating run scripts...${NC}"

cat > run_demo.sh << 'EOF'
#!/bin/bash
# POWER-SUPPLaY Demo Script

echo "==================================="
echo "POWER-SUPPLaY Demonstration"
echo "==================================="
echo ""
echo "Available demos:"
echo "1) Generate 440Hz tone (audible)"
echo "2) Transmit 'HELLO' via FSK"
echo "3) Ultrasonic transmission (20kHz)"
echo "4) OFDM parallel transmission"
echo "5) Performance test"
echo ""
read -p "Select demo (1-5): " choice

case $choice in
    1)
        echo "Generating 440Hz tone for 3 seconds..."
        ./build/transmitter tone 440 3000 4
        ;;
    2)
        echo "Transmitting 'HELLO' using FSK modulation..."
        ./build/transmitter fsk 8000 8500 50 4 "HELLO"
        ;;
    3)
        echo "Ultrasonic transmission at 20kHz (inaudible to most adults)..."
        ./build/transmitter tone 20000 2000 4
        ;;
    4)
        if [ -f "./build/ofdm_transmitter" ]; then
            echo "OFDM transmission with 4 subcarriers..."
            ./build/ofdm_transmitter 8000 200 4 "OFDM"
        else
            echo "OFDM transmitter not built yet"
        fi
        ;;
    5)
        echo "Performance test with varying core counts..."
        for cores in 1 2 4; do
            echo "Testing with $cores cores:"
            ./build/transmitter tone 8000 1000 $cores
            sleep 1
        done
        ;;
    *)
        echo "Invalid choice"
        ;;
esac
EOF

chmod +x run_demo.sh
echo "✓ Created run_demo.sh"

# Create monitoring script
cat > monitor.sh << 'EOF'
#!/bin/bash
# Monitor CPU usage during POWER-SUPPLaY transmission

echo "CPU Usage Monitor"
echo "Press Ctrl+C to stop"
echo ""
echo "Time     CPU%   Freq(MHz)"
echo "-------- ------ ---------"

while true; do
    TIME=$(date +%H:%M:%S)
    CPU=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)
    FREQ=$(cat /proc/cpuinfo | grep "cpu MHz" | head -1 | awk '{print $4}')
    printf "%s  %5.1f%%  %8.0f\n" "$TIME" "$CPU" "$FREQ"
    sleep 1
done
EOF

chmod +x monitor.sh
echo "✓ Created monitor.sh"

# Create README if it doesn't exist
if [ ! -f "README.md" ]; then
    cat > README.md << 'EOF'
# POWER-SUPPLaY Project

Implementation of acoustic data transmission through power supply units.

## Quick Start

1. Run setup: `./setup.sh`
2. Run demo: `./run_demo.sh`
3. Monitor CPU: `./monitor.sh`

## Usage

Basic tone generation:
```bash
./build/transmitter tone <frequency> <duration_ms> <cores>
```

FSK data transmission:
```bash
./build/transmitter fsk <freq0> <freq1> <bit_ms> <cores> "message"
```

## Safety Warning

This tool generates acoustic signals through hardware manipulation.
Use responsibly and only for educational/research purposes.

## License

For educational and research purposes only.
EOF
    echo "✓ Created README.md"
fi

# Final setup complete
echo ""
echo "=================================================="
echo -e "${GREEN}Setup Complete!${NC}"
echo "=================================================="
echo ""
echo "Project structure:"
echo "  build/        - Compiled binaries"
echo "  src/          - Source code"
echo "  samples/      - Test samples"
echo "  docs/         - Documentation"
echo "  experiments/  - Experiment results"
echo ""
echo "Next steps:"
echo "  1. Run demo: ./run_demo.sh"
echo "  2. Monitor CPU: ./monitor.sh"
echo "  3. Read documentation: cat README.md"
echo ""
echo -e "${YELLOW}⚠ Warning: This tool generates acoustic signals${NC}"
echo -e "${YELLOW}  Use only for educational/research purposes${NC}"
echo -e "${YELLOW}  Some frequencies may be audible and annoying${NC}"
echo ""

# Performance warning
CORES=$(nproc)
if [ $CORES -lt 4 ]; then
    echo -e "${YELLOW}Note: Your system has $CORES CPU cores${NC}"
    echo -e "${YELLOW}      Performance may be limited${NC}"
    echo ""
fi

echo "Setup completed successfully!"