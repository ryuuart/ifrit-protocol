"""CMake wheel builds repaired by the platform's native wheel tooling."""

import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory

from scikit_build_core.build import (
    build_editable,
    build_sdist,
    get_requires_for_build_editable,
    get_requires_for_build_sdist,
    get_requires_for_build_wheel,
    prepare_metadata_for_build_editable,
    prepare_metadata_for_build_wheel,
)
from scikit_build_core.build import build_wheel as _build_wheel

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


def build_wheel(wheel_directory, config_settings=None, metadata_directory=None):
    destination = Path(wheel_directory).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="sigil-wheel-") as directory:
        staging = Path(directory)
        filename = _build_wheel(str(staging), config_settings, metadata_directory)
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
