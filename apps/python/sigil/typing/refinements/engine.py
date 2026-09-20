"""Apply a subject's refinements to the declarations stubgen produced.

Property-backed keyword records use their setter types. The kit and brush
field helpers share documented conversions, which the fragments mirror; other
method conversions are named explicitly and fail generation when removed.
"""

from __future__ import annotations

import ast
import copy
import re

from .table import COLOR, MATERIAL, PAINT, Table


def expression(text: str) -> ast.expr:
    return ast.parse(text, mode="eval").body


def show(node: ast.AST | None) -> str:
    return ast.unparse(node) if node is not None else ""


def erased_annotation(node: ast.AST | None) -> bool:
    text = show(node)
    return any(
        token in text for token in ("typing.Any", "...", "os.PathLike |")
    ) or text in {
        "collections.abc.Callable",
        "collections.abc.Iterable",
        "collections.abc.Sequence",
        "tuple",
        "list",
        "dict",
        "list[tuple]",
        "collections.abc.Sequence[tuple]",
    }


def setter_type(annotation: ast.expr, module: str) -> ast.expr:
    text = show(annotation)
    mappings = {"float": "_t.FloatLike", "int": "typing.SupportsInt"}
    if module.startswith(
        ("_sigil.compose.kit", "_sigil.sketch.kit", "_sigil.compose.layouts")
    ):
        mappings.update(
            {
                "_sigil.skia.Point": "_t.PointLike",
                "_sigil.skia.Size": "_t.SizeLike",
                "_sigil.compose.Dimension": "_t.DimensionLike",
                "_sigil.compose.Fill": "_t.FillLike",
                "_sigil.compose.SurfacePaint": "_t.SurfacePaintLike",
                "_sigil.compose.Align": "_t.AlignLike",
                "_sigil.compose.Justify": "_t.JustifyLike",
            }
        )
    elif module == "_sigil.draw.brush":
        mappings.update({"_sigil.skia.Point": "_t.PointLike"})
    if text in mappings:
        return expression(mappings[text])
    if text.endswith(" | None"):
        inner = setter_type(expression(text.removesuffix(" | None")), module)
        return expression(show(inner) + " | None")
    if text.startswith("list["):
        return expression("collections.abc.Sequence[" + text[5:])
    return copy.deepcopy(annotation)


def refine(table: Table, seen: set[str], module: str, tree: ast.Module) -> None:
    erased = table.erased_arguments
    returns = table.return_types
    declarations = table.declarations
    parameters = table.parameter_types
    attributes = table.attribute_types

    # Inside the paint's own module the generated stubs spell it bare,
    # and Skia's unrelated Paint is spelled bare in its module, so the
    # name a material paint answers to is decided per module.
    paint_name = "Paint" if module == "_sigil.material.skia" else PAINT
    if module == "_sigil.skia":
        paint_name = ""

    def visit(body: list[ast.stmt], path: str, class_name: str | None = None) -> None:
        getters = {
            n.name: n
            for n in body
            if isinstance(n, ast.FunctionDef)
            and any(show(d) == "property" for d in n.decorator_list)
        }
        output: list[ast.stmt] = []
        replaced: set[str] = set()
        for node in body:
            if isinstance(node, (ast.FunctionDef, ast.ClassDef)):
                seen.add(path + "." + node.name)
            elif isinstance(node, ast.AnnAssign):
                seen.add(path + "." + show(node.target))
            if isinstance(node, ast.ClassDef):
                if path == "_sigil.draw.brush.Pressure" and node.name in {
                    "Gaussian",
                    "Variation",
                }:
                    output.extend(
                        ast.parse(f"{node.name} = _sigil.draw.brush.{node.name}").body
                    )
                    continue
                visit(node.body, path + "." + node.name, node.name)
            elif (
                isinstance(node, ast.AnnAssign)
                and path + "." + show(node.target) in declarations
            ):
                output.extend(
                    ast.parse(declarations[path + "." + show(node.target)]).body
                )
                continue
            elif isinstance(node, ast.AnnAssign) and show(node.annotation) == "tuple":
                dim = 4 if path.endswith(".BoxOptions") else 3
                name = show(node.target)
                output.extend(
                    ast.parse(
                        f"@property\ndef {name}(self) -> _t.Vec{dim}: ...\n@{name}.setter\ndef {name}(self, value: _t.Vec{dim}Like) -> None: ..."
                    ).body
                )
                continue
            elif (
                isinstance(node, ast.AnnAssign)
                and show(node.annotation) == COLOR
                and path + "." + show(node.target) not in attributes
            ):
                # A colour field reads back as the colour class and is
                # written with any colour spelling, because the binding's
                # caster reads every one of them.
                name = show(node.target)
                output.extend(
                    ast.parse(
                        f"@property\ndef {name}(self) -> {COLOR}: ...\n"
                        f"@{name}.setter\ndef {name}(self, value: _t.ColorLike) -> None: ..."
                    ).body
                )
                continue
            elif (
                isinstance(node, ast.AnnAssign)
                and path + "." + show(node.target) in attributes
            ):
                node.annotation = expression(attributes[path + "." + show(node.target)])
            elif isinstance(node, ast.FunctionDef):
                if node.name.startswith("__keyword_"):
                    continue
                full = path + "." + node.name
                if full in declarations:
                    if full not in replaced:
                        output.extend(ast.parse(declarations[full]).body)
                        replaced.add(full)
                    continue
                all_args = [
                    *node.args.posonlyargs,
                    *node.args.args,
                    *node.args.kwonlyargs,
                ]
                anonymous = any(re.fullmatch(r"arg\d+", a.arg) for a in all_args)
                # Property access and Python protocols pass these inputs
                # positionally; pybind11 supplies no author-chosen names.
                if any(show(d).endswith(".setter") for d in node.decorator_list):
                    all_args[-1].arg = "value"
                elif node.name in {"__eq__", "__ne__"}:
                    all_args[-1].arg = "other"
                elif node.name == "__setstate__":
                    all_args[-1].arg = "state"
                elif (
                    node.name == "__init__"
                    and any(
                        isinstance(n, ast.FunctionDef)
                        and n.name == "__members__"
                        or isinstance(n, ast.AnnAssign)
                        and show(n.target) == "__members__"
                        for n in body
                    )
                    and len(all_args) == 2
                ):
                    all_args[-1].arg = "value"
                if anonymous and not any(
                    re.fullmatch(r"arg\d+", a.arg) for a in all_args
                ):
                    node.args.posonlyargs.extend(node.args.args)
                    node.args.args.clear()
                unknown = [a for a in all_args if show(a.annotation) == "typing.Any"]
                if node.name in {"__eq__", "__ne__"}:
                    for arg in all_args:
                        if arg.arg != "self":
                            arg.annotation = expression("builtins.object")
                elif full in erased:
                    values = erased[full]
                    if unknown:
                        if len(values) != len(unknown):
                            raise RuntimeError(
                                f"{full}: expected {len(values)} erased arguments, found {len(unknown)}"
                            )
                        for arg, type_ in zip(unknown, values, strict=True):
                            arg.annotation = expression(type_)
                elif (
                    any(show(d).endswith(".setter") for d in node.decorator_list)
                    and unknown
                ):
                    getter = getters[node.name]
                    if getter.returns is not None:
                        unknown[-1].annotation = setter_type(getter.returns, module)
                if full in parameters:
                    for arg in all_args:
                        if arg.arg in parameters[full] and erased_annotation(
                            arg.annotation
                        ):
                            arg.annotation = expression(parameters[full][arg.arg])
                # Every parameter written as a colour reads every colour
                # spelling, because one caster stands between Python and
                # the native colour, and a sequence of colours reads them
                # one by one; a colour RETURNED is the class itself.
                for arg in all_args:
                    text = show(arg.annotation)
                    if COLOR in text:
                        text = text.replace(COLOR, "_t.ColorLike")
                        arg.annotation = expression(text)
                    # The paint's own constructor states the material
                    # overload the conversion is registered against, so
                    # widening its paint overload would only repeat it.
                    if (
                        paint_name
                        and paint_name in text
                        and full != PAINT + ".__init__"
                    ):
                        arg.annotation = expression(
                            text.replace(paint_name, f"({paint_name} | {MATERIAL})")
                        )
                if full in returns and not any(
                    show(d).endswith(".setter") for d in node.decorator_list
                ):
                    node.returns = expression(returns[full])
                positional = [*node.args.posonlyargs, *node.args.args]
                for arg, default in zip(
                    positional[-len(node.args.defaults) :], node.args.defaults
                ):
                    if (
                        isinstance(default, ast.Constant)
                        and default.value is None
                        and arg.annotation is not None
                        and "None" not in show(arg.annotation)
                    ):
                        arg.annotation = expression(show(arg.annotation) + " | None")
                if (
                    isinstance(node.returns, ast.Constant)
                    and node.returns.value is Ellipsis
                ):
                    raise RuntimeError(f"Unregistered native return type: {full}")
            output.append(node)
        body[:] = output
        # Record constructors assign the same exposed properties as edits do.
        setters = {
            show(n.target): n.annotation
            for n in body
            if isinstance(n, ast.AnnAssign)
            and not show(n.target).startswith("_")
            and "ClassVar" not in show(n.annotation)
        }
        setters.update(
            {
                n.name: [*n.args.posonlyargs, *n.args.args][-1].annotation
                for n in body
                if isinstance(n, ast.FunctionDef)
                and any(show(d).endswith(".setter") for d in n.decorator_list)
            }
        )
        for node in body:
            if (
                isinstance(node, ast.FunctionDef)
                and node.name == "__init__"
                and node.args.kwarg
            ):
                node.args.kwarg = None
                node.args.kwonlyargs = [
                    ast.arg(arg=name, annotation=copy.deepcopy(value))
                    for name, value in setters.items()
                ]
                node.args.kw_defaults = [ast.Constant(Ellipsis) for _ in setters]

    visit(tree.body, module)
    if module == "_sigil.draw.brush":
        tree.body[:] = [
            n
            for n in tree.body
            if not (isinstance(n, ast.ImportFrom) and n.module == "builtins")
        ]
        tree.body.append(ast.parse("Stroke: typing.TypeAlias = list[Sample]").body[0])
    imports = ast.parse(
        "import _sigil\nimport _sigil._types as _t\nimport builtins\nimport collections.abc\nimport typing\nimport types\nimport os\n"
    ).body
    tree.body[1:1] = imports
    prune_imports(tree)


def exported_names(tree: ast.Module) -> set[str]:
    exports: set[str] = set()
    for node in tree.body:
        if (
            isinstance(node, ast.AnnAssign)
            and show(node.target) == "__all__"
            and isinstance(node.value, ast.List)
        ):
            exports.update(
                item.value
                for item in node.value.elts
                if isinstance(item, ast.Constant) and isinstance(item.value, str)
            )
    return exports


def prune_imports(tree: ast.Module, keep: frozenset[str] = frozenset()) -> None:
    """Drop the imports nothing in this module names, and the repeated ones."""
    used = {node.id for node in ast.walk(tree) if isinstance(node, ast.Name)}
    used.update(
        show(node) for node in ast.walk(tree) if isinstance(node, ast.Attribute)
    )
    exports = exported_names(tree) | keep
    imported: set[str] = set()
    clean: list[ast.stmt] = []
    for node in tree.body:
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            text = show(node)
            if text in imported:
                continue
            imported.add(text)
            if isinstance(node, ast.Import):
                node.names[:] = [
                    alias
                    for alias in node.names
                    if (alias.asname or alias.name) in used
                ]
            elif node.module != "__future__":
                node.names[:] = [
                    alias
                    for alias in node.names
                    if (alias.asname or alias.name) in used | exports
                ]
            if not node.names:
                continue
        clean.append(node)
    tree.body[:] = clean
