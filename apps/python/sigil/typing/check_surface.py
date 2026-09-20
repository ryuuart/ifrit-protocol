"""Check the surface an author reads against the extension behind it.

Every public module must declare what it exports and export what the
extension registered under it. Untyped holes in the declarations — an Any,
a bare collection, an unnamed argument — are rejected wherever they appear.
"""

from __future__ import annotations

import argparse
import ast
import importlib
import inspect
import keyword
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import surface


def members(body: list[ast.stmt]) -> dict[str, ast.stmt]:
    result: dict[str, ast.stmt] = {}
    for node in body:
        if isinstance(node, (ast.FunctionDef, ast.ClassDef)):
            result[node.name] = node
        elif isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name):
            result[node.target.id] = node
        elif isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    result[target.id] = node
        elif isinstance(node, (ast.Import, ast.ImportFrom)):
            for alias in node.names:
                result[alias.asname or alias.name.rsplit(".", 1)[-1]] = node
    return result


def public_object(name: str) -> object:
    """Import a public module, or reach the one the extension carries."""
    parts = name.split(".")
    for size in range(len(parts), 0, -1):
        try:
            module = importlib.import_module(".".join(parts[:size]))
        except ImportError:
            continue
        found: object = module
        for attribute in parts[size:]:
            found = getattr(found, attribute)
        return found
    raise ImportError(name)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--declarations",
        type=Path,
        required=True,
        help="the generated declaration package this extension was built with",
    )
    options = parser.parse_args()
    failures: list[str] = []
    count = 0
    optional = 0

    def inspect_members(
        native: object, tree: ast.Module | ast.ClassDef, path: str
    ) -> None:
        nonlocal count
        declared = members(tree.body)
        for name in dir(native):
            if name.startswith("_"):
                continue
            if keyword.iskeyword(name):
                # Native aliases such as Line.from are accessible only by
                # getattr. Their escaped Python spelling is the static API.
                if name + "_" not in declared:
                    failures.append(f"{path}.{name}: no escaped keyword alias")
                continue
            count += 1
            if name not in declared:
                if inspect.isclass(native) and any(
                    name in base.__dict__ and base.__module__.startswith("sigil")
                    for base in native.__mro__[1:]
                ):
                    continue
                failures.append(f"{path}.{name}: missing declaration")
                continue
            value = getattr(native, name)
            declaration = declared[name]
            if inspect.isclass(value) and isinstance(declaration, ast.ClassDef):
                inspect_members(value, declaration, path + "." + name)
            elif callable(value) and re.search(r"\barg\d+\s*:", value.__doc__ or ""):
                failures.append(f"{path}.{name}: unnamed native argument")

    generic = {
        "list",
        "tuple",
        "dict",
        "set",
        "type",
        "collections.abc.Callable",
        "collections.abc.Iterable",
        "collections.abc.Sequence",
        "os.PathLike",
    }

    def annotation(node: ast.expr, location: str, parameterized: bool = False) -> None:
        text = ast.unparse(node)
        if text in {"typing.Any", "Any"}:
            failures.append(f"{location}: Any erases the binding contract")
        if text in generic and not parameterized:
            failures.append(f"{location}: generic {text} has no type arguments")
        if isinstance(node, ast.Subscript):
            annotation(node.value, location, True)
            if ast.unparse(node.value) == "tuple" and isinstance(node.slice, ast.Tuple):
                for item in node.slice.elts:
                    if not (isinstance(item, ast.Constant) and item.value is Ellipsis):
                        annotation(item, location)
            else:
                annotation(node.slice, location)
        elif isinstance(node, ast.Constant) and node.value is Ellipsis:
            failures.append(f"{location}: unresolved native type")
        else:
            for child in ast.iter_child_nodes(node):
                if isinstance(child, ast.expr):
                    annotation(child, location)

    for path in sorted(options.declarations.rglob("*.pyi")):
        relative = path.relative_to(options.declarations).with_suffix("")
        parts = list(relative.parts)
        if parts[-1] == "__init__":
            parts.pop()
        name = ".".join([surface.PUBLIC_ROOT, *parts])
        tree = ast.parse(path.read_text())
        if name not in {"sigil", "sigil._types"}:
            try:
                inspect_members(public_object(name), tree, name)
            except ImportError:
                if not any(raw in surface.OPTIONAL_MODULES
                           for raw in surface.raw_modules(name)):
                    raise
                continue
        for node in ast.walk(tree):
            if isinstance(node, ast.AnnAssign):
                annotation(node.annotation, f"{name}:{node.lineno}")
            elif isinstance(node, ast.arg):
                if re.fullmatch(r"arg\d+", node.arg):
                    failures.append(
                        f"{name}:{node.lineno}: unnamed argument {node.arg}"
                    )
                if node.annotation is None and node.arg not in {"self", "cls"}:
                    failures.append(
                        f"{name}:{node.lineno}: untyped argument {node.arg}"
                    )
                elif node.annotation is not None:
                    annotation(node.annotation, f"{name}:{node.lineno}")
            elif isinstance(node, ast.FunctionDef):
                if node.returns is None:
                    failures.append(f"{name}:{node.lineno}: untyped return")
                else:
                    annotation(node.returns, f"{name}:{node.lineno}")

    # No public module may carry a name the table did not put there, and the
    # raw module an author never imports must have nowhere to be imported from.
    for raw in surface.PUBLIC_MODULES:
        if raw in surface.DECLARATION_ONLY:
            continue
        public = surface.public_module(raw)
        try:
            native = importlib.import_module(raw)
        except ImportError:
            # A library behind a licensed SDK this build does not carry
            # registers nothing, so there is nothing here to check against.
            if raw in surface.OPTIONAL_MODULES:
                optional += 1
                continue
            raise
        holder = public_object(public)
        for exported in dir(native):
            if exported.startswith("_"):
                continue
            child = raw + "." + exported
            if (
                child in surface.PUBLIC_MODULES
                and surface.public_module(child) != public + "." + exported
            ):
                if getattr(holder, exported, None) is getattr(native, exported):
                    failures.append(
                        f"{public}.{exported}: {child} surfaces as "
                        f"{surface.public_module(child)}, so nothing carries it here"
                    )
                continue
            spelling = surface.public_name(public, exported)
            if not hasattr(holder, spelling):
                failures.append(
                    f"{public}.{spelling}: {child} reaches no public module"
                )
    if failures:
        print("\n".join(failures))
        return 1
    print(
        f"Public surface passed: {count} exported members; named arguments, "
        "no Any or bare collection signatures, nothing raw left over"
        + (f"; {optional} optional modules this build does not carry." if optional
           else ".")
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
