"""Reader one: the C++ surface, out of the Doxygen XML inventory.

The inventory is written by the docs verb's third pass with everything
extracted, so a member carrying no comment is still in it. That is the
whole reason this reads XML rather than the generated HTML: a catalogue
that lists only what somebody has already described is a catalogue of
the prose, not of the library.

Nothing here decides what a page says. It decides what exists, what
shape it has, and which other declarations each type mentions.
"""

import re
from pathlib import Path
from xml.etree import ElementTree

from sigil.reference import model

# A class becomes its library's node when this many of its own members
# hand it back for chaining. Below that a fluent pair of setters is just
# a pair of setters, and a library that has no element tree at all would
# otherwise get a Verbs catalogue holding both of them.
NODE_FLOOR = 10

COMPOUND_TYPES = ("class", "struct", "union", "interface")

# Doxygen writes a member's identifier as its compound's identifier, the
# separator, and the anchor the HTML page gives it.
ANCHOR_SEPARATOR = "_1"


def text_of(node) -> str:
    """A `<type>` or `<defval>` as a reader would type it."""
    if node is None:
        return ""
    return re.sub(r"\s+", " ", "".join(node.itertext())).strip()


def refs_of(node) -> tuple:
    """The compounds a type mentions, by Doxygen's own identifier.

    Only a compound reference counts. A member reference inside a type
    is an enumerator or a constant used as an array bound, which names
    no type and would put the enclosing class in the graph twice.
    """
    if node is None:
        return ()
    found = []
    for reference in node.iter("ref"):
        if reference.get("kindref") == "compound":
            identifier = reference.get("refid")
            if identifier and identifier not in found:
                found.append(identifier)
    return tuple(found)


def _inline(node) -> str:
    """One description element, flattened to Markdown."""
    pieces = []
    if node.text:
        pieces.append(node.text)
    for child in node:
        tag = child.tag
        if tag == "computeroutput":
            pieces.append(f"`{''.join(child.itertext()).strip()}`")
        elif tag in ("bold", "b"):
            pieces.append(f"**{''.join(child.itertext()).strip()}**")
        elif tag in ("emphasis", "i"):
            pieces.append(f"*{''.join(child.itertext()).strip()}*")
        elif tag == "linebreak":
            pieces.append("\n")
        elif tag in ("itemizedlist", "orderedlist"):
            marker = "-" if tag == "itemizedlist" else "1."
            pieces.append("\n\n")
            for item in child.findall("listitem"):
                body = " ".join(_inline(para) for para in item.findall("para"))
                pieces.append(f"{marker} {body.strip()}\n")
            pieces.append("\n")
        elif tag == "simplesect":
            title = child.get("kind", "").capitalize()
            body = " ".join(_inline(para) for para in child.findall("para"))
            pieces.append(f"\n\n**{title}.** {body.strip()}\n\n")
        elif tag == "parameterlist":
            continue
        elif tag == "programlisting":
            lines = []
            for line in child.findall("codeline"):
                lines.append("".join(line.itertext()))
            pieces.append("\n\n```cpp\n" + "\n".join(lines) + "\n```\n\n")
        else:
            pieces.append(_inline(child))
        if child.tail:
            pieces.append(child.tail)
    return "".join(pieces)


def describe(node) -> str:
    """A `<briefdescription>` or `<detaileddescription>` as Markdown."""
    if node is None:
        return ""
    paragraphs = [_inline(para).strip() for para in node.findall("para")]
    text = "\n\n".join(part for part in paragraphs if part)
    # A comment block wrapped in the source arrives with its wrapping,
    # and a paragraph reflowed by the renderer must not keep it.
    text = re.sub(r"[ \t]*\n[ \t]*(?![\n\-*`])", " ", text)
    return re.sub(r"\n{3,}", "\n\n", text).strip()


def parameters_of(node) -> list:
    found = []
    for param in node.findall("param"):
        found.append(
            model.Parameter(
                name=text_of(param.find("declname")),
                type_text=text_of(param.find("type")),
                type_refs=refs_of(param.find("type")),
                default=text_of(param.find("defval")),
            )
        )
    return found


class Declaration:
    """One `<memberdef>`, read but not yet sorted into a kind."""

    def __init__(self, element, compound_refid: str, compound_name: str):
        self.element = element
        self.kind = element.get("kind", "")
        self.refid = element.get("id", "")
        self.compound_refid = compound_refid
        self.owner = compound_name
        self.name = element.findtext("name", "").strip()
        self.qualified = element.findtext("qualifiedname", "").strip()
        if not self.qualified:
            self.qualified = (
                f"{compound_name}::{self.name}" if compound_name else self.name
            )
        self.returns = text_of(element.find("type"))
        self.return_refs = refs_of(element.find("type"))
        self.parameters = parameters_of(element)
        self.brief = describe(element.find("briefdescription"))
        self.detail = describe(element.find("detaileddescription"))
        self.explicit = element.get("explicit") == "yes"
        self.static = element.get("static") == "yes"
        self.constant = element.get("const") == "yes"
        self.strong = element.get("strong") == "yes"
        location = element.find("location")
        self.header = (
            location.get("declfile") or location.get("file")
            if location is not None
            else ""
        )
        self.line = (
            int(location.get("declline") or location.get("line") or 0)
            if location is not None
            else 0
        )
        self.enumerators = [
            (
                value.findtext("name", "").strip(),
                describe(value.find("briefdescription")),
            )
            for value in element.findall("enumvalue")
        ]

    def anchor(self) -> str:
        head = self.compound_refid + ANCHOR_SEPARATOR
        return self.refid[len(head) :] if self.refid.startswith(head) else ""

    def qualifiers(self) -> str:
        return "const" if self.constant else ""

    def signature(self) -> model.Signature:
        return model.Signature(
            returns=self.returns,
            return_refs=self.return_refs,
            parameters=self.parameters,
            qualifiers=self.qualifiers(),
        )


class Compound:
    """One class, struct, union or namespace, with what it declares."""

    def __init__(self, path: Path):
        root = ElementTree.parse(path).getroot()
        self.element = root.find("compounddef")
        self.kind = self.element.get("kind", "")
        self.refid = self.element.get("id", "")
        self.name = self.element.findtext("compoundname", "").strip()
        self.brief = describe(self.element.find("briefdescription"))
        self.detail = describe(self.element.find("detaileddescription"))
        location = self.element.find("location")
        self.header = location.get("file") if location is not None else ""
        self.line = int(location.get("line") or 0) if location is not None else 0
        self.bases = [
            base.text.strip()
            for base in self.element.findall("basecompoundref")
            if base.text
        ]
        self.declarations = [
            Declaration(member, self.refid, self.name)
            for section in self.element.findall("sectiondef")
            for member in section.findall("memberdef")
            if member.get("prot") == "public"
        ]

    def page_file(self) -> str:
        return f"{self.refid}.html"

    def simple_name(self) -> str:
        """The name without the namespaces, keeping an enclosing class."""
        parts = self.name.split("::")
        head = [part for part in parts if part and part[:1].isupper()]
        return "::".join(head) if head else parts[-1]


class Inventory:
    """Everything one library's XML declares."""

    def __init__(self, library: str, directory: Path):
        self.library = library
        self.directory = directory
        self.compounds = {}
        self.declarations = {}
        self.node = ""
        self.node_refid = ""
        self.namespace = ""
        self._read()
        self._find_node()
        self._find_namespace()

    def _read(self) -> None:
        for path in sorted(self.directory.glob("*.xml")):
            stem = path.stem
            if stem in ("index", "Doxyfile") or stem.startswith(("dir_", "page_")):
                continue
            if stem.endswith(("_8h", "_8hpp", "_8md")):
                continue
            try:
                compound = Compound(path)
            except ElementTree.ParseError:
                continue
            if compound.element is None or not compound.name:
                continue
            if "::detail" in compound.name or compound.name.startswith("detail"):
                continue
            self.compounds[compound.refid] = compound
            for declaration in compound.declarations:
                # A member is written into both its own compound and the
                # file that declares it; the first spelling is the one
                # with the enclosing scope on it.
                self.declarations.setdefault(declaration.refid, declaration)

    def _find_node(self) -> None:
        """The library's node type: what its verbs hand back.

        A verb is a member that returns its own class for chaining, so
        the class most of them belong to is the node. Deciding it from
        the shape rather than from a list means a library that grows one
        gets its verbs catalogued without anything being registered.
        """
        best = 0
        for refid, compound in self.compounds.items():
            if compound.kind not in COMPOUND_TYPES:
                continue
            simple = compound.name.rsplit("::", 1)[-1]
            count = sum(
                1
                for declaration in compound.declarations
                if declaration.kind == "function"
                and declaration.returns.endswith("&")
                and declaration.returns.replace(" ", "").rstrip("&")
                in (simple, compound.name)
            )
            if count > best:
                best, self.node, self.node_refid = count, compound.name, refid
        if best < NODE_FLOOR:
            self.node, self.node_refid = "", ""

    def _find_namespace(self) -> None:
        """The namespace this library's names are written inside.

        A row in a catalogue reads as `Element::fill`, not as
        `sigil::compose::Element::fill`: the library is its own column.
        Taking the namespace off needs to know what it is, and the
        library's own declarations are the only place that is written.
        """
        counted = {}
        for compound in self.compounds.values():
            if compound.kind == "namespace" and compound.declarations:
                counted[compound.name] = counted.get(compound.name, 0) + len(
                    compound.declarations
                )
            elif compound.kind in COMPOUND_TYPES:
                scope = compound.name.rpartition("::")[0]
                if scope:
                    counted[scope] = counted.get(scope, 0) + 1
        if not counted:
            return
        leading = max(counted.items(), key=lambda pair: pair[1])[0]
        # The shortest scope above the busiest one that still holds most
        # of the library. Shortest, because a library whose names sit in
        # several sub-namespaces shares the one above them; and never
        # the first segment alone, which is the whole tree's root and
        # belongs to no library.
        parts = leading.split("::")
        total = sum(counted.values())
        for depth in range(min(2, len(parts)), len(parts) + 1):
            prefix = "::".join(parts[:depth])
            held = sum(
                weight
                for scope, weight in counted.items()
                if scope == prefix or scope.startswith(f"{prefix}::")
            )
            if held * 2 >= total:
                self.namespace = prefix
                return
        self.namespace = leading


def read(work: Path, libraries: list) -> dict:
    """One inventory per library whose XML the docs build has written."""
    found = {}
    for name in libraries:
        directory = work / name / "xml"
        if (directory / "index.xml").exists():
            found[name] = Inventory(name, directory)
    return found
