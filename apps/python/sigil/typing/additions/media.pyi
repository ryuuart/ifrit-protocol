import os
import pathlib

import sigil.skia

def load(
    path: str | os.PathLike[str],
    *,
    width: int = ...,
    height: int = ...,
    layer: str = ...,
) -> Image: ...
def save(
    image: sigil.skia.Image | Image,
    path: str | os.PathLike[str],
    *,
    format: Format | None = ...,
    quality: int = ...,
) -> pathlib.Path: ...
