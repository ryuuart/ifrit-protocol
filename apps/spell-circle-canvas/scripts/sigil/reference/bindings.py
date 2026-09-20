"""Reader three: how a C++ name reaches Python, out of the binding sources.

The stubs say what Python declares and the XML says what C++ declares;
neither says how the two are joined. A binding does, one call per line:
`.def("absolute", &Element::absolute)` is the same call under both
spellings, while `.def("alignItems", [](…){ … })` is a widened one and
`.def("size", [](…){ … })` composes two C++ verbs into a convenience
that has no C++ counterpart at all. Those three shapes, plus a C++
member nothing binds, are the four states a page's badge names.

A scan is honest here because the only thing it decides is that badge,
and it sits between two inventories that check it: a name it finds
that no header declares is either a convenience only Python has, which
the stubs then confirm, or a contradiction the coverage report prints
by name.
"""

import dataclasses
import re
from pathlib import Path

from sigil.reference import model

# Every pybind11 call that gives Python a name.
DEFINE = re.compile(
    r"\.def(?P<flavour>_static|_readwrite|_readonly|_property_readonly|_property|_buffer)?"
    # The tail is looked at rather than consumed: a short direct
    # binding is followed immediately by the next one, and a match that
    # swallowed what comes after it would hide every other name.
    r"\s*\(\s*\"(?P<name>[^\"]+)\"\s*(?:,\s*)?(?=(?P<tail>.{0,80}))",
    re.DOTALL,
)
HOLDER = re.compile(
    r"py::(?P<holder>class_|enum_)\s*<(?P<arguments>.+?)>\s*(?P<variable>\w+)?\s*\(",
    re.DOTALL,
)
ALIAS = re.compile(r"^\s*namespace\s+(\w+)\s*=\s*([\w:]+)\s*;", re.MULTILINE)
USING = re.compile(r"^\s*using\s+([\w:]+::\w+)\s*;", re.MULTILINE)
INCLUDE = re.compile(r"^#include\s+<(sigil\w+|ifrit\w+|spellcircle)/", re.MULTILINE)

# Not every name is spelled at a `.def`. A run of verbs that differ only
# in which setter they call is written as a table of name-and-member
# pairs, or handed to a local helper one pair at a time; either way the
# member pointer beside the name says which C++ member it is.
PAIRED = re.compile(r"\"(?P<name>[A-Za-z_]\w*)\"\s*,\s*&\s*(?P<target>[\w:]+)")

# A run that differs only in a flag names no member at all: the loop
# carries the Python names and the body decides from them.
NAME_LIST = re.compile(r"\bfor\s*\([^)]*:\s*\{(?P<names>(?:\s*\"[^\"]+\"\s*,?)+)\}")
DEFINE_BY_VARIABLE = re.compile(
    r"\.def(?P<flavour>_static)?\s*\(\s*(?P<argument>[A-Za-z_]\w*)\s*,"
)
ASSIGNED = re.compile(r"\bauto\s+(?P<variable>\w+)\s*=\s*")
SUBMODULE = re.compile(
    r"(?P<parent>\w+)?((?:\s*\.def_submodule\s*\(\s*\"(?P<leaf>\w+)\"\s*\))+)"
)
SUBMODULE_LEAF = re.compile(r"\.def_submodule\s*\(\s*\"(\w+)\"\s*\)")
# A submodule another file already made is reached by attribute rather
# than made again.
ATTACHED = re.compile(
    r"(?P<parent>\w+)\s*\.attr\s*\(\s*\"(?P<leaf>\w+)\"\s*\)\s*\.cast\s*<\s*py::module_"
)
ENTRY = re.compile(
    r"\bvoid\s+bind\w*\s*\(\s*(?:py|pybind11)::module_\s*&\s*(?P<parameter>\w+)\s*\)"
)
# An enumerator, and the C++ expression it is given. The two differ
# whenever the C++ spelling is not a legal Python name, so the name
# alone cannot be appended to the enumeration to recover it.
VALUE = re.compile(r"\.value\s*\(\s*\"(?P<name>[^\"]+)\"\s*(?:,\s*(?P<target>[\w:]+))?")
# `py::class_<Fill>(composition, "Fill")` — the module it lands in and
# the name Python knows the type by, which is the only place a type's
# Python spelling is written down.
HELD_NAME = re.compile(r"\s*(?P<parent>\w+)\s*,\s*\"(?P<name>\w+)\"")

# A binding function takes the module it writes into as its one
# parameter, and the name that parameter is given is the module: `root`
# and `module` are the extension's own root, anything else is the
# submodule the caller passed down.
ROOTS = ("module", "root")


@dataclasses.dataclass
class Binding:
    """One name pybind11 gives Python, and where it came from.

    `roots` are the include prefixes the binding source opens with.
    A binding writes `Element` for whichever Element its own file
    included, and the include line is the only thing in the file that
    says which library that is.
    """

    name: str
    owner: str = ""
    module: str = ""
    style: str = model.DIRECT
    target: str = ""
    flavour: str = "def"
    source: str = ""
    line: int = 0
    roots: tuple = ()

    def is_member(self) -> bool:
        return bool(self.owner)


def deduplicate(found: list) -> list:
    """One record per name a class or module is given.

    A direct binding is spelled as a name beside a member pointer, and
    the looser pattern that reads a table of those pairs sees the same
    text. Where both saw one call, the direct reading is the true one:
    the pair pattern cannot tell a table apart from a plain call.
    """
    held = {}
    order = []
    for binding in found:
        key = (binding.owner, binding.module, binding.name, binding.line)
        if key not in held:
            held[key] = binding
            order.append(key)
        elif binding.style == model.DIRECT:
            held[key] = binding
    return [held[key] for key in order]


def mask(text: str) -> str:
    """The source with every comment and literal blanked, same length.

    A statement ends at a semicolon, and a semicolon inside a string or
    a comment ends nothing. Keeping the length means an offset into the
    mask is an offset into the source.
    """
    out = []
    index = 0
    length = len(text)
    while index < length:
        char = text[index]
        pair = text[index : index + 2]
        if pair == "//":
            end = text.find("\n", index)
            end = length if end < 0 else end
            out.append(" " * (end - index))
            index = end
        elif pair == "/*":
            end = text.find("*/", index + 2)
            end = length if end < 0 else end + 2
            out.append(" " * (end - index))
            index = end
        elif char in "\"'":
            index += 1
            out.append(" ")
            while index < length and text[index] != char:
                step = 2 if text[index] == "\\" else 1
                out.append(" " * min(step, length - index))
                index += step
            if index < length:
                out.append(" ")
                index += 1
        else:
            out.append(char)
            index += 1
    return "".join(out)


def statements(text: str) -> list:
    """The source split into statements, as (offset, source) pairs.

    Only a semicolon outside every parenthesis ends one. A binding chain
    is mostly lambdas, and every statement inside a lambda body carries
    a semicolon that would otherwise cut the chain away from the class
    it is being written onto — while a lambda body is always inside the
    call it is an argument to, so parentheses alone tell the two apart.
    Braces are not counted: a namespace's brace stands open over the
    whole file and would make it one statement.
    """
    blanked = mask(text)
    found = []
    start = 0
    depth = 0
    for index, char in enumerate(blanked):
        if char in "([":
            depth += 1
        elif char in ")]":
            depth -= 1
        elif char == ";" and depth <= 0:
            found.append((start, text[start : index + 1]))
            start = index + 1
            depth = 0
    if text[start:].strip():
        found.append((start, text[start:]))
    return found


def first_argument(arguments: str) -> str:
    """The held type out of `py::class_<Held, Base, …>`."""
    depth = 0
    for index, char in enumerate(arguments):
        if char in "<([":
            depth += 1
        elif char in ">)]":
            depth -= 1
        elif char == "," and depth == 0:
            return arguments[:index].strip()
    return arguments.strip()


def qualify(name: str, aliases: dict, usings: dict) -> str:
    """A held type with the file's own shorthands spelled out.

    A binding writes `mskia::Paint` for a namespace alias and a bare
    `Element` for a using-declaration, where the inventory writes both
    in full. Expanding the two is what lets the names be matched on
    their tails; a namespace a `using namespace` opened is left off,
    and the join resolves that by suffix against the headers the file
    includes.
    """
    name = name.replace("const ", "").replace("&", "").strip()
    name = name.removeprefix("::")
    head, separator, rest = name.partition("::")
    if separator and head in aliases:
        return f"{aliases[head]}::{rest}"
    if not separator and name in usings:
        return usings[name]
    if separator and head in usings:
        return f"{usings[head]}::{rest}"
    return name


class Scan:
    """One binding source, read into the names it gives Python."""

    def __init__(self, path: Path):
        self.path = path
        self.bindings = []
        self.holders = {}
        self.enums = set()
        self.modules = {}
        self.aliases = {}
        self.usings = {}
        self.roots = ()
        text = path.read_text()
        self.text = text
        self._read(text)

    def _line_of(self, offset: int) -> int:
        return self.text.count("\n", 0, offset) + 1

    def _read(self, text: str) -> None:
        self.aliases = dict(ALIAS.findall(text))
        self.usings = {
            spelling.rsplit("::", 1)[-1]: spelling for spelling in USING.findall(text)
        }
        self.roots = tuple(dict.fromkeys(INCLUDE.findall(text)))
        root = ""
        pending = []
        for offset, statement in statements(text):
            entry = ENTRY.search(statement)
            if entry is not None:
                parameter = entry.group("parameter")
                root = "" if parameter in ROOTS else parameter
                self.modules[parameter] = root
            pending = self._statement(offset, statement, root, pending)
        self.bindings = deduplicate(self.bindings)

    def _statement(self, offset: int, statement: str, root: str, pending: list) -> list:
        owner, module = self._owner(offset, statement, root)
        listed = NAME_LIST.search(statement)
        if listed is not None:
            return re.findall(r"\"([^\"]+)\"", listed.group("names"))
        for match in DEFINE_BY_VARIABLE.finditer(statement):
            for name in pending:
                self.bindings.append(
                    Binding(
                        name=name,
                        owner=owner,
                        module=module,
                        style=model.WRAPPED,
                        flavour="def",
                        source=str(self.path),
                        roots=self.roots,
                        line=self._line_of(offset + match.start()),
                    )
                )
        # A name beside a member pointer is a table entry only where it
        # is not already a `.def` call: the two patterns see the same
        # text, and the call is the exact reading.
        defined = {match.start("name") for match in DEFINE.finditer(statement)}
        for match in PAIRED.finditer(statement):
            if match.start("name") in defined:
                continue
            target = qualify(match.group("target"), self.aliases, self.usings)
            scope, separator, _ = target.rpartition("::")
            if not separator:
                continue
            self.bindings.append(
                Binding(
                    name=match.group("name"),
                    owner=scope,
                    module=module,
                    style=model.WRAPPED,
                    target=target,
                    flavour="def",
                    source=str(self.path),
                    roots=self.roots,
                    line=self._line_of(offset + match.start()),
                )
            )
        for match in DEFINE.finditer(statement):
            tail = match.group("tail").lstrip()
            style = model.DIRECT if tail.startswith("&") else model.WRAPPED
            target = ""
            if style == model.DIRECT:
                named = re.match(r"&\s*([\w:]+)", tail)
                target = (
                    qualify(named.group(1), self.aliases, self.usings) if named else ""
                )
            self.bindings.append(
                Binding(
                    name=match.group("name"),
                    owner=owner,
                    module=module,
                    style=style,
                    target=target,
                    flavour=match.group("flavour") or "def",
                    source=str(self.path),
                    roots=self.roots,
                    line=self._line_of(offset + match.start()),
                )
            )
        if owner in self.enums:
            for match in VALUE.finditer(statement):
                spelled = match.group("target")
                self.bindings.append(
                    Binding(
                        name=match.group("name"),
                        owner=owner,
                        module=module,
                        style=model.DIRECT,
                        target=qualify(spelled, self.aliases, self.usings)
                        if spelled
                        else f"{owner}::{match.group('name')}",
                        flavour="value",
                        source=str(self.path),
                        roots=self.roots,
                        line=self._line_of(offset + match.start()),
                    )
                )
        return []

    def _owner(self, offset: int, statement: str, root: str) -> tuple:
        """What the chain in this statement is writing into."""
        assigned = ASSIGNED.search(statement)
        holder = HOLDER.search(statement)
        if holder is not None:
            held = qualify(
                first_argument(holder.group("arguments")), self.aliases, self.usings
            )
            variable = holder.group("variable") or (
                assigned.group("variable") if assigned else ""
            )
            if variable:
                self.holders[variable] = held
            if holder.group("holder") == "enum_":
                self.enums.add(held)
            self._registered(offset, statement, holder.end(), held, root)
            return held, ""

        attached = ATTACHED.search(statement)
        if attached is not None:
            parent = attached.group("parent")
            base = self.modules.get(parent, "" if parent in ROOTS else parent)
            path = ".".join(part for part in [base, attached.group("leaf")] if part)
            if assigned is not None:
                self.modules[assigned.group("variable")] = path
            return "", path

        submodule = SUBMODULE.search(statement)
        if submodule is not None:
            parent = submodule.group("parent") or ""
            leaves = SUBMODULE_LEAF.findall(submodule.group(0))
            base = self.modules.get(parent, "" if parent in ROOTS else parent)
            path = ".".join(part for part in [base, *leaves] if part)
            if assigned is not None:
                self.modules[assigned.group("variable")] = path
            return "", path

        head = re.match(r"\s*(\w+)\s*\.", statement)
        if head is not None:
            variable = head.group(1)
            if variable in self.holders:
                return self.holders[variable], ""
            if variable in self.modules:
                return "", self.modules[variable]
        return "", root

    def _registered(
        self, offset: int, statement: str, at: int, held: str, root: str
    ) -> None:
        """The name a type itself is given, which is not a `.def`."""
        named = HELD_NAME.match(statement, at)
        if named is None:
            return
        parent = named.group("parent")
        self.bindings.append(
            Binding(
                name=named.group("name"),
                module=self.modules.get(parent, "" if parent in ROOTS else root),
                style=model.DIRECT,
                target=held,
                flavour="class",
                source=str(self.path),
                roots=self.roots,
                line=self._line_of(offset + named.start()),
            )
        )


def read(sources: list) -> list:
    """Every binding under every source directory, however deep.

    A binding package keeps one directory per library, so the sources
    are walked, and a directory holding no binding adds nothing.
    """
    found = []
    for directory in sources:
        for path in sorted(Path(directory).rglob("*.cpp")):
            found.extend(Scan(path).bindings)
    return found
