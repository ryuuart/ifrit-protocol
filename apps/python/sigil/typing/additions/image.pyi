import os
import pathlib

import sigil.skia

def load(
    path: str | os.PathLike[str], *, width: int = ..., height: int = ...
) -> sigil.skia.Image: ...
def save(
    image: sigil.skia.Image,
    path: str | os.PathLike[str],
    *,
    format: Format = ...,
    quality: int = ...,
) -> pathlib.Path: ...
