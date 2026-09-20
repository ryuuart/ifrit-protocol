"""Reader two: the Python surface, out of the declaration stubs.

Two trees are read, and the difference between them is the point. The
stubs under `stubs/_sigil/` carry the signatures — they are generated
from the compiled bindings, so they are what the extension really
declares. The package under `sigil/` carries the SPELLINGS — what an
author types — and a name arrives there re-exported, renamed, or
declared outright, which is a thing only that tree knows.

A page shows the public spelling and the stub's signature. Where the
public tree declares a name itself, that declaration wins: it is the
one that was written for a reader rather than derived from a binding.

The role unions in `typing/_types.pyi` are read here too. They are the
Python half of "what can I pass here" and are invisible to a reader,
being a type checker's artefact; expanded onto a page they answer the
question outright.
"""

import ast
import dataclasses
from pathlib import Path

from sigil.reference import model

NATIVE = "_sigil"
PUBLIC = "sigil"

# The public package re-exports every native module at the top level, so
# a module whose whole content is those same modules under a second name
# would give every name in the tree a second spelling and no way to
# choose between them.
SECOND_SPELLINGS = ("sigil.native",)


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
        self.stars = []
        self.submodules = {}
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
            elif isinstance(statement, ast.Import):
                for alias in statement.names:
                    if alias.name.startswith(NATIVE):
                        self.submodules[alias.asname or alias.name] = alias.name

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
                self.stars.append(source)
            else:
                self.exports[alias.asname or alias.name] = f"{source}.{alias.name}"

    def _resolve(self, node: ast.ImportFrom) -> str | None:
        if node.level:
            parts = self.dotted.split(".")
            base = parts[: len(parts) - node.level + 1]
            return ".".join(base + ([node.module] if node.module else []))
        return node.module

    def public_names(self) -> list:
        """What this module offers, whether or not it says so.

        A generated stub declares `__all__`; a hand-written re-export
        usually does not, and one written as `import *` declares nothing
        at all — so the names it offers are the starred module's, which
        only the caller holding both modules can resolve.
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

    `declaration(path)` takes either spelling. `public_of(path)` takes a
    native path and hands back the shortest public one, which is the
    spelling a page shows.
    """

    def __init__(self, native: dict, public: dict):
        self.native = native
        self.public = public
        self.spellings = {}
        self._map_public()

    def _map_public(self) -> None:
        for dotted, module in sorted(self.public.items()):
            if dotted in SECOND_SPELLINGS:
                continue
            for name in self._offered(module):
                if name.startswith("_"):
                    continue
                self._record(f"{dotted}.{name}", module, name)

    def _offered(self, module: Module) -> list:
        names = list(module.public_names())
        for source in module.stars:
            starred = self.native.get(source) or self.public.get(source)
            if starred is not None:
                names.extend(
                    name for name in starred.public_names() if name not in names
                )
        return names

    def _record(self, spelling: str, module: Module, name: str) -> None:
        target = module.exports.get(name)
        if target is None and name in module.declarations:
            target = f"{module.dotted}.{name}"
        if target is None:
            for source in module.stars:
                candidate = f"{source}.{name}"
                if self._exists(candidate):
                    target = candidate
                    break
        if target is None:
            return
        self._claim(target, spelling)
        owner = target.rsplit(".", 1)
        native = self.native.get(target)
        if native is not None:
            # A module re-exported whole brings its own names with it.
            for inner in native.public_names():
                if not inner.startswith("_"):
                    self._record(f"{spelling}.{inner}", native, inner)
            return
        holder = self.native.get(owner[0]) or self.public.get(owner[0])
        if holder is None:
            return
        for key, declaration in holder.declarations.items():
            if declaration.owner == owner[1]:
                self._claim(f"{owner[0]}.{key}", f"{spelling}.{declaration.name}")

    def _claim(self, target: str, spelling: str) -> None:
        held = self.spellings.get(target)
        if held is None or (len(spelling), spelling) < (len(held), held):
            self.spellings[target] = spelling

    def _exists(self, path: str) -> bool:
        if path in self.native or path in self.public:
            return True
        module, _, name = path.rpartition(".")
        holder = self.native.get(module) or self.public.get(module)
        return holder is not None and name in holder.declarations

    def declaration(self, path: str):
        module, _, name = path.rpartition(".")
        holder = self.native.get(module) or self.public.get(module)
        if holder is not None and name in holder.declarations:
            return holder.declarations[name]
        # A method: the owner is the class, one segment further up.
        head, _, owner = module.rpartition(".")
        holder = self.native.get(head) or self.public.get(head)
        if holder is not None:
            return holder.declarations.get(f"{owner}.{name}")
        return None

    def public_of(self, path: str) -> str:
        return self.spellings.get(path, "")

    def declared(self) -> list:
        """Every native declaration that has a public spelling."""
        found = []
        for target, spelling in sorted(self.spellings.items()):
            declaration = self.declaration(target)
            if declaration is not None:
                found.append((spelling, declaration))
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
        self.modules = {}
        tree = ast.parse(path.read_text(), filename=str(path))
        for statement in tree.body:
            if isinstance(statement, ast.ImportFrom) and statement.module == NATIVE:
                for alias in statement.names:
                    self.modules[alias.asname or alias.name] = f"{NATIVE}.{alias.name}"
            if (
                isinstance(statement, ast.AnnAssign)
                and annotation_of(statement.annotation) == "TypeAlias"
                and isinstance(statement.target, ast.Name)
                and statement.value is not None
            ):
                self.members[statement.target.id] = [
                    ast.unparse(operand) for operand in _operands(statement.value)
                ]

    def native_path(self, spelling: str) -> str:
        """`compose.Fill` as this file writes it, as a native path."""
        head, _, rest = spelling.partition(".")
        base = self.modules.get(head)
        return f"{base}.{rest}" if base and rest else ""

    def expand(self, alias: str, seen: tuple = ()) -> list:
        """The union's members, with a nested union's own members after it."""
        if alias not in self.members or alias in seen:
            return []
        found = []
        for member in self.members[alias]:
            found.append(member)
            found.extend(self.expand(member, seen + (alias,)))
        return found

    def containing(self, native_path: str) -> list:
        """Every union a type is a member of, nesting followed outward."""
        direct = [
            alias
            for alias, members in self.members.items()
            if any(self.native_path(member) == native_path for member in members)
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


def read(package: Path) -> tuple:
    """The whole Python side: the surface and the role unions."""
    native = read_tree(package / "stubs" / NATIVE, NATIVE)
    public = read_tree(package / PUBLIC, PUBLIC)
    surface = Surface(native, public)
    roles = Roles(package / "typing" / "_types.pyi")
    return surface, roles
