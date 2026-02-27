import sys
import os
# Add the ROS package directory to path to import the local dialect
sys.path.append(os.path.join(os.getcwd(), "src/python_pkg/python_pkg"))
import mavlink_ardupilotmega as mavlink2
import struct

print(f"MAVLINK_MSG_ID_WHEEL_RPM = {mavlink2.MAVLINK_MSG_ID_WHEEL_RPM}")

# Sample data for testing (might need updated hex for WHEEL_RPM)
hex_data = "FD 20 00 00 BF C8 BF 16 2B 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 7D 00 FB 00 78 01 F6 01 D2 78"
data_bytes = bytes.fromhex(hex_data.replace(' ', ''))

mav_parser = mavlink2.MAVLink(None)
messages = []

for b in data_bytes:
    msg = mav_parser.parse_char(bytes([b]))
    if msg:
        messages.append(msg)

for msg in messages:
    print(f"Parsed message ID: {msg.get_msgId()}")
    print(f"Message object: {msg}")
    if msg.get_msgId() == mavlink2.MAVLINK_MSG_ID_WHEEL_RPM:
        print("Matched WHEEL_RPM")
    else:
        print("Did NOT match WHEEL_RPM")
