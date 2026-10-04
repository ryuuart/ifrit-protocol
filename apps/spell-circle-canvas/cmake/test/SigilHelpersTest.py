#!/usr/bin/env python3
"""Build C++ consumers and validate documentation with the shared CMake helpers."""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
    )
    if result.returncode:
        raise RuntimeError(result.stdout)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--compiler", required=True)
    args = parser.parse_args()
    fixtures = Path(__file__).resolve().parent / "sigil_helpers"
    module_directory = fixtures.parent.parent
    with tempfile.TemporaryDirectory(prefix="sigil-cmake-helpers-") as directory:
        work = Path(directory)
        configure = [
            args.cmake,
            "-S",
            str(fixtures),
            "-G",
            "Ninja",
            f"-DSIGIL_MODULE_DIRECTORY={module_directory}",
            f"-DCMAKE_CXX_COMPILER={args.compiler}",
            f"-DPython3_EXECUTABLE={sys.executable}",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=TRUE",
        ]
        build = work / "consumers"
        run(configure + ["-B", str(build)])
        run([args.cmake, "--build", str(build)])

        missing = subprocess.run(
            configure + ["-B", str(work / "missing-chapter"), "-DMISSING_CHAPTER=ON"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        output = " ".join(missing.stdout.split())
        if (
            missing.returncode == 0
            or "MissingChapter.md" not in output
            or "does not exist" not in output
        ):
            raise RuntimeError(
                "a missing documentation chapter was not refused without Doxygen:\n"
                + missing.stdout
            )


if __name__ == "__main__":
    main()
