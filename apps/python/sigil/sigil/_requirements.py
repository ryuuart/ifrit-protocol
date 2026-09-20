"""The optional modules a sketch source declares, and which are absent.

A sketch names what it needs in a module-level ``REQUIRES`` tuple, read
from the source text without importing it: a study whose module is not
installed says so by name rather than raising from its own import line.
The module has no import-time side effects, so a command that only wants
to read a declaration does not install a loader to do it.
"""

import ast
import importlib.util
from pathlib import Path


def declared_requirements(source):
    """The module names `source`'s REQUIRES names, in order, without
    repeats. A source with no REQUIRES declares none; one whose REQUIRES
    is not a literal list of module names raises ValueError, and one that
    does not parse answers none and leaves the error to whoever runs it."""
    text = Path(source).read_text(encoding="utf-8")
    try:
        tree = ast.parse(text, filename=str(source))
    except SyntaxError:
        return ()
    for statement in tree.body:
        if isinstance(statement, ast.Assign):
            targets = statement.targets
        elif isinstance(statement, ast.AnnAssign):
            targets = [statement.target]
        else:
            continue
        if not any(
            isinstance(target, ast.Name) and target.id == "REQUIRES"
            for target in targets
        ):
            continue
        modules = ast.literal_eval(statement.value)
        if not isinstance(modules, (tuple, list)) or any(
            not isinstance(module, str)
            or not module
            or not all(part.isidentifier() for part in module.split("."))
            for module in modules
        ):
            raise ValueError(
                f"{source}: REQUIRES must be a literal list of module names"
            )
        return tuple(dict.fromkeys(modules))
    return ()


def missing_requirements(source):
    """The declared modules this interpreter cannot find, in order."""
    missing = []
    for module in declared_requirements(source):
        try:
            found = importlib.util.find_spec(module) is not None
        except (ImportError, ValueError):
            # A dotted name whose parent package is absent raises while it
            # is being discovered, which is the parent being absent.
            found = False
        if not found:
            missing.append(module)
    return missing
