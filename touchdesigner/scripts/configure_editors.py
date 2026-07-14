#!/usr/bin/env python3
"""Set up the TouchDesigner Python scripting workspace: create the venv the
way TouchDesigner's own Python Env Manager does (via its bundled
TDPyEnvManagerHelper, run under TouchDesigner's interpreter) if it doesn't
exist yet, bootstrap uv into it, sync project dependencies, then generate
machine-local VSCode settings pointing at it.

Safe to re-run any time — it no-ops the venv creation if one already exists,
re-syncs dependencies, and always rewrites the settings file.

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

# Creates the venv in-process via TDPyEnvManagerHelper.createPythonEnv(), run under
# TouchDesigner's own interpreter, then force-exits: TD's helper leaves something in
# its shutdown path (unrelated to our venv work) that hangs the interpreter on a
# normal return, so a clean process exit relies on os._exit after the call succeeds.
_CREATE_VENV_SNIPPET = """
import os
import sys
from tdutils.TDPyEnvManagerHelper import TDPyEnvManagerHelper

helper = TDPyEnvManagerHelper()
helper.logger = helper.setupLogger()
helper.thread.start()
try:
    helper.createPythonEnv(sys.argv[1], sys.argv[2])
    helper.stopWorker()
    sys.stdout.flush()
    os._exit(0)
except Exception:
    sys.stdout.flush()
    os._exit(1)
"""


def find_touchdesigner_python() -> Path:
    """Finds the executable for TouchDesigner's bundled Python version."""
    current = TD_PYTHON_VERSIONS / "Current"
    if not current.exists():
        raise SystemExit(
            f"TouchDesigner not found at {TD_APP} — install it or update "
            "this script's path."
        )

    binary_directory = current / "bin"
    executable = next(
        (
            candidate
            for candidate in binary_directory.iterdir()
            if PYTHON_EXE_RE.match(candidate.name)
        ),
        None,
    )
    if executable is None:
        raise SystemExit(
            f"no python3.x executable found under {binary_directory}"
        )
    return executable


def ensure_environment(touchdesigner_python: Path) -> None:
    """Creates the editor environment and synchronizes its dependencies."""
    if not (VENV_DIR / "pyvenv.cfg").exists():
        print(
            f"creating venv at {VENV_DIR} via TDPyEnvManagerHelper "
            f"({touchdesigner_python})"
        )
        subprocess.run(
            [
                str(touchdesigner_python),
                "-c",
                _CREATE_VENV_SNIPPET,
                str(SCRIPTS_DIR),
                VENV_DIR.name,
            ],
            check=True,
        )

    environment_python = VENV_DIR / "bin" / "python"
    print("bootstrapping uv into venv")
    subprocess.run(
        [str(environment_python), "-m", "pip", "install", "-q", "uv"],
        check=True,
    )
    print("running uv sync")
    subprocess.run(
        [str(environment_python), "-m", "uv", "sync"],
        cwd=SCRIPTS_DIR,
        check=True,
    )


def main() -> None:
    """Configures the TouchDesigner Python environment and VS Code paths."""
    touchdesigner_python = find_touchdesigner_python()
    ensure_environment(touchdesigner_python)

    # ".../Versions/Current" resolves to the concrete ".../Versions/3.11".
    python_version_directory = touchdesigner_python.parents[1].resolve()
    python_version_tag = "python" + python_version_directory.name

    touchdesigner_site_packages = (
        python_version_directory
        / "lib"
        / python_version_tag
        / "site-packages"
    )
    if not touchdesigner_site_packages.is_dir():
        raise SystemExit(
            "expected TouchDesigner site-packages at "
            f"{touchdesigner_site_packages}, not found"
        )

    # Point Pylance at the venv's own interpreter (not TD's raw bundled one) so it
    # recognizes .venv/lib/.../site-packages as *the* environment's site-packages.
    # That's what makes it apply setuptools' editable-install finder (MAPPING dict
    # in __editable___*_finder.py) and resolve SpellCircle's real source for
    # autocomplete/types — a plain extraPaths entry doesn't get that treatment.
    environment_python = VENV_DIR / "bin" / python_version_tag
    analysis_paths = [str(touchdesigner_site_packages)]

    editor_settings = {
        "python.defaultInterpreterPath": str(environment_python),
        "python.analysis.autoSearchPaths": True,
        "python.autoComplete.extraPaths": analysis_paths,
        "python.analysis.extraPaths": analysis_paths,
        "python-envs.defaultEnvManager": "ms-python.python:venv",
    }

    vscode_directory = TOUCHDESIGNER_DIR / ".vscode"
    vscode_directory.mkdir(exist_ok=True)
    settings_path = vscode_directory / "settings.json"
    settings_path.write_text(json.dumps(editor_settings, indent=4) + "\n")

    print(f"wrote {settings_path}")
    print(f"  interpreter: {environment_python}")
    print(f"  extraPaths:  {analysis_paths}")


if __name__ == "__main__":
    main()
