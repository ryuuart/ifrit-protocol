#!/usr/bin/env python3
"""Set up the TouchDesigner Python scripting workspace: create the uv venv
against TouchDesigner's bundled interpreter if it doesn't exist yet, then
generate machine-local VSCode settings pointing at it.

Safe to re-run any time — it no-ops the venv creation if one already exists,
and always rewrites the settings file.

Usage: python3 configure_editors.py
"""

from __future__ import annotations

import json
import re
import subprocess
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parent
TOUCHDESIGNER_DIR = SCRIPTS_DIR.parent
VENV_DIR = SCRIPTS_DIR / ".venv"

TD_APP = Path("/Applications/TouchDesigner.app")
TD_PYTHON_VERSIONS = TD_APP / "Contents/Frameworks/Python.framework/Versions"

PYTHON_EXE_RE = re.compile(r"^python3\.\d+$")


def find_td_python() -> Path:
    current = TD_PYTHON_VERSIONS / "Current"
    if not current.exists():
        raise SystemExit(f"TouchDesigner not found at {TD_APP} — install it or update this script's path.")

    bin_dir = current / "bin"
    exe = next((p for p in bin_dir.iterdir() if PYTHON_EXE_RE.match(p.name)), None)
    if exe is None:
        raise SystemExit(f"no python3.x executable found under {bin_dir}")
    return exe


def ensure_venv(td_python: Path) -> None:
    if (VENV_DIR / "pyvenv.cfg").exists():
        return
    print(f"creating venv at {VENV_DIR} using {td_python}")
    subprocess.run(["uv", "sync", "--python", str(td_python)], cwd=SCRIPTS_DIR, check=True)


def main() -> None:
    td_python = find_td_python()
    ensure_venv(td_python)

    version_dir = td_python.parents[1].resolve()  # ".../Versions/Current" -> ".../Versions/3.11"
    py_tag = "python" + version_dir.name

    td_site_packages = version_dir / "lib" / py_tag / "site-packages"
    if not td_site_packages.is_dir():
        raise SystemExit(f"expected TouchDesigner site-packages at {td_site_packages}, not found")

    # Point Pylance at the venv's own interpreter (not TD's raw bundled one) so it
    # recognizes .venv/lib/.../site-packages as *the* environment's site-packages.
    # That's what makes it apply setuptools' editable-install finder (MAPPING dict
    # in __editable___*_finder.py) and resolve SpellCircle's real source for
    # autocomplete/types — a plain extraPaths entry doesn't get that treatment.
    venv_python = VENV_DIR / "bin" / py_tag
    extra_paths = [str(td_site_packages)]

    vscode_settings = {
        "python.defaultInterpreterPath": str(venv_python),
        "python.analysis.autoSearchPaths": True,
        "python.autoComplete.extraPaths": extra_paths,
        "python.analysis.extraPaths": extra_paths,
        "python-envs.defaultEnvManager": "ms-python.python:venv",
    }

    vscode_dir = TOUCHDESIGNER_DIR / ".vscode"
    vscode_dir.mkdir(exist_ok=True)
    (vscode_dir / "settings.json").write_text(json.dumps(vscode_settings, indent=4) + "\n")

    print(f"wrote {vscode_dir / 'settings.json'}")
    print(f"  interpreter: {venv_python}")
    print(f"  extraPaths:  {extra_paths}")


if __name__ == "__main__":
    main()
