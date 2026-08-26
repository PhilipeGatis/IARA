"""Stamps the UI version into the LittleFS image at buildfs time.

The version used to be baked into the JS bundle by vite, reading git at
`npm run build`. That can never be right: the natural order is build, then
commit, so at build time HEAD is still the *previous* commit and the tree is
dirty with the very changes being compiled. Every image reported its parent
commit with a `-dirty` suffix, understating what it actually contained.

The honest question is not "what was HEAD when the bundle was compiled" but
"which commit do the bundled assets belong to". Git answers that directly: the
last commit that touched `data/assets`. Doc-only commits landing afterwards no
longer bump a UI that did not change.

Written to `data/version.json`, which is generated rather than tracked — see
.gitignore. The dashboard fetches it and falls back to the compile-time value
when it is missing (the vite dev server, for instance).
"""

import json
import os
import subprocess

Import("env")

PROJECT_DIR = env["PROJECT_DIR"]
ASSETS_DIR = os.path.join("data", "assets")
OUTPUT = os.path.join(PROJECT_DIR, "data", "version.json")


def _git(*args: str) -> str:
    return (
        subprocess.check_output(["git", *args], cwd=PROJECT_DIR)
        .decode("utf-8", "replace")
        .strip()
    )


def _package_version() -> str:
    try:
        with open(os.path.join(PROJECT_DIR, "frontend", "package.json")) as f:
            return json.load(f).get("version", "0.0.0")
    except Exception:
        return "0.0.0"


def _ui_version() -> str:
    pkg = _package_version()
    try:
        commit = _git("log", "-1", "--format=%h", "--", ASSETS_DIR)
    except Exception:
        return pkg
    if not commit:
        return pkg  # assets have never been committed

    # Uncommitted frontend source means the bundle in data/ may not match it,
    # and uncommitted assets mean the bundle is newer than any commit. Either
    # way the commit above no longer identifies what is being packaged.
    try:
        pending = _git("status", "--porcelain", "--", ASSETS_DIR, "frontend/src")
    except Exception:
        pending = ""

    return f"{pkg}-{commit}{'-dirty' if pending else ''}"


version = _ui_version()
os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
with open(OUTPUT, "w") as f:
    json.dump({"ui": version}, f)

print(f"[fs_version] UI {version} -> data/version.json")
