from os import PathLike
from pathlib import Path

from _sigil.image import (
    Format,
    ImageAsset,
    decode,
    decodeAsset,
    encode,
    from_rgba,
)
from _sigil.skia import Image

def load(
    path: str | PathLike[str], *, width: int = ..., height: int = ...
) -> Image: ...
def save(
    image: Image,
    path: str | PathLike[str],
    *,
    format: Format = ...,
    quality: int = ...,
) -> Path: ...

__all__ = [
    "Format",
    "ImageAsset",
    "decode",
    "decodeAsset",
    "encode",
    "from_rgba",
    "load",
    "save",
]
