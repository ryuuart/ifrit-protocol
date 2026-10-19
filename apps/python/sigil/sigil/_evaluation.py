"""Text a client sends, run inside the running sketch.

The host calls ``evaluate`` on the thread that draws the sketch, between two
of its frames, with the interpreter held. The text runs in the namespace of
the module the sketch's class was written in, so the author's own names are
the ones it reads and assigns; beside them it finds the running instance as
``self``, the context its hooks are handed as ``ctx``, and the ``sigil``
package with each of its modules the sketch's module did not bind itself.
Whatever the text raises is caught and answered, so the sketch runs on as the
text left it.
"""

import contextlib
import importlib
import io
import itertools
import linecache
import pkgutil
import sys
import traceback

_entries = itertools.count(1)


def _bindPackage(namespace):
    """Binds ``sigil`` and each of its public modules under its own name,
    where the namespace binds nothing there and no standard module answers to
    the name."""
    package = importlib.import_module("sigil")
    namespace.setdefault("sigil", package)
    for module in pkgutil.iter_modules(package.__path__):
        name = module.name
        if name.startswith("_") or name in sys.stdlib_module_names:
            continue
        if name in namespace or name in {"cli", "environment", "examples", "testing"}:
            continue
        try:
            namespace[name] = importlib.import_module(f"sigil.{name}")
        except ImportError:
            continue


def _described(error):
    """The exception as (type, message, traceback), the traceback from the
    evaluated text down."""
    trace = error.__traceback__
    # The first frame is this module's own call of the text.
    if trace is not None and trace.tb_frame.f_code is evaluate.__code__:
        trace = trace.tb_next
    lines = traceback.format_exception(type(error), error, trace)
    return type(error).__name__, str(error), "".join(lines)


def evaluate(instance, context, code, expression):
    """Runs @p code against the running @p instance and answers
    ``(value, output, error, redescribe)``: the expression's ``repr`` — empty
    for statements and for a value of ``None`` — what the text printed, what
    it raised as ``(type, message, traceback)`` or ``None``, and whether the
    sketch declares its tree in ``setup`` and is to be asked for it again."""
    module = sys.modules.get(type(instance).__module__)
    namespace = vars(module) if module is not None else {"__name__": "__prompt__"}
    namespace["self"] = instance
    namespace["ctx"] = context
    _bindPackage(namespace)
    name = f"<prompt {next(_entries)}>"
    # Kept so a traceback through the text shows its lines.
    linecache.cache[name] = (len(code), None, code.splitlines(True), name)
    written = io.StringIO()
    value = ""
    try:
        with contextlib.redirect_stdout(written), contextlib.redirect_stderr(written):
            compiled = compile(code, name, "eval" if expression else "exec")
            if expression:
                result = eval(compiled, namespace)
                if result is not None:
                    value = repr(result)
            else:
                exec(compiled, namespace)
    except BaseException as error:
        return "", written.getvalue(), _described(error), False
    redescribe = bool(getattr(type(instance), "__sigil_redescribes__", True))
    return value, written.getvalue(), None, redescribe
