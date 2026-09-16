"""Install a wheel with uv and render from an isolated directory."""

import argparse
import os
import struct
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

SKETCH = """from sigil.compose import box, column, text
from sigil.sketch import sketch

@sketch(size=(240, 160), background="#142333", capture_at=0.5)
class InstalledSketch:
    def setup(self, ctx):
        ctx.render(column(
            text("Installed Sigil", size=22, color="#e8eef2"),
            box(width=160, height=24, fill="#8bd0bd", corners=6),
            padding=24, gap=16, absolute=True, inset=0,
        ))
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
    options = parser.parse_args()
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
from sigil.sketch import render_file
root = pathlib.Path(sys.prefix).resolve()
assert pathlib.Path(sigil.__file__).resolve().is_relative_to(root)
assert pathlib.Path(_sigil.__file__).resolve().is_relative_to(root)
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
