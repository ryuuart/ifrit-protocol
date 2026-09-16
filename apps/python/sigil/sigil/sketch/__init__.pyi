from os import PathLike

from _sigil._types import ColorLike
from _sigil.sketch import Context as SketchContext

from . import kit as kit
from ._typing import SketchDecorator as _SketchDecorator

__all__ = ["SketchContext", "render_file", "sketch"]

def render_file(
    source: str | PathLike[str],
    output: str | PathLike[str],
    *,
    at: float | None = ...,
) -> str: ...
def sketch(
    *,
    size: tuple[float, float] | list[float] = ...,
    background: ColorLike = ...,
    capture_at: float = ...,
) -> _SketchDecorator: ...
