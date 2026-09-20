"""The site: the shell every page stands in, the indexes, and the search.

Static files only. The generated tree is served by nginx from a
container and opened straight off the disk just as often, so every link
is relative and nothing is computed at request time. The search index
is one JSON file, which the serving rules already compress.
"""

import html
import json
import shutil
from pathlib import Path

from sigil.reference import markdown, model

SEARCH = "search-index.json"
# The same rows as a script that assigns them. A page opened straight
# off the disk cannot fetch a file beside it — the browser refuses a
# cross-origin read from `file://` — and the generated tree is opened
# that way as often as it is served, so the search loads the script and
# the JSON stands beside it as the data.
SEARCH_SCRIPT = "search-index.js"
STYLE = "reference.css"
SCRIPT = "reference.js"

NAVIGATION = (
    ("Overview", "overview/index.html"),
    ("Guides", "guides/index.html"),
    ("Reference", "reference/index.html"),
    ("Values", "values/index.html"),
    ("Glossary", "glossary.html"),
    ("Literal API", "index.html"),
)


def write_if_changed(path: Path, text: str) -> None:
    if path.exists() and path.read_text() == text:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


def value_page(qualified: str) -> str:
    """The one page a value has, wherever in the tree it is declared."""
    return f"values/{qualified.replace('::', '.')}.html"


class Shell:
    """The one page template, filled per page."""

    def __init__(self, root: Path, values: dict):
        self.root = root
        self.values = values

    def links(self, depth: int) -> markdown.Links:
        return markdown.Links(depth, self.values)

    def write(
        self,
        path: str,
        title: str,
        body: str,
        *,
        lede: str = "",
        badge: str = "",
        trail: list | None = None,
        aside: str = "",
    ) -> None:
        depth = path.count("/")
        up = "../" * depth
        navigation = "".join(
            f'<a href="{up}{href}">{html.escape(name)}</a>' for name, href in NAVIGATION
        )
        crumbs = "".join(
            f'<a href="{up}{href}">{html.escape(name)}</a>'
            for name, href in (trail or [])
        )
        badged = f'<span class="badge">{html.escape(badge)}</span>' if badge else ""
        write_if_changed(
            self.root / path,
            f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{html.escape(title)} — SpellCircle</title>
<link rel="stylesheet" href="{up}{STYLE}">
<script>window.referenceRoot = "{up}";</script>
</head><body>
<header class="masthead">
  <a class="wordmark" href="{up}index.html">SpellCircle</a>
  <nav class="primary">{navigation}</nav>
  <form class="find" role="search"><input type="search" id="find"
    placeholder="Search — try accepts:Fill or kind:verb" autocomplete="off"></form>
</header>
<div id="results" hidden></div>
<main>
  <nav class="trail">{crumbs}</nav>
  <h1>{html.escape(title)}{badged}</h1>
  {f'<p class="lede">{lede}</p>' if lede else ""}
  {body}
</main>
{f'<aside class="sidecar">{aside}</aside>' if aside else ""}
<script src="{up}{SCRIPT}"></script>
</body></html>
""",
        )


class Index:
    """The rows one index page shows, and the JSON search reads."""

    def __init__(self):
        self.rows = []

    def add(self, entity: model.Entity, url: str, accepts: list, returns: str) -> None:
        self.rows.append(
            {
                "name": entity.name,
                "kind": entity.kind,
                "library": entity.library,
                "group": entity.group,
                "python": entity.python,
                "summary": entity.summary(),
                "url": url,
                "accepts": accepts,
                "returns": returns,
                "state": entity.binding_state,
            }
        )

    def value(self, qualified: str, made: int, taken: int, library: str) -> None:
        self.rows.append(
            {
                "name": qualified.rsplit("::", 1)[-1],
                "kind": "value",
                "library": library,
                "group": "",
                "python": "",
                "summary": f"{made} ways to make one, taken in {taken} places",
                "url": value_page(qualified),
                "accepts": [],
                "returns": qualified,
                "state": "",
            }
        )

    def write(self, root: Path) -> None:
        body = json.dumps(self.rows, separators=(",", ":"), sort_keys=True)
        write_if_changed(root / SEARCH, body)
        write_if_changed(root / SEARCH_SCRIPT, f"window.referenceIndex = {body};\n")


def stage(templates: Path, root: Path) -> None:
    """The layer's own stylesheet and script, beside the Doxygen sites."""
    root.mkdir(parents=True, exist_ok=True)
    for name in (STYLE, SCRIPT):
        source = templates / name
        if source.exists():
            shutil.copyfile(source, root / name)
