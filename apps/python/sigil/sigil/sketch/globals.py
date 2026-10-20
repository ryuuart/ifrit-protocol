"""A sketch written as a file of functions, the way p5.js writes one.

A module that defines ``setup()``, ``draw(pen)`` or ``describe(seconds)`` at
its top level, and no sketch class, is a sketch::

    from sigil.sketch.globals import *

    size(640, 360)

    def draw(pen):
        pen.background("#121720")
        pen.circle(mouseX, mouseY, 24)

The host loads it as it loads a ``@sketch`` class: the module's functions are
wrapped into that class, and what they define decides the kind, by the rule
the decorator reads off a class. ``update(elapsed)`` runs on the session
clock beside them, and ``mousePressed()``, ``mouseReleased()``,
``mouseMoved()``, ``mouseDragged()``, ``keyPressed()`` and ``keyReleased()``
are called when the pointer or the keyboard changes between two frames.

``size``, ``background``, ``captureAt`` and ``theme`` declare the canvas, at
the top of the file or inside ``setup``; ``captureAt`` may also be a module
constant. The readings below are written into the sketch module's own
namespace before every hook runs, so a hook reads them as plain names:
``width`` and ``height`` of the canvas, ``frameCount``, ``seconds`` of scene
time, ``deltaTime`` in milliseconds as the pen spells it, ``mouseX``,
``mouseY``, ``mouseIsPressed``, ``key``, ``keyCode`` and ``keyIsPressed``.
The pointer and the keyboard are read from the pen a ``draw`` is handed, and
in a sketch without ``draw`` from the context's ``pointer`` and ``keys``
before ``update`` or ``describe`` runs, so its input hooks run there.
"""

import inspect
import sys
import threading
from contextlib import contextmanager, nullcontext
from math import isfinite
from numbers import Real
from types import ModuleType
from typing import Any

width: float = 0.0
"""The canvas width in canvas units."""
height: float = 0.0
"""The canvas height in canvas units."""
frameCount: int = 0
"""Frames drawn since the sketch began."""
seconds: float = 0.0
"""Scene seconds since the sketch began."""
deltaTime: float = 0.0
"""Milliseconds the last frame took."""
mouseX: float = 0.0
"""Where the pointer stands across the canvas."""
mouseY: float = 0.0
"""Where the pointer stands down the canvas."""
mouseIsPressed: bool = False
"""Whether the pointer's button is down."""
key: str = ""
"""The name of the last key pressed, as a keyboard spells it."""
keyCode: int = 0
"""The code of the last key pressed."""
keyIsPressed: bool = False
"""Whether any key is down."""

HOOKS = ("setup", "draw", "update", "describe")
"""The functions a module defines to be a sketch; any one of setup, draw and
describe makes it one."""
EVENTS = (
    "mouseMoved",
    "mouseDragged",
    "mousePressed",
    "mouseReleased",
    "keyPressed",
    "keyReleased",
)
"""The input hooks, in the order one frame calls them."""
READINGS = (
    "width",
    "height",
    "frameCount",
    "seconds",
    "deltaTime",
    "mouseX",
    "mouseY",
    "mouseIsPressed",
    "key",
    "keyCode",
    "keyIsPressed",
)
"""The names the host writes into a sketch module before each hook."""

_DECLARED = "__sigil_declared__"
_running = threading.local()


def _declaring_module(depth):
    """The namespace of the module whose code called a declaration."""
    if getattr(_running, "hook", None):
        raise RuntimeError(
            "size, background, captureAt and theme declare the canvas at the "
            "top of the sketch file or inside setup; a running hook paints "
            "its ground with pen.background(...)"
        )
    return sys._getframe(depth).f_globals


def _record(name, value):
    namespace = _declaring_module(3)
    declared = dict(namespace.get(_DECLARED, {}))
    declared[name] = value
    namespace[_DECLARED] = declared


def _number(value, name, *, positive=False):
    if isinstance(value, bool) or not isinstance(value, Real):
        raise TypeError(f"{name} must be a finite number")
    if not isfinite(value) or (value <= 0 if positive else value < 0):
        raise ValueError(
            f"{name} must be finite and {'positive' if positive else 'nonnegative'}"
        )
    return value


def size(width: float, height: float) -> None:
    """Declare the canvas size; the module's ``width`` and ``height`` read
    it at once."""
    width = _number(width, "canvas width", positive=True)
    height = _number(height, "canvas height", positive=True)
    _record("size", (width, height))
    namespace = sys._getframe(1).f_globals
    namespace["width"], namespace["height"] = width, height


def background(color: Any) -> None:
    """Declare what stands behind the canvas: a colour, or anything a colour
    is read from."""
    _record("background", color)


def captureAt(seconds: float) -> None:
    """Declare the scene time a still of the sketch is taken at."""
    _record("captureAt", _number(seconds, "captureAt"))


def theme(look: Any, /) -> None:
    """Declare a kit theme: provided around every hook, and staging the
    canvas with the theme's ground where no background is declared."""
    _record("theme", look)


__all__ = [
    "background",
    "captureAt",
    "deltaTime",
    "frameCount",
    "height",
    "key",
    "keyCode",
    "keyIsPressed",
    "mouseIsPressed",
    "mouseX",
    "mouseY",
    "seconds",
    "size",
    "theme",
    "width",
]


def _defined(module: ModuleType, name: str):
    """The function module defines at its top level under name, or None: one
    it imported from elsewhere is not its hook."""
    value = vars(module).get(name)
    if inspect.isfunction(value) and value.__module__ == module.__name__:
        return value
    return None


def hookNames(module: ModuleType) -> list[str]:
    """The hook and input hook functions module defines at its top level."""
    return [name for name in (*HOOKS, *EVENTS) if _defined(module, name)]


def isFunctionSketch(module: ModuleType) -> bool:
    """Whether module's own functions make it a sketch."""
    return any(_defined(module, name) for name in ("setup", "draw", "describe"))


def _declarations(module: ModuleType) -> dict:
    """What module declared of its canvas, by call or by constant."""
    declared = dict(vars(module).get(_DECLARED, {}))
    moment = vars(module).get("captureAt")
    if isinstance(moment, Real) and not isinstance(moment, bool):
        declared["captureAt"] = _number(moment, "captureAt")
    return declared


@contextmanager
def _hook():
    """A hook running: a declaration made now is refused, since it cannot
    reach a canvas already drawing."""
    outer = getattr(_running, "hook", False)
    _running.hook = True
    try:
        yield
    finally:
        _running.hook = outer


class _Hooks:
    """Calls of the module's functions by name, each looked up when it is
    called, so a function redefined while the sketch runs is the one the
    next frame calls; each is measured for the arguments it names once."""

    def __init__(self, module):
        self.module = module
        self.measured = {}

    def __call__(self, name, *arguments):
        function = vars(self.module).get(name)
        if not callable(function):
            return None
        named = self.measured.get(function)
        if named is None:
            from .._callbacks import arity

            named = self.measured[function] = arity(function, len(arguments))
        return function(*arguments[:named])


class _Readings:
    """The readings one sketch instance last wrote, and the input hooks a
    change between two of them calls."""

    def __init__(self, module, hooks, size):
        self.module = module
        self.hooks = hooks
        self.values = {name: globals()[name] for name in READINGS}
        self.values["width"], self.values["height"] = size
        self.drawn = False

    def write(self):
        vars(self.module).update(self.values)

    def step(self, at, canvas=None):
        """A step with no pen: the time, the canvas and one more frame."""
        values = self.values
        if canvas is not None:
            values["width"], values["height"] = canvas
        values["deltaTime"] = max(at - values["seconds"], 0.0) * 1000.0
        values["seconds"] = at
        values["frameCount"] += 1
        self.write()

    def time(self, at, canvas=None):
        """The time and the canvas alone, where another hook counts frames."""
        if canvas is not None:
            self.values["width"], self.values["height"] = canvas
        self.values["seconds"] = at
        self.write()

    def fromPen(self, pen):
        """Everything the pen a draw is handed reads, then each input hook
        the change since the last frame calls."""
        self._input(
            width=pen.width,
            height=pen.height,
            frameCount=pen.frameCount,
            seconds=pen.millis() / 1000.0,
            deltaTime=pen.deltaTime,
            mouseX=pen.mouseX,
            mouseY=pen.mouseY,
            mouseIsPressed=pen.mouseIsPressed,
            key=pen.key,
            keyCode=pen.keyCode,
            keyIsPressed=pen.keyIsPressed,
        )

    def fromContext(self, ctx):
        """The pointer and the keyboard a context answers, where no draw is
        handed a pen, then each input hook the change since the last frame
        calls; a context that answers neither leaves them at rest."""
        try:
            pointer, keys = ctx.pointer, ctx.keys
        except (AttributeError, RuntimeError):
            return
        self._input(
            mouseX=pointer.at.x,
            mouseY=pointer.at.y,
            mouseIsPressed=pointer.pressed,
            key=keys.key,
            keyCode=keys.keyCode,
            keyIsPressed=keys.pressed,
        )

    def _input(self, **readings):
        previous = dict(self.values)
        first = not self.drawn
        self.drawn = True
        self.values.update(readings)
        self.write()
        for name in _changes(previous, self.values, first=first):
            self.hooks(name)


def _changes(previous: dict, current: dict, *, first: bool = False) -> list[str]:
    """The input hooks a change from previous to current readings calls, in
    order: a move, a press, a release, a key down, a key up. The first frame
    has no position to move from."""
    fired = []
    moved = (previous["mouseX"], previous["mouseY"]) != (
        current["mouseX"],
        current["mouseY"],
    )
    if moved and not first:
        fired.append("mouseDragged" if current["mouseIsPressed"] else "mouseMoved")
    if current["mouseIsPressed"] and not previous["mouseIsPressed"]:
        fired.append("mousePressed")
    if previous["mouseIsPressed"] and not current["mouseIsPressed"]:
        fired.append("mouseReleased")
    if current["keyIsPressed"] and (
        not previous["keyIsPressed"] or current["keyCode"] != previous["keyCode"]
    ):
        fired.append("keyPressed")
    if previous["keyIsPressed"] and not current["keyIsPressed"]:
        fired.append("keyReleased")
    return fired


def _check_describe(function):
    signature = inspect.signature(function)
    try:
        signature.bind(0.0)
    except TypeError:
        raise TypeError(
            "describe is a sketch hook: describe(seconds) returns the frame or "
            f"tree at a scene time, and this one takes {signature}"
        ) from None


def sketchOf(module: ModuleType) -> type:
    """The ``@sketch`` class a module of functions is run as: its hooks
    wrapped as methods of one class, declared with what the module declared
    at its top level."""
    from . import _describes_frame, sketch

    if not isFunctionSketch(module):
        raise TypeError(
            "a sketch file defines setup(), draw(pen) or describe(seconds) at "
            "its top level, or one @sketch class"
        )
    hooks = _Hooks(module)
    loaded = _declarations(module)
    canvas = tuple(loaded.get("size", (960, 640)))
    needsFrame = False
    describe = _defined(module, "describe")
    if describe is not None:
        _check_describe(describe)
        needsFrame = _describes_frame(describe)
    drawn = _defined(module, "draw") is not None
    counts = "draw" if drawn else ("describe" if describe is not None else "update")

    def themed(self):
        """The theme declared after the file loaded, provided around a hook;
        one declared at its top level the decorator provides."""
        current = _declarations(module).get("theme")
        if current is None or current is loaded.get("theme"):
            return nullcontext()
        from .kit import provide

        return provide(current)

    def __init__(self):
        self._sigilReadings = _Readings(module, hooks, canvas)
        self._sigilReadings.write()
        self._sigilContext = None

    def setup(self, ctx):
        # The context answers for the session's life, so a describe handed
        # only the scene time reads the pointer and the keyboard through it.
        self._sigilContext = ctx
        readings = self._sigilReadings
        readings.time(getattr(ctx, "elapsed", 0.0), _size(ctx))
        with themed(self):
            returned = hooks("setup", ctx)
        current = _declarations(module)
        if current != loaded:
            _declare(ctx, current)
            readings.time(readings.values["seconds"], _size(ctx))
        # Text run inside the sketch asks for its tree again only where
        # setup returned one: a setup that only sets state up runs once, as
        # p5 runs it.
        type(self).__sigil_redescribes__ = returned is not None
        return returned

    def draw(self, pen, ctx):
        with themed(self), _hook():
            self._sigilReadings.fromPen(pen)
            hooks("draw", pen, ctx)

    def update(self, elapsed, ctx):
        readings = self._sigilReadings
        if counts == "update":
            readings.step(elapsed, _size(ctx))
        else:
            readings.time(elapsed, _size(ctx))
        with themed(self), _hook():
            if not drawn:
                readings.fromContext(ctx)
            hooks("update", elapsed, ctx)

    def describeAt(self, seconds):
        readings = self._sigilReadings
        if counts == "describe":
            readings.step(seconds)
        else:
            readings.time(seconds)
        with themed(self), _hook():
            if not drawn and self._sigilContext is not None:
                readings.fromContext(self._sigilContext)
            return hooks("describe", seconds)

    describeAt.__name__ = describeAt.__qualname__ = "describe"
    if needsFrame:
        describeAt.__annotations__ = {"return": "Frame"}

    namespace: dict[str, Any] = {
        "__module__": module.__name__,
        "__doc__": module.__doc__,
        "__init__": __init__,
        "setup": setup,
    }
    if drawn:
        namespace["draw"] = draw
    # Without a draw the input hooks are called from the step, so a file of
    # input hooks and no update is stepped all the same.
    listens = not drawn and any(_defined(module, name) for name in EVENTS)
    if _defined(module, "update") is not None or listens:
        namespace["update"] = update
    if describe is not None:
        namespace["describe"] = describeAt
    if vars(module).get("needsDevice") is True:
        namespace["needsDevice"] = True
    name = module.__name__.rpartition(".")[2]
    body = type(name if name.isidentifier() else "Sketch", (), namespace)
    options = {
        key: loaded[key]
        for key in ("size", "background", "captureAt", "theme")
        if key in loaded
    }
    declared = sketch(**options)(body)
    declared.__sigil_redescribes__ = False
    return declared


def _size(ctx):
    try:
        return (ctx.width, ctx.height)
    except (AttributeError, RuntimeError):
        return None


def _declare(ctx, declared):
    """Apply what setup declared to the canvas the decorator declared."""
    look = declared.get("theme")
    if look is not None:
        from .kit import provide, stage

        properties = {
            key: declared[key]
            for key in ("size", "captureAt", "background")
            if key in declared
        }
        with provide(look):
            stage(ctx, **properties)
        return
    if "size" in declared:
        ctx.canvas(*declared["size"])
    if "background" in declared:
        ctx.background(declared["background"])
    if "captureAt" in declared:
        ctx.captureAt(declared["captureAt"])
