import sys
from smbus2 import SMBus, i2c_msg
from time import sleep

# Usage: python3 i2c_read_test.py <bus_num> <i2c_address> <num_bytes>
# if len(sys.argv) != 4:
#     print("Usage: python3 i2c_read_test.py <bus_num> <i2c_address> <num_bytes>")
#     sys.exit(1)

bus_num = 1
i2c_address = 0x42
num_bytes = 4


# bus_num = int(sys.argv[1])
# i2c_address = int(sys.argv[2], 0)
# num_bytes = int(sys.argv[3])
while True:
    sleep(0.1)
    with SMBus(bus_num) as bus:
        write_msg = i2c_msg.write(i2c_address, [0x00])  # Dummy write to trigger read
        read_msg = i2c_msg.read(i2c_address, num_bytes)
        bus.i2c_rdwr(write_msg, read_msg)
        data = bytearray(read_msg)
        print(f"I2C read ({num_bytes} bytes) from 0x{i2c_address:02X}: {[f'0x{b:02X}' for b in read_msg]}")
