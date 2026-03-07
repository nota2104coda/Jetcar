import os
import subprocess
import sys

# This script is intended to be run by PlatformIO as a pre-script
# It regenerates MAVLink C headers from XML definitions.

try:
    from SCons.Script import Import
    Import("env")
    RUNNING_IN_PIO = True
except ImportError:
    RUNNING_IN_PIO = False

def run_mavgen():
    if RUNNING_IN_PIO:
        # env['PROJECT_DIR'] is e.g. src/mcu/esp32-PlatformIO
        project_dir = os.path.abspath(env['PROJECT_DIR'])
        script_dir = os.path.abspath(os.path.join(project_dir, "..", "include"))
    else:
        # Fallback for manual testing outside PIO
        script_dir = os.path.dirname(os.path.realpath(__file__))
        project_dir = os.path.abspath(os.path.join(script_dir, "..", "esp32-PlatformIO"))

    xml_dir = os.path.join(script_dir, "mavlink_definitions")
    
    # We expect the repo root to be three levels up from src/mcu/esp32-PlatformIO/
    # or two levels up from src/mcu/include/
    repo_root = os.path.abspath(os.path.join(script_dir, "..", "..", ".."))
    
    venv_python = os.path.join(repo_root, ".venv", "bin", "python3")
    python_exe = venv_python if os.path.exists(venv_python) else sys.executable
    mavgen_py = os.path.join(repo_root, ".venv", "bin", "mavgen.py")

    if os.path.exists(mavgen_py):
        base_cmd = [python_exe, mavgen_py]
        print(f"[MAVGEN] Using mavgen.py at {mavgen_py}")
    else:
        base_cmd = [python_exe, "-m", "pymavlink.tools.mavgen"]
        print("[MAVGEN] Falling back to 'python -m pymavlink.tools.mavgen'")

    # --- C Generation (MCU) ---
    output_dir_c = os.path.join(project_dir, "lib", "mavlink-arduino", "mavlink")
    os.makedirs(output_dir_c, exist_ok=True)
    cmd_c = base_cmd + [
        "--lang=C", "--wire-protocol=2.0", "--no-validate",
        "--output", output_dir_c,
        os.path.join(xml_dir, "all.xml")
    ]
    
    # --- Python Generation (ROS) ---
    # Generate directly into the ROS package so it is tracked by Git
    output_dir_py = os.path.join(repo_root, "src", "python_pkg", "python_pkg")
    os.makedirs(output_dir_py, exist_ok=True)
    cmd_py = base_cmd + [
        "--lang=Python", "--wire-protocol=2.0", "--no-validate",
        "--output", os.path.join(output_dir_py, "mavlink_ardupilotmega.py"),
        os.path.join(xml_dir, "ardupilotmega.xml")
    ]
    
    print(f"[MAVGEN] Generating C headers into: {output_dir_c}")
    try:
        subprocess.run(cmd_c, check=True, capture_output=True, text=True)
        print("[MAVGEN] C Success!")
    except subprocess.CalledProcessError as e:
        print(f"[MAVGEN] C Error:\n{e.stderr}")

    print(f"[MAVGEN] Generating Python dialect into: {output_dir_py}")
    try:
        # mavgen Python output is a single file
        subprocess.run(cmd_py, check=True, capture_output=True, text=True)
        print("[MAVGEN] Python Success!")
    except subprocess.CalledProcessError as e:
        print(f"[MAVGEN] Python Error:\n{e.stderr}")

def _pio_pre_build(target=None, source=None, env=None, **_):
    # SCons may pass keyword arguments like target/source; ignore what we don't need.
    run_mavgen()


if RUNNING_IN_PIO:
    # Generate headers right before linking the firmware image so normal builds trigger it
    env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", _pio_pre_build)
else:
    run_mavgen()
