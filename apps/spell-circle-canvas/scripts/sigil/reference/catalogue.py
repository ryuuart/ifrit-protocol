"""What exists: the three readers joined into one list of entities.

The readers each see one language. This is where a C++ member, the
Python name it is bound under and the shape of that binding become one
record, and where every declaration is sorted into the kind whose page
it gets.

Sorting is by shape, not by a list. A free function that hands back the
library's node starts a tree, so it is an element; the same function
declared under a `kit/` header is a component, because a kit is
composed out of what is already there and the header is the only place
that says so.
"""

import re
from pathlib import Path

from sigil.reference import model

# Names that stand in a signature but are nobody's page: the standard
# library has its own reference, and Skia's C types are a dependency's.
FOREIGN = ("std", "Sk", "sk_sp", "gr", "skgpu", "hb", "icu", "glm", "Diligent")

KEYWORDS = {
    "const",
    "volatile",
    "unsigned",
    "signed",
    "static",
    "inline",
    "constexpr",
    "void",
    "bool",
    "char",
    "short",
    "int",
    "long",
    "float",
    "double",
    "auto",
    "size_t",
    "ptrdiff_t",
    "nullptr_t",
    "typename",
    "class",
    "struct",
    "enum",
    "true",
    "false",
    "noexcept",
    "mutable",
    "operator",
    "template",
}

# A header under a directory of this name declares composed components
# rather than primitives.
KIT_DIRECTORY = "kit"


def mentions(text: str) -> list:
    """The type names a signature spells, each with what wraps it.

    A parameter written `motion::Animatable< Fill >` mentions a `Fill`
    through an `Animatable`, and a type page that said only "accepted
    by fill" would lose the half of the answer that tells a reader what
    to build.
    """
    found = []
    stack = []
    last = ""
    for match in re.finditer(r"(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*|[<>]", text):
        token = match.group(0)
        if token == "<":
            stack.append(last)
            continue
        if token == ">":
            if stack:
                stack.pop()
            continue
        last = token
        simple = token.rsplit("::", 1)[-1]
        if simple in KEYWORDS or token.split("::", 1)[0] in FOREIGN:
            continue
        found.append((token, stack[-1] if stack else ""))
    return found


class Catalogue:
    """Every entity in the tree, and how to look one up by name."""

    def __init__(self, inventories: dict, surface, roles, bound: list):
        self.inventories = inventories
        self.surface = surface
        self.roles = roles
        self.entities = []
        self.by_qualified = {}
        self.by_suffix = {}
        self.declared = {}
        self.declared_suffix = {}
        self.unmatched = []
        self.constructors = {}
        # Every declaration a value can travel through, by its own
        # identifier: what it is called, whose it is, and what shape it
        # has. The type graph is one walk over this.
        self.sites = {}
        self.bindings = {}
        self.namespaces = {
            name: inventory.namespace for name, inventory in inventories.items()
        }
        self._collect()
        self._index()
        self._join(bound)

    def display(self, qualified: str, library: str) -> str:
        """A name as a row in that library's own table spells it.

        The library is a column of its own, so its namespace is not
        repeated in every row: `Element::fill`, `kit::dot`, `hsv`.
        """
        prefix = self.namespaces.get(library, "")
        if prefix and qualified.startswith(f"{prefix}::"):
            return qualified[len(prefix) + 2 :]
        return qualified

    # ---------------------------------------------------------------- read

    def _collect(self) -> None:
        for library, inventory in sorted(self.inventories.items()):
            for compound in inventory.compounds.values():
                if compound.kind in ("class", "struct", "union", "interface"):
                    self._type(library, inventory, compound)
                elif compound.kind == "namespace":
                    self._namespace(library, inventory, compound)

    def _type(self, library: str, inventory, compound) -> None:
        entity = model.Entity(
            library=library,
            kind=model.TYPE,
            name=compound.simple_name(),
            qualified=compound.name,
            header=compound.header,
            line=compound.line,
            refid=compound.refid,
            compound_refid=compound.refid,
            brief=compound.brief,
            detail=compound.detail,
        )
        entity.doxygen = f"{library}/html/{compound.page_file()}"
        self.entities.append(entity)
        simple = compound.name.rsplit("::", 1)[-1]
        node = compound.name == inventory.node
        for declaration in compound.declarations:
            if declaration.kind == "enum":
                self._enum(library, declaration)
            elif declaration.kind == "variable":
                self._site(
                    declaration, f"{simple}::{declaration.name}", library, "field"
                )
            elif declaration.kind == "function":
                self._member(library, compound, declaration, simple, node)

    def _member(self, library, compound, declaration, simple, node) -> None:
        if declaration.name.startswith("operator") or declaration.name.startswith("~"):
            return
        if declaration.name == simple:
            self.constructors.setdefault(compound.name, []).append(declaration)
            return
        returns_self = declaration.returns.replace(" ", "").rstrip("&") in (
            simple,
            compound.name,
        )
        if node and returns_self and declaration.returns.endswith("&"):
            entity = self._entity(library, model.VERB, declaration)
            entity.owner = compound.name
            self._attach(entity, declaration)
            return
        if declaration.static and returns_self:
            self.constructors.setdefault(compound.name, []).append(declaration)
            return
        # Every other member is a place a value is taken or handed back,
        # and it is documented on the page of the type that declares it.
        self._site(declaration, f"{simple}::{declaration.name}", library, "member")

    def _namespace(self, library: str, inventory, compound) -> None:
        for declaration in compound.declarations:
            if declaration.kind == "enum":
                self._enum(library, declaration)
            elif declaration.kind == "function":
                self._free(library, inventory, declaration)
            elif declaration.kind == "variable":
                self._site(declaration, declaration.qualified, library, "constant")

    def _free(self, library: str, inventory, declaration) -> None:
        if declaration.name.startswith("operator"):
            self._site(declaration, declaration.qualified, library, "operator")
            return
        kind = model.FUNCTION
        node = inventory.node
        if node:
            simple = node.rsplit("::", 1)[-1]
            returned = declaration.returns.replace(" ", "").rstrip("&")
            if returned in (simple, node):
                kind = model.KIT if self._is_kit(declaration.header) else model.ELEMENT
        entity = self._entity(library, kind, declaration)
        self._attach(entity, declaration)

    @staticmethod
    def _is_kit(header: str) -> bool:
        return KIT_DIRECTORY in Path(header).parts[:-1]

    def _enum(self, library: str, declaration) -> None:
        entity = self._entity(library, model.ENUM, declaration)
        entity.members = declaration.enumerators
        entity.signatures = []
        self.entities.append(entity)

    def _entity(self, library: str, kind: str, declaration) -> model.Entity:
        name = declaration.qualified.rsplit("::", 1)[-1]
        owner = declaration.owner.rsplit("::", 1)[-1] if declaration.owner else ""
        if owner[:1].isupper() and kind in (model.ENUM,):
            name = f"{owner}::{name}"
        entity = model.Entity(
            library=library,
            kind=kind,
            name=name,
            qualified=declaration.qualified,
            owner=declaration.owner,
            header=declaration.header,
            line=declaration.line,
            refid=declaration.refid,
            compound_refid=declaration.compound_refid,
            brief=declaration.brief,
            detail=declaration.detail,
        )
        anchor = declaration.anchor()
        entity.doxygen = (
            f"{library}/html/{declaration.compound_refid}.html#{anchor}"
            if anchor
            else f"{library}/html/{declaration.compound_refid}.html"
        )
        return entity

    def _attach(self, entity: model.Entity, declaration) -> None:
        """Adds one overload, folding it into a page that already exists."""
        held = self.by_qualified.get(entity.qualified)
        if held is None or held.kind != entity.kind:
            entity.signatures.append(declaration.signature())
            entity.explicit_only = declaration.explicit
            self.entities.append(entity)
            self.by_qualified[entity.qualified] = entity
        else:
            held.signatures.append(declaration.signature())
            if not held.brief:
                held.brief, held.detail = declaration.brief, declaration.detail
        self._site(declaration, entity.qualified, entity.library, entity.kind)

    def _site(self, declaration, label: str, library: str, kind: str) -> None:
        prefix = self.namespaces.get(library, "")
        if prefix and label.startswith(f"{prefix}::"):
            label = label[len(prefix) + 2 :]
        self.sites.setdefault(declaration.refid, (label, library, kind, declaration))

    # --------------------------------------------------------------- index

    def _index(self) -> None:
        self.by_qualified = {}
        for entity in self.entities:
            self.by_qualified.setdefault(entity.qualified, entity)
        for qualified in self.by_qualified:
            self._suffixes(self.by_suffix, qualified)
        # Everything C++ declares, page or not. A field and a member of
        # a type that is not the node carry no page of their own, and a
        # binding that names one is still a binding that was found.
        for inventory in self.inventories.values():
            for declaration in inventory.declarations.values():
                if declaration.qualified not in self.declared:
                    self.declared[declaration.qualified] = declaration
                    self._suffixes(self.declared_suffix, declaration.qualified)

    @staticmethod
    def _suffixes(index: dict, qualified: str) -> None:
        parts = qualified.split("::")
        for start in range(len(parts)):
            index.setdefault("::".join(parts[start:]), []).append(qualified)

    def resolve(self, spelling: str, within: str = "", roots: tuple = ()) -> str:
        """A name as a signature writes it, as the tree's own name.

        A signature inside a library writes a neighbour's type without
        its namespace and another library's with as much of one as it
        needs, so a name is matched on its tail. Where a tail fits more
        than one type the enclosing namespace decides, and where a
        binding is asking, the include roots its source opens with do —
        a file that includes only `sigilcompose/` means that Element.
        A tail that still fits several resolves to nothing rather than
        to a guess.
        """
        if spelling in self.by_qualified:
            return spelling
        candidates = self.by_suffix.get(spelling)
        if not candidates:
            return ""
        if len(candidates) == 1:
            return candidates[0]
        scope = within.rsplit("::", 1)[0] if within else ""
        while scope:
            for candidate in candidates:
                if candidate.startswith(f"{scope}::"):
                    return candidate
            scope = scope.rpartition("::")[0]
        for root in roots:
            for candidate in candidates:
                entity = self.by_qualified.get(candidate)
                if entity is not None and entity.header.startswith(f"{root}/"):
                    return candidate
        return ""

    def resolve_declared(self, spelling: str, roots: tuple = ()) -> str:
        """The same lookup over everything C++ declares, page or not."""
        found = self.resolve(spelling, roots=roots)
        if found:
            return found
        if spelling in self.declared:
            return spelling
        candidates = self.declared_suffix.get(spelling) or []
        if len(candidates) == 1:
            return candidates[0]
        for root in roots:
            matched = [
                candidate
                for candidate in candidates
                if self.declared[candidate].header.startswith(f"{root}/")
            ]
            if len(matched) == 1:
                return matched[0]
        return ""

    def entity(self, qualified: str):
        return self.by_qualified.get(qualified)

    # ---------------------------------------------------------------- join

    def _join(self, bound: list) -> None:
        """Every binding against the C++ name and the Python spelling."""
        for binding in bound:
            target = ""
            if binding.target:
                target = self.resolve_declared(binding.target, roots=binding.roots)
            if not target and binding.owner:
                owner = self.resolve_declared(binding.owner, roots=binding.roots)
                if owner:
                    target = self.resolve_declared(f"{owner}::{binding.name}")
            if not target and binding.module:
                namespace = "sigil::" + binding.module.replace(".", "::")
                target = self.resolve_declared(f"{namespace}::{binding.name}")
            if target:
                self.bindings.setdefault(target, []).append(binding)
            else:
                self.unmatched.append(binding)

        # A type's own spelling has to be settled before a member of it
        # can be spelled, since a member is written under its class.
        for entity in sorted(self.entities, key=lambda one: one.kind != model.TYPE):
            held = self.bindings.get(entity.qualified)
            if held:
                entity.binding_state = (
                    model.DIRECT
                    if any(one.style == model.DIRECT for one in held)
                    else model.WRAPPED
                )
                entity.python = self._spelling(entity, held)
            declaration = (
                self.surface.declaration(self._native(entity.python))
                if entity.python
                else None
            )
            if declaration is not None:
                entity.python_signatures = declaration.signatures

    def _native(self, public: str) -> str:
        for target, spelling in self.surface.spellings.items():
            if spelling == public:
                return target
        return public

    def _spelling(self, entity: model.Entity, held: list) -> str:
        """What an author types for this entity.

        The binding says which module or class the name lands on and
        the stubs say what that module is really called in the public
        package, which is where a rename shows up. A name the stubs do
        not carry is one the extension does not actually export, and
        that contradiction belongs in the report rather than on a page.
        """
        binding = held[0]
        if binding.owner:
            owner = self.resolve(binding.owner, roots=binding.roots)
            holder = self.by_qualified.get(owner)
            if holder is not None and holder.python:
                return f"{holder.python}.{binding.name}"
            return ""
        if binding.module:
            native = f"_sigil.{binding.module}.{binding.name}"
            spelled = self.surface.public_of(native)
            if spelled:
                return spelled
        return self._only_spelling(binding.name)

    def _only_spelling(self, name: str) -> str:
        """The public path of a name, when exactly one module offers it."""
        found = [
            spelling
            for target, spelling in self.surface.spellings.items()
            if target.rsplit(".", 1)[-1] == name and spelling.count(".") <= 2
        ]
        return found[0] if len(found) == 1 else ""

    def unbound_python(self) -> list:
        """Every public Python name that no C++ entity answers for."""
        claimed = {entity.python for entity in self.entities if entity.python}
        found = []
        for spelling, declaration in self.surface.declared():
            if spelling not in claimed and declaration.kind in ("function", "method"):
                found.append((spelling, declaration))
        return found
