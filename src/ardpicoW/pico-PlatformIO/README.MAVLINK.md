MAVLink integration (custom dialect)

Overview
--------
This project uses a small custom MAVLink dialect (`msgs/picow_car.xml`) to define two messages:
- `PICOW_TELEMETRY` (id 200)
- `PICOW_MOTOR_CMD` (id 201)

Workflow to generate C headers (one-time, or re-run when XML changes)
-------------------------------------------------------------------
1. Install pymavlink (on your dev machine):
   python -m pip install pymavlink

2. Run the helper script (from project root):
   ./scripts/generate_mavlink.sh

   This will generate C headers into `lib/mavlink_generated/`.

3. Add generated files to version control (recommended):
   git add lib/mavlink_generated

Why generate the headers?
------------------------
Using generated MAVLink C code gives you:
- Correct, tested packing/unpacking and CRC handling
- Convenient `mavlink_msg_<name>_pack` and `mavlink_msg_to_send_buffer()` helpers
- Compatibility with `pymavlink` on the Jetson side

How the code uses it
--------------------
- `MavlinkComm.cpp` chooses generated helpers when they are present (guarded with `#ifdef MAVLINK_MSG_ID_PICOW_TELEMETRY`).
- If you don't generate the headers, the project falls back to the earlier minimal native implementation (still usable for testing).

Notes
-----
- After generation, you should be able to build the project in PlatformIO normally. If the build can't find `mavlink.h`, ensure `lib/mavlink_generated` is present and contains the generated headers.
- If you want to use an Arduino MAVLink wrapper library instead of generated headers, you can add it via `lib_deps`, but generated headers give the most control.
