"""Sketch declarations and file rendering through the native sketch sessions."""

import sys as _sys
from functools import wraps as _wraps
from math import isfinite as _isfinite
from numbers import Real as _Real
from os import fspath as _fspath

from _sigil import renderFile as _native_render_file


def renderFile(source, output, *, at=None):
    """Render a sketch file to PNG through the native headless session."""
    return _native_render_file(_fspath(source), _fspath(output), at=at)


def _number(value, name, *, positive=False):
    if isinstance(value, bool) or not isinstance(value, _Real):
        raise TypeError(f"{name} must be a finite number")
    if not _isfinite(value) or (value <= 0 if positive else value < 0):
        constraint = "positive" if positive else "nonnegative"
        raise ValueError(f"{name} must be finite and {constraint}")
    return value


def _return_annotation(function):
    """The return annotation of function as the source text spells it, or
    None where it states none. Read as text so a name the module imports
    under any spelling, or one not yet defined, is never evaluated."""
    try:
        import annotationlib

        annotations = annotationlib.get_annotations(
            function, format=annotationlib.Format.STRING
        )
    except ImportError:
        import inspect

        annotations = inspect.get_annotations(function)
    annotation = annotations.get("return")
    if annotation is None or isinstance(annotation, str):
        return annotation
    return getattr(annotation, "__qualname__", repr(annotation))


def _describes_frame(describe):
    """Whether describe states that it returns a world frame: its return
    annotation names ``Frame``, alone or after a module. The host reads the
    file's text by the same rule, so a listing and an opened session agree
    on the kind."""
    annotation = _return_annotation(describe)
    if not annotation:
        return False
    return annotation.strip().strip("\"'").rpartition(".")[2] == "Frame"


def _check_describe(describe):
    """Refuse a describe that cannot be handed the scene time alone: the
    word is the sketch's own, like setup, update and draw."""
    import inspect

    signature = inspect.signature(describe, follow_wrapped=False)
    try:
        signature.bind(None, 0.0)
    except TypeError:
        raise TypeError(
            "describe is a sketch hook: describe(self, seconds) returns the "
            f"frame or tree at a scene time, and this one takes {signature}"
        ) from None


def _caller(method, count):
    """A call of method on an instance that hands it as many of count
    leading arguments as it names, measured once rather than at every
    step."""
    from .._callbacks import arity

    named = max(arity(method, count + 1) - 1, 0)

    def call(owner, *arguments):
        return method(owner, *arguments[:named])

    return call


def _current(name, count):
    """A call of the hook the instance's class holds under name at the
    moment it is called, so a method assigned on the class while the sketch
    runs is the one the next step calls; each function is measured once."""
    measured = {}

    def call(owner, *arguments):
        method = getattr(type(owner), name)
        caller = measured.get(method)
        if caller is None:
            measured.clear()
            caller = measured[method] = _caller(method, count)
        return caller(owner, *arguments)

    return call


# How long the end of a session waits for its cancelled tasks to finish
# what they do on the way out, such as a WebSocket's closing handshake,
# before the loop closes under whatever is still pending.
_FAREWELL_SECONDS = 0.25


class _EventLoop:
    """``self.loop`` on a scene2d sketch: an asyncio event loop of the
    sketch's own, made the first time it is asked for and turned once a
    frame on the sketch thread.

    A turn is asyncio's documented single pass, ``loop.stop()`` and then
    ``loop.run_forever()``: the selector is polled once without waiting,
    and every callback that was ready, or that the poll made ready, runs.
    A task therefore resumes on the thread that runs ``update`` and
    ``describe``, and the next description reads what it wrote onto the
    sketch as it stands, with no queue between them. A frame never waits
    on the network: a task awaiting a socket resumes on the first turn
    after its bytes arrive.

    The turn is a timer on the session's engine, so the loop turns on
    every frame the session steps, whichever hooks the class defines. The
    session lets the timer go when it closes, and that cancels every task
    still pending and closes the loop.
    """

    def __get__(self, owner, cls=None):
        if owner is None:
            return self
        return _event_loop(owner)


def _event_loop(owner):
    state = vars(owner)
    loop = state.get("__sigil_loop__")
    if loop is not None and not loop.is_closed():
        return loop
    ctx = state.get("__sigil_context__")
    if ctx is None:
        raise RuntimeError(
            "a sketch's loop turns on its session's clock; ask for it from "
            "setup, update, describe or draw"
        )
    import asyncio

    loop = asyncio.new_event_loop()
    ctx.engine.timer(_turn(loop))
    state["__sigil_loop__"] = loop
    return loop


def _turn(loop):
    """One pass of loop for each frame, and the loop's end once the session
    lets the pass go: nothing but the engine's timer holds it."""
    import weakref

    def turn():
        if not loop.is_closed():
            loop.stop()
            loop.run_forever()

    weakref.finalize(turn, _finish, loop)
    return turn


def _finish(loop):
    """Cancel what still runs on loop, let it unwind briefly, close it."""
    import asyncio

    if loop.is_closed():
        return
    for patience in (_FAREWELL_SECONDS, 0):
        pending = asyncio.all_tasks(loop)
        if not pending:
            break
        for task in pending:
            task.cancel()
        loop.run_until_complete(asyncio.wait(pending, timeout=patience))
    loop.run_until_complete(loop.shutdown_asyncgens())
    loop.close()


def sketch(
    *,
    size=(960, 640),
    background=None,
    captureAt=None,
    theme=None,
    sheet=None,
):
    """Declare a sketch class; what it defines decides its kind.

    A class whose ``describe(self, seconds)`` is annotated ``-> world.Frame``
    is a scene3d sketch: the host draws the frame it returns on the device,
    every step, and ``setup(self, ctx)`` is handed a ``Scene3DContext`` to
    ask for the lens, pages and screens there. Any other class is a scene2d
    sketch: ``setup(self, ctx)`` may return the tree to render,
    ``describe(self, seconds)`` returns the tree again at every step,
    ``update(self, elapsed, ctx)`` runs on the session clock, and
    ``draw(self, pen, ctx)`` supplies a full-canvas drawing program.
    ``setup``, ``update``, ``draw`` and ``describe`` are the hook words: each
    is engaged only when the class defines it, and ``describe`` takes exactly
    one argument, the seconds. setup, update and draw may omit trailing
    arguments, including all context arguments.

    A scene2d sketch also has ``self.loop``, an asyncio event loop turned
    once a frame on the sketch thread: a task started there with
    ``self.loop.create_task`` writes onto the sketch directly, and is
    cancelled when the session closes. A class that defines its own
    ``loop`` keeps it.

    ``size``, ``background`` and ``captureAt`` declare the canvas, what is
    behind it and the moment a still is taken at. ``theme``, a kit theme, is
    staged: its sheet stands at the root of every tree the sketch renders,
    so every component in it reads the theme where it lands, and the canvas
    takes the theme's ground where no background is given. ``sheet``, a
    ``compose.StyleSheet``, is joined after the theme's at that root: a
    variant of a stock look is rules there, such as
    ``rule(":root").var("spacing-caption-gap", 8)``.
    """
    if not isinstance(size, (tuple, list)) or len(size) != 2:
        raise TypeError("size must contain a width and a height")
    width = _number(size[0], "canvas width", positive=True)
    height = _number(size[1], "canvas height", positive=True)
    moment = _number(1.0 if captureAt is None else captureAt, "captureAt")
    if theme is None and sheet is None and background is None:
        background = "#121720"

    def declare(ctx):
        if theme is not None or sheet is not None:
            from .kit import stage

            properties = dict(size=(width, height), captureAt=moment, theme=theme)
            if sheet is not None:
                properties["sheet"] = sheet
            if background is not None:
                properties["background"] = background
            stage(ctx, **properties)
            return
        ctx.canvas(width, height)
        ctx.background(background)
        ctx.captureAt(moment)

    def decorate(cls):
        if not isinstance(cls, type):
            raise TypeError("@sketch decorates a class")
        if "__sigil_decorated__" in cls.__dict__:
            raise TypeError("a sketch class can only be decorated once")
        setup = getattr(cls, "setup", None)
        update = getattr(cls, "update", None)
        draw = getattr(cls, "draw", None)
        describe = getattr(cls, "describe", None)
        for name, method in (
            ("setup", setup),
            ("update", update),
            ("draw", draw),
            ("describe", describe),
        ):
            if method is not None and not callable(method):
                raise TypeError(f"sketch {name} must be callable")
        module = _sys.modules[cls.__module__]
        existing = getattr(module, "__sigil_sketch__", None)
        if existing is not None and existing is not cls:
            raise ValueError("a module can export only one @sketch class")
        if describe is not None:
            _check_describe(describe)
        if describe is not None and _describes_frame(describe):
            _scene3d(cls, setup, update, draw, describe, declare)
        else:
            _scene2d(cls, setup, update, draw, describe, declare)
        cls.__sigil_decorated__ = True
        module.__sigil_sketch__ = cls
        return cls

    return decorate


def _scene3d(cls, setup, update, draw, describe, declare):
    if update is not None or draw is not None:
        raise TypeError(
            "a scene3d sketch is a function of the scene time: describe "
            "returns its frame, and it has no update or draw"
        )

    initialize = _caller(setup, 1) if setup is not None else None
    described = _caller(describe, 1)

    def configure(self, ctx):
        declare(ctx)
        if initialize is not None and initialize(self, ctx) is not None:
            raise TypeError(
                "a scene3d sketch's setup returns nothing; describe "
                "returns its frame"
            )

    def frame(self, seconds):
        return described(self, seconds)

    if setup is not None:
        configure = _wraps(setup)(configure)
    cls.setup = configure
    cls.describe = _wraps(describe)(frame)
    cls.__sigil_kind__ = "scene3d"
    # describe is asked for the frame at every step, so text evaluated inside
    # the running sketch shows at the next one without asking again.
    cls.__sigil_redescribes__ = False


def _scene2d(cls, setup, update, draw, describe, declare):
    if setup is None and draw is None and describe is None:
        raise TypeError(
            "a sketch class needs setup(self, ctx), describe(self, seconds) "
            "or draw(self, pen)"
        )
    if draw is not None and describe is not None:
        raise TypeError(
            "draw(pen) and describe(seconds) each render the whole sketch; define one"
        )

    def render(ctx, tree):
        from _sigil.world import Frame

        if isinstance(tree, Frame):
            raise TypeError(
                "describe returned a world.Frame; annotate it "
                "`-> world.Frame` so the host draws this sketch as a scene3d "
                "sketch"
            )
        ctx.render(tree)

    initialize = _caller(setup, 1) if setup is not None else None
    advanced = _caller(update, 2) if update is not None else None
    # draw and describe stay on the class as written, so each step calls the
    # one the class holds then: one assigned while the sketch runs takes over
    # at the next frame.
    painted = _current("draw", 2) if draw is not None else None
    described = _current("describe", 1) if describe is not None else None

    def configure(self, ctx):
        vars(self)["__sigil_context__"] = ctx
        declare(ctx)
        if painted is not None:
            from ..compose import graphics

            def program(pen):
                painted(self, pen, ctx)

            ctx.render(graphics("sketch.draw", program).absolute().inset(0))
        if initialize is not None:
            returned = initialize(self, ctx)
            if returned is not None:
                render(ctx, returned)
        if described is not None:
            render(ctx, described(self, ctx.elapsed))

    def advance(self, elapsed, ctx):
        if advanced is not None:
            advanced(self, elapsed, ctx)
        if described is not None:
            render(ctx, described(self, elapsed))

    if setup is not None:
        configure = _wraps(setup)(configure)
    cls.setup = configure
    if getattr(cls, "loop", None) is None:
        cls.loop = _EventLoop()
    if update is not None or describe is not None:
        if update is not None:
            advance = _wraps(update)(advance)
        cls.update = advance
    cls.__sigil_kind__ = "scene2d"
    # Whether text evaluated inside the running sketch asks it for its tree
    # again: a setup declares the tree once, where draw and describe are
    # asked at every step anyway.
    cls.__sigil_redescribes__ = setup is not None
