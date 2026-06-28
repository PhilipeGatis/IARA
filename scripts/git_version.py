import subprocess
import json
import os
Import("env")

def get_git_revision_hash() -> str:
    try:
        return subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD']).decode('ascii').strip()
    except Exception:
        return "unknown"

def is_dirty() -> bool:
    try:
        status = subprocess.check_output(['git', 'status', '--porcelain']).decode('ascii').strip()
        return len(status) > 0
    except Exception:
        return False

def get_package_version() -> str:
    try:
        # Assuming script runs from project root
        with open("frontend/package.json", "r") as f:
            data = json.load(f)
            return data.get("version", "0.0.0")
    except Exception:
        return "0.0.0"

pkg_version = get_package_version()
git_hash = get_git_revision_hash()
if is_dirty():
    git_hash += "-dirty"

full_version = f"{pkg_version}-{git_hash}"

env.Append(
    BUILD_FLAGS=[
        f'-D FIRMWARE_VERSION=\\"{full_version}\\"'
    ]
)
