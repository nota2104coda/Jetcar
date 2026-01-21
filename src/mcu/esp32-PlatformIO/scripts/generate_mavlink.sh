#!/usr/bin/env bash
# Generate MAVLink C headers for the picow_car dialect using pymavlink/mavgen.
# Requirements: python -m pip install pymavlink
# Usage: ./generate_mavlink.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
MSGDIR="$ROOT/msgs"
OUTDIR="$ROOT/lib/mavlink_generated"
mkdir -p "$OUTDIR"

echo "Generating MAVLink headers for picow_car dialect..."
# Use mavgen via pymavlink
python - <<'PY'
from pymavlink import mavutil
from pymavlink.dialects.v20 import mavlink
import subprocess, sys, os
msgxml = os.path.join(os.path.dirname(__file__), '..', 'msgs', 'picow_car.xml')
outdir = os.path.join(os.path.dirname(__file__), '..', 'lib', 'mavlink_generated')
print('msgxml:', msgxml)
# Run mavgen C generator
cmd = [sys.executable, '-m', 'pymavlink.tools.mavgen', '--lang', 'C', '--wire-protocol', '2.0', msgxml, '-o', outdir]
print('Running:', ' '.join(cmd))
subprocess.check_call(cmd)
print('Generated files in', outdir)
PY

echo "Done. Add $OUTDIR to your include path (PlatformIO will find headers under lib/)." 

# End
