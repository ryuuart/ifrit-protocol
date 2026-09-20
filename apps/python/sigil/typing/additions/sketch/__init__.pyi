import os
import typing

import sigil._types as _t

_Sketch = typing.TypeVar("_Sketch")

class _SketchDecorator(typing.Protocol):
    def __call__(self, cls: type[_Sketch], /) -> type[_Sketch]: ...

def render_file(
    source: str | os.PathLike[str],
    output: str | os.PathLike[str],
    *,
    at: float | None = ...,
) -> str: ...
def sketch(
    *,
    size: tuple[float, float] | list[float] = ...,
    background: _t.ColorLike = ...,
    capture_at: float = ...,
) -> _SketchDecorator: ...
