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
- `src/main.cpp` now pulls in `MAVLink_ardupilotmega.h` from the vendored [`mavlink-arduino`](lib/mavlink-arduino) library and maps PicoW sensors into standard MAVLink messages (`HIGHRES_IMU`, `DISTANCE_SENSOR`, `ESC_TELEMETRY_1_TO_4`).
- If you regenerate headers under `lib/mavlink_generated` for a custom dialect, you can still include them from `main.cpp` (just update the include path) should you need bespoke messages in the future.

Notes
-----
- After generation, you should be able to build the project in PlatformIO normally. If the build can't find `mavlink.h`, ensure `lib/mavlink_generated` is present and contains the generated headers.
- If you want to use an Arduino MAVLink wrapper library instead of generated headers, you can add it via `lib_deps`, but generated headers give the most control.
