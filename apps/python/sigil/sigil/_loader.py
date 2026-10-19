"""Fresh, source-only packages for live sketch generations."""

import importlib.abc
import importlib.machinery
import importlib.util
import inspect
import itertools
import sys
import types
import warnings
from pathlib import Path

_PREFIX = "_sigil_guest_"
_generations = itertools.count()


class _SourceLoader(importlib.machinery.SourceFileLoader):
    def get_code(self, fullname):
        # Timestamp-based bytecode caches cannot distinguish every rapid edit
        # of the same size. Sketch imports always compile the current source.
        return self.source_to_code(self.get_data(self.path), self.path)


class _SketchFinder(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path=None, target=None):
        if not fullname.startswith(_PREFIX):
            return None
        spec = importlib.machinery.PathFinder.find_spec(fullname, path)
        if spec is not None and isinstance(
            spec.loader, importlib.machinery.SourceFileLoader
        ):
            spec.loader = _SourceLoader(fullname, spec.origin)
        return spec


sys.meta_path.insert(0, _SketchFinder())


def release(prefix):
    """Remove this generation's import entries without mutating its objects."""
    for name in tuple(sys.modules):
        if name == prefix or name.startswith(prefix + "."):
            del sys.modules[name]


def _body(module, name, source):
    """The class a loaded module is run as: its @sketch class, or its one
    class with setup(ctx), or a class wrapping the module's own setup, draw
    and describe functions. A class wins over the functions, and a module
    that defines both is told which one runs."""
    from .sketch.globals import hookNames, isFunctionSketch, sketchOf

    body = getattr(module, "__sigil_sketch__", None)
    if body is None:
        candidates = [
            value
            for value in vars(module).values()
            if inspect.isclass(value)
            and value.__module__ == name
            and callable(getattr(value, "setup", None))
        ]
        if len(candidates) > 1 or not candidates and not isFunctionSketch(module):
            raise TypeError(
                "Export one @sketch class, one class with setup(ctx), or "
                "setup(), draw(pen) or describe(seconds) as functions of the file"
            )
        if not candidates:
            return sketchOf(module)
        body = candidates[0]
    functions = hookNames(module)
    if functions:
        warnings.warn(
            f"{source.name} defines the sketch class {body.__name__} and the "
            f"functions {', '.join(functions)} at its top level; the class is "
            "the sketch, and those functions are not its hooks",
            stacklevel=2,
        )
    return body


def load(source):
    """Return a body class and the package that owns its local imports."""
    source = Path(source).resolve(strict=True)
    importlib.invalidate_caches()
    prefix = f"{_PREFIX}{next(_generations)}"
    package = types.ModuleType(prefix)
    package.__path__ = [str(source.parent)]
    package.__package__ = prefix
    package.__spec__ = importlib.machinery.ModuleSpec(
        prefix, loader=None, is_package=True
    )
    sys.modules[prefix] = package
    name = prefix + "." + source.stem
    try:
        loader = _SourceLoader(name, str(source))
        spec = importlib.util.spec_from_file_location(name, source, loader=loader)
        module = importlib.util.module_from_spec(spec)
        sys.modules[name] = module
        loader.exec_module(module)
        body = _body(module, name, source)
        if not inspect.isclass(body) or not callable(getattr(body, "setup", None)):
            raise TypeError("The sketch must be a class with setup(ctx)")
        return body, prefix
    except BaseException:
        release(prefix)
        raise
