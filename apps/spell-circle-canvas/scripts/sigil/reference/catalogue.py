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

# What a dependency's own type names start with, for the one question
# the tuple above cannot answer: a binding hands Python `SkBlendMode`
# whole, and the leading segment is the type rather than a namespace.
DEPENDENCY_HEADS = ("Sk", "Gr", "sk_sp", "skgpu", "hb_", "choreograph", "Diligent")

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


def is_dunder(name: str) -> bool:
    """Whether a Python name is one of the language's own protocols."""
    return name.startswith("__") and name.endswith("__")


def is_dependency(spelling: str) -> bool:
    """Whether a C++ name a binding gives Python belongs to a dependency."""
    return spelling.split("::", 1)[0].startswith(DEPENDENCY_HEADS)


def mentioned(text: str) -> list:
    """The same names, each with the span it occupies in the text.

    A caller rewriting one name in place has to know where it stands: a
    type text mentioning both a `SurfaceFill` and a `Fill` holds one
    name inside the other, and searching for the shorter one again
    would land in the middle of the longer.
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
        found.append((match.start(), match.end(), token, stack[-1] if stack else ""))
    return found


def mentions(text: str) -> list:
    """The type names a signature spells, each with what wraps it.

    A parameter written `motion::Animatable< Fill >` mentions a `Fill`
    through an `Animatable`, and a type page that said only "accepted
    by fill" would lose the half of the answer that tells a reader what
    to build.
    """
    return [(token, wrapper) for _, _, token, wrapper in mentioned(text)]


class Catalogue:
    """Every entity in the tree, and how to look one up by name."""

    def __init__(self, inventories: dict, surface, roles, bound: list):
        self.inventories = inventories
        self.surface = surface
        self.roles = roles
        self.entities = []
        self.by_qualified = {}
        self.by_suffix = {}
        self.by_refid = {}
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
        self.nodes = {name: inventory.node for name, inventory in inventories.items()}
        self._collect()
        self._index()
        self._join(bound)
        self._disambiguate()

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
            if entity.kind == model.TYPE and entity.compound_refid:
                self.by_refid.setdefault(entity.compound_refid, entity.qualified)
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
                # An enumerator is written as a child of its enum rather
                # than as a declaration of its own, while a binding
                # names it under the enum. Without it here, every bound
                # enumerator would read as a name C++ never declared.
                for name, _ in declaration.enumerators:
                    spelling = f"{declaration.qualified}::{name}"
                    if spelling not in self.declared:
                        self.declared[spelling] = declaration
                        self._suffixes(self.declared_suffix, spelling)

    @staticmethod
    def _suffixes(index: dict, qualified: str) -> None:
        parts = qualified.split("::")
        for start in range(len(parts)):
            index.setdefault("::".join(parts[start:]), []).append(qualified)

    def resolve(
        self,
        spelling: str,
        within: str = "",
        roots: tuple = (),
        refs: tuple = (),
    ) -> str:
        """A name as a signature writes it, as the tree's own name.

        Doxygen links a type it knows, and that link is exact, so a
        refid the signature carries settles the name before anything is
        guessed. It carries one only inside the library that declares
        the type: the XML pass reads no tag files, so a type from
        another library is plain text.

        Then a signature inside a library writes a neighbour's type
        without its namespace and another library's with as much of one
        as it needs, so a name is matched on its tail. Where a tail fits
        more than one type the enclosing namespace decides — the scope's
        own member first, then anything under it — and where a binding
        is asking, the include roots its source opens with do: a file
        that includes only `sigilcompose/` means that Element. A tail
        that still fits several resolves to nothing rather than a guess.
        """
        tail = spelling.rsplit("::", 1)[-1]
        for refid in refs:
            qualified = self.by_refid.get(refid)
            if qualified and qualified.rsplit("::", 1)[-1] == tail:
                return qualified
        if spelling in self.by_qualified:
            return spelling
        candidates = self.by_suffix.get(spelling)
        if not candidates:
            return ""
        if len(candidates) == 1:
            return candidates[0]
        scope = within.rsplit("::", 1)[0] if within else ""
        while scope:
            if f"{scope}::{spelling}" in candidates:
                return f"{scope}::{spelling}"
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
            elif is_dependency(binding.target) or is_dependency(binding.owner):
                # A dependency's own type, handed to Python as it
                # stands. It has a reference of its own, and it is not
                # a name this tree declared and then lost.
                continue
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
                entity.python = self._spelling(held[0])
            declaration = (
                self.surface.declaration(self._native(entity.python))
                if entity.python
                else None
            )
            if declaration is not None:
                entity.python_signatures = declaration.signatures
        self._python_only()

    def _python_only(self) -> None:
        """A bound name no header declares, where the stubs declare one.

        A convenience written in the binding layer — a verb that sets
        two C++ properties in one call, a function that takes a Python
        object — is a name an author can type and a name the C++ side
        has nothing to match. It is an entity of the library whose
        vocabulary it extends, and it wears the badge that says so.

        What is left over is a binding naming something neither side
        declares, which is the contradiction the coverage report is
        for.
        """
        standing, self.unmatched = self.unmatched, []
        for binding in standing:
            entity = self._convenience(binding)
            if entity is None:
                self.unmatched.append(binding)
                continue
            # An overload is bound once per overload and is one name.
            if entity.qualified not in self.by_qualified:
                self.by_qualified[entity.qualified] = entity
                self.entities.append(entity)

    def _convenience(self, binding):
        """One unmatched binding as a Python-only entity, or nothing."""
        if is_dunder(binding.name) or binding.flavour not in ("def", "_static"):
            return None
        owner = ""
        if binding.owner:
            holder = self.by_qualified.get(
                self.resolve(binding.owner, roots=binding.roots)
            )
            # A member of a type that is not the node has no page of its
            # own on the C++ side either, so there is none to write.
            if holder is None or not holder.python:
                return None
            if holder.qualified != self.nodes.get(holder.library):
                return None
            spelling, library, owner = (
                f"{holder.python}.{binding.name}",
                holder.library,
                holder.qualified,
            )
        elif binding.module:
            spelling = self.surface.public_of(f"_sigil.{binding.module}.{binding.name}")
            library = self._library_of("sigil::" + binding.module.replace(".", "::"))
        else:
            return None
        if not spelling or not library:
            return None
        declaration = self.surface.declaration(self._native(spelling))
        if declaration is None:
            return None
        return model.Entity(
            library=library,
            kind=self._convenience_kind(library, owner, declaration),
            name=binding.name,
            qualified=spelling,
            owner=owner,
            python=spelling,
            python_signatures=declaration.signatures,
            binding_state=model.PYTHON_ONLY,
        )

    def _convenience_kind(self, library: str, owner: str, declaration) -> str:
        """The same shapes the C++ side is sorted by, read off the stub."""
        if owner:
            return model.VERB
        node = self.nodes.get(library, "")
        returns = declaration.signatures[0].returns if declaration.signatures else ""
        if node and returns.rsplit(".", 1)[-1] == node.rsplit("::", 1)[-1]:
            return model.ELEMENT
        return model.FUNCTION

    def _library_of(self, namespace: str) -> str:
        """The library whose own namespace holds this one.

        The longest one that fits: `sigil::geometry::mesh` is under
        SigilGeometry, and a library whose namespace nests inside
        another's would otherwise lose its own names to it.
        """
        found = ""
        longest = ""
        for library, prefix in self.namespaces.items():
            fits = prefix and (
                namespace == prefix or namespace.startswith(f"{prefix}::")
            )
            if fits and len(prefix) > len(longest):
                found, longest = library, prefix
        return found

    def _disambiguate(self) -> None:
        """A page name two entities share is qualified until it is one's.

        A value's page is keyed on its qualified name and cannot
        collide. Every other kind is filed under its simple name, and a
        library that declares `connector` in two namespaces would
        otherwise write one of the two pages over the other.
        """
        held = {}
        for entity in self.entities:
            if entity.kind in (model.TYPE, model.ENUM):
                continue
            key = (entity.library, entity.kind, entity.name)
            held.setdefault(key, []).append(entity)
        for group in held.values():
            if len(group) < 2:
                continue
            for entity in group:
                entity.page_name = self.display(entity.qualified, entity.library)

    def _native(self, public: str) -> str:
        for target, spelling in self.surface.spellings.items():
            if spelling == public:
                return target
        return public

    def _spelling(self, binding) -> str:
        """What an author types for the entity this binding names.

        The binding says which module or class the name lands on and
        the stubs say what that module is really called in the public
        package, which is where a rename shows up. A name the stubs do
        not carry is one the extension does not actually export, and
        that contradiction belongs in the report rather than on a page.
        """
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
        """Every public Python name that no entity answers for.

        What pybind11 writes onto every class it binds is not a gap in
        the catalogue: a protocol method belongs to Python itself, and
        the `name` and `value` properties come with every bound
        enumeration whether or not anybody asked. Listing them would
        bury the names a reader actually has no page for.
        """
        claimed = {entity.python for entity in self.entities if entity.python}
        found = []
        for spelling, declaration in self.surface.declared():
            if spelling in claimed or declaration.kind not in ("function", "method"):
                continue
            if is_dunder(declaration.name) or self._enumeration_property(declaration):
                continue
            found.append((spelling, declaration))
        return found

    def _enumeration_property(self, declaration) -> bool:
        if declaration.name not in ("name", "value") or not declaration.owner:
            return False
        holder = self.surface.native.get(declaration.module) or self.surface.public.get(
            declaration.module
        )
        # An enumeration pybind11 bound carries the members table it
        # writes, and nothing a person declares carries that name.
        return (
            holder is not None
            and f"{declaration.owner}.__members__" in holder.declarations
        )
