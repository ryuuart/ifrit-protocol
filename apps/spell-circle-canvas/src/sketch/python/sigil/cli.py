"""Render sketches or open their live host from an installed Sigil package."""

import argparse
import json
import math
import os
import platform
import shutil
import struct
import subprocess
import sys
import sysconfig
from importlib.metadata import PackageNotFoundError, version
from importlib.resources import as_file, files
from pathlib import Path

from .environment import _compatibility_mismatches, _machine


def _examples():
    directory = files("sigil").joinpath("examples")
    if not directory.is_dir():
        return {}
    return {
        entry.name.removesuffix(".py"): entry
        for entry in directory.iterdir()
        if entry.name.endswith(".py")
    }


def _seconds(value):
    try:
        seconds = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("time must be a number of seconds") from error
    if not math.isfinite(seconds) or seconds < 0:
        raise argparse.ArgumentTypeError("time must be finite and nonnegative")
    return seconds


def _publication(value):
    if not value.strip():
        raise argparse.ArgumentTypeError("publication name must not be empty")
    return value


def _executable(path, label):
    path = Path(os.path.abspath(os.path.expanduser(path)))
    if not path.is_file() or not os.access(path, os.X_OK):
        raise ValueError(f"{label} is not an executable file: {path}")
    return path


def _sketchbook(source, explicit):
    if explicit is not None:
        return _executable(explicit, "--sketchbook")
    configured = os.environ.get("SIGIL_SKETCHBOOK")
    if configured:
        return _executable(configured, "SIGIL_SKETCHBOOK")
    found = shutil.which("Sketchbook")
    if found:
        return _executable(found, "Sketchbook on PATH")

    seen = set()
    for origin in (Path.cwd(), source.parent, Path(__file__).resolve().parent):
        for directory in (origin, *origin.parents):
            if directory in seen:
                continue
            seen.add(directory)
            for relative in (
                "build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook",
                "build/bin/Release/Sketchbook",
            ):
                candidate = directory / relative
                if candidate.is_file() and os.access(candidate, os.X_OK):
                    return candidate
    if sys.platform == "darwin":
        for directory in (Path("/Applications"), Path.home() / "Applications"):
            candidate = directory / "Sketchbook.app/Contents/MacOS/Sketchbook"
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return candidate
    raise FileNotFoundError(
        "Sketchbook was not found. Build or install the native Sketchbook app, "
        "then pass --sketchbook /path/to/Sketchbook or set SIGIL_SKETCHBOOK."
    )


def _compatible_python(host, environment):
    if sys.implementation.name != "cpython":
        raise ValueError("Sketchbook requires a CPython environment.")
    abi = sysconfig.get_config_var("SOABI")
    if not isinstance(abi, str) or not abi:
        raise ValueError("The current Python interpreter does not report its SOABI.")
    expected = {
        "implementation": "cpython",
        "version": [sys.version_info.major, sys.version_info.minor],
        "soabi": abi,
        "machine": _machine(platform.machine()),
        "pointer_bits": struct.calcsize("P") * 8,
    }
    try:
        result = subprocess.run(
            [str(host), "--python-info"],
            check=True,
            capture_output=True,
            text=True,
            timeout=10,
            env=environment,
        )
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(
            f"Sketchbook did not answer --python-info within 10 seconds: {host}"
        ) from error
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or "").strip()
        raise RuntimeError(
            f"Could not query Sketchbook's Python compatibility: {host}. "
            "Use a host with --python-info support." + (f" {detail}" if detail else "")
        ) from error
    try:
        info = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise RuntimeError(
            f"Sketchbook returned invalid --python-info JSON: {host}"
        ) from error
    try:
        mismatches = _compatibility_mismatches(info, expected, "host", "current Python")
    except ValueError as error:
        raise RuntimeError(
            f"Sketchbook returned invalid Python compatibility fields: {host}"
        ) from error
    if mismatches:
        raise ValueError(
            "Sketchbook and the current Python environment are incompatible ("
            + "; ".join(mismatches)
            + "). Use a Sketchbook built for this interpreter, or run sigil "
            "with a matching Python environment."
        )
    return abi


def _open(source, explicit, forwarded):
    for argument in forwarded:
        if argument.split("=", 1)[0] in {
            "--python-executable",
            "--python-abi",
            "--python-info",
        }:
            raise ValueError(
                f"sigil open owns {argument.split('=', 1)[0]}; it cannot be forwarded"
            )
    source = source.expanduser().resolve()
    if not source.is_file():
        raise FileNotFoundError(f"Python sketch was not found: {source}")
    if source.suffix != ".py":
        raise ValueError(f"sigil open requires a Python .py sketch: {source}")
    if not os.access(source, os.R_OK):
        raise PermissionError(f"Python sketch is not readable: {source}")
    host = _sketchbook(source, explicit)
    if not sys.executable:
        raise ValueError("The current Python interpreter has no executable path.")
    executable = _executable(sys.executable, "Current Python interpreter")
    environment = os.environ.copy()
    environment.pop("PYTHONEXECUTABLE", None)
    environment.pop("__PYVENV_LAUNCHER__", None)
    abi = _compatible_python(host, environment)
    os.execve(
        str(host),
        [
            str(host),
            str(source),
            "--python-executable",
            str(executable),
            "--python-abi",
            abi,
            *forwarded,
        ],
        environment,
    )


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="sigil", description="Render or open Python sketches with Sigil."
    )
    try:
        installed_version = version("sigil-sketch")
    except PackageNotFoundError:
        installed_version = "development"
    parser.add_argument(
        "--version", action="version", version=f"sigil {installed_version}"
    )
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("examples", help="list the sketches included in the package")
    render = commands.add_parser("render", help="render a sketch to a PNG image")
    render.add_argument("source", nargs="?", type=Path, help="Python sketch file")
    render.add_argument("--example", help="name of a packaged sketch")
    render.add_argument(
        "-o", "--output", type=Path, required=True, help="output PNG path"
    )
    render.add_argument(
        "--at",
        type=_seconds,
        help="scene seconds; defaults to the sketch's capture time",
    )
    live = commands.add_parser("open", help="open a sketch in the live Sketchbook host")
    live.add_argument("source", type=Path, help="Python sketch file")
    live.add_argument("--sketchbook", type=Path, help="native Sketchbook executable")
    live.add_argument(
        "--publish",
        nargs="?",
        const=True,
        type=_publication,
        metavar="NAME",
        help="publish the live canvas over Syphon on macOS; defaults to the sketch name",
    )
    arguments = list(sys.argv[1:] if argv is None else argv)
    forwarded = []
    if arguments[:1] == ["open"] and "--" in arguments:
        separator = arguments.index("--")
        forwarded = arguments[separator + 1 :]
        arguments = arguments[:separator]
    options = parser.parse_args(arguments)

    if options.command == "examples":
        print("\n".join(sorted(_examples())))
        return 0
    if options.command == "open":
        try:
            if options.publish is not None:
                if any(arg.split("=", 1)[0] == "--publish" for arg in forwarded):
                    raise ValueError("--publish must be supplied only once")
                forwarded.append(
                    "--publish"
                    if options.publish is True
                    else f"--publish={options.publish}"
                )
            _open(options.source, options.sketchbook, forwarded)
        except (OSError, RuntimeError, ValueError) as error:
            parser.exit(1, f"sigil: {error}\n")
        return 0
    if (options.source is None) == (options.example is None):
        parser.error("render requires a source file or --example NAME")

    from .sketch import render_file

    try:
        output = options.output.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        if options.example is not None:
            examples = _examples()
            if options.example not in examples:
                raise ValueError(
                    f"unknown example {options.example!r}; run 'sigil examples' to list sketches"
                )
            with as_file(examples[options.example]) as source:
                render_file(source, output, at=options.at)
        else:
            render_file(options.source.resolve(), output, at=options.at)
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(1, f"sigil: {error}\n")
    print(output)
    return 0
