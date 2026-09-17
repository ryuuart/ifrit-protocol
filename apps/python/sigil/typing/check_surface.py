"""Check native exports and reject untyped holes in the shipped native stubs."""

from __future__ import annotations

import argparse
import ast
import importlib
import inspect
import keyword
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stubs", type=Path, default=HERE.parent / "stubs" / "_sigil")
    parser.add_argument("--public", type=Path, default=HERE.parent / "sigil")
    options = parser.parse_args()
    failures: list[str] = []
    count = 0

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
                    name in base.__dict__ and base.__module__.startswith("_sigil")
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

    for stub in sorted(options.stubs.rglob("*.pyi")):
        relative = stub.relative_to(options.stubs).with_suffix("")
        parts = list(relative.parts)
        if parts[-1] == "__init__":
            parts.pop()
        name = ".".join(["_sigil", *parts])
        tree = ast.parse(stub.read_text())
        if name != "_sigil._types":
            inspect_members(importlib.import_module(name), tree, name)
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
    for name in ("weave", "data", "io", "material"):
        native = importlib.import_module("_sigil." + name)
        public = importlib.import_module("sigil." + name)
        body = ast.parse((options.public / f"{name}.pyi").read_text()).body
        declared = members(body)
        if any(
            isinstance(node, ast.ImportFrom)
            and node.module == "_sigil." + name
            and any(alias.name == "*" for alias in node.names)
            for node in body
        ):
            declared.update({exported: body[0] for exported in dir(native)})
        for exported in dir(native):
            if exported.startswith("_"):
                continue
            if exported not in declared or not hasattr(public, exported):
                failures.append(
                    f"sigil.{name}.{exported}: native export missing from public package or stub"
                )
    if failures:
        print("\n".join(failures))
        return 1
    print(
        f"Native surface passed: {count} exported members; named arguments, no Any or bare collection signatures."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
