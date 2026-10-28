import os
import typing

import sigil._types as _t
import sigil.compose
import sigil.sketch.kit as _kit

_Sketch = typing.TypeVar("_Sketch")

class _SketchDecorator(typing.Protocol):
    def __call__(self, cls: type[_Sketch], /) -> type[_Sketch]: ...

def renderFile(
    source: str | os.PathLike[str],
    output: str | os.PathLike[str],
    *,
    at: float | None = ...,
) -> str: ...
def sketch(
    *,
    size: tuple[float, float] | list[float] = ...,
    background: _t.ColorLike | None = ...,
    captureAt: float | None = ...,
    theme: _kit.Theme | None = ...,
    sheet: sigil.compose.StyleSheet | None = ...,
) -> _SketchDecorator: ...
