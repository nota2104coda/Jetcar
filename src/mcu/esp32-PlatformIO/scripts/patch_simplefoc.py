Import("env")
from pathlib import Path

def apply_patch(source, target):
    if not source.is_file() or not target.is_dir():
        return
    env.Execute([
        "patch",
        "-d",
        str(target),
        "-p1",
        "-i",
        str(source),
    ])

lib_root = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "Arduino-FOC"
patch_file = Path(env.subst("$PROJECT_DIR")) / "patches/simplefoc-idf5.patch"

def ensure_patch(*_, **__):
    apply_patch(patch_file, lib_root)

ensure_patch()
env.AddPreAction("$BUILD_DIR", ensure_patch)