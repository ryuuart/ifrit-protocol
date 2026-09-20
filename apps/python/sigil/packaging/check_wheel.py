"""Install a wheel with uv and render from an isolated directory."""

import argparse
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

SKETCH = """from sigil.compose import box, column, memo, text
from sigil.compose import kit as marks
from sigil.motion import Output, bind
from sigil.sketch import kit
from sigil.sketch import sketch


def content(words):
    look = kit.theme()
    return kit.well(
        (
            column()
            .gap(16)
            .children(
                [
                    text(words[0], size=22, color=look.palette.ink),
                    marks.line(length=160, thickness=24, fill=look.palette.figure),
                ]
            )
        ),
        width=208,
        height=128,
        padding=16,
    )


@sketch(size=(240, 160), background="#142333", capture_at=0.5)
class InstalledSketch:
    def setup(self, ctx):
        look = kit.house_theme()
        look.palette.cellGround = "#142333"
        look.palette.ink = "#e8eef2"
        look.palette.figure = "#8bd0bd"
        alpha = Output(1)
        with kit.provide(look):
            tree = memo(("Installed Sigil",), content)
        ctx.render(
            (
                box()
                .absolute()
                .inset(16)
                .opacity(bind(alpha))
                .children(
                    [
                        tree,
                    ]
                )
            )
        )
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wheel", type=Path)
    parser.add_argument(
        "--python",
        default=sys.executable,
        help="interpreter for the isolated environment",
    )
    parser.add_argument(
        "--output", type=Path, help="keep the verified PNG at this path"
    )
    parser.add_argument(
        "--checker",
        type=Path,
        default=shutil.which("basedpyright"),
        help="basedpyright executable for installed-package type checks",
    )
    options = parser.parse_args()
    if options.checker is None:
        parser.error(
            "basedpyright is required; install the typing dependency group or pass --checker"
        )
    checker = options.checker.resolve(strict=True)
    wheel = options.wheel.resolve(strict=True)
    with TemporaryDirectory(prefix="sigil-installed-") as directory:
        root = Path(directory)
        environment = os.environ.copy()
        environment.pop("PYTHONPATH", None)
        environment.pop("PYTHONHOME", None)
        environment["UV_CACHE_DIR"] = str(root / "cache")
        venv = root / "environment"
        subprocess.run(
            ["uv", "venv", str(venv), "--python", options.python],
            cwd=root,
            env=environment,
            check=True,
        )
        binary = venv / ("Scripts" if os.name == "nt" else "bin")
        python = binary / ("python.exe" if os.name == "nt" else "python")
        subprocess.run(
            ["uv", "pip", "install", "--no-index", "--python", str(python), str(wheel)],
            cwd=root,
            env=environment,
            check=True,
        )
        source = root / "scene.py"
        source.write_text(SKETCH)
        command = binary / ("sigil.exe" if os.name == "nt" else "sigil")
        examples = subprocess.check_output(
            [str(command), "examples"], cwd=root, env=environment, text=True
        )
        if not {"python_orbits", "python_live_signals"}.issubset(examples.splitlines()):
            raise RuntimeError(
                "The installed package does not include its example sketches"
            )
        subprocess.run(
            [
                str(command),
                "render",
                "--example",
                "python_orbits",
                "-o",
                str(root / "example.png"),
                "--at",
                "0",
            ],
            cwd=root,
            env=environment,
            check=True,
        )
        subprocess.run(
            [
                str(command),
                "render",
                "--example",
                "python_live_signals",
                "-o",
                str(root / "signals.png"),
            ],
            cwd=root,
            env=environment,
            check=True,
        )
        subprocess.run(
            [
                str(python),
                "-I",
                "-m",
                "sigil.examples.tools.send_live_signals",
                "--help",
            ],
            cwd=root,
            env=environment,
            check=True,
            stdout=subprocess.DEVNULL,
        )
        first, second = root / "command.png", root / "api.png"
        subprocess.run(
            [str(command), "render", str(source), "-o", str(first)],
            cwd=root,
            env=environment,
            check=True,
        )
        program = """import importlib.util, pathlib, sys, time
import sigil
from importlib.metadata import version
from sigil import compose, data, io, material, motion, sketch
from sigil.compose import kit as compose_kit
from sigil.motion import Output
from sigil.sketch import kit as sketch_kit
from sigil.sketch import render_file

root = pathlib.Path(sys.prefix).resolve()
assert pathlib.Path(sigil.__file__).resolve().is_relative_to(root)
extension = importlib.util.find_spec("_sigil")
assert extension is not None and extension.origin is not None
assert pathlib.Path(extension.origin).resolve().is_relative_to(root)
assert pathlib.Path(extension.origin).suffix != ".py", extension.origin
package = pathlib.Path(sigil.__file__).parent
assert (package / "py.typed").is_file()
assert (package / "__init__.pyi").is_file()
assert (package / "compose" / "__init__.pyi").is_file()
assert (package / "material" / "__init__.pyi").is_file()
assert not (package.parent / "_sigil").is_dir(), "the extension ships declarations"
assert not (package.parent / "sigil-stubs").is_dir(), "a second typed spelling"
assert not (package / "native.py").is_file()
assert compose_kit.__name__ == "sigil.compose.kit"
assert compose_kit.Well.__module__ == "sigil.compose.kit"
assert material.Paint.__module__ == "sigil.material"
assert not hasattr(material, "skia")
assert compose_kit.Well is compose.kit.Well
assert sketch_kit.Theme is sketch.kit.Theme
assert Output is motion.Output
assert data.Json.__module__ == "sigil.data"
assert io.Hub.__module__ == "sigil.io"
payload = {"values": [1, 2, 3], "label": "installed"}
assert data.decodeJson(data.encodeJson(payload)).to_python() == payload
assert data.Scale(domain=(0, 100), range=(20, 420))(25) == 120
assert compose_kit.line(length=24, thickness=2).__class__ is compose.Element
hub = io.Hub()
hub.mount("out://", pathlib.Path.cwd() / "output")
encoded = data.encodeJson(payload).encode("utf-8")
assert hub.write("out://readings.json", encoded)
assert hub.blob("out://readings.json") == encoded
io.registerUdp(hub)
listener = hub.feed("udp://:0")
assert listener.opened(), listener.error()
peer = hub.feed("udp://127.0.0.1:" + listener.address().rsplit(":", 1)[1])


def receive(feed):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        arrival = feed.receive()
        if arrival is not None:
            return arrival
        time.sleep(0.005)
    raise AssertionError("Installed SigilIO loopback did not receive its message")


try:
    assert peer.send(encoded), peer.error()
    arrival = receive(listener)
    assert arrival.bytes == encoded
    assert listener.sendTo(arrival.from_, b"ack")
    assert receive(peer).bytes == b"ack"
finally:
    peer.close()
    listener.close()
search_path = tuple(sys.path)
render_file(sys.argv[1], sys.argv[2])
assert tuple(sys.path) == search_path, "Rendering changed Python's package search path"
print("Installed", version("sigil-sketch"), "from", sigil.__file__)
"""
        subprocess.run(
            [str(python), "-I", "-c", program, str(source), str(second)],
            cwd=root,
            env=environment,
            check=True,
        )
        typing_check = (
            Path(__file__).resolve().parents[1] / "test" / "typing" / "check.py"
        )
        subprocess.run(
            [
                sys.executable,
                str(typing_check),
                "--python",
                str(python),
                "--checker",
                str(checker),
            ],
            cwd=root,
            env=environment,
            check=True,
        )
        image = first.read_bytes()
        if image != second.read_bytes():
            raise RuntimeError(
                "The installed CLI and Python API rendered different images"
            )
        if image[:8] != b"\x89PNG\r\n\x1a\n" or struct.unpack(">II", image[16:24]) != (
            240,
            160,
        ):
            raise RuntimeError(
                "The installed renderer did not produce the expected PNG"
            )
        if options.output:
            options.output.parent.mkdir(parents=True, exist_ok=True)
            options.output.write_bytes(image)
        print(
            "Installed CLI and isolated Python API produced identical 240 x 160 PNGs."
        )


if __name__ == "__main__":
    main()
