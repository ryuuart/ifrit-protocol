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
    return kit.well(column(
        text(words[0], size=22, color=look.palette.ink),
        marks.line(length=160, thickness=24, fill=look.palette.figure),
        gap=16,
    ), width=208, height=128, padding=16)

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
        ctx.render(box(tree, absolute=True, inset=16, opacity=bind(alpha)))
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
        if "python_orbits" not in examples.splitlines():
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
        first, second = root / "command.png", root / "api.png"
        subprocess.run(
            [str(command), "render", str(source), "-o", str(first)],
            cwd=root,
            env=environment,
            check=True,
        )
        program = """import pathlib, sys
import sigil, _sigil
from importlib.metadata import version
from sigil import data
from sigil.compose import kit as compose_kit
from sigil.motion import Output
from sigil.native import compose, data as native_data, motion, sketch
from sigil.sketch import kit as sketch_kit
from sigil.sketch import render_file
root = pathlib.Path(sys.prefix).resolve()
assert pathlib.Path(sigil.__file__).resolve().is_relative_to(root)
assert pathlib.Path(_sigil.__file__).resolve().is_relative_to(root)
package = pathlib.Path(sigil.__file__).parent
assert (package / "py.typed").is_file()
assert (package / "compose" / "__init__.pyi").is_file()
assert (package.parent / "_sigil" / "__init__.pyi").is_file()
assert (package.parent / "_sigil" / "py.typed").is_file()
assert compose_kit.__name__ == "sigil.compose.kit"
assert compose_kit.Well is compose.kit.Well
assert sketch_kit.Theme is sketch.kit.Theme
assert Output is motion.Output
assert data.Json is native_data.Json
payload = {"values": [1, 2, 3], "label": "installed"}
assert data.decodeJson(data.encodeJson(payload)).to_python() == payload
assert data.Scale(domain=(0, 100), range=(20, 420))(25) == 120
assert compose_kit.line(length=24, thickness=2).__class__ is compose.Element
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
