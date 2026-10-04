#!/usr/bin/env python3
"""Build a normal CMake module and photograph its declared Canvas session."""

import argparse
import os
import subprocess
from pathlib import Path

from window_orientation import read_png


def run(command: list[str], work: Path, *, check: bool = True) -> str:
    environment = os.environ.copy()
    environment["CXX"] = "/no/compiler/in/the/native/host"
    environment["QT_QPA_PLATFORM"] = "offscreen"
    if "--gpu" in command:
        unavailable_driver = str(work / "no-vulkan-driver.json")
        environment["VK_DRIVER_FILES"] = unavailable_driver
        environment["VK_ICD_FILENAMES"] = unavailable_driver
    result = subprocess.run(
        command,
        cwd=work,
        env=environment,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=120,
    )
    with (work / "commands.log").open("a") as stream:
        stream.write(f"{command!r}\n{result.stdout}\n")
    if (
        check
        and result.returncode
        and "--gpu" in command
        and (
            "capture: no canvas GPU device" in result.stdout
            or "capture: the device has no Graphite context" in result.stdout
        )
    ):
        print(result.stdout)
        raise SystemExit(77)
    if check and result.returncode:
        raise RuntimeError(result.stdout)
    if not check and result.returncode == 0:
        raise RuntimeError("an incompatible capture request succeeded")
    return result.stdout


def inspect(path: Path, scale: float, *, blue: bool = False) -> bytes:
    width, height, channels, pixels = read_png(path)
    if (width, height) != (int(320 * scale), int(160 * scale)):
        raise RuntimeError(f"{path}: unexpected capture dimensions")

    def colour_at(horizontal: float, vertical: float) -> tuple[int, ...]:
        offset = (int(vertical * scale) * width + int(horizontal * scale)) * channels
        return tuple(pixels[offset : offset + 3])

    if colour_at(64, 64) != colour_at(4, 4):
        raise RuntimeError(f"{path}: the annulus hole lost its background")
    if colour_at(100, 64)[0] < 240 or colour_at(100, 64)[1] < 110:
        raise RuntimeError(f"{path}: the annulus lost its filled contour")
    marker = colour_at(300, 128)
    if marker[2 if blue else 0] < 240 or marker[0 if blue else 2] > 10:
        raise RuntimeError(f"{path}: the scene did not reach the requested time")
    glyph_pixels = sum(
        min(colour_at(horizontal, vertical)) > 200
        for vertical in range(20, 76)
        for horizontal in range(128, 282)
    )
    if glyph_pixels < 100:
        raise RuntimeError(f"{path}: the glyph run is missing")
    return pixels


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--sdk", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--sketchbook", required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--set-probe", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--gpu", action="store_true")
    args = parser.parse_args()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    (work / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(CapturePlugin LANGUAGES CXX)\n"
        "find_package(SigilSketchSDK CONFIG REQUIRED)\n"
        f'sigil_sketch_plugin(capture SOURCES "{args.probe}" '
        "LIBRARIES SigilComposeCore SigilDraw SigilGeometryKit SigilMaterialColor)\n"
        f'sigil_sketch_plugin(capture_set SOURCES "{args.set_probe}" '
        "LIBRARIES SigilWorldFrame)\n"
    )
    run(
        [
            args.cmake,
            "-S",
            str(work),
            "-B",
            str(work / "build"),
            "-G",
            "Ninja",
            f"-DSigilSketchSDK_DIR={args.sdk}",
            f"-DCMAKE_CXX_COMPILER={args.compiler}",
            f"-DCMAKE_BUILD_TYPE={args.config}",
        ],
        work,
    )
    run([args.cmake, "--build", str(work / "build")], work)
    modules = [
        path
        for path in (work / "build").glob("capture.*")
        if path.suffix in {".dylib", ".so", ".bundle"}
    ]
    if len(modules) != 1:
        raise RuntimeError("the toolchain did not emit one module")
    command = [
        args.sketchbook,
        "--plugin",
        str(modules[0]),
        "--state",
        str(work / "state"),
    ]
    if args.gpu:
        command += ["--gpu"]
    logs = []
    frame = work / "frame.png"
    logs.append(
        run([*command, "--frame", str(frame), "--at", "0.25", "--scale", "1.25"], work)
    )
    expected = inspect(frame, 1.25)
    plates = work / "plates"
    logs.append(
        run(
            [
                *command,
                "--headless",
                str(plates),
                "--at",
                "0.25",
                "--scale",
                "1.25",
                "--kind",
                "canvas",
            ],
            work,
        )
    )
    if inspect(plates / "plate_capture.png", 1.25) != expected:
        raise RuntimeError(
            "frame and headless module captures differ at the same moment"
        )
    logs.append(
        run(
            [
                *command,
                "--headless",
                str(plates),
                "--promotion",
                "--at",
                "0.75",
                "--scale",
                "0.5",
            ],
            work,
        )
    )
    inspect(plates / "plate_capture.png", 0.5, blue=True)
    logs.append(
        run([*command, "--headless", str(plates), "--no-promotion", "--at", "0"], work)
    )
    inspect(plates / "plate_capture.png", 1)
    sequence = work / "sequence.png"
    logs.append(
        run(
            [
                *command,
                "--frame",
                str(sequence),
                "--at",
                "0.47",
                "--frames",
                "3",
                "--fps",
                "60",
            ],
            work,
        )
    )
    for index in range(1, 4):
        inspect(work / f"sequence_{index:04d}.png", 1, blue=index > 1)
    wrong = run(
        [*command, "--headless", str(plates), "--kind", "set"], work, check=False
    )
    wrong_frame = run(
        [*command, "--frame", str(frame), "--kind", "set"], work, check=False
    )
    if "plugin runtime is canvas" not in wrong_frame:
        raise RuntimeError("the frame lane ignored its incompatible kind")
    if "plugin runtime is canvas" not in wrong:
        raise RuntimeError("the incompatible kind was not diagnosed")
    if "no device runtime" in wrong or "no device runtime" in wrong_frame:
        raise RuntimeError("the incompatible kind initialized the World device")
    for log in logs:
        if (
            f"backend: {'Graphite GPU' if args.gpu else 'CPU raster'}, runtime: canvas"
            not in log
        ):
            raise RuntimeError("the capture did not identify its actual backend")
        if args.gpu and "PLUGIN_CAPTURE recorder=Graphite" not in log:
            raise RuntimeError("the module did not draw into a Graphite canvas")
        if args.gpu and "PLUGIN_CAPTURE world_device=absent" not in log:
            raise RuntimeError("pure Canvas capture initialized the World device")
    if args.gpu:
        set_module = modules[0].with_name("capture_set" + modules[0].suffix)
        if not set_module.is_file():
            raise RuntimeError("the toolchain did not emit the Set module")
        failed_frame = work / "unavailable-set.png"
        failed_frame.unlink(missing_ok=True)
        failure = run(
            [
                args.sketchbook,
                "--plugin",
                str(set_module),
                "--gpu",
                "--frame",
                str(failed_frame),
                "--state",
                str(work / "set-state"),
            ],
            work,
            check=False,
        )
        if (
            "no device runtime" not in failure
            or "device executor could not open" not in failure
        ):
            raise RuntimeError("the Set did not require its unavailable World executor")
        if (
            "PLUGIN_SET_BODY" in failure
            or "PLUGIN_SET_SETUP" in failure
            or failed_frame.exists()
        ):
            raise RuntimeError("the Set opened or captured before its required device")
        logs.append(failure)
    (work / "capture.log").write_text("\n".join([*logs, wrong]))
    print(
        "prebuilt Canvas capture retained glyphs, holes, density and time without World"
    )


if __name__ == "__main__":
    main()
