"""CMake wheel builds repaired by the platform's native wheel tooling."""

import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

from scikit_build_core.build import build_editable as _build_editable
from scikit_build_core.build import build_sdist as _build_sdist
from scikit_build_core.build import build_wheel as _build_wheel
from scikit_build_core.build import (
    get_requires_for_build_editable,
    get_requires_for_build_sdist,
    get_requires_for_build_wheel,
    prepare_metadata_for_build_editable,
    prepare_metadata_for_build_wheel,
)

__all__ = [
    "build_editable",
    "build_sdist",
    "build_wheel",
    "get_requires_for_build_editable",
    "get_requires_for_build_sdist",
    "get_requires_for_build_wheel",
    "prepare_metadata_for_build_editable",
    "prepare_metadata_for_build_wheel",
]

PROJECT = Path(__file__).resolve().parents[1]
NATIVE_SOURCES = (
    "CMakeLists.txt",
    "CMakePresets.json",
    "COPYING",
    "COPYING.LESSER",
    "README.md",
    "vcpkg.json",
    "vcpkg-configuration.json",
    "cmake",
    "docs",
    "scripts",
    "src",
)


def native_source(project=PROJECT):
    """Use the sdist's native bundle or the checkout's sibling C++ project."""
    for source in (project / "native", project.parents[1] / "spell-circle-canvas"):
        if (source / "CMakeLists.txt").is_file():
            return source.resolve()
    raise RuntimeError(
        "The Sigil package needs its native source bundle or C++ checkout"
    )


def native_settings(config_settings=None):
    settings = dict(config_settings or {})
    settings.setdefault("cmake.source-dir", str(native_source()))
    settings.setdefault("cmake.define.SIGIL_PYTHON_PACKAGE_DIR", str(PROJECT))
    return settings


def build_sdist(sdist_directory, config_settings=None):
    settings = dict(config_settings or {})
    source = native_source()
    if source != PROJECT / "native":
        for name in NATIVE_SOURCES:
            settings[f"sdist.force-include.{source / name}"] = f"native/{name}"
    return _build_sdist(sdist_directory, settings)


def build_editable(wheel_directory, config_settings=None, metadata_directory=None):
    return _build_editable(
        wheel_directory, native_settings(config_settings), metadata_directory
    )


def build_wheel(wheel_directory, config_settings=None, metadata_directory=None):
    destination = Path(wheel_directory).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="sigil-wheel-") as directory:
        staging = Path(directory)
        filename = _build_wheel(
            str(staging), native_settings(config_settings), metadata_directory
        )
        artifact = staging / filename
        if sys.platform == "darwin":
            repaired = staging / "repaired"
            subprocess.run(
                [
                    sys.executable,
                    "-m",
                    "delocate.cmd.delocate_wheel",
                    "--check-archs",
                    "--wheel-dir",
                    str(repaired),
                    str(artifact),
                ],
                check=True,
            )
            (artifact,) = repaired.glob("*.whl")
        final = destination / artifact.name
        artifact.replace(final)
        return final.name
