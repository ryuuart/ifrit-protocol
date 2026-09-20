"""The whole run: read three sources, join them, and write the site.

Everything above is a part; this is the order they go in. It reads the
Doxygen XML inventory the docs verb's third pass wrote, the Python
declaration stubs, and the binding sources; joins them into one
catalogue; builds the type graph; and writes a page for every entity,
an index for every kind, a page for every value and the tree-wide index
over all of them.

The reference build never runs Doxygen. It reads what the earlier
passes left in the work directory, so a writer iterating on prose
rebuilds the site in a second or two.
"""

import dataclasses
import html
import re
import subprocess
import sys
from pathlib import Path

from sigil.reference import bindings, doxygen_xml, markdown, model, pages
from sigil.reference import catalogue as catalogue_module
from sigil.reference import graph as graph_module
from sigil.reference import report as report_module
from sigil.reference import site as site_module

# Where a library keeps the prose written for its own reference pages.
REFERENCE = "reference"
PAGES = "pages"
EXAMPLES = "examples"

# The pages that belong to no library.
CHAPTERS = ("overview", "guides")


@dataclasses.dataclass
class Options:
    """What one reference build was asked for."""

    package: Path
    binding_sources: list
    example_images: bool = False
    report: bool = False
    strict: bool = False


class Build:
    """One run, from the three readers to the written site."""

    def __init__(self, manifest, options: Options):
        self.manifest = manifest
        self.options = options
        self.root = manifest.root
        self.work = manifest.work
        self.templates = manifest.templates
        self.application = manifest.templates.parent
        self.written = []

    # ----------------------------------------------------------- reading

    def read(self) -> bool:
        # Every library, always. Narrowing is a Doxygen concern — one
        # site is one library — and this layer's whole point is that a
        # value's answer crosses libraries: read one and the index
        # would say a `Fill` is taken in two places.
        wanted = [library.name for library in self.manifest.libraries]
        self.roots = {
            library.name: self._library_root(library)
            for library in self.manifest.libraries
        }
        inventories = doxygen_xml.read(self.work, wanted)
        if not inventories:
            print(
                "no inventory to read — run the docs target once so its third "
                "pass writes the XML",
                file=sys.stderr,
            )
            return False
        self.missing = [name for name in wanted if name not in inventories]
        for name in self.missing:
            print(f"note: no inventory for {name} — its pages are not written")
        surface, roles = python_surface(self.options.package)
        bound = bindings.read(self.options.binding_sources)
        self.catalogue = catalogue_module.Catalogue(inventories, surface, roles, bound)
        self.graph = graph_module.Graph(self.catalogue)
        self.values = {
            entity.qualified: site_module.value_page(entity.qualified)
            for entity in self.catalogue.entities
            if entity.kind in (model.TYPE, model.ENUM)
        }
        self.shell = site_module.Shell(self.root, self.values)
        self.index = site_module.Index()
        self.composer = pages.Composer(self.catalogue, self.graph, self.values)
        self.examples = Examples(self)
        self._read_prose()
        return True

    def _library_root(self, library) -> Path:
        """Where a library's own source tree starts.

        The manifest records what comes off a printed path: the
        library's include root, the library itself, and the application
        above it. The middle one is the library.
        """
        candidates = [
            Path(entry) for entry in library.strip if Path(entry) != self.application
        ]
        if not candidates:
            return self.application
        return min(candidates, key=lambda path: len(str(path)))

    def _read_prose(self) -> None:
        """Every hand-written page, matched to the entity it describes."""
        self.orphans = []
        self.chapters = {}
        for library, root in self.roots.items():
            directory = root / REFERENCE / PAGES
            if not directory.is_dir():
                continue
            for path in sorted(directory.rglob("*.md")):
                page = read_page(path)
                entity = self._entity_for(library, path, page)
                if entity is None:
                    self.orphans.append((library, path))
                    continue
                entity.page = page
                if page.front.get("group"):
                    entity.group = page.front["group"]
        for chapter in CHAPTERS:
            directory = self.templates / chapter
            if directory.is_dir():
                for path in sorted(directory.glob("*.md")):
                    self.chapters.setdefault(chapter, []).append(read_page(path))
        glossary = self.templates / "glossary.md"
        if glossary.exists():
            self.chapters["glossary"] = [read_page(glossary)]

    def _entity_for(self, library: str, path: Path, page: model.Page):
        qualified = page.front.get("qualified")
        if qualified:
            return self.catalogue.entity(qualified)
        kind = page.front.get("kind") or _kind_of(path)
        name = page.front.get("name") or path.stem.replace(".", "::")
        for entity in self.catalogue.entities:
            if (
                entity.library == library
                and entity.kind == kind
                and entity.name == name
            ):
                return entity
        return None

    # ----------------------------------------------------------- writing

    def write(self) -> int:
        site_module.stage(self.templates, self.root)
        self._write_entities()
        self._write_kind_indexes()
        self._write_library_indexes()
        self._write_reference_index()
        self._write_values()
        self._write_chapters()
        self.index.write(self.root)
        print(f"Reference written to {self.root / 'reference' / 'index.html'}")
        return 0

    def _write_entities(self) -> None:
        for entity in self.catalogue.entities:
            path = f"{entity.path()}.html"
            body = self.composer.compose(entity)
            depth = path.count("/")
            links = self.shell.links(depth)
            rendered = markdown.render(
                body, links, {"example": self.examples.directive(links)}
            )
            self.shell.write(
                path,
                entity.name,
                rendered + self._footer(entity, links),
                badge=model.BADGES.get(entity.binding_state, ""),
                trail=self._trail(entity),
            )
            self.index.add(
                entity,
                path,
                sorted(self._accepts(entity)),
                entity.signatures[0].returns if entity.signatures else "",
            )
            self.written.append(entity.qualified)

    def _accepts(self, entity: model.Entity) -> set:
        found = set()
        for signature in entity.signatures:
            for parameter in signature.parameters:
                for spelling, _ in catalogue_module.mentions(parameter.type_text):
                    target = self.catalogue.resolve(spelling, within=entity.qualified)
                    if target:
                        found.add(target.rsplit("::", 1)[-1])
        return found

    def _trail(self, entity: model.Entity) -> list:
        trail = [("Reference", "reference/index.html")]
        if entity.kind in (model.TYPE, model.ENUM):
            trail = [("Values", "values/index.html")]
        return trail + [
            (entity.library, f"reference/{entity.library}/index.html"),
            (
                model.TITLES[entity.kind],
                f"reference/{entity.library}/{model.DIRECTORIES[entity.kind]}/index.html",
            ),
        ]

    def _footer(self, entity: model.Entity, links: markdown.Links) -> str:
        parts = []
        if entity.header:
            parts.append(f"<code>#include &lt;{html.escape(entity.header)}&gt;</code>")
        if entity.python:
            parts.append(f"<code>{html.escape(entity.python)}</code>")
        if entity.doxygen:
            parts.append(f'<a href="{links.to(entity.doxygen)}">The literal API</a>')
        if entity.qualified in self.values:
            parts.append(
                f'<a href="{links.to(self.values[entity.qualified])}">'
                "What makes one and what takes one</a>"
            )
        return '<footer class="origin">' + " · ".join(parts) + "</footer>"

    def _write_kind_indexes(self) -> None:
        for library in sorted({entity.library for entity in self.catalogue.entities}):
            for kind in model.KINDS:
                held = [
                    entity
                    for entity in self.catalogue.entities
                    if entity.library == library and entity.kind == kind
                ]
                if not held:
                    continue
                path = f"reference/{library}/{model.DIRECTORIES[kind]}/index.html"
                links = self.shell.links(path.count("/"))
                self.shell.write(
                    path,
                    f"{library} {model.TITLES[kind].lower()}",
                    self._kind_table(held, links),
                    lede=html.escape(model.DESCRIPTIONS[kind]),
                    trail=[
                        ("Reference", "reference/index.html"),
                        (library, f"reference/{library}/index.html"),
                    ],
                )

    def _kind_table(self, held: list, links: markdown.Links) -> str:
        groups = {}
        for entity in sorted(held, key=lambda one: one.name.lower()):
            groups.setdefault(entity.group, []).append(entity)
        out = []
        for group in sorted(groups, key=lambda name: (name == "", name)):
            if group:
                out.append(f'<h2 id="{markdown.slug(group)}">{html.escape(group)}</h2>')
            rows = []
            for entity in groups[group]:
                python = (
                    f"<code>{html.escape(entity.python)}</code>"
                    if entity.python
                    else '<span class="unbound">not bound</span>'
                )
                rows.append(
                    "<tr>"
                    f'<td><a href="{links.to(entity.path())}.html">'
                    f"<code>{html.escape(entity.name)}</code></a></td>"
                    f"<td>{html.escape(pages.first_sentence(entity.summary()))}</td>"
                    f"<td>{python}</td></tr>"
                )
            out.append(
                '<div class="scroller"><table><thead><tr><th>Name</th>'
                "<th>What it does</th><th>Python</th></tr></thead><tbody>"
                + "".join(rows)
                + "</tbody></table></div>"
            )
        return "".join(out)

    def _write_library_indexes(self) -> None:
        for library in sorted({entity.library for entity in self.catalogue.entities}):
            held = [
                entity
                for entity in self.catalogue.entities
                if entity.library == library
            ]
            path = f"reference/{library}/index.html"
            links = self.shell.links(path.count("/"))
            cards = []
            for kind in model.KINDS:
                count = sum(1 for entity in held if entity.kind == kind)
                if not count:
                    continue
                target = links.to(
                    f"reference/{library}/{model.DIRECTORIES[kind]}/index.html"
                )
                cards.append(
                    f'<li><a href="{target}"><b>{model.TITLES[kind]}</b>'
                    f"<span>{count} — {html.escape(model.DESCRIPTIONS[kind])}</span></a></li>"
                )
            brief = (
                self.manifest.find(library).brief if self._registered(library) else ""
            )
            self.shell.write(
                path,
                library,
                f'<ul class="cards">{"".join(cards)}</ul>',
                lede=html.escape(pages.first_sentence(brief)),
                trail=[("Reference", "reference/index.html")],
            )

    def _registered(self, library: str) -> bool:
        return any(one.name == library for one in self.manifest.libraries)

    def _write_reference_index(self) -> None:
        links = self.shell.links(1)
        rows = []
        for library in sorted({entity.library for entity in self.catalogue.entities}):
            held = [
                entity
                for entity in self.catalogue.entities
                if entity.library == library
            ]
            counts = {
                kind: sum(1 for entity in held if entity.kind == kind)
                for kind in model.KINDS
            }
            cells = "".join(f"<td>{counts[kind] or '—'}</td>" for kind in model.KINDS)
            rows.append(
                f'<tr><th scope="row"><a href="{links.to(f"reference/{library}/index.html")}">'
                f"{html.escape(library)}</a></th>{cells}"
                f"<td>{len(held)}</td></tr>"
            )
        header = "".join(f"<th>{model.TITLES[kind]}</th>" for kind in model.KINDS)
        self.shell.write(
            "reference/index.html",
            "Reference",
            '<div class="scroller"><table><thead><tr><th>Library</th>'
            f"{header}<th>All</th></tr></thead><tbody>"
            + "".join(rows)
            + "</tbody></table></div>",
            lede="Every library, every kind, one screen. A kind is what a "
            "thing is: something you create, something you say to one, a "
            "value you pass.",
        )

    def _write_values(self) -> None:
        rows = []
        links = self.shell.links(1)
        ordered = sorted(
            self.values,
            key=lambda name: (
                self.catalogue.entity(name).library,
                self.catalogue.display(name, self.catalogue.entity(name).library),
            ),
        )
        for qualified in ordered:
            path = self.values[qualified]
            entity = self.catalogue.entity(qualified)
            made, taken, given = self.graph.counts(qualified)
            rows.append(
                f'<tr><td><a href="{links.to(path)}"><code>'
                f"{html.escape(self.catalogue.display(qualified, entity.library))}"
                f"</code></a></td>"
                f"<td>{html.escape(entity.library)}</td>"
                f"<td>{made}</td><td>{taken}</td><td>{given}</td>"
                f"<td>{html.escape(entity.python)}</td></tr>"
            )
            self.index.value(qualified, made, taken, entity.library)
        self.shell.write(
            "values/index.html",
            "Values",
            '<div class="scroller"><table id="values"><thead><tr><th>Type</th>'
            "<th>Library</th><th>Ways to make one</th><th>Places that take one</th>"
            "<th>Things that return one</th><th>Python</th></tr></thead><tbody>"
            + "".join(rows)
            + "</tbody></table></div>",
            lede="Every value in the tree, and for each one: what makes it, "
            "what takes it, and what hands one back. This is the index to "
            "open when a signature asks for something you have not made before.",
        )

    def _write_chapters(self) -> None:
        for chapter in CHAPTERS:
            held = self.chapters.get(chapter, [])
            for page in held:
                name = page.path.stem
                path = f"{chapter}/{name}.html"
                links = self.shell.links(1)
                self.shell.write(
                    path,
                    page.front.get("title") or name.replace("-", " ").capitalize(),
                    markdown.render(
                        page.body, links, {"example": self.examples.directive(links)}
                    ),
                    lede=html.escape(page.front.get("summary", "")),
                )
            if not any(page.path.stem == "index" for page in held):
                self._write_chapter_index(chapter, held)
        glossary = self.chapters.get("glossary")
        if glossary:
            links = self.shell.links(0)
            self.shell.write(
                "glossary.html",
                glossary[0].front.get("title") or "Glossary",
                markdown.render(glossary[0].body, links),
            )

    def _write_chapter_index(self, chapter: str, held: list) -> None:
        links = self.shell.links(1)
        rows = "".join(
            f'<li><a href="{links.to(f"{chapter}/{page.path.stem}.html")}">'
            f"<b>{html.escape(page.front.get('title') or page.path.stem)}</b>"
            f"<span>{html.escape(page.front.get('summary', ''))}</span></a></li>"
            for page in held
        )
        self.shell.write(
            f"{chapter}/index.html",
            chapter.capitalize(),
            f'<ul class="cards">{rows}</ul>'
            if rows
            else "<p>Nothing written here yet.</p>",
        )


class Examples:
    """A page's picture: two sketches that must draw the same thing.

    The example is a real sketch on disk in both languages. Rendering
    is off by default because a C++ sketch compiles before it draws,
    and a writer moving prose should not wait for it; with the flag on,
    both are rendered and differenced, so "this is the Python spelling"
    is a claim the build checks rather than one a page asserts.
    """

    def __init__(self, build: Build):
        self.build = build
        self.failures = []

    def find(self, stem: str) -> tuple:
        for root in self.build.roots.values():
            directory = root / REFERENCE / EXAMPLES
            source = directory / f"{stem}.cpp"
            twin = directory / f"{stem}.py"
            if source.exists() or twin.exists():
                return (
                    source if source.exists() else None,
                    twin if twin.exists() else None,
                )
        return (None, None)

    def directive(self, links: markdown.Links):
        def render(stem: str) -> str:
            source, twin = self.find(stem)
            if source is None and twin is None:
                self.failures.append((stem, "no example of that name"))
                return (
                    '<figure class="example missing"><figcaption>'
                    f"No example named {html.escape(stem)}</figcaption></figure>"
                )
            picture = self._picture(stem, source, twin, links)
            blocks = []
            if source is not None:
                blocks.append(("cpp", source.read_text()))
            if twin is not None:
                blocks.append(("python", twin.read_text()))
            body = markdown.Renderer(links).code(blocks)
            # The sketch is a whole file: a canvas, a capture moment and
            # the picture. Folded, because the picture is what the page
            # is showing and the file is what a reader opens next.
            return (
                f'<figure class="example">{picture}'
                f"<details><summary>The sketch that drew this — "
                f"<code>{html.escape(stem)}</code></summary>{body}</details>"
                "</figure>"
            )

        return render

    def _picture(self, stem: str, source, twin, links: markdown.Links) -> str:
        if not self.build.options.example_images:
            return '<div class="placeholder">Built without --example-images</div>'
        out = self.build.root / "examples"
        out.mkdir(parents=True, exist_ok=True)
        drawn = out / f"{stem}.png"
        if source is not None and not self._sketchbook(source, drawn):
            return '<div class="placeholder">This example did not render</div>'
        if twin is not None:
            self._python(stem, twin, out)
        return (
            f'<img src="{links.to(f"examples/{stem}.png")}" alt="{html.escape(stem)}">'
        )

    def _sketchbook(self, source: Path, out: Path) -> bool:
        binary = (
            self.build.application
            / "build"
            / "bin"
            / "Release"
            / "Sketchbook.app"
            / "Contents"
            / "MacOS"
            / "Sketchbook"
        )
        if not binary.exists():
            self.failures.append((source.stem, "Sketchbook is not built"))
            return False
        finished = subprocess.run(
            [str(binary), str(source), "--frame", str(out)],
            capture_output=True,
            text=True,
        )
        if finished.returncode != 0:
            self.failures.append((source.stem, finished.stderr.strip()[:200]))
            return False
        return True

    def _python(self, stem: str, twin: Path, out: Path) -> None:
        interpreter = (
            self.build.application.parent / "python" / ".venv" / "bin" / "python"
        )
        if not interpreter.exists():
            self.failures.append((stem, "no Python workspace to render the twin"))
            return
        twinned = out / f"{stem}.python.png"
        finished = subprocess.run(
            [
                str(interpreter),
                "-m",
                "sigil",
                "render",
                str(twin),
                "--output",
                str(twinned),
            ],
            capture_output=True,
            text=True,
        )
        if finished.returncode != 0:
            self.failures.append((stem, finished.stderr.strip()[:200]))


def _kind_of(path: Path) -> str:
    directory = path.parent.name
    for kind, name in model.DIRECTORIES.items():
        if name == directory:
            return kind
    return ""


TITLE = re.compile(r"\A\s*#\s+.*\n")


def read_page(path: Path) -> model.Page:
    front, body = markdown.front_matter(path.read_text())
    # The page's own title is generated, with the binding badge beside
    # it, so a heading a writer opened with is dropped rather than
    # rendered a second time under the first. A chapter, which has no
    # entity to take a name from, keeps it as its title.
    opening = TITLE.match(body)
    if opening is not None:
        front.setdefault("title", opening.group(0).lstrip("# ").strip())
        body = body[opening.end() :]
    lede, _ = pages.sections_of(body)
    return model.Page(
        path=path,
        front=front,
        body=body,
        lede=lede,
        summary=front.get("summary") or pages.first_sentence(lede),
    )


def python_surface(package: Path):
    from sigil.reference import python_stubs

    return python_stubs.read(package)


def build(manifest, options: Options) -> int:
    run = Build(manifest, options)
    if not run.read():
        return 1
    code = run.write()
    if options.report or options.strict:
        code = report_module.write(run) or code
    return code
