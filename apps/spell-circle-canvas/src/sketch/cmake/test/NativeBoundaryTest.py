#!/usr/bin/env python3
"""Configure the native libraries without applications or bindings and load modules."""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--cache", required=True)
    parser.add_argument("--config", required=True)
    args = parser.parse_args()
    fixture = Path(__file__).parent / "native"
    directory = Path(tempfile.mkdtemp(prefix="sigil-native-boundary-"))

    def run(command: list[str], **kwargs: object) -> str:
        result = subprocess.run(
            command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            **kwargs,
        )
        with (directory / "commands.log").open("a") as log:
            log.write(repr(command) + "\n" + result.stdout + "\n")
        if result.returncode:
            raise RuntimeError(result.stdout)
        return result.stdout

    try:
        boundary = directory / "boundary"
        shutil.copytree(fixture / "boundary", boundary)
        source = directory / "Plugin.cpp"
        shutil.copyfile(fixture / "Plugin.cpp", source)
        build = directory / "build"
        configure = [
            args.cmake,
            "-S",
            str(args.source),
            "-B",
            str(build),
            "-G",
            "Ninja Multi-Config",
            "-C",
            args.cache,
            "-DSIGIL_BUILD_APPS=OFF",
            "-DSIGIL_BUILD_PYTHON=OFF",
            "-DSIGIL_SKETCH_NATIVE_FIXTURES=ON",
            "-DVCPKG_MANIFEST_INSTALL=OFF",
            f"-DCMAKE_BUILD_TYPE={args.config}",
            f"-DCMAKE_DEFAULT_BUILD_TYPE={args.config}",
            f"-DSIGIL_NATIVE_TEST_BOUNDARY_DIR={boundary}",
            f"-DSIGIL_NATIVE_TEST_PLUGIN_SOURCE={source}",
        ]
        run(configure)
        cache = (build / "CMakeCache.txt").read_text()
        if "Qt6_DIR:" in cache or "pybind11_DIR:" in cache:
            raise RuntimeError("native library configuration discovered Qt or pybind11")
        command = [args.cmake, "--build", str(build), "--config", args.config]
        run(
            [
                *command,
                "--target",
                "sketch_native_host",
                "sketch_native_plugin",
                "sketch_compose_consumer",
                "sketch_typography_consumer",
            ]
        )
        missing = subprocess.run(
            [*command, "--target", "sketch_typography_missing"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        if (
            missing.returncode == 0
            or "scramble" not in missing.stdout
            or not any(
                diagnostic in missing.stdout
                for diagnostic in (
                    "Undefined symbols",
                    "undefined reference",
                    "symbol(s) not found",
                )
            )
        ):
            raise RuntimeError(
                "a consumer without Typography did not fail at its symbol boundary:\n"
                + missing.stdout
            )
        host = build / "bin" / args.config / "sketch_native_host"
        consumer = build / "bin" / args.config / "sketch_compose_consumer"
        modules = [
            path
            for path in (build / "lib" / args.config).glob("sketch_native_plugin.*")
            if path.suffix in {".dylib", ".so", ".bundle"}
        ]
        if len(modules) != 1:
            raise RuntimeError("native build did not produce one module")
        module = modules[0]
        run([str(consumer)])
        run([str(build / "bin" / args.config / "sketch_typography_consumer")])
        run([str(host), str(module)])
        metadata = Path(str(module) + ".sigil-build")
        accepted = metadata.read_text()
        boundaries_path = build / "sdk" / args.config / "boundaries.json"
        identities = json.loads(boundaries_path.read_text())
        metadata.unlink()
        run([*command, "--target", "sketch_native_plugin"])
        if metadata.read_text() != accepted:
            raise RuntimeError(
                "unchanged build did not restore the compiled module's sidecar"
            )
        run([str(host), str(module)])

        # An unconsumed library can change while a freshly compiled plugin
        # still loads into the executable built against the prior catalog.
        (boundary / "include" / "Unrelated.h").write_text(
            "#pragma once\nstruct UnrelatedValue { double added; };\n"
        )
        with (boundary / "Build.cmake").open("a") as stream:
            stream.write("\n# An unrelated build declaration changed.\n")
        with source.open("a") as stream:
            stream.write("\n// Force a fresh plugin generation.\n")
        run([*command, "--target", "sketch_native_plugin"])
        if metadata.read_text().splitlines()[2:] != accepted.splitlines()[2:]:
            raise RuntimeError(
                "an unconsumed origin changed the plugin compatibility payload"
            )
        changed = json.loads(boundaries_path.read_text())
        if (
            identities["SigilSketchUnconsumedFixture"]
            == changed["SigilSketchUnconsumedFixture"]
        ):
            raise RuntimeError(
                "the changed unconsumed origin was not represented in the catalog"
            )
        run([str(host), str(module)])

        # This indirect public header is absent from the target's SOURCES.
        # Its changed layout must still be guarded before the factory is invoked.
        (boundary / "include" / "BoundaryDetails.h").write_text(
            "#pragma once\nstruct BoundaryValue { int width = 37; double added = 0; };\n"
        )
        run([*command, "--target", "sketch_native_plugin"])
        marker = directory / "factory-opened"
        environment = os.environ.copy()
        environment["SIGIL_NATIVE_FACTORY_MARKER"] = str(marker)
        run([str(host), str(module), "--reject"], env=environment)
        if marker.exists():
            raise RuntimeError("an incompatible module's factory ran before rejection")
        # Recovery may only copy the rejected artifact's embedded identity.
        rejected = metadata.read_text()
        metadata.unlink()
        run([*command, "--target", "sketch_native_plugin"])
        if metadata.read_text() != rejected:
            raise RuntimeError("sidecar recovery reassigned an old artifact's identity")
        run([str(host), str(module), "--reject"], env=environment)
        shutil.copyfile(
            fixture / "boundary" / "include" / "BoundaryDetails.h",
            boundary / "include" / "BoundaryDetails.h",
        )
        run([*command, "--target", "sketch_native_plugin"])
        run([str(host), str(module)])
        (boundary / "Build.cmake").write_text(
            "target_compile_definitions(SigilSketchNativeFixture INTERFACE SIGIL_NATIVE_BOUNDARY_MODE=1)\n"
        )
        run([*command, "--target", "sketch_native_plugin"])
        marker.unlink(missing_ok=True)
        run([str(host), str(module), "--reject"], env=environment)
        if marker.exists():
            raise RuntimeError(
                "a changed public compile requirement reached instance creation before rejection"
            )
        (boundary / "Build.cmake").write_text(
            'target_link_libraries(SigilSketchNativeFixture INTERFACE "$<$<BOOL:1>:SigilSketchUnconsumedFixture>")\n'
        )
        run([*command, "--target", "sketch_native_plugin"])
        run([str(host), str(module), "--reject"], env=environment)
        if marker.exists():
            raise RuntimeError(
                "an active changed public dependency reached instance creation before rejection"
            )

    except BaseException:
        print(
            f"Native boundary fixture and commands.log retained at {directory}",
            file=sys.stderr,
        )
        raise
    else:
        shutil.rmtree(directory)


if __name__ == "__main__":
    main()
