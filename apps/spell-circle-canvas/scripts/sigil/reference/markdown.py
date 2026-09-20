"""A restricted Markdown renderer, and the front matter above it.

Restricted on purpose. The pages are written to one template and read
by one renderer, so the grammar is exactly what a reference page needs:
headings, paragraphs, fenced code, tables, links, lists, blockquotes,
inline code, emphasis, rules, and two directives of our own. Anything
outside that renders as the text it is, which is a visible mistake
rather than a silent one.

Two things this does that a stock renderer would not. A `cpp` block
immediately followed by a `python` block becomes one two-tab widget, so
a page carries both spellings without becoming two pages. And a link
target written `value:sigil::compose::Fill` or `page:SigilCompose/verbs/fill`
is resolved against where the page will stand, so a relative href is
correct from any depth and from a file:// window as well as a server.
"""

import html
import re

FENCE = re.compile(r"^(```|~~~)\s*([\w+-]*)\s*$")
HEADING = re.compile(r"^(#{1,6})\s+(.*)$")
BULLET = re.compile(r"^(\s*)([-*+])\s+(.*)$")
NUMBERED = re.compile(r"^(\s*)(\d+)[.)]\s+(.*)$")
RULE = re.compile(r"^(-{3,}|\*{3,}|_{3,})\s*$")
QUOTE = re.compile(r"^>\s?(.*)$")
DIVIDER = re.compile(r"^\s*\|?\s*:?-{2,}:?\s*(\|\s*:?-{2,}:?\s*)*\|?\s*$")
DIRECTIVE = re.compile(r"^<!--\s*(\w+):\s*(.*?)\s*-->$")
FRONT = re.compile(r"\A---\s*\n(.*?)\n---\s*\n?", re.DOTALL)

INLINE = re.compile(
    r"(?P<code>`+)(?P<codetext>.+?)(?P=code)"
    r"|!\[(?P<alt>[^\]]*)\]\((?P<image>[^)\s]+)\)"
    r"|\[(?P<text>[^\]]+)\]\((?P<target>[^)\s]+)\)"
    r"|<(?P<autolink>https?://[^>\s]+)>"
    r"|\*\*(?P<strong>.+?)\*\*"
    r"|(?<![\w*])\*(?P<emphasis>[^*\n]+)\*(?![\w*])",
    re.DOTALL,
)


def front_matter(text: str) -> tuple:
    """The `key: value` block above a page, and the body under it.

    One key per line, a scalar or a bracketed list, no nesting. A page
    is data with prose in it, and a fuller YAML would only offer shapes
    the generator has nowhere to put.
    """
    match = FRONT.match(text)
    if match is None:
        return {}, text
    found = {}
    for line in match.group(1).splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        key, separator, value = stripped.partition(":")
        if not separator:
            continue
        found[key.strip()] = _scalar(value.strip())
    return found, text[match.end() :]


def _scalar(value: str):
    if value.startswith("[") and value.endswith("]"):
        return [
            part.strip().strip("'\"") for part in value[1:-1].split(",") if part.strip()
        ]
    return value.strip("'\"")


class Links:
    """How a shorthand target becomes an href from one page.

    `depth` is how many directories down the page stands from the site
    root, which is the only thing a relative path needs to know.
    """

    SCHEMES = ("value", "page", "guide", "overview", "glossary", "doxygen")

    def __init__(self, depth: int, values: dict | None = None):
        self.depth = depth
        self.values = values or {}
        self.unresolved = []

    def up(self) -> str:
        return "../" * self.depth

    def to(self, path: str) -> str:
        return f"{self.up()}{path}"

    def resolve(self, target: str) -> str:
        scheme, separator, rest = target.partition(":")
        if separator and scheme in self.SCHEMES:
            return getattr(self, f"_{scheme}")(rest)
        if target.startswith("/"):
            return self.to(target[1:])
        return target

    def _value(self, qualified: str) -> str:
        page = self.values.get(qualified)
        if page is None:
            self.unresolved.append(f"value:{qualified}")
            return self.to("values/index.html")
        return self.to(page)

    def _page(self, path: str) -> str:
        return self.to(f"reference/{path}.html")

    def _guide(self, name: str) -> str:
        return self.to(f"guides/{name}.html")

    def _overview(self, name: str) -> str:
        return self.to(f"overview/{name}.html")

    def _glossary(self, term: str) -> str:
        anchor = f"#{slug(term)}" if term else ""
        return self.to(f"glossary.html{anchor}")

    def _doxygen(self, path: str) -> str:
        if "/" not in path:
            path = f"{path}/html/index.html"
        return self.to(path)


def slug(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", text.strip().lower()).strip("-")


def inline(text: str, links: Links) -> str:
    """One line of prose, with its spans."""
    out = []
    at = 0
    for match in INLINE.finditer(text):
        out.append(html.escape(text[at : match.start()]))
        at = match.end()
        if match.group("codetext") is not None:
            out.append(f"<code>{html.escape(match.group('codetext').strip())}</code>")
        elif match.group("image") is not None:
            source = links.resolve(match.group("image"))
            out.append(
                f'<img src="{html.escape(source)}" '
                f'alt="{html.escape(match.group("alt"))}">'
            )
        elif match.group("target") is not None:
            href = links.resolve(match.group("target"))
            out.append(
                f'<a href="{html.escape(href)}">{inline(match.group("text"), links)}</a>'
            )
        elif match.group("autolink") is not None:
            address = html.escape(match.group("autolink"))
            out.append(f'<a href="{address}">{address}</a>')
        elif match.group("strong") is not None:
            out.append(f"<strong>{inline(match.group('strong'), links)}</strong>")
        else:
            out.append(f"<em>{inline(match.group('emphasis'), links)}</em>")
    out.append(html.escape(text[at:]))
    return "".join(out)


class Renderer:
    """Markdown to HTML, one document at a time."""

    def __init__(self, links: Links, directives=None):
        self.links = links
        self.directives = directives or {}
        self.headings = []

    def render(self, text: str) -> str:
        lines = text.replace("\r\n", "\n").split("\n")
        self.out = []
        self.index = 0
        self.lines = lines
        while self.index < len(lines):
            self._block()
        return "\n".join(part for part in self.out if part)

    # ------------------------------------------------------------- blocks

    def _block(self) -> None:
        line = self.lines[self.index]
        if not line.strip():
            self.index += 1
            return
        for reader in (
            self._directive,
            self._fence,
            self._heading,
            self._rule,
            self._table,
            self._list,
            self._quote,
        ):
            if reader(line):
                return
        self._paragraph()

    def _directive(self, line: str) -> bool:
        match = DIRECTIVE.match(line.strip())
        if match is None:
            return False
        self.index += 1
        handler = self.directives.get(match.group(1))
        if handler is not None:
            self.out.append(handler(match.group(2)))
        return True

    def _fence(self, line: str) -> bool:
        match = FENCE.match(line.strip())
        if match is None:
            return False
        blocks = []
        while self.index < len(self.lines):
            opening = FENCE.match(self.lines[self.index].strip())
            if opening is None:
                break
            language = opening.group(2) or "text"
            body, closed = self._fenced(opening.group(1))
            blocks.append((language, body))
            if not closed:
                break
            while self.index < len(self.lines) and not self.lines[self.index].strip():
                if len(blocks) == 1 and blocks[0][0] == "cpp":
                    self.index += 1
                    continue
                break
            if len(blocks) == 2:
                break
        self.out.append(self.code(blocks))
        return True

    def _fenced(self, marker: str) -> tuple:
        self.index += 1
        body = []
        while self.index < len(self.lines):
            line = self.lines[self.index]
            self.index += 1
            if line.strip().startswith(marker):
                return "\n".join(body), True
            body.append(line)
        return "\n".join(body), False

    def code(self, blocks: list) -> str:
        """One code block, or the two-tab widget a language pair makes."""
        if len(blocks) == 2 and [name for name, _ in blocks] == ["cpp", "python"]:
            return tabs(blocks)
        return "".join(
            f'<pre class="code" data-language="{html.escape(language)}">'
            f"<code>{html.escape(body)}</code></pre>"
            for language, body in blocks
        )

    def _heading(self, line: str) -> bool:
        match = HEADING.match(line)
        if match is None:
            return False
        self.index += 1
        level = len(match.group(1))
        title = match.group(2).strip()
        identifier = slug(title)
        self.headings.append((level, title, identifier))
        self.out.append(
            f'<h{level} id="{identifier}">{inline(title, self.links)}</h{level}>'
        )
        return True

    def _rule(self, line: str) -> bool:
        if RULE.match(line.strip()) is None:
            return False
        self.index += 1
        self.out.append("<hr>")
        return True

    def _table(self, line: str) -> bool:
        if "|" not in line or self.index + 1 >= len(self.lines):
            return False
        if DIVIDER.match(self.lines[self.index + 1]) is None:
            return False
        header = _cells(line)
        self.index += 2
        rows = []
        while self.index < len(self.lines) and "|" in self.lines[self.index]:
            rows.append(_cells(self.lines[self.index]))
            self.index += 1
        head = "".join(f"<th>{inline(cell, self.links)}</th>" for cell in header)
        body = "".join(
            "<tr>"
            + "".join(f"<td>{inline(cell, self.links)}</td>" for cell in row)
            + "</tr>"
            for row in rows
        )
        self.out.append(
            f'<div class="scroller"><table><thead><tr>{head}</tr></thead>'
            f"<tbody>{body}</tbody></table></div>"
        )
        return True

    def _list(self, line: str) -> bool:
        if BULLET.match(line) is None and NUMBERED.match(line) is None:
            return False
        items = []
        ordered = NUMBERED.match(line) is not None
        while self.index < len(self.lines):
            current = self.lines[self.index]
            match = NUMBERED.match(current) if ordered else BULLET.match(current)
            if match is None:
                if current.strip() and current.startswith(("  ", "\t")) and items:
                    items[-1] += " " + current.strip()
                    self.index += 1
                    continue
                break
            items.append(match.group(3))
            self.index += 1
        tag = "ol" if ordered else "ul"
        body = "".join(f"<li>{inline(item, self.links)}</li>" for item in items)
        self.out.append(f"<{tag}>{body}</{tag}>")
        return True

    def _quote(self, line: str) -> bool:
        if QUOTE.match(line) is None:
            return False
        held = []
        while self.index < len(self.lines):
            match = QUOTE.match(self.lines[self.index])
            if match is None:
                break
            held.append(match.group(1))
            self.index += 1
        inner = Renderer(self.links, self.directives).render("\n".join(held))
        self.out.append(f"<blockquote>{inner}</blockquote>")
        return True

    def _paragraph(self) -> None:
        held = []
        while self.index < len(self.lines):
            line = self.lines[self.index]
            if not line.strip() or HEADING.match(line) or FENCE.match(line.strip()):
                break
            if BULLET.match(line) or NUMBERED.match(line) or QUOTE.match(line):
                break
            held.append(line.strip())
            self.index += 1
        if held:
            self.out.append(f"<p>{inline(' '.join(held), self.links)}</p>")


def _cells(line: str) -> list:
    stripped = line.strip().strip("|")
    return [cell.strip() for cell in stripped.split("|")]


def tabs(blocks: list) -> str:
    """The two-language widget: one pair of buttons, one pair of panes."""
    buttons = "".join(
        f'<button type="button" class="tab" data-language="{name}">'
        f"{'C++' if name == 'cpp' else 'Python'}</button>"
        for name, _ in blocks
    )
    panes = "".join(
        f'<pre class="pane" data-language="{name}"><code>{html.escape(body)}</code></pre>'
        for name, body in blocks
    )
    return f'<div class="languages"><div class="tabs">{buttons}</div>{panes}</div>'


def render(text: str, links: Links, directives=None) -> str:
    return Renderer(links, directives).render(text)
