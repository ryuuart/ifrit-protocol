from typing import Protocol, TypeVar

from _sigil.draw import Pen
from _sigil.sketch import Context

_Sketch = TypeVar("_Sketch")

class SketchDecorator(Protocol):
    def __call__(self, cls: type[_Sketch], /) -> type[_Sketch]: ...

class Setup(Protocol):
    def __call__(self, ctx: Context, /) -> None: ...

class Update(Protocol):
    def __call__(self, elapsed: float, ctx: Context, /) -> None: ...

class Draw(Protocol):
    def __call__(self, pen: Pen, ctx: Context, /) -> None: ...

class Paint(Protocol):
    def __call__(self, pen: Pen, /) -> None: ...
