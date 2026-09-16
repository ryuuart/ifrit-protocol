"""Native image decoding and encoding, with ordinary Python file access."""

from pathlib import Path

from _sigil.image import Format, ImageAsset, decode, decodeAsset, encode, from_rgba

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


def load(path, *, width=0, height=0):
    """Decode a still image; vector sources can request a raster size."""
    path = Path(path)
    return decode(path.read_bytes(), width=width, height=height, hint=str(path))


def save(image, path, *, format=Format.Png, quality=100):
    """Encode an image in the explicitly chosen format and write its bytes."""
    path = Path(path)
    path.write_bytes(encode(image, format, quality))
    return path
