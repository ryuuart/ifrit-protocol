"""Sketch declarations and file rendering through the native canvas session."""

import sys
from functools import wraps
from math import isfinite
from numbers import Real
from os import fspath

from _sigil.sketch import Context as SketchContext

__all__ = ["SketchContext", "render_file", "sketch"]


def render_file(source, output, *, at=None):
    """Render a sketch file to PNG using the native headless canvas session."""
    from _sigil import render_file as native_render_file

    return native_render_file(fspath(source), fspath(output), at=at)


def _number(value, name, *, positive=False):
    if isinstance(value, bool) or not isinstance(value, Real):
        raise TypeError(f"{name} must be a finite number")
    if not isfinite(value) or (value <= 0 if positive else value < 0):
        constraint = "positive" if positive else "nonnegative"
        raise ValueError(f"{name} must be finite and {constraint}")
    return value


def sketch(*, size=(960, 640), background="#121720", capture_at=1.0):
    """Declare a canvas sketch with optional setup, update and draw methods.

    ``setup(self, ctx)`` initializes state and may render a native element.
    ``update(self, elapsed, ctx)`` runs on the session clock when present.
    ``draw(self, pen, ctx)`` supplies a full-canvas drawing program when present.
    Methods may omit trailing arguments, including all context arguments.
    An explicit render in setup replaces that default drawing node.
    """
    if not isinstance(size, (tuple, list)) or len(size) != 2:
        raise TypeError("size must contain a width and a height")
    width = _number(size[0], "canvas width", positive=True)
    height = _number(size[1], "canvas height", positive=True)
    capture_at = _number(capture_at, "capture_at")

    def decorate(cls):
        if not isinstance(cls, type):
            raise TypeError("@sketch decorates a class")
        if "__sigil_decorated__" in cls.__dict__:
            raise TypeError("a sketch class can only be decorated once")
        setup = getattr(cls, "setup", None)
        draw = getattr(cls, "draw", None)
        if setup is None and draw is None:
            raise TypeError("a sketch class needs setup(self, ctx) or draw(self, pen)")
        for name in ("setup", "update", "draw"):
            method = getattr(cls, name, None)
            if method is not None and not callable(method):
                raise TypeError(f"sketch {name} must be callable")
        module = sys.modules[cls.__module__]
        existing = getattr(module, "__sigil_sketch__", None)
        if existing is not None and existing is not cls:
            raise ValueError("a module can export only one @sketch class")

        def configure(self, ctx):
            from .._loader import arity

            ctx.canvas(width, height)
            ctx.background(background)
            ctx.captureAt(capture_at)
            if draw is not None:
                from ..compose import graphics

                paint = draw.__get__(self, cls)
                count = arity(paint, 2)
                if count == 0:

                    def program(pen):
                        paint()
                elif count == 1:
                    program = paint
                else:

                    def program(pen):
                        paint(pen, ctx)

                ctx.render(graphics(program, key="sketch.draw", absolute=True, inset=0))
            if setup is not None:
                initialize = setup.__get__(self, cls)
                if arity(initialize, 1) == 0:
                    initialize()
                else:
                    initialize(ctx)

        if setup is not None:
            configure = wraps(setup)(configure)
        cls.setup = configure
        cls.__sigil_decorated__ = True
        module.__sigil_sketch__ = cls
        return cls

    return decorate
