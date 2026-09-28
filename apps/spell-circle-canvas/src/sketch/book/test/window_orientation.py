#!/usr/bin/env python3
"""The presented window shows the canvas the same way up the still does.

The live canvas is a QQuickRhiItem that never runs a QRhi render pass: it
either lets Graphite write the item's texture or rasterises and uploads
the rows. Either way the texture is written top down. Qt mirrors the
textured quad on a backend whose framebuffers are y-up, which is the
compensation a render pass needs — so on such a backend the mirror is
uncompensated and the sketch presents upside down while the controls
around it stay upright. Nothing in a still says so: `--frame` never opens
a window.

So this photographs one. A probe sketch paints a red band across the top
of its canvas and a blue band across the bottom, and the shot of the real
window must carry the red band above the blue one — as the still of the
same sketch does, which is the control that the probe itself is the right
way up.

Usage (invoked by the build; the paths are all absolute):
  window_orientation.py --sketchbook <Sketchbook binary> \\
      --probe <orientation_probe.cpp> --work <scratch dir>
"""

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

# The bands, exactly as the probe paints them. Both survive a PNG
# encoder untouched, and neither is a colour the window's own controls
# fill any area with.
TOP_BAND = (255, 0, 0)
BOTTOM_BAND = (0, 0, 255)

# How many pixels of one band a row must hold to count as that band's.
# Above the width of any stray control pixel, below the width of the
# canvas in either the still or the shot.
ROW_RUN = 100


def read_png(path: Path) -> tuple[int, int, int, bytes]:
    """An 8-bit non-interlaced PNG as (width, height, channels, pixels).

    Read without a decoder: the chunks are walked, the IDAT streams
    inflated and every scanline unfiltered, which is all five filter
    types over a fixed channel count. Qt writes a grab as RGB and the
    still lane writes RGBA, so both colour types are read here.
    """
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{path} is not a PNG")
    width = height = channels = 0
    idat = b""
    offset = 8
    while offset + 8 <= len(data):
        (length,) = struct.unpack(">I", data[offset : offset + 4])
        chunk = data[offset + 4 : offset + 8]
        body = data[offset + 8 : offset + 8 + length]
        if chunk == b"IHDR":
            width, height, depth, colour, _, _, interlace = struct.unpack(
                ">IIBBBBB", body
            )
            channels = {2: 3, 6: 4}.get(colour, 0)
            if depth != 8 or interlace != 0 or not channels:
                sys.exit(
                    f"{path}: expected a non-interlaced 8-bit RGB or RGBA "
                    f"PNG, got depth {depth}, colour type {colour}, "
                    f"interlace {interlace}"
                )
        elif chunk == b"IDAT":
            idat += body
        elif chunk == b"IEND":
            break
        offset += 12 + length
    raw = zlib.decompress(idat)
    stride = width * channels
    pixels = bytearray(stride * height)
    previous = bytes(stride)
    at = 0
    for row in range(height):
        filter_type = raw[at]
        at += 1
        line = bytearray(raw[at : at + stride])
        at += stride
        for index in range(stride):
            left = line[index - channels] if index >= channels else 0
            above = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            if filter_type == 1:
                line[index] = (line[index] + left) & 0xFF
            elif filter_type == 2:
                line[index] = (line[index] + above) & 0xFF
            elif filter_type == 3:
                line[index] = (line[index] + ((left + above) >> 1)) & 0xFF
            elif filter_type == 4:
                estimate = left + above - upper_left
                distance_left = abs(estimate - left)
                distance_above = abs(estimate - above)
                distance_upper_left = abs(estimate - upper_left)
                if (
                    distance_left <= distance_above
                    and distance_left <= distance_upper_left
                ):
                    nearest = left
                elif distance_above <= distance_upper_left:
                    nearest = above
                else:
                    nearest = upper_left
                line[index] = (line[index] + nearest) & 0xFF
            elif filter_type != 0:
                sys.exit(f"{path}: unknown filter type {filter_type}")
        pixels[row * stride : (row + 1) * stride] = line
        previous = bytes(line)
    return width, height, channels, bytes(pixels)


def band_rows(picture: tuple[int, int, int, bytes], colour) -> list[int]:
    """Which rows hold a run of @p colour wide enough to be a band."""
    width, height, channels, pixels = picture
    rows = []
    for row in range(height):
        base = row * width * channels
        found = 0
        for column in range(width):
            at = base + column * channels
            if tuple(pixels[at : at + 3]) == colour:
                found += 1
        if found >= ROW_RUN:
            rows.append(row)
    return rows


def assert_red_above_blue(picture: tuple[int, int, int, bytes], what: str):
    red = band_rows(picture, TOP_BAND)
    blue = band_rows(picture, BOTTOM_BAND)
    if not red or not blue:
        sys.exit(
            f"{what}: found {len(red)} rows of the top band and "
            f"{len(blue)} of the bottom one — the probe did not render"
        )
    if max(red) >= min(blue):
        sys.exit(
            f"{what}: the top band reaches row {max(red)} and the bottom "
            f"band starts at row {min(blue)} — the canvas is upside down"
        )
    print(
        f"{what}: top band rows {min(red)}–{max(red)}, "
        f"bottom band rows {min(blue)}–{max(blue)}",
        flush=True,
    )


def run(command: list[str], environment: dict, expected_exit: int = 0) -> str:
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(
        command, capture_output=True, text=True, env=environment, timeout=120
    )
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if result.returncode != expected_exit:
        sys.exit(f"exited {result.returncode}, expected {expected_exit}")
    return result.stdout


def benchmark_file(sketchbook: Path, probe: Path, work: Path, environment: dict):
    """The window lane measures the selected source and observes its edits."""
    fixture = work / "external_window_subject.cpp"
    source = probe.read_text()
    for width, height in ((640, 420), (672, 448)):
        fixture.write_text(
            source.replace("kWidth = 640", f"kWidth = {width}").replace(
                "kHeight = 420", f"kHeight = {height}"
            )
        )
        output = run(
            [str(sketchbook), str(fixture), "--state", str(work / "state"),
             "--window-bench", "0.3", "--window-size", "900x700"],
            environment,
        )
        rows = [line for line in output.splitlines() if line.startswith("WINDOW ")]
        if len(rows) != 1 or not re.match(
            rf"WINDOW {fixture.stem} window=\S+ canvas={width}x{height} kind=canvas fps=",
            rows[0],
        ):
            sys.exit(f"the window benchmark did not measure the current file: {rows}")

    (work / "broken.sksl").write_text(
        "half4 main(float2 xy) { return missingColour; }"
    )
    fixture.write_text('''#include <sigilsketch/canvas/Sketch.h>
#include <sigilmaterial/program/Shader.h>
struct BrokenShader {
  void setup(sigil::sketch::SketchContext& ctx) {
    ctx.canvas(640, 420);
    ctx.composer.render(sigil::compose::box().width(640).height(420).fill(
        sigil::material::shader(ctx.assets.hub(), ctx.local("broken.sksl"))));
  }
};
SIGIL_SKETCH(BrokenShader, "Test", "a shader fallback fails the benchmark")
''')
    output = run(
        [str(sketchbook), str(fixture), "--state", str(work / "state"),
         "--window-bench", "0.3", "--window-size", "900x700"],
        environment, expected_exit=1,
    )
    rows = [line for line in output.splitlines() if line.startswith("WINDOW ")]
    if rows != [f"WINDOW {fixture.stem} FAILED resource or runtime error"]:
        sys.exit(f"the window benchmark accepted a shader fallback: {rows}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sketchbook", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--benchmark", action="store_true")
    args = parser.parse_args()

    work = args.work
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    if args.benchmark:
        benchmark_file(args.sketchbook, args.probe, work, environment)
        return
    # Both runs keep their state under the scratch directory: a build the
    # still left there is what the window reuses, and neither writes
    # outside it.
    state = str(work / "state")

    # THE CONTROL, and it comes first: a still of the same probe, which
    # opens no window and so cannot carry the defect. If the bands are
    # the wrong way round here the probe is wrong, not the presentation.
    still = work / "still.png"
    run(
        [
            str(args.sketchbook),
            "--state",
            state,
            str(args.probe),
            "--frame",
            str(still),
        ],
        environment,
    )
    assert_red_above_blue(read_png(still), "the still")

    # THE PRESENTED WINDOW, on the backend whose framebuffers are y-up.
    # The shot is the whole window, the canvas inside its chrome, so the
    # bands are found by colour rather than by position.
    shot = work / "window.png"
    environment["QSG_RHI_BACKEND"] = "opengl"
    run(
        [
            str(args.sketchbook),
            "--state",
            state,
            str(args.probe),
            "--shot",
            str(shot),
        ],
        environment,
    )
    assert_red_above_blue(read_png(shot), "the presented window")


if __name__ == "__main__":
    main()
