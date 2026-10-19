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
import time
from contextlib import contextmanager
from importlib.metadata import PackageNotFoundError, version
from importlib.resources import as_file, files
from pathlib import Path

from .environment import _compatibility_mismatches, _machine


@contextmanager
def _examples():
    """The packaged examples, as the catalog names them: key -> file.

    One source for both `sigil examples` and `render --example`, read the
    way a browser reads any file opened by path. The files stand on disk
    while the context is open; outside an installed wheel there are none."""
    from .sketch import catalog

    directory = files("sigil").joinpath("examples")
    if not directory.is_dir():
        yield {}
        return
    with as_file(directory) as root:
        rows = catalog(sorted(Path(root).glob("*.py")))
        yield {row.key: row.path for row in rows if row.external}


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


def _grimoire(source, explicit):
    if explicit is not None:
        return _executable(explicit, "--grimoire")
    configured = os.environ.get("SIGIL_GRIMOIRE")
    if configured:
        return _executable(configured, "SIGIL_GRIMOIRE")
    found = shutil.which("Grimoire")
    if found:
        return _executable(found, "Grimoire on PATH")

    package = Path(__file__).resolve().parent
    origins = [Path.cwd(), source.parent, package]
    if package.parts[-4:] == ("apps", "python", "sigil", "sigil"):
        origins.append(package.parents[2] / "spell-circle-canvas")
    seen = set()
    for origin in origins:
        for directory in (origin, *origin.parents):
            if directory in seen:
                continue
            seen.add(directory)
            for relative in (
                "build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire",
                "build/bin/Release/Grimoire",
            ):
                candidate = directory / relative
                if candidate.is_file() and os.access(candidate, os.X_OK):
                    return candidate
    if sys.platform == "darwin":
        for directory in (Path("/Applications"), Path.home() / "Applications"):
            candidate = directory / "Grimoire.app/Contents/MacOS/Grimoire"
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return candidate
    raise FileNotFoundError(
        "Grimoire was not found. Build or install the native Grimoire app, "
        "then pass --grimoire /path/to/Grimoire or set SIGIL_GRIMOIRE."
    )


def _compatible_python(host, environment):
    if sys.implementation.name != "cpython":
        raise ValueError("Grimoire requires a CPython environment.")
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
            f"Grimoire did not answer --python-info within 10 seconds: {host}"
        ) from error
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or "").strip()
        raise RuntimeError(
            f"Could not query Grimoire's Python compatibility: {host}. "
            "Use a host with --python-info support." + (f" {detail}" if detail else "")
        ) from error
    try:
        info = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise RuntimeError(
            f"Grimoire returned invalid --python-info JSON: {host}"
        ) from error
    try:
        mismatches = _compatibility_mismatches(info, expected, "host", "current Python")
    except ValueError as error:
        raise RuntimeError(
            f"Grimoire returned invalid Python compatibility fields: {host}"
        ) from error
    if mismatches:
        raise ValueError(
            "Grimoire and the current Python environment are incompatible ("
            + "; ".join(mismatches)
            + "). Use a Grimoire built for this interpreter, or run sigil "
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
    host = _grimoire(source, explicit)
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


def _platform_state_root():
    """Where Grimoire keeps its state, its protocol's address file among it,
    when it was started without --state: the platform's application-local
    data location for the application."""
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Application Support" / "Grimoire"
    if sys.platform == "win32":
        local = os.environ.get("LOCALAPPDATA")
        return (Path(local) if local else Path.home() / "AppData" / "Local") / "Grimoire"
    data = os.environ.get("XDG_DATA_HOME")
    return (Path(data) if data else Path.home() / ".local" / "share") / "Grimoire"


def _endpoint(to):
    """The websocket address --to names: HOST:PORT, or a whole address."""
    return to if "://" in to else f"ws://{to}/sigil"


def _start_window(source, explicit, state, timeout=120.0):
    """Grimoire's window started on @p source with its evaluate door open,
    and a connection to it once it listens: (connection, process)."""
    from .protocol import connect

    host = _grimoire(source, explicit)
    if not sys.executable:
        raise ValueError("The current Python interpreter has no executable path.")
    executable = _executable(sys.executable, "Current Python interpreter")
    environment = os.environ.copy()
    environment.pop("PYTHONEXECUTABLE", None)
    environment.pop("__PYVENV_LAUNCHER__", None)
    abi = _compatible_python(host, environment)
    root = Path(state) if state is not None else _platform_state_root()
    address = root / "protocol-address"
    try:
        before = address.stat().st_mtime_ns
    except OSError:
        before = None
    arguments = [
        str(host),
        str(source),
        "--repl",
        "--python-executable",
        str(executable),
        "--python-abi",
        abi,
    ]
    if state is not None:
        arguments += ["--state", str(state)]
    process = subprocess.Popen(
        arguments,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        env=environment,
    )
    deadline = time.monotonic() + timeout
    try:
        while True:
            if process.poll() is not None:
                raise RuntimeError(
                    f"Grimoire ended with status {process.returncode} before it listened"
                )
            try:
                written = address.stat().st_mtime_ns
            except OSError:
                written = None
            if written is not None and written != before:
                try:
                    return connect(root), process
                except OSError:
                    pass
            if time.monotonic() > deadline:
                raise TimeoutError(f"Grimoire wrote no address under {root} in {timeout:.0f} s")
            time.sleep(0.1)
    except BaseException:
        _stop(process)
        raise


def _stop(process):
    """Ends a Grimoire this command started."""
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def _running_sketch(connection, source, timeout=60.0):
    """The file of the sketch the host runs, once one runs: @p source, where
    one was named, asked to open where the host opens what a client asks."""
    from .protocol import ErrorCode, Host, ProtocolError, Session

    deadline = time.monotonic() + timeout
    asked = False
    while True:
        sessions = Host(connection).describe().sessions
        running = Path(sessions[0].sketch) if sessions else None
        if running is not None and (
            source is None or (running.is_file() and running.resolve() == source)
        ):
            return running if running.is_file() else None
        if source is not None and not asked:
            asked = True
            try:
                Session(connection).open(sketch=str(source))
                continue
            except ProtocolError as error:
                if error.error.code != ErrorCode.notMounted:
                    raise RuntimeError(error.error.message) from error
                if running is not None:
                    raise RuntimeError(
                        f"Grimoire shows {running}; open {source} there, or "
                        "quit it and run sigil repl again"
                    ) from error
        if time.monotonic() > deadline:
            raise RuntimeError("Grimoire shows no running sketch")
        time.sleep(0.1)


def _prompt(options):
    """``sigil repl``: a prompt inside a sketch running in Grimoire."""
    from ._console import SketchConsole
    from .protocol import ProtocolError, Session, connect

    source = None
    if options.source is not None:
        source = options.source.expanduser().resolve()
        if not source.is_file():
            raise FileNotFoundError(f"Python sketch was not found: {source}")
        if source.suffix != ".py":
            raise ValueError(f"sigil repl evaluates Python; {source} is not a .py sketch")
    where = (
        _endpoint(options.to)
        if options.to
        else (options.state if options.state is not None else _platform_state_root())
    )
    process = None
    try:
        connection = connect(where)
    except OSError as error:
        if options.to or source is None:
            raise RuntimeError(
                f"no Grimoire answers at {where} ({error}); start one with "
                "--repl, or name a sketch file to start one on it"
            ) from error
        connection, process = _start_window(source, options.grimoire, options.state)
    try:
        sketch = _running_sketch(connection, source)
        session = Session(connection)
        try:
            session.evaluate(code="None")
        except ProtocolError as error:
            raise RuntimeError(
                f"{error.error.message}. Start Grimoire with --repl to open the "
                "door, or quit it and let sigil repl start one"
            ) from error
        SketchConsole(session, sketch=sketch).run()
    finally:
        connection.close()
        _stop(process)
    return 0


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
    live = commands.add_parser("open", help="open a sketch in the live Grimoire host")
    live.add_argument("source", type=Path, help="Python sketch file")
    live.add_argument("--grimoire", type=Path, help="native Grimoire executable")
    live.add_argument(
        "--publish",
        nargs="?",
        const=True,
        type=_publication,
        metavar="NAME",
        help="publish the live canvas over Syphon on macOS; defaults to the sketch name",
    )
    prompt = commands.add_parser(
        "repl",
        help="a Python prompt inside a sketch running in Grimoire",
        description=(
            "Read Python at a prompt and run each input inside the sketch a "
            "running Grimoire shows, through its evaluate door. The door "
            "runs any code a client on this machine sends, so Grimoire opens "
            "it only under --repl; with no Grimoire answering, this starts "
            "one on the named file with the door open, and ends it on :quit."
        ),
    )
    prompt.add_argument(
        "source", nargs="?", type=Path, help="Python sketch file to explore"
    )
    prompt.add_argument(
        "--to",
        metavar="HOST:PORT",
        help="the protocol endpoint of a running Grimoire",
    )
    prompt.add_argument(
        "--state",
        type=Path,
        help="the state directory whose protocol-address names the endpoint",
    )
    prompt.add_argument("--grimoire", type=Path, help="native Grimoire executable")
    arguments = list(sys.argv[1:] if argv is None else argv)
    forwarded = []
    if arguments[:1] == ["open"] and "--" in arguments:
        separator = arguments.index("--")
        forwarded = arguments[separator + 1 :]
        arguments = arguments[:separator]
    options = parser.parse_args(arguments)

    if options.command == "examples":
        with _examples() as examples:
            if examples:
                print("\n".join(sorted(examples)))
        return 0
    if options.command == "repl":
        try:
            return _prompt(options)
        except (OSError, RuntimeError, ValueError) as error:
            parser.exit(1, f"sigil: {error}\n")
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
            _open(options.source, options.grimoire, forwarded)
        except (OSError, RuntimeError, ValueError) as error:
            parser.exit(1, f"sigil: {error}\n")
        return 0
    if (options.source is None) == (options.example is None):
        parser.error("render requires a source file or --example NAME")

    from ._requirements import missing_requirements
    from .sketch import renderFile

    def _render(source, output):
        # A sketch names the modules it needs, and the browser marks an
        # entry unavailable by them; a render says the same thing rather
        # than letting the sketch's own import line raise.
        missing = missing_requirements(source)
        if missing:
            raise ValueError(
                f"{Path(source).name} declares modules this interpreter does "
                "not have: " + ", ".join(missing)
            )
        renderFile(source, output, at=options.at)

    try:
        output = options.output.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        if options.example is not None:
            with _examples() as examples:
                if options.example not in examples:
                    raise ValueError(
                        f"unknown example {options.example!r}; run 'sigil examples' to list sketches"
                    )
                _render(examples[options.example], output)
        else:
            _render(options.source.resolve(), output)
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(1, f"sigil: {error}\n")
    print(output)
    return 0
