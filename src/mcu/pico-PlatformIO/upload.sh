#!/bin/bash
# Quick upload script for Pico - holds BOOTSEL and reconnect USB, then run this

echo "Waiting for Pico in bootloader mode..."
for i in {1..20}; do 
    if [ -d "/media/$USER/RPI-RP2" ] || [ -d "/run/media/$USER/RPI-RP2" ]; then
        echo "✓ Pico detected! Uploading..."
        cp .pio/build/pico_w/firmware.uf2 /media/$USER/RPI-RP2/ 2>/dev/null || cp .pio/build/pico_w/firmware.uf2 /run/media/$USER/RPI-RP2/
        echo "✓ Upload complete! Pico will reboot."
        exit 0
    fi
    sleep 0.5
done
echo "✗ Timeout - Pico not detected. Hold BOOTSEL and reconnect USB."
exit 1
