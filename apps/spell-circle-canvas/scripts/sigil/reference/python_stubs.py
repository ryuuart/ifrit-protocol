"""Reader two: the Python surface, out of the generated declarations.

One tree is read: the declarations written beside the extension when it
is built. They carry both halves of what a page needs — the signatures,
because they are generated from the compiled bindings, and the
SPELLINGS, because they are written under the module an author imports
from and under the name an author types.

A binding names the module it registered into, which is not a module
anybody imports. The package's own table says where each of those
surfaces and which names it renames on the way, so a binding can be
followed to the page that answers for it.

The role unions in `_types.pyi` are read here too. They are the Python
half of "what can I pass here" and are invisible to a reader, being a
type checker's artefact; expanded onto a page they answer the question
outright.
"""

import ast
import dataclasses
import importlib.util
from pathlib import Path

from sigil.reference import model

PUBLIC = "sigil"


def surface_table(package: Path):
    """The package's own raw-to-public table, loaded from its file.

    It is read rather than imported by name because it belongs to the
    Python project, which is not on this tool's import path and must
    not become a dependency of it.
    """
    path = package / "typing" / "surface.py"
    specification = importlib.util.spec_from_file_location("sigil_surface", path)
    if specification is None or specification.loader is None:
        raise FileNotFoundError(path)
    table = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(table)
    return table


@dataclasses.dataclass
class Declaration:
    """One class, function or attribute a stub declares."""

    module: str
    owner: str
    name: str
    kind: str
    signatures: list = dataclasses.field(default_factory=list)
    doc: str = ""
    annotation: str = ""

    def dotted(self) -> str:
        parts = [self.module, self.owner, self.name]
        return ".".join(part for part in parts if part)


def annotation_of(node) -> str:
    return ast.unparse(node) if node is not None else ""


def signature_of(node: ast.FunctionDef) -> model.Signature:
    parameters = []
    arguments = node.args
    positional = list(arguments.posonlyargs) + list(arguments.args)
    defaults = list(arguments.defaults)
    filled = [None] * (len(positional) - len(defaults)) + defaults
    for argument, default in zip(positional, filled):
        parameters.append(
            model.Parameter(
                name=argument.arg,
                type_text=annotation_of(argument.annotation),
                default=ast.unparse(default) if default is not None else "",
            )
        )
    if arguments.vararg is not None:
        parameters.append(
            model.Parameter(
                name=f"*{arguments.vararg.arg}",
                type_text=annotation_of(arguments.vararg.annotation),
            )
        )
    for argument, default in zip(arguments.kwonlyargs, arguments.kw_defaults):
        parameters.append(
            model.Parameter(
                name=argument.arg,
                type_text=annotation_of(argument.annotation),
                default=ast.unparse(default) if default is not None else "",
            )
        )
    if arguments.kwarg is not None:
        parameters.append(
            model.Parameter(
                name=f"**{arguments.kwarg.arg}",
                type_text=annotation_of(arguments.kwarg.annotation),
            )
        )
    return model.Signature(
        returns=annotation_of(node.returns),
        parameters=parameters,
        language="python",
    )


def _functions(body: list) -> list:
    return [
        statement
        for statement in body
        if isinstance(statement, (ast.FunctionDef, ast.AsyncFunctionDef))
    ]


class Module:
    """One parsed `.pyi` file, and what it says is in it."""

    def __init__(self, dotted: str, path: Path):
        self.dotted = dotted
        self.path = path
        self.declarations = {}
        self.exports = {}
        self.declared_all = None
        tree = ast.parse(path.read_text(), filename=str(path))
        self._walk(tree)

    def _walk(self, tree: ast.Module) -> None:
        for statement in tree.body:
            if isinstance(statement, ast.ClassDef):
                self._class(statement)
            elif isinstance(statement, (ast.FunctionDef, ast.AsyncFunctionDef)):
                self._function(statement)
            elif isinstance(statement, ast.AnnAssign):
                self._annotated(statement)
            elif isinstance(statement, ast.Assign):
                self._assigned(statement)
            elif isinstance(statement, ast.ImportFrom):
                self._imported(statement)

    def _class(self, node: ast.ClassDef) -> None:
        declaration = Declaration(
            module=self.dotted,
            owner="",
            name=node.name,
            kind="class",
            doc=ast.get_docstring(node) or "",
        )
        self.declarations[node.name] = declaration
        for member in _functions(node.body):
            name = member.name
            existing = self.declarations.get(f"{node.name}.{name}")
            if existing is None:
                existing = Declaration(
                    module=self.dotted,
                    owner=node.name,
                    name=name,
                    kind="method",
                    doc=ast.get_docstring(member) or "",
                )
                self.declarations[f"{node.name}.{name}"] = existing
            existing.signatures.append(signature_of(member))
        for member in node.body:
            if isinstance(member, ast.AnnAssign) and isinstance(
                member.target, ast.Name
            ):
                name = member.target.id
                self.declarations[f"{node.name}.{name}"] = Declaration(
                    module=self.dotted,
                    owner=node.name,
                    name=name,
                    kind="attribute",
                    annotation=annotation_of(member.annotation),
                )

    def _function(self, node) -> None:
        existing = self.declarations.get(node.name)
        if existing is None or existing.kind != "function":
            existing = Declaration(
                module=self.dotted,
                owner="",
                name=node.name,
                kind="function",
                doc=ast.get_docstring(node) or "",
            )
            self.declarations[node.name] = existing
        existing.signatures.append(signature_of(node))

    def _annotated(self, node: ast.AnnAssign) -> None:
        if not isinstance(node.target, ast.Name):
            return
        name = node.target.id
        if name == "__all__":
            if node.value is not None:
                self.declared_all = _string_list(node.value)
            return
        self.declarations[name] = Declaration(
            module=self.dotted,
            owner="",
            name=name,
            kind="alias"
            if annotation_of(node.annotation) == "TypeAlias"
            else "attribute",
            annotation=ast.unparse(node.value) if node.value is not None else "",
        )

    def _assigned(self, node: ast.Assign) -> None:
        for target in node.targets:
            if isinstance(target, ast.Name) and target.id == "__all__":
                self.declared_all = _string_list(node.value)

    def _imported(self, node: ast.ImportFrom) -> None:
        source = self._resolve(node)
        if source is None:
            return
        for alias in node.names:
            if alias.name == "*":
                # A star carries no name of its own, so there is nothing here
                # to answer for. The generated declarations write none.
                continue
            self.exports[alias.asname or alias.name] = f"{source}.{alias.name}"

    def _resolve(self, node: ast.ImportFrom) -> str | None:
        if node.level:
            parts = self.dotted.split(".")
            base = parts[: len(parts) - node.level + 1]
            return ".".join(base + ([node.module] if node.module else []))
        return node.module

    def public_names(self) -> list:
        """What this module offers, whether or not it says so.

        A generated declaration states its members outright and reaches
        its submodules by importing them under a name of their own, so
        what it offers is what it declares plus what it imports that
        way. The role unions are the one file that also carries an
        `__all__`, and where there is one it has the last word.
        """
        if self.declared_all is not None:
            return list(self.declared_all)
        names = [
            name
            for name in self.declarations
            if "." not in name and not name.startswith("_")
        ]
        names.extend(name for name in self.exports if not name.startswith("_"))
        return names


def _string_list(node) -> list:
    if isinstance(node, (ast.List, ast.Tuple)):
        return [
            item.value
            for item in node.elts
            if isinstance(item, ast.Constant) and isinstance(item.value, str)
        ]
    return []


def _dotted_for(root: Path, path: Path, top: str) -> str:
    relative = path.relative_to(root).with_suffix("")
    parts = list(relative.parts)
    if parts and parts[-1] == "__init__":
        parts.pop()
    return ".".join([top, *parts]) if parts else top


def read_tree(root: Path, top: str) -> dict:
    """Every `.pyi` under one package root, keyed by its dotted name."""
    modules = {}
    for path in sorted(root.rglob("*.pyi")):
        dotted = _dotted_for(root, path, top)
        modules[dotted] = Module(dotted, path)
    return modules


class Surface:
    """The Python surface: what is declared, and what it is called.

    `declaration(path)` takes either spelling. `public_of(path)` takes
    the path a binding registered and hands back the spelling a page
    shows, or nothing where the package does not carry that name.
    """

    def __init__(self, public: dict, table):
        self.public = public
        self.table = table
        self.paths = sorted(
            f"{dotted}.{name}"
            for dotted, module in public.items()
            for name in module.public_names()
            if not name.startswith("_") and name in module.declarations
        )

    def public_of(self, path: str) -> str:
        """The spelling an author types for a path a binding registered.

        The longest module prefix the table knows wins, so a class and a
        method of that class both resolve through the module they are
        registered in.
        """
        parts = path.split(".")
        for size in range(len(parts) - 1, 0, -1):
            try:
                holder = self.table.public_module(".".join(parts[:size]))
            except KeyError:
                continue
            rest = parts[size:]
            spelling = ".".join(
                [holder, self.table.public_name(holder, rest[0]), *rest[1:]]
            )
            return spelling if self._declared(spelling) is not None else ""
        return ""

    def declaration(self, path: str):
        return self._declared(path) or self._declared(self.public_of(path))

    def _declared(self, path: str):
        module, _, name = path.rpartition(".")
        holder = self.public.get(module)
        if holder is not None and name in holder.declarations:
            return holder.declarations[name]
        # A method: the owner is the class, one segment further up.
        head, _, owner = module.rpartition(".")
        holder = self.public.get(head)
        if holder is not None:
            return holder.declarations.get(f"{owner}.{name}")
        return None

    def declared(self) -> list:
        """Every declaration in the package, under the name it is read by."""
        found = []
        for dotted, module in sorted(self.public.items()):
            for key, declaration in module.declarations.items():
                if key.startswith("_") or "._" in key:
                    continue
                found.append((f"{dotted}.{key}", declaration))
        return found


class Roles:
    """The `…Like` unions: what a parameter of that role really takes.

    A union is stored with its operands as written and with the nested
    unions kept as their own names, because "a `PaintLike` is a
    `MotionFillLike`, a `Paint` or a `SurfacePaint`" is what a reader
    needs and the flattened twelve members are not.
    """

    def __init__(self, path: Path):
        self.members = {}
        tree = ast.parse(path.read_text(), filename=str(path))
        for statement in tree.body:
            if (
                isinstance(statement, ast.AnnAssign)
                and annotation_of(statement.annotation) == "TypeAlias"
                and isinstance(statement.target, ast.Name)
                and statement.value is not None
            ):
                self.members[statement.target.id] = [
                    ast.unparse(operand) for operand in _operands(statement.value)
                ]

    def path_of(self, member: str) -> str:
        """The member's public path, or nothing when it names no type."""
        return member if member.startswith(PUBLIC + ".") else ""

    def expand(self, alias: str, seen: tuple = ()) -> list:
        """The union's members, with a nested union's own members after it."""
        if alias not in self.members or alias in seen:
            return []
        found = []
        for member in self.members[alias]:
            found.append(member)
            found.extend(self.expand(member, seen + (alias,)))
        return found

    def containing(self, path: str) -> list:
        """Every union a type is a member of, nesting followed outward."""
        direct = [
            alias
            for alias, members in self.members.items()
            if any(self.path_of(member) == path for member in members)
        ]
        found = list(direct)
        frontier = list(direct)
        while frontier:
            inner = frontier.pop()
            for alias, members in self.members.items():
                if inner in members and alias not in found:
                    found.append(alias)
                    frontier.append(alias)
        return sorted(found)


def _operands(node) -> list:
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.BitOr):
        return _operands(node.left) + _operands(node.right)
    return [node]


def read(package: Path, declarations: Path) -> tuple:
    """The whole Python side: the surface and the role unions."""
    public = read_tree(declarations, PUBLIC)
    surface = Surface(public, surface_table(package))
    roles = Roles(declarations / "_types.pyi")
    return surface, roles
