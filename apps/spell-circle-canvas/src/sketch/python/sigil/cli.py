"""Headless sketch rendering from an installed Sigil package."""

import argparse
import math
from importlib.metadata import PackageNotFoundError, version
from importlib.resources import as_file, files
from pathlib import Path


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


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="sigil", description="Render Python sketches with Sigil."
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
    options = parser.parse_args(argv)

    if options.command == "examples":
        print("\n".join(sorted(_examples())))
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
