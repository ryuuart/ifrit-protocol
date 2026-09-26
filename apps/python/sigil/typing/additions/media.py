"""Pictures, still and moving, with ordinary Python file access."""

from pathlib import Path as _Path


def load(path, *, width=0, height=0, layer=""):
    """Decode an image document from a file; a vector source can request a
    raster size and an EXR a layer."""
    path = _Path(path)
    return decode(
        Image, path.read_bytes(), width=width, height=height, layer=layer,
        hint=str(path),
    )


def save(image, path, *, format=None, quality=100):
    """Encode an image in the format the file's suffix names, or the one
    given, and write its bytes."""
    path = _Path(path)
    if format is None:
        format = {
            ".png": Format.Png,
            ".jpg": Format.Jpeg,
            ".jpeg": Format.Jpeg,
            ".webp": Format.Webp,
            ".exr": Format.Exr,
        }.get(path.suffix.lower())
        if format is None:
            raise ValueError(f"{path.name} names no still format")
    path.write_bytes(encode(image, format, quality))
    return path
