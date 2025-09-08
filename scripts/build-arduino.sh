#!/bin/bash
# Arduino compilation script

set -e

echo "Building Arduino projects..."

# Create build directory
mkdir -p build

# Build Pi Pico W project
echo "Compiling Pi Pico W firmware..."
docker-compose -f docker/docker-compose.dev.yml run --rm arduino-dev \
    arduino-cli compile \
    --fqbn rp2040:rp2040:rpipicow \
    --output-dir /workspace/build/picow \
    picow/pico-arduinoIDE/picoW-robot-master/picoW-robot-master.ino

# Build Arduino Mega project
echo "Compiling Arduino Mega firmware..."
docker-compose -f docker/docker-compose.dev.yml run --rm arduino-dev \
    arduino-cli compile \
    --fqbn arduino:avr:mega \
    --output-dir /workspace/build/arduino-mega \
    arduino-mega/ArdMega-robot-slave/ArdMega-robot-slave.ino

echo "Build complete! Binaries are in the build/ directory"
echo "Pi Pico W: build/picow/"
echo "Arduino Mega: build/arduino-mega/"