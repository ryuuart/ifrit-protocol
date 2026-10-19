"""The authoring tools' texture folders by role, and the sprite atlas.

Input contracts for the erased signatures of the
material/texture-sets package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table

PATH_INPUT = "os.PathLike[str] | os.PathLike[bytes] | str | bytes"
PIXELS = "_sigil.media.Image | _sigil.media.Video | _sigil.skia.Image"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    texture = "_sigil.material.texture"
    files = f"collections.abc.Mapping[Role, {PATH_INPUT}]"
    table.parameters(f"{texture}.TextureSet.__init__", files=files)
    table.accessor(f"{texture}.TextureSet", "files", "dict[Role, pathlib.Path]", files)
    table.parameters(f"{texture}.discover", directory=PATH_INPUT)
    # A decoder answers the pixels of one file, or None where it cannot.
    table.parameters(
        f"{texture}.fromFiles",
        decode=f"collections.abc.Callable[[pathlib.Path], {PIXELS} | None] | None",
    )
    table.parameters(
        f"{texture}.fromUsageMap",
        byUsage=f"collections.abc.Mapping[str, {PIXELS} | None]",
    )
