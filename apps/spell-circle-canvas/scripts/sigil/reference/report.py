"""The coverage report: every entity with no page, named.

A documentation layer rots by silence — a verb is added, nothing says
the verb has no page, and a year later the catalogue is a catalogue of
what somebody happened to write about. This prints the gap by name, so
adding a binding or a header declaration produces a line the next time
anyone builds the docs.

It is not a gate by default. Verification in this tree is one pass
before a push, not a check between edits, and a documentation report
that broke a library build would be exactly the thing that rule
forbids. `--strict` belongs in that one pass: it fails on a DROP in
coverage against the ledger, never on the standing gap.
"""

import json
import re
from pathlib import Path

from sigil import baseline
from sigil.reference import model, pages

LEDGER = "reference_coverage"

# Every finding, in the order a reader should act on them.
FINDINGS = (
    "no page",
    "no summary",
    "no example",
    "orphan page",
    "contradiction",
    "unresolved link",
    "stale front matter",
    "hand-written section",
    "unbound python",
)

# A page must not spell what the generator writes, because a hand-held
# copy can only go stale.
GENERATED_KEYS = ("accepts", "returns", "doxygen", "python", "header")

PYTHON_PATH = re.compile(r"\bsigil(?:\.[A-Za-z_]\w*)+")
PYTHON_BLOCK = re.compile(r"```python\n(.*?)```", re.DOTALL)


class Finding:
    def __init__(self, kind: str, where: str, detail: str = ""):
        self.kind = kind
        self.where = where
        self.detail = detail


def collect(run) -> dict:
    """Every finding, grouped by library."""
    found = {}

    def add(library: str, kind: str, where: str, detail: str = "") -> None:
        found.setdefault(library, []).append(Finding(kind, where, detail))

    for entity in run.catalogue.entities:
        where = f"{model.DIRECTORIES[entity.kind]}/{entity.slug()}"
        origin = f"{entity.header}:{entity.line}" if entity.header else ""
        if entity.page is None:
            bound = f"bound as {entity.python}" if entity.python else ""
            add(
                entity.library,
                "no page",
                where,
                " ".join(x for x in (origin, bound) if x),
            )
        elif not entity.summary():
            add(entity.library, "no summary", where, origin)
        if entity.page is not None and not entity.page.front.get("example"):
            add(entity.library, "no example", where, origin)
        if entity.page is not None:
            for key in GENERATED_KEYS:
                if key in entity.page.front:
                    add(
                        entity.library,
                        "stale front matter",
                        where,
                        f"the page spells `{key}:`, which the generator writes",
                    )
            _, written = pages.sections_of(entity.page.body)
            for title in pages.GENERATED.get(entity.kind, ()):
                if title in written:
                    add(
                        entity.library,
                        "hand-written section",
                        where,
                        f"`{title}` is written out, so it no longer follows the code",
                    )

    for library, path in run.orphans:
        add(library, "orphan page", str(path), "no entity of that kind and name")

    for binding in run.catalogue.unmatched:
        name = binding.owner or binding.module or "?"
        add(
            "Bindings",
            "contradiction",
            f"{name}.{binding.name}",
            f"bound at {Path(binding.source).name}:{binding.line}, "
            "and no C++ declaration of that name",
        )

    for spelling, _ in run.catalogue.unbound_python():
        add("Bindings", "unbound python", spelling, "declared in the stubs, no page")

    for library, entity, name in _python_names(run):
        add(library, "unresolved link", entity, f"`{name}` is not in the stub tree")

    for stem, why in run.examples.failures:
        add("Examples", "no example", stem, why)

    return found


def _python_names(run) -> list:
    """Every Sigil path a page spells, checked against the stubs."""
    found = []
    surface = run.catalogue.surface
    known = set(surface.spellings.values())
    for entity in run.catalogue.entities:
        page = entity.page
        if page is None:
            continue
        excluded = page.front.get("python_exclude") or []
        if isinstance(excluded, str):
            excluded = [excluded]
        for block in PYTHON_BLOCK.findall(page.body):
            for name in PYTHON_PATH.findall(block):
                if name in excluded or _known(name, known):
                    continue
                found.append((entity.library, str(page.path), name))
    return found


def _known(name: str, known: set) -> bool:
    while name.count(".") > 1:
        if name in known:
            return True
        name = name.rsplit(".", 1)[0]
    return name in known


def counts(run) -> dict:
    """The ledger's rows: one per library, so a narrowed run merges.

    A run over one library must be able to raise that library's count
    without discarding what it never looked at, which is the rule every
    other ledger in this tree keeps.
    """
    tally = {}
    for entity in run.catalogue.entities:
        row = tally.setdefault(
            entity.library, {"entities": 0, "pages": 0, "bound": 0, "values": 0}
        )
        row["entities"] += 1
        row["pages"] += entity.page is not None
        row["bound"] += bool(entity.python)
        row["values"] += entity.kind in (model.TYPE, model.ENUM)
    return tally


def total(tally: dict, key: str) -> int:
    return sum(row[key] for row in tally.values())


def write(run) -> int:
    """Prints the report, keeps the ledger, and judges it under --strict."""
    found = collect(run)
    for library in sorted(found):
        print(library)
        for finding in sorted(
            found[library], key=lambda one: (FINDINGS.index(one.kind), one.where)
        ):
            print(f"  {finding.kind:<18} {finding.where:<44} {finding.detail}")
    tally = counts(run)
    print(
        "Summary  "
        f"{total(tally, 'entities')} entities, {total(tally, 'pages')} pages, "
        f"{total(tally, 'entities') - total(tally, 'pages')} missing, "
        f"{total(tally, 'values')} values, "
        f"{total(tally, 'bound')} bound to Python, "
        f"{sum(len(held) for held in found.values())} findings"
    )
    return judge(run, tally)


def judge(run, tally: dict) -> int:
    """The ledger: a rise in coverage is written, a drop fails --strict.

    One row per library, merged over what stands, so a run that could
    not read every library's inventory raises the ones it read without
    discarding the ones it did not.

    The file carries no host and no moment. Every other ledger in this
    tree records both because its numbers are measurements of a
    machine; a page either exists or does not, on every machine at
    once, so a header naming this one would be churn in review and
    nothing else.
    """
    path = Path(run.templates) / f"{LEDGER}.json"
    held = baseline.load(path) or {}
    standing = held.get("coverage", {})
    merged = dict(standing)
    merged.update(tally)
    before = sum(row.get("pages", 0) for row in standing.values())
    after = sum(row.get("pages", 0) for row in merged.values())
    if after < before and run.options.strict:
        # The drop stays visible: adopting it by writing the lower
        # number is a deliberate act, not what a failing run does.
        print(f"coverage dropped by {before - after} pages")
        return 1
    path.write_text(
        json.dumps({"coverage": dict(sorted(merged.items()))}, indent=1) + "\n"
    )
    if after < before:
        print(f"coverage dropped by {before - after} pages")
    return 0
