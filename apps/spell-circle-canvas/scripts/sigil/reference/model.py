"""The records the readers produce and every later stage reads.

One `Entity` per thing a reader can reach for, whichever language
declares it, plus the smaller records an entity is made of. Nothing here
knows where the records came from or what is done with them.
"""

import dataclasses

# The six kinds a reference page comes in, and the directory each is
# written under. An entity's kind is decided by its shape, never by
# where its header sits — except for kit, which IS a header question:
# a component is an element factory a caller could have written.
ELEMENT = "element"
VERB = "verb"
TYPE = "type"
KIT = "kit"
ENUM = "enum"
FUNCTION = "function"

KINDS = (ELEMENT, VERB, TYPE, KIT, ENUM, FUNCTION)

DIRECTORIES = {
    ELEMENT: "elements",
    VERB: "verbs",
    TYPE: "types",
    KIT: "kit",
    ENUM: "enums",
    FUNCTION: "functions",
}

TITLES = {
    ELEMENT: "Elements",
    VERB: "Verbs",
    TYPE: "Types",
    KIT: "Kit components",
    ENUM: "Enumerations",
    FUNCTION: "Functions",
}

# What each kind is, in one sentence, for the index page that lists them.
DESCRIPTIONS = {
    ELEMENT: "Things you create. A factory that returns a node and starts a tree.",
    VERB: "Things you say to one. A member that returns its node for chaining.",
    TYPE: "Values you pass. What a verb or a factory accepts or hands back.",
    KIT: "Composed components, built out of elements and verbs alone.",
    ENUM: "Enumerations, with one row per member.",
    FUNCTION: "Free functions that return something other than a node.",
}

# How a binding's C++ and Python spellings relate, and the badge the page
# wears for it. `direct` wears none: the two spellings are the same call.
DIRECT = "direct"
WRAPPED = "wrapped"
CPP_ONLY = "cpp-only"
PYTHON_ONLY = "python-only"

BADGES = {
    WRAPPED: "differs",
    CPP_ONLY: "C++ only",
    PYTHON_ONLY: "Python only",
}


# The order the rows of a type page read in: a factory or a verb is
# something a caller writes, a field is something they fill in, and an
# operator is something they never spell at all.
SITE_ORDER = (
    "constructor",
    "factory",
    "value",
    ELEMENT,
    KIT,
    VERB,
    FUNCTION,
    "member",
    TYPE,
    "field",
    "constant",
    "operator",
)


@dataclasses.dataclass(frozen=True)
class Parameter:
    """One parameter of one signature, in either language."""

    name: str
    type_text: str
    # Doxygen's own identifiers for the compounds this type mentions.
    # A refid settles a name no tail can: `Reading` inside a namespace
    # that also holds a `Channel::Reading` is one of the two, and the
    # link Doxygen wrote says which. It is written only for a type the
    # same library declares, so the tail answers the rest.
    type_refs: tuple = ()
    default: str = ""

    def spelling(self, language: str = "cpp") -> str:
        if language == "python":
            text = f"{self.name}: {self.type_text}" if self.type_text else self.name
            return f"{text}={self.default}" if self.default else text
        text = f"{self.type_text} {self.name}".strip()
        return f"{text} = {self.default}" if self.default else text


@dataclasses.dataclass
class Signature:
    """One overload, as a reader would type it."""

    returns: str = ""
    return_refs: tuple = ()
    parameters: list = dataclasses.field(default_factory=list)
    qualifiers: str = ""
    language: str = "cpp"

    def spelling(self, name: str) -> str:
        arguments = ", ".join(
            parameter.spelling(self.language) for parameter in self.parameters
        )
        if self.language == "python":
            returns = f" -> {self.returns}" if self.returns else ""
            return f"def {name}({arguments}){returns}: ..."
        lead = f"{self.returns} " if self.returns else ""
        tail = f" {self.qualifiers}" if self.qualifiers else ""
        return f"{lead}{name}({arguments}){tail};"


@dataclasses.dataclass
class Entity:
    """One thing a page can be written about.

    `qualified` is the key everything else joins on, and it is the C++
    spelling wherever C++ declares the thing. A Python-only convenience
    has no C++ name, so its qualified name is its Python one — which is
    unambiguous, because the two namings never collide.
    """

    library: str
    kind: str
    name: str
    qualified: str
    owner: str = ""
    header: str = ""
    line: int = 0
    refid: str = ""
    compound_refid: str = ""
    brief: str = ""
    detail: str = ""
    group: str = ""
    signatures: list = dataclasses.field(default_factory=list)
    # Enumerators, as (name, brief) in declaration order.
    members: list = dataclasses.field(default_factory=list)
    python: str = ""
    python_signatures: list = dataclasses.field(default_factory=list)
    binding_state: str = CPP_ONLY
    doxygen: str = ""
    explicit_only: bool = False
    page: object = None
    # What the page is filed under when the simple name is not this
    # entity's alone. A library that declares `connector` in two
    # namespaces has two pages to write and one name to write them
    # under, so the one that shares it is qualified until it is unique.
    page_name: str = ""

    def slug(self) -> str:
        """The file name the page is written under."""
        return (self.page_name or self.name).replace("::", ".")

    def path(self) -> str:
        """Where the page stands in the site, without an extension.

        A value has ONE page, and it is tree-wide. A `Fill` is made in
        SigilCompose, widened by SigilMaterial and taken by SigilDraw
        and SigilSketch; filing it under the library that declares it
        would hide the half of the answer that crosses libraries. The
        declaring library's own index still lists it and links here.
        """
        if self.kind in (TYPE, ENUM):
            return f"values/{self.qualified.replace('::', '.')}"
        return f"reference/{self.library}/{DIRECTORIES[self.kind]}/{self.slug()}"

    def summary(self) -> str:
        if self.page is not None and self.page.summary:
            return self.page.summary
        return self.brief


@dataclasses.dataclass
class Site:
    """One place a value is made, taken or handed back.

    The three sections of a type page are lists of these, and a site is
    written the same way wherever it appears: what it is called, what
    shape it has, and which library it belongs to.
    """

    label: str
    kind: str
    library: str
    language: str = "cpp"
    note: str = ""
    target: str = ""

    def sort_key(self) -> tuple:
        # C++ first, then the Python-only spellings; inside each, the
        # kinds a reader reaches for before the ones they read off a
        # struct, then the library and the name.
        return (
            self.language != "cpp",
            SITE_ORDER.index(self.kind) if self.kind in SITE_ORDER else len(SITE_ORDER),
            self.library,
            self.label,
        )


@dataclasses.dataclass
class Page:
    """A hand-written Markdown file, parsed but not yet rendered."""

    path: object
    front: dict = dataclasses.field(default_factory=dict)
    body: str = ""
    sections: dict = dataclasses.field(default_factory=dict)
    lede: str = ""
    summary: str = ""
