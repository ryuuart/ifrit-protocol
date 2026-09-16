"""Resolve one sketch project's compatible Python environment without importing Sigil."""

import argparse
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

import tomllib

_PYTHON_INFO = (
    "import json, platform, struct, sys, sysconfig; "
    "print(json.dumps({'implementation': sys.implementation.name, "
    "'version': list(sys.version_info[:2]), "
    "'soabi': sysconfig.get_config_var('SOABI'), 'machine': platform.machine(), "
    "'pointer_bits': struct.calcsize('P') * 8}))"
)


def _machine(value):
    value = value.casefold()
    return {"aarch64": "arm64", "amd64": "x86_64"}.get(value, value)


def _compatibility_mismatches(info, expected, actual_name, expected_name):
    """Validate and compare a native host or interpreter's compatibility report."""
    if (
        not isinstance(info, dict)
        or any(
            not isinstance(info.get(name), str) or not info[name]
            for name in ("implementation", "soabi", "machine")
        )
        or not isinstance(info.get("version"), list)
        or len(info["version"]) != 2
        or any(type(value) is not int for value in info["version"])
        or type(info.get("pointer_bits")) is not int
    ):
        raise ValueError("Invalid Python compatibility fields")
    normalized = {**info, "machine": _machine(info["machine"])}
    return [
        f"{name}: {actual_name} {normalized[name]!r}, {expected_name} {value!r}"
        for name, value in expected.items()
        if normalized[name] != value
    ]


def _version(value):
    if not re.fullmatch(r"[0-9]+\.[0-9]+", value):
        raise argparse.ArgumentTypeError("version must be major.minor, such as 3.14")
    return [int(part) for part in value.split(".")]


def _environment():
    environment = os.environ.copy()
    for name in (
        "VIRTUAL_ENV",
        "PYTHONHOME",
        "PYTHONPATH",
        "PYTHONEXECUTABLE",
        "__PYVENV_LAUNCHER__",
        "UV_PROJECT",
        "UV_PROJECT_ENVIRONMENT",
        "UV_PYTHON",
        "UV_WORKING_DIR",
    ):
        environment.pop(name, None)
    return environment


def _project(source, boundary):
    source = source.expanduser().resolve(strict=True)
    directory = source if source.is_dir() else source.parent
    if boundary is not None:
        boundary = boundary.expanduser().resolve(strict=True)
        if not boundary.is_dir() or not directory.is_relative_to(boundary):
            raise ValueError(
                f"Sketch source is outside the project search boundary: {boundary}"
            )
    for root in (directory, *directory.parents):
        pyproject = root / "pyproject.toml"
        configured = False
        if pyproject.is_file():
            try:
                with pyproject.open("rb") as stream:
                    settings = tomllib.load(stream)
            except tomllib.TOMLDecodeError as error:
                raise ValueError(
                    f"Invalid project configuration {pyproject}: {error}"
                ) from error
            tool = settings.get("tool", {})
            configured = isinstance(settings.get("project"), dict) or (
                isinstance(tool, dict) and isinstance(tool.get("uv"), dict)
            )
        if configured or (root / ".venv/pyvenv.cfg").is_file():
            return root, configured
        if root == boundary:
            break
    return None, False


def _uv(environment):
    found = shutil.which("uv", path=environment.get("PATH", os.defpath))
    if found:
        return Path(os.path.abspath(found))
    for candidate in (
        Path.home() / ".local/bin/uv",
        Path("/opt/homebrew/bin/uv"),
        Path("/usr/local/bin/uv"),
    ):
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    raise FileNotFoundError(
        "This Python project needs uv to prepare its dependencies. "
        "Install uv and make it available on PATH, then open the project again."
    )


def _run(arguments, root, environment, purpose, *, timeout=120):
    try:
        result = subprocess.run(
            [str(argument) for argument in arguments],
            cwd=root,
            env=environment,
            check=True,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(
            f"{purpose} timed out after {timeout} seconds in {root}."
        ) from error
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or error.stdout or "").strip()
        raise RuntimeError(
            f"{purpose} failed in {root}." + (f"\n{detail}" if detail else "")
        ) from error
    except OSError as error:
        raise RuntimeError(f"{purpose} could not start in {root}: {error}") from error
    return result.stdout.strip()


def _interpreter(root):
    return root / (
        ".venv/Scripts/python.exe" if os.name == "nt" else ".venv/bin/python"
    )


def _compatible(executable, expected, root, environment):
    executable = Path(os.path.abspath(executable))
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise ValueError(
            f"The project Python interpreter is missing or not executable: {executable}"
        )
    response = _run(
        [executable, "-I", "-c", _PYTHON_INFO],
        root,
        environment,
        "Inspecting the project Python interpreter",
        timeout=15,
    )
    try:
        info = json.loads(response)
    except json.JSONDecodeError as error:
        raise ValueError(
            f"The project interpreter returned invalid compatibility JSON: {executable}"
        ) from error
    try:
        mismatches = _compatibility_mismatches(info, expected, "project", "Sketchbook")
    except ValueError as error:
        raise ValueError(
            f"The project interpreter returned invalid compatibility fields: {executable}"
        ) from error
    if mismatches:
        raise ValueError(
            f"The project Python interpreter is incompatible with Sketchbook: {executable} ("
            + "; ".join(mismatches)
            + "). Use a compatible project environment or a matching Sketchbook build."
        )
    return {"executable": str(executable), "abi": info["soabi"]}


def resolve(source, *, version, abi, machine, pointer_bits, boundary=None):
    """Prepare the nearest project and return its validated interpreter handoff."""
    expected = {
        "implementation": "cpython",
        "version": version,
        "soabi": abi,
        "machine": _machine(machine),
        "pointer_bits": pointer_bits,
    }
    root, configured = _project(
        Path(source), None if boundary is None else Path(boundary)
    )
    if root is None:
        return {"executable": "", "abi": ""}
    environment = _environment()
    executable = _interpreter(root)
    existing = (root / ".venv/pyvenv.cfg").is_file()
    if existing:
        selected = _compatible(executable, expected, root, environment)
        if not configured:
            return selected
    pin = root / ".python-version"
    if pin.is_file():
        requested = [
            line.strip()
            for line in pin.read_text().splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        ]
        simple = (
            re.fullmatch(
                r"(?:cpython@?|cp)?([0-9]+)\.([0-9]+)(?:\.[0-9]+)?", requested[0]
            )
            if len(requested) == 1
            else None
        )
        if simple and [int(part) for part in simple.groups()] != version:
            raise ValueError(
                f"The project's {pin.name} requests Python {requested[0]}, but Sketchbook requires "
                + ".".join(map(str, version))
                + ". Use a matching project environment or Sketchbook build."
            )
    uv = _uv(environment)
    if pin.is_file():
        found = _run(
            [uv, "python", "find", "--project", root, "--system"],
            root,
            environment,
            "Finding the project's pinned Python interpreter",
        )
        _compatible(Path(found), expected, root, environment)
    command = [
        uv,
        "sync",
        "--project",
        root,
        "--inexact",
        "--no-install-project",
        "--no-install-package",
        "sigil-sketch",
    ]
    if not existing and not pin.is_file():
        command.extend(["--python", ".".join(map(str, version))])
    _run(command, root, environment, "Preparing Python project dependencies")
    return _compatible(executable, expected, root, environment)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="sketch file or project folder")
    parser.add_argument(
        "--version", type=_version, required=True, help="host Python major.minor"
    )
    parser.add_argument("--abi", required=True, help="host Python SOABI")
    parser.add_argument("--machine", required=True, help="host machine architecture")
    parser.add_argument("--pointer-bits", type=int, choices=(32, 64), required=True)
    parser.add_argument(
        "--boundary", type=Path, help="inclusive project search boundary"
    )
    options = parser.parse_args(argv)
    try:
        result = resolve(
            options.source,
            version=options.version,
            abi=options.abi,
            machine=options.machine,
            pointer_bits=options.pointer_bits,
            boundary=options.boundary,
        )
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(1, f"sigil environment: {error}\n")
    print(json.dumps(result))
    return 0


if __name__ == "__main__":
    main()
