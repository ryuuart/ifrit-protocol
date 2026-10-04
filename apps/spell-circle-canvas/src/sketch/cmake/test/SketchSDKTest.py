#!/usr/bin/env python3
"""Build an external plugin with normal CMake, then load it without a compiler."""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


def run(
    command: list[str], check: bool = True, **kwargs: object
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs
    )
    if check and result.returncode:
        raise RuntimeError(result.stdout)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--sdk", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--host", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--in-tree-plugin", required=True)
    parser.add_argument("--nm", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="sigil-sdk-toolchain-") as directory:
        build = Path(directory)
        sdk = build / "sdk"
        shutil.copytree(args.sdk, sdk)
        run(
            [
                args.cmake,
                "-S",
                args.source,
                "-B",
                str(build),
                "-G",
                "Ninja Multi-Config",
                f"-DSigilSketchSDK_DIR={sdk}",
                f"-DCMAKE_CXX_COMPILER={args.compiler}",
            ]
        )
        wrong = "Debug" if args.config != "Debug" else "Release"
        result = run(
            [args.cmake, "--build", str(build), "--config", wrong],
            check=False,
        )
        if (
            result.returncode == 0
            or "matching this SigilSketchSDK" not in result.stdout
        ):
            raise RuntimeError(
                "a mismatching configuration was not refused:\n" + result.stdout
            )
        run([args.cmake, "--build", str(build), "--config", args.config])
        artifacts = [
            path
            for path in (build / args.config).glob("example.*")
            if path.suffix in {".dylib", ".so", ".bundle"}
        ]
        if (
            len(artifacts) != 1
            or not Path(str(artifacts[0]) + ".sigil-build").is_file()
        ):
            raise RuntimeError(
                "the SDK did not emit one native module and its build sidecar"
            )
        environment = os.environ.copy()
        for artifact in (Path(args.in_tree_plugin), artifacts[0]):
            # A native module must leave the framework implementation to the
            # host, including the explicitly named Compose authoring target.
            undefined = run([args.nm, "-u", str(artifact)]).stdout
            if "compose3box" not in undefined:
                raise RuntimeError(
                    "the plugin copied or lost the host's Compose implementation"
                )
            environment["SIGIL_SKETCH_SDK_TEST_PLUGIN"] = str(artifact)
            print(
                run(
                    [
                        args.host,
                        "--gtest_filter=SketchPlugin.ARegularToolchainArtifactCanBeLoaded",
                    ],
                    env=environment,
                ).stdout
            )
        sidecar = Path(str(artifacts[0]) + ".sigil-build")
        metadata = sidecar.read_text()
        sidecar.unlink()
        run([args.cmake, "--build", str(build), "--config", args.config])
        if sidecar.read_text() != metadata:
            raise RuntimeError(
                "an unchanged external build did not restore its sidecar"
            )

        kit_build = build / "kit"
        run(
            [
                args.cmake,
                "-S",
                args.source,
                "-B",
                str(kit_build),
                "-G",
                "Ninja",
                f"-DSigilSketchSDK_DIR={sdk}",
                f"-DCMAKE_BUILD_TYPE={args.config}",
                f"-DCMAKE_CXX_COMPILER={args.compiler}",
                "-DSIGIL_TEST_PLUGIN_LIBRARIES=SigilComposeCore;SigilComposeKit",
            ]
        )
        run([args.cmake, "--build", str(kit_build)])
        kit_artifacts = [
            path
            for path in kit_build.glob("example.*")
            if path.suffix in {".dylib", ".so", ".bundle"}
        ]
        if len(kit_artifacts) != 1:
            raise RuntimeError(
                "an additional originating library did not build one module"
            )
        environment["SIGIL_SKETCH_SDK_TEST_PLUGIN"] = str(kit_artifacts[0])
        print(
            run(
                [
                    args.host,
                    "--gtest_filter=SketchPlugin.ARegularToolchainArtifactCanBeLoaded",
                ],
                env=environment,
            ).stdout
        )

        # Origin catalogs are independent of existing plugin fixtures, and
        # validating a Core consumer must not walk an unused Kit manifest.
        unconsumed = build / "unconsumed.h"
        unconsumed.write_text("struct KitValue {};\n")
        scoped = sdk / "origin-inputs.json"
        inputs = json.loads(scoped.read_text())
        inputs["SigilComposeKit"].append(
            f"{hashlib.sha256(unconsumed.read_bytes()).hexdigest()} {unconsumed}"
        )
        scoped.write_text(json.dumps(inputs))
        unconsumed.write_text("struct KitValue { int changed; };\n")
        run([args.cmake, "--build", str(build), "--config", args.config])
        kit = run([args.cmake, "--build", str(kit_build)], check=False)
        if kit.returncode == 0 or "SigilSketchSDK is stale" not in kit.stdout:
            raise RuntimeError(
                "a declared additional origin did not enforce its own manifest:\n"
                + kit.stdout
            )
        header = build / "authoring.h"
        header.write_text("struct AuthoringValue {};\n")
        manifest = sdk / "inputs.sha256"
        with manifest.open("a") as stream:
            stream.write(
                f"{hashlib.sha256(header.read_bytes()).hexdigest()} {header}\n"
            )
        header.write_text("struct AuthoringValue { int changed; };\n")
        stale = run(
            [args.cmake, "--build", str(build), "--config", args.config],
            check=False,
        )
        if stale.returncode == 0 or "SigilSketchSDK is stale" not in stale.stdout:
            raise RuntimeError("changed SDK headers were not refused:\n" + stale.stdout)


if __name__ == "__main__":
    main()
