"""One entity plus whatever prose was written for it, as one page.

The generated sections and the hand-written ones meet here. A writer
supplies the lede, the description, the examples and the see-also; the
signature, the table of what a verb accepts, and the three tables that
answer what a value is made of come from the readers. A writer who
writes one of the generated sections anyway keeps it: the machine is
right about the shapes and can still be wrong about a page.

`docs/REFERENCE.md` is the canon for what a writer may spell.
"""

import re

from sigil.reference import model

# The sections of a page, in the order they read, and which of them the
# generator supplies for which kind.
ORDER = (
    "Syntax",
    "Parameters",
    "Values",
    "Members",
    "Make one",
    "Pass it to",
    "Also returned by",
    "Description",
    "Examples",
    "See also",
)

GENERATED = {
    model.VERB: ("Syntax", "Values"),
    model.ELEMENT: ("Syntax", "Parameters", "Values"),
    model.KIT: ("Syntax", "Parameters", "Values"),
    model.FUNCTION: ("Syntax", "Parameters", "Values"),
    model.TYPE: ("Make one", "Pass it to", "Also returned by"),
    model.ENUM: ("Members", "Make one", "Pass it to", "Also returned by"),
}

HEADING = re.compile(r"^##\s+(.+?)\s*$", re.MULTILINE)

# Sections that answer the same question. A verb's parameters ARE the
# values it takes, so a writer who tabulates them under either heading
# has taken that slot and the generator adds neither.
SLOTS = (("Parameters", "Values"),)


def sections_of(body: str) -> tuple:
    """A hand-written body split into its lede and its `##` sections."""
    matches = list(HEADING.finditer(body))
    lede = body[: matches[0].start()] if matches else body
    found = {}
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(body)
        found[match.group(1)] = body[match.end() : end].strip()
    return lede.strip(), found


def first_sentence(text: str) -> str:
    stripped = re.sub(r"\s+", " ", text).strip()
    cut = re.search(r"(?<=[.!?])\s", stripped)
    return stripped[: cut.start()] if cut else stripped


def escape_cell(text: str) -> str:
    return text.replace("|", "\\|")


def _is_template_parameter(text: str) -> bool:
    """Whether a parameter's type is the caller's, not a type of ours.

    A forwarding reference over a template parameter accepts whatever
    the overload set resolves; naming `P &&` as a value a reader could
    go and make says nothing.
    """
    return re.fullmatch(r"(const\s+)?[A-Z]\w{0,2}\s*&{0,2}", text.strip()) is not None


def table(header: list, rows: list) -> str:
    if not rows:
        return ""
    lines = ["| " + " | ".join(header) + " |"]
    lines.append("|" + "|".join(" --- " for _ in header) + "|")
    for row in rows:
        lines.append("| " + " | ".join(escape_cell(cell) for cell in row) + " |")
    return "\n".join(lines)


class Composer:
    """Builds one page's Markdown out of the entity and the prose."""

    def __init__(self, catalogue, graph, values: dict):
        self.catalogue = catalogue
        self.graph = graph
        self.values = values

    # ----------------------------------------------------------- helpers

    def value_link(self, qualified: str, label: str = "") -> str:
        shown = label or self.catalogue.display(qualified, "")
        if qualified in self.values:
            return f"[`{shown}`](value:{qualified})"
        return f"`{shown}`"

    def type_cell(self, text: str, within: str) -> str:
        """A type as a signature writes it, linked where we have a page."""
        from sigil.reference.catalogue import mentions

        out = text
        for spelling, _ in mentions(text):
            target = self.catalogue.resolve(spelling, within=within)
            if target in self.values:
                out = out.replace(spelling, f"@@{target}@@", 1)
        for target in set(re.findall(r"@@([\w:]+)@@", out)):
            shown = target.rsplit("::", 1)[-1]
            out = out.replace(f"@@{target}@@", f"[{shown}](value:{target})")
        return f"`{out}`" if "@@" not in out and "](" not in out else out

    def summary_of(self, qualified: str) -> str:
        entity = self.catalogue.entity(qualified)
        return first_sentence(entity.brief) if entity is not None else ""

    # --------------------------------------------------------- the pieces

    def syntax(self, entity: model.Entity) -> str:
        parts = []
        if entity.signatures:
            body = "\n".join(
                signature.spelling(entity.name) for signature in entity.signatures
            )
            parts.append(f"```cpp\n{body}\n```")
        if entity.python_signatures:
            name = entity.python.rsplit(".", 1)[-1]
            body = "\n".join(
                signature.spelling(name) for signature in entity.python_signatures
            )
            parts.append(f"```python\n{body}\n```")
        elif entity.python:
            parts.append(f"```python\n{entity.python}\n```")
        return "\n\n".join(parts)

    def parameters(self, entity: model.Entity) -> str:
        rows = []
        seen = set()
        for signature in entity.signatures:
            for parameter in signature.parameters:
                key = (parameter.name, parameter.type_text)
                if not parameter.name or key in seen:
                    continue
                seen.add(key)
                rows.append(
                    [
                        f"`{parameter.name}`",
                        self.type_cell(parameter.type_text, entity.qualified),
                        f"`{parameter.default}`" if parameter.default else "—",
                    ]
                )
        return table(["Name", "Type", "Default"], rows)

    def values_table(self, entity: model.Entity) -> str:
        """What this takes, and where one of each comes from."""
        rows = []
        seen = set()
        for signature in entity.signatures:
            for parameter in signature.parameters:
                text = parameter.type_text
                if text in seen or _is_template_parameter(text):
                    continue
                seen.add(text)
                target = self._principal(text, entity.qualified)
                rows.append(
                    [
                        self.type_cell(text, entity.qualified),
                        self.summary_of(target) or "—",
                        self.value_link(target) if target else "—",
                    ]
                )
        body = table(["Value", "What it is", "Where one comes from"], rows)
        note = self.python_note(entity)
        return "\n\n".join(part for part in (body, note) if part)

    def _principal(self, text: str, within: str) -> str:
        from sigil.reference.catalogue import mentions

        innermost = ""
        for spelling, wrapper in mentions(text):
            target = self.catalogue.resolve(spelling, within=within)
            if target and (wrapper or not innermost) or target and not innermost:
                innermost = target
        return innermost

    def python_note(self, entity: model.Entity) -> str:
        """The role union a Python parameter names, spelled out."""
        roles = self.catalogue.roles
        lines = []
        for signature in entity.python_signatures:
            for parameter in signature.parameters:
                alias = parameter.type_text.rsplit(".", 1)[-1]
                if alias not in roles.members:
                    continue
                members = [
                    member
                    for member in roles.expand(alias)
                    if member not in roles.members
                ]
                spelled = ", ".join(
                    f"`{self._python_member(member)}`" for member in members
                )
                line = (
                    f"In Python the `{parameter.name}` parameter is `{alias}`, "
                    f"which accepts {spelled}."
                )
                if line not in lines:
                    lines.append(line)
        return "\n\n".join(lines)

    def _python_member(self, member: str) -> str:
        roles = self.catalogue.roles
        native = roles.native_path(member)
        if not native:
            return member
        return self.catalogue.surface.public_of(native) or member

    def members(self, entity: model.Entity) -> str:
        rows = [
            [f"`{name}`", first_sentence(brief) or "—"]
            for name, brief in entity.members
        ]
        return table(["Member", "What it means"], rows)

    def sites(self, title: str, entity: model.Entity) -> str:
        reader = {
            "Make one": self.graph.make_one,
            "Pass it to": self.graph.pass_it_to,
            "Also returned by": self.graph.also_returned_by,
        }[title]
        found = reader(entity.qualified)
        if not found:
            return ""
        rows = [
            [
                f"`{site.label}`",
                "Python" if site.language == "python" else site.kind,
                site.library or "—",
                site.note or "",
            ]
            for site in found
        ]
        header = ["Spelling", "Kind", "Library", "Notes"]
        if title == "Pass it to":
            header = ["Where", "Kind", "Library", "Notes"]
        return table(header, rows)

    # ------------------------------------------------------------ compose

    def compose(self, entity: model.Entity) -> str:
        page = entity.page
        lede, written = sections_of(page.body) if page is not None else ("", {})
        built = {}
        taken = set(written)
        for slot in SLOTS:
            if taken.intersection(slot):
                taken.update(slot)
        for title in GENERATED.get(entity.kind, ()):
            if title in taken:
                continue
            if title in ("Make one", "Pass it to", "Also returned by"):
                built[title] = self.sites(title, entity)
            elif title == "Syntax":
                built[title] = self.syntax(entity)
            elif title == "Parameters":
                built[title] = self.parameters(entity)
            elif title == "Values":
                built[title] = self.values_table(entity)
            elif title == "Members":
                built[title] = self.members(entity)
        if "Description" not in written and entity.detail:
            built["Description"] = entity.detail

        parts = [lede or entity.brief]
        if (
            page is not None
            and page.front.get("example")
            and "<!-- example:" not in (lede or "")
        ):
            parts.append(f"<!-- example: {page.front['example']} -->")
        for title in ORDER:
            body = written.get(title) or built.get(title)
            if body:
                parts.append(f"## {title}\n\n{body}")
        for title, body in written.items():
            if title not in ORDER and body:
                parts.append(f"## {title}\n\n{body}")
        return "\n\n".join(part for part in parts if part)
