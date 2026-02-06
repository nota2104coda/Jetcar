from pymavlink.dialects.v20 import ardupilotmega as mavlink2
import struct

print(f"MAVLINK_MSG_ID_ESC_TELEMETRY_1_TO_4 = {mavlink2.MAVLINK_MSG_ID_ESC_TELEMETRY_1_TO_4}")

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
    if msg.get_msgId() == mavlink2.MAVLINK_MSG_ID_ESC_TELEMETRY_1_TO_4:
        print("Matched ESC_TELEMETRY_1_TO_4")
    else:
        print("Did NOT match ESC_TELEMETRY_1_TO_4")
