"""Native image decoding and encoding, with ordinary Python file access."""

from pathlib import Path as _Path


def load(path, *, width=0, height=0):
    """Decode a still image; vector sources can request a raster size."""
    path = _Path(path)
    return decode(path.read_bytes(), width=width, height=height, hint=str(path))


def save(image, path, *, format=Format.Png, quality=100):
    """Encode an image in the explicitly chosen format and write its bytes."""
    path = _Path(path)
    path.write_bytes(encode(image, format, quality))
    return path
