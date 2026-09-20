"""The type graph: for every value in the tree, where it comes from and
where it goes.

This is the half of the site that no amount of prose produces. A
signature says a verb takes a `Fill`; nothing on that page says what
makes a `Fill`, that a string is one in Python, or that four other
verbs in two other libraries take the same value. Three questions, one
forward pass over every declaration the readers found:

  make one       — constructors, factories, conversions, role unions
  pass it to     — every parameter and every public field of that type
  also returned  — every function that hands one back

The pass is linear: each signature is walked once and appends to the
lists of the types it mentions. The nested loop over types against
signatures would be tens of millions of comparisons and answer the same
thing.
"""

from sigil.reference import catalogue as catalogue_module
from sigil.reference import model

# A C++ constructor taking exactly one argument it does not default,
# declared without `explicit`, is a conversion: the type is what the
# caller writes and the argument is what they actually pass.
CONVERSION = "implicit conversion"


def _wrapped(note: str) -> str:
    return f"through `{note}`" if note else ""


class Graph:
    """Where every value is made, taken and handed back."""

    def __init__(self, catalogue):
        self.catalogue = catalogue
        self.made = {}
        self.taken = {}
        self.given = {}
        self._walk_declarations()
        self._walk_constructors()
        self._walk_roles()

    # ---------------------------------------------------------------- add

    def _add(self, index: dict, qualified: str, site: model.Site) -> None:
        held = index.setdefault(qualified, [])
        if not any(
            one.label == site.label and one.kind == site.kind and one.note == site.note
            for one in held
        ):
            held.append(site)

    def _resolve(self, spelling: str, within: str, refs: tuple = ()) -> str:
        return self.catalogue.resolve(spelling, within=within, refs=refs)

    def _python_of(self, qualified: str) -> str:
        entity = self.catalogue.entity(qualified)
        return entity.python if entity is not None else ""

    # --------------------------------------------------------------- walk

    def _walk_declarations(self) -> None:
        for label, library, kind, declaration in self.catalogue.sites.values():
            within = declaration.qualified
            returns, return_refs = declaration.returns, declaration.return_refs
            accepts = [
                (parameter.type_text, parameter.type_refs)
                for parameter in declaration.parameters
            ]
            if kind in ("field", "constant"):
                # A public field is both ways through: setting it is how
                # a value goes in, reading it is how one comes out.
                accepts.append((returns, return_refs))
            for text, refs in accepts:
                self._sites(
                    self.taken, text, refs, within, label, library, kind, declaration
                )
            self._sites(
                self.given,
                returns,
                return_refs,
                within,
                label,
                library,
                kind,
                declaration,
            )

    def _sites(
        self, index, text, refs, within, label, library, kind, declaration
    ) -> None:
        for spelling, wrapper in catalogue_module.mentions(text):
            target = self._resolve(spelling, within, refs)
            if not target or target == declaration.owner:
                continue
            self._add(
                index,
                target,
                model.Site(
                    label=label,
                    kind=kind,
                    library=library,
                    note=_wrapped(wrapper),
                    target=declaration.qualified,
                ),
            )

    def _walk_constructors(self) -> None:
        wanted = set(self.catalogue.constructors)
        wanted.update(
            entity.qualified
            for entity in self.catalogue.entities
            if entity.kind in (model.TYPE, model.ENUM)
        )
        for qualified in sorted(wanted):
            declarations = self.catalogue.constructors.get(qualified, [])
            entity = self.catalogue.entity(qualified)
            library = entity.library if entity is not None else ""
            simple = qualified.rsplit("::", 1)[-1]
            for declaration in declarations:
                arguments = ", ".join(
                    parameter.type_text for parameter in declaration.parameters
                )
                self._add(
                    self.made,
                    qualified,
                    model.Site(
                        label=f"{simple}({arguments})",
                        kind="constructor" if not declaration.static else "factory",
                        library=library,
                        note=self._conversion(declaration),
                    ),
                )
                if self._conversion(declaration):
                    self._converts(qualified, simple, declaration, library)
            # A free function that hands the type back makes one too, and
            # it is the spelling most callers actually write.
            for site in self.given.get(qualified, []):
                if site.kind in (model.FUNCTION, model.ELEMENT, model.KIT):
                    self._add(self.made, qualified, site)

    def _conversion(self, declaration) -> str:
        """Whether this constructor is one, and from what.

        A constructor over a template parameter converts from whatever
        the caller has, which names nothing a reader can go and make,
        so it is not reported as a conversion at all.
        """
        required = [
            parameter for parameter in declaration.parameters if not parameter.default
        ]
        if declaration.explicit or declaration.static or len(required) != 1:
            return ""
        source = required[0].type_text
        named = [
            spelling
            for spelling, _ in catalogue_module.mentions(source)
            if self._resolve(spelling, declaration.qualified, required[0].type_refs)
        ]
        outside = source.replace("const ", "").strip(" &").split("<", 1)[0]
        if not named and not outside.startswith(("std::", "Sk", "sk_sp")):
            return ""
        return f"{CONVERSION} from `{source}`"

    def _converts(self, qualified: str, simple: str, declaration, library: str) -> None:
        """A value that converts is taken wherever the target is taken."""
        source = next(
            parameter for parameter in declaration.parameters if not parameter.default
        )
        for spelling, _ in catalogue_module.mentions(source.type_text):
            target = self._resolve(spelling, qualified, source.type_refs)
            if not target or target == qualified:
                continue
            self._add(
                self.taken,
                target,
                model.Site(
                    label=simple,
                    kind="type",
                    library=library,
                    note=f"{CONVERSION}, so anywhere a `{simple}` is taken",
                    target=qualified,
                ),
            )

    def _walk_roles(self) -> None:
        """What Python accepts where the C++ type is asked for.

        A role union is the Python answer to "what can I pass here",
        written for a type checker and invisible to a reader. Expanded
        onto the type's own page it is the shortest answer the site
        gives: a string is a colour, a three-tuple is a colour.
        """
        roles = self.catalogue.roles
        for alias in roles.members:
            for member in roles.members[alias]:
                path = roles.path_of(member)
                if not path:
                    continue
                qualified = self._from_python(path)
                if not qualified:
                    continue
                for other in roles.expand(alias):
                    if other in roles.members or roles.path_of(other) == path:
                        continue
                    self._add(
                        self.made,
                        qualified,
                        model.Site(
                            label=other,
                            kind="value",
                            library="",
                            language="python",
                            note=f"implicit, through `{alias}`",
                        ),
                    )

    def _from_python(self, spelling: str) -> str:
        """The C++ type one Python path names."""
        for entity in self.catalogue.entities:
            if entity.python == spelling:
                return entity.qualified
        return ""

    # -------------------------------------------------------------- reads

    def make_one(self, qualified: str) -> list:
        return sorted(self.made.get(qualified, []), key=model.Site.sort_key)

    def pass_it_to(self, qualified: str) -> list:
        return sorted(self.taken.get(qualified, []), key=model.Site.sort_key)

    def also_returned_by(self, qualified: str) -> list:
        made = {site.label for site in self.made.get(qualified, [])}
        return sorted(
            (site for site in self.given.get(qualified, []) if site.label not in made),
            key=model.Site.sort_key,
        )

    def counts(self, qualified: str) -> tuple:
        return (
            len(self.make_one(qualified)),
            len(self.pass_it_to(qualified)),
            len(self.also_returned_by(qualified)),
        )
