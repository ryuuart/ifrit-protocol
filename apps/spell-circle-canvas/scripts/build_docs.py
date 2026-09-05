#!/usr/bin/env python3
"""Generates the per-library Doxygen sites and the page that lists them.

Driven by the manifest sigil_finalize_docs() writes, which is the list of
libraries registered through sigil_library_root() or sigil_add_docs():
name, brief, input directories, path prefix to strip, and the document
to use as the site's front page.

Usage (the `docs` and `docs-<Lib>` targets call it):
  scripts/build_docs.py --manifest build/docs-manifest.txt
  scripts/build_docs.py --manifest build/docs-manifest.txt --library SigilWeave

Generation runs in TWO PASSES because the libraries reference each
other's types in both directions — SigilWorld takes SigilGeometry's
meshes, SigilCompose takes SigilMotion's animatables — and a Doxygen tag
file can only be read after it has been written. The first pass writes
every tag file and no HTML; the second reads all of them and writes the
HTML. A single pass would resolve only the edges that happen to run in
the order the libraries were registered.

Everything used to produce a site — the rendered Doxyfiles, the tag
files, the theme, the generated header — lands in the work directory,
apart from the sites themselves, which are what gets served. Both are
disposable: this script rewrites whatever is missing.
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fetch_assets import Asset, fetch

# jothepro/doxygen-awesome-css, MIT. The commit release v2.4.2 points at.
#
# Doxygen's stock HTML is close to unusable on a phone: fixed-width
# tables, a navigation tree that assumes a mouse, and no viewport-aware
# layout. Doxygen Awesome replaces the stylesheet without touching the
# generated HTML structure, so the fix is a download rather than a fork.
# The rules are the ones scripts/fetch_assets.py states: open licence,
# the licence file alongside, pinned to a commit, hashed per file, never
# vendored.
_THEME = (
    "https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/"
    "d52eafe3e9303399fda15661f3d7bb8fe3d7eabc"
)

THEME_CSS = [
    "doxygen-awesome.css",
    "doxygen-awesome-sidebar-only.css",
    "doxygen-awesome-sidebar-only-darkmode-toggle.css",
]
THEME_JS = [
    "doxygen-awesome-darkmode-toggle.js",
    "doxygen-awesome-fragment-copy-button.js",
    "doxygen-awesome-paragraph-link.js",
    "doxygen-awesome-interactive-toc.js",
]
THEME = [
    Asset(f"{_THEME}/{name}", name, digest)
    for name, digest in (
        (
            "doxygen-awesome.css",
            "5ec49e2dfd097f6b5384e3aae0476eab47748e311fc70e207925f8fcc37477b9",
        ),
        (
            "doxygen-awesome-sidebar-only.css",
            "dc7ddd235375b71ecb0af920faa6b925ee9445ac617f3bc962b0b0db97da7b4f",
        ),
        (
            "doxygen-awesome-sidebar-only-darkmode-toggle.css",
            "c1939ca910d2282068482abc72e9edcf9835e4de153ebe8b428cbace92ed4c2c",
        ),
        (
            "doxygen-awesome-darkmode-toggle.js",
            "de752867789ed21154983c22ef34441137b4cc558d5a2f92013f5b894483e5a4",
        ),
        (
            "doxygen-awesome-fragment-copy-button.js",
            "009b4c9982c18bc68c6366321298316e9054a620e37b99de1276ff6a1e2c65a0",
        ),
        (
            "doxygen-awesome-paragraph-link.js",
            "f9fe333b516cdc259a25475b0ca472e8e091fd7abf9020e54949c4677a7a427f",
        ),
        (
            "doxygen-awesome-interactive-toc.js",
            "a7d6a4d59809b650afd011af6fc8805075aeb5e310940fb9583a42652fe87ba8",
        ),
        (
            "LICENSE",
            "e3da754c3f657cc78594fa2e8a3283665f78c743df2485fa9e498a8973051191",
        ),
    )
]

# $relpath^ is Doxygen's own placeholder for the path back to the output
# root, so one header works at every depth of the generated tree.
HEADER_SCRIPTS = """<script type="text/javascript" src="$relpath^doxygen-awesome-darkmode-toggle.js"></script>
<script type="text/javascript" src="$relpath^doxygen-awesome-fragment-copy-button.js"></script>
<script type="text/javascript" src="$relpath^doxygen-awesome-paragraph-link.js"></script>
<script type="text/javascript" src="$relpath^doxygen-awesome-interactive-toc.js"></script>
<script type="text/javascript">
  DoxygenAwesomeDarkModeToggle.init()
  DoxygenAwesomeFragmentCopyButton.init()
  DoxygenAwesomeParagraphLink.init()
  DoxygenAwesomeInteractiveToc.init()
</script>
</head>"""

# A Doxyfile setting continues onto the next line with a trailing
# backslash, and the continuation lines are indented to the column the
# values start in.
CONTINUE = " \\\n" + " " * 25

SOURCE_PATTERNS = ("*.h", "*.hpp", "*.md")


class Library:
    """One registered library: what to read, and how to present it."""

    def __init__(self, name: str):
        self.name = name
        self.brief = ""
        self.mainpage = ""
        self.input: list[str] = []
        self.strip: list[str] = []

    def sources(self) -> list[Path]:
        """Every file Doxygen will read for this library.

        An INPUT entry is either a directory to walk or a single file
        named outright, which is how a library's README joins its
        headers.
        """
        found = []
        for entry in self.input:
            path = Path(entry)
            if path.is_dir():
                for pattern in SOURCE_PATTERNS:
                    found.extend(path.rglob(pattern))
            else:
                found.append(path)
        return found


class Manifest:
    """The settings and the libraries sigil_finalize_docs() recorded."""

    def __init__(self, path: Path):
        settings: dict[str, str] = {}
        self.libraries: list[Library] = []
        current: Library | None = None
        for line in path.read_text().splitlines():
            if not line or line.startswith("#"):
                continue
            key, _, value = line.partition("=")
            if key == "library":
                current = Library(value)
                self.libraries.append(current)
            elif current is None:
                settings[key] = value
            elif key in ("input", "strip"):
                setattr(current, key, [part for part in value.split(";") if part])
            else:
                setattr(current, key, value)

        self.doxygen = settings["doxygen"]
        self.have_dot = settings["have_dot"]
        self.warn_undocumented = settings["warn_undocumented"]
        self.root = Path(settings["docs_root"])
        self.work = Path(settings["work"])
        self.module_dir = Path(settings["module_dir"])

    def find(self, name: str) -> Library:
        for library in self.libraries:
            if library.name == name:
                return library
        sys.exit(f"no library named {name} in the manifest")


def write_if_changed(path: Path, text: str) -> None:
    """Leaves a file's timestamp alone when its content has not moved.

    Doxygen is re-run from a file's timestamp, so rewriting an identical
    Doxyfile would re-index every library on every build.
    """
    if path.exists() and path.read_text() == text:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


def render_doxyfile(template: str, values: dict[str, str]) -> str:
    def substitute(match: re.Match) -> str:
        name = match.group(1)
        if name not in values:
            sys.exit(f"Doxyfile.in names @{name}@, which nothing supplies")
        return values[name]

    return re.sub(r"@([A-Za-z0-9_]+)@", substitute, template)


def make_header(manifest: Manifest) -> Path:
    """Doxygen's own HTML header, with the theme's scripts spliced in.

    Generated rather than checked in: Doxygen emits the header its own
    version expects — the stock one already carries the viewport meta tag
    a phone needs — and a copy frozen in the source tree would silently
    drift from it on every Doxygen upgrade, which shows as a half-styled
    page rather than an error. Only the script tags are ours.
    """
    work = manifest.work / "header-work"
    work.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [manifest.doxygen, "-w", "html", "header.html", "footer.html", "style.css"],
        cwd=work,
        check=True,
        stdout=subprocess.DEVNULL,
    )
    header = (work / "header.html").read_text()
    if "</head>" not in header:
        sys.exit("doxygen's generated header has no </head>")
    out = manifest.work / "header.html"
    write_if_changed(out, header.replace("</head>", HEADER_SCRIPTS))
    return out


def doxyfile_values(manifest: Manifest, library: Library, **overrides) -> dict:
    values = {
        "SIGIL_DOCS_NAME": library.name,
        "SIGIL_DOCS_BRIEF": library.brief,
        "SIGIL_DOCS_OUTPUT": str(manifest.root / library.name),
        "SIGIL_DOCS_INPUT": CONTINUE.join(library.input),
        "SIGIL_DOCS_STRIP": CONTINUE.join(library.strip or library.input),
        "SIGIL_DOCS_MAINPAGE": library.mainpage,
        "SIGIL_DOCS_GENERATE_HTML": "NO",
        "SIGIL_DOCS_TAGFILE": "",
        "SIGIL_DOCS_TAGFILES_IN": "",
        "SIGIL_DOCS_WARN_UNDOCUMENTED": "NO",
        "SIGIL_DOCS_HAVE_DOT": "NO",
        "SIGIL_DOCS_HEADER": "",
        "SIGIL_DOCS_STYLESHEETS": "",
        "SIGIL_DOCS_EXTRA_FILES": "",
    }
    values.update(overrides)
    return values


def run_doxygen(manifest: Manifest, doxyfile: Path, library: Library) -> None:
    (manifest.root / library.name).mkdir(parents=True, exist_ok=True)
    subprocess.run([manifest.doxygen, str(doxyfile)], check=True)


def index_pass(manifest: Manifest, template: str) -> None:
    """Pass one: every tag file, no HTML.

    A tag file is re-indexed when a header, a README or the Doxyfile that
    reads them is newer than it, which is the same rule the build applies
    to any other generated file.
    """
    for library in manifest.libraries:
        tagfile = manifest.work / f"{library.name}.tag"
        doxyfile = manifest.work / library.name / "Doxyfile.tags"
        write_if_changed(
            doxyfile,
            render_doxyfile(
                template,
                doxyfile_values(manifest, library, SIGIL_DOCS_TAGFILE=str(tagfile)),
            ),
        )
        inputs = [*library.sources(), doxyfile]
        if tagfile.exists():
            stamp = tagfile.stat().st_mtime
            if all(source.stat().st_mtime <= stamp for source in inputs):
                continue
        print(f"Indexing {library.name} for cross-library links")
        run_doxygen(manifest, doxyfile, library)


def html_pass(manifest: Manifest, template: str, wanted: list[Library]) -> None:
    """Pass two: HTML, each library reading every other library's tag file."""
    theme = manifest.work / "theme"
    stylesheets = [theme / name for name in THEME_CSS]
    stylesheets.append(manifest.module_dir / "custom.css")
    header = make_header(manifest)

    for library in wanted:
        # `tag=path` points a resolved name at the site that documents
        # it. The path is relative so the whole docs tree can be moved or
        # served from anywhere.
        tagfiles = [
            f"{manifest.work / other.name}.tag=../../{other.name}/html"
            for other in manifest.libraries
            if other.name != library.name
        ]
        doxyfile = manifest.work / library.name / "Doxyfile"
        write_if_changed(
            doxyfile,
            render_doxyfile(
                template,
                doxyfile_values(
                    manifest,
                    library,
                    SIGIL_DOCS_GENERATE_HTML="YES",
                    SIGIL_DOCS_TAGFILES_IN=CONTINUE.join(tagfiles),
                    SIGIL_DOCS_WARN_UNDOCUMENTED=manifest.warn_undocumented,
                    SIGIL_DOCS_HAVE_DOT=manifest.have_dot,
                    SIGIL_DOCS_HEADER=str(header),
                    SIGIL_DOCS_STYLESHEETS=CONTINUE.join(
                        str(path) for path in stylesheets
                    ),
                    SIGIL_DOCS_EXTRA_FILES=CONTINUE.join(
                        str(theme / name) for name in THEME_JS
                    ),
                ),
            ),
        )
        print(f"Writing the {library.name} documentation")
        run_doxygen(manifest, doxyfile, library)


def write_index(manifest: Manifest) -> None:
    """The landing page.

    Doxygen writes one self-contained site per library and has no notion
    of a set of them, so the set gets its own page. Registration order is
    subdirectory order, which is a build concern and means nothing to a
    reader looking for a library by name.
    """
    cards = "".join(
        f'  <li><a href="{library.name}/html/index.html"><b>{library.name}</b>'
        f"<span>{library.brief}</span></a></li>\n"
        for library in sorted(manifest.libraries, key=lambda lib: lib.name.lower())
    )
    write_if_changed(
        manifest.root / "index.html",
        """<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SpellCircle libraries</title>
<style>
  :root { color-scheme: light dark; }
  body { font: 16px/1.6 -apple-system, system-ui, sans-serif;
         max-width: 46rem; margin: 4rem auto; padding: 0 1.5rem; }
  h1 { font-size: 1.5rem; margin-bottom: .25rem; }
  p.lede { color: GrayText; margin-top: 0; }
  ul { list-style: none; padding: 0; }
  li { margin: .5rem 0; }
  a { display: block; padding: .8rem 1rem; border: 1px solid;
      border-color: color-mix(in srgb, currentColor 20%, transparent);
      border-radius: .5rem; text-decoration: none; color: inherit; }
  a:hover { border-color: color-mix(in srgb, currentColor 45%, transparent); }
  b { display: block; }
  span { color: GrayText; font-size: .9rem; }
</style></head><body>
<h1>SpellCircle libraries</h1>
<p class="lede">Generated from the headers. Each library's README is its
front page, and types resolve across library boundaries.</p>
<ul>
"""
        + cards
        + """</ul>
</body></html>
""",
    )


def stage_container(manifest: Manifest) -> None:
    """Everything docker needs travels with the generated site, so the
    output tree is a complete build context on its own."""
    for name, staged in (
        ("Dockerfile", "Dockerfile"),
        ("nginx.conf", "nginx.conf"),
        ("dockerignore", ".dockerignore"),
    ):
        shutil.copyfile(manifest.module_dir / name, manifest.root / staged)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument(
        "--library",
        action="append",
        help="write only this library's site (repeatable); the landing "
        "page and the container files are left alone",
    )
    args = parser.parse_args()

    manifest = Manifest(args.manifest)
    manifest.root.mkdir(parents=True, exist_ok=True)
    manifest.work.mkdir(parents=True, exist_ok=True)
    fetch(THEME, manifest.work / "theme", quiet=True)

    template = (manifest.module_dir / "Doxyfile.in").read_text()
    index_pass(manifest, template)
    if args.library:
        html_pass(manifest, template, [manifest.find(name) for name in args.library])
        return
    html_pass(manifest, template, manifest.libraries)
    write_index(manifest)
    stage_container(manifest)
    print(f"Documentation written to {manifest.root / 'index.html'}")


if __name__ == "__main__":
    main()
