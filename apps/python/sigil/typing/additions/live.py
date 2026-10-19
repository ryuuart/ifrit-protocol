"""A window in this interpreter: a sketch drawn live, its objects at the prompt.

``window = live.open(size=(960, 540))`` puts a window on screen in the
process that asked for it. ``window.show(tree)`` draws a compose tree,
``window.show(Sketch)`` runs a ``@sketch`` class as Grimoire runs it, and
``window.sketch`` is the running instance, so an attribute assigned at the
prompt is what the next frame's ``draw`` or ``describe`` reads.
``window.still(path)`` writes a picture and ``window.close()`` ends it.

The window draws on the thread that opened it, which on macOS is the main
thread, and only while that thread turns it. At ``python -i`` and at
``code.interact`` it turns while the prompt waits for a line; under IPython
it is registered as IPython's input hook, and in a Jupyter kernel a task on
the kernel's event loop turns it. A script that is not interactive calls
``window.run()``, which turns it until it is closed.

The window library and Qt are loaded the first time a window opens; no
other module of the package loads them.
"""

import sys as _sys
import time as _time
from os import fspath as _fspath

from _sigil import live as _native
from _sigil.live import Window as _NativeWindow

# How often a window turns while something else waits: once a frame at the
# display rates a window is shown at.
_FRAME_SECONDS = 1.0 / 120.0


def open(
    size=(960, 540), *, background="#121720", title="Sigil", headless=False, assets=None
):
    """Open a window in this process and answer it.

    ``size`` is the pane in points; a sketch's own canvas is fitted into it
    and never stretched. ``background`` is the ground a shown tree stands
    on. ``headless`` opens no window at all: the sketch is stepped each
    time the window turns and photographed by ``still``, which is what a
    test or a machine with no display opens. ``assets`` is what mounts at
    ``res://`` for the sketches shown.
    """
    return Window(
        size, background=background, title=title, headless=headless, assets=assets
    )


class Window:
    """A window this interpreter opened, and the sketch it shows."""

    def __init__(
        self,
        size=(960, 540),
        *,
        background="#121720",
        title="Sigil",
        headless=False,
        assets=None,
    ):
        if not isinstance(size, (tuple, list)) or len(size) != 2:
            raise TypeError("size must contain a width and a height")
        width, height = (round(float(extent)) for extent in size)
        if width < 1 or height < 1:
            raise ValueError("a window is at least one point wide and tall")
        self._native = _NativeWindow(
            width, height, title=str(title), headless=bool(headless)
        )
        self._size = (width, height)
        self._background = background
        self._assets = None if assets is None else _fspath(assets)
        self._sketch = None
        _keep_turning(self)

    def show(self, what):
        """Draw ``what`` from the next frame on, and answer what is running.

        ``what`` is a compose tree, drawn on a canvas the window's size over
        its background; a ``@sketch`` class, instantiated once and run as
        Grimoire runs it, with setup, update, draw or describe, its timers
        and its engine; or an instance of one, run as it stands. The answer
        is the running instance, kept as ``window.sketch``. A sketch whose
        setup raises leaves nothing on screen and raises its error here.
        """
        if isinstance(what, type):
            body, scene3d, needsDevice = self._class_body(what)
        elif _is_sketch(what):
            body, scene3d, needsDevice = self._instance_body(what)
        else:
            body, scene3d, needsDevice = self._tree_body(what)
        self._native.show(
            body, scene3d=scene3d, needsDevice=needsDevice, assets=self._assets
        )
        if not self._native.live:
            raise RuntimeError(self._native.errorLog or "the sketch opened no session")
        return self._sketch

    @property
    def sketch(self):
        """The running instance; for a tree, the tree."""
        return self._sketch

    def still(self, path, *, density=1.0):
        """Write the sketch on screen to ``path`` as a picture, at ``density``
        pixels per canvas unit, and answer the path."""
        target = _fspath(path)
        if not self._native.still(target, float(density)):
            raise RuntimeError(
                self._native.errorLog or f"no still was written to {target}"
            )
        return target

    def restart(self):
        """Run the shown sketch's setup again, on the same instance, so a tree
        a setup returns is built again from the attributes as they stand."""
        if not self._native.restart():
            raise RuntimeError(self._native.errorLog or "nothing is shown to restart")

    def pump(self, seconds=0.0):
        """Turn the window once, handling its events for up to ``seconds``;
        answer whether it is still open."""
        return self._native.pump(float(seconds))

    def run(self, seconds=None):
        """Turn the window until it is closed, or for ``seconds`` when given:
        what a script that is not interactive calls to keep it on screen."""
        deadline = None if seconds is None else _time.monotonic() + float(seconds)
        while self._native.pump(_FRAME_SECONDS):
            if deadline is not None and _time.monotonic() >= deadline:
                break
            _time.sleep(_FRAME_SECONDS / 2)

    def close(self):
        """Take the window off screen and let the sketch go."""
        self._native.close()

    @property
    def isOpen(self):
        """Whether the window is still open."""
        return self._native.isOpen

    @property
    def size(self):
        """The pane, in points."""
        return self._size

    @property
    def status(self):
        """What the window's host says it is doing."""
        return self._native.status

    @property
    def errorLog(self):
        """Why the sketch is not drawing, when it is not."""
        return self._native.errorLog

    def __enter__(self):
        return self

    def __exit__(self, *exception):
        self.close()

    def _class_body(self, cls):
        def body():
            if not isinstance(self._sketch, cls):
                self._sketch = cls()
            return self._sketch

        self._sketch = None
        return (
            body,
            _kind_of(cls) == "scene3d",
            bool(getattr(cls, "needsDevice", False)),
        )

    def _instance_body(self, instance):
        self._sketch = instance
        cls = type(instance)
        return (
            (lambda: instance),
            _kind_of(cls) == "scene3d",
            bool(getattr(instance, "needsDevice", False)),
        )

    def _tree_body(self, tree):
        width, height = self._size
        background = self._background

        class Shown:
            def setup(self, ctx):
                ctx.canvas(width, height)
                if background is not None:
                    ctx.background(background)
                ctx.render(tree)

        shown = Shown()
        self._sketch = tree
        return (lambda: shown), False, False


def _kind_of(cls):
    return str(getattr(cls, "__sigil_kind__", "scene2d"))


def _is_sketch(value):
    """Whether ``value`` is an instance of a sketch class rather than a tree:
    a class the ``@sketch`` decorator declared, or one with a hook of its
    own."""
    cls = type(value)
    if getattr(cls, "__sigil_decorated__", False):
        return True
    return any(
        callable(getattr(cls, hook, None)) for hook in ("setup", "draw", "describe")
    )


# The integrations already installed in this process, so a second window
# installs nothing twice.
_installed = set()


def _keep_turning(window):
    """Turn every open window while this interpreter waits on its reader."""
    shell = _ipython()
    if shell is not None and _is_kernel(shell):
        _turn_on_kernel_loop()
        return
    if shell is not None:
        _turn_in_ipython(shell)
        return
    if "prompt" not in _installed and _native.installPromptHook():
        _installed.add("prompt")


def _ipython():
    module = _sys.modules.get("IPython")
    if module is None:
        return None
    get = getattr(module, "get_ipython", None)
    return get() if callable(get) else None


def _is_kernel(shell):
    return "ipykernel" in _sys.modules and type(shell).__name__ == "ZMQInteractiveShell"


def _ipython_inputhook(context):
    """IPython's wait for a line, spent turning every open window."""
    while not context.input_is_ready():
        if not _native.pumpOpenWindows():
            return
        _time.sleep(_FRAME_SECONDS)


def _turn_in_ipython(shell):
    if "ipython" in _installed:
        return
    from IPython.terminal.pt_inputhooks import register

    register("sigil", _ipython_inputhook)
    shell.enable_gui("sigil")
    _installed.add("ipython")


def _turn_on_kernel_loop():
    if "kernel" in _installed:
        return
    import asyncio

    async def turn():
        while _native.pumpOpenWindows():
            await asyncio.sleep(_FRAME_SECONDS)
        _installed.discard("kernel")

    asyncio.get_event_loop().create_task(turn())
    _installed.add("kernel")
