#!/usr/bin/env python3
"""Resolve every sketch stem a document names against the registry.

A library's README points its reader at the sketches that draw the feature
beside it, and the stems it names are hand-typed.  A sketch renamed in the
registry leaves the prose pointing at a file that is not there, and nothing
notices: the document still reads correctly, the stem still looks like a
stem, and the reader learns otherwise by running the host and being told
there is no such sketch.

So the stems get the same treatment the compose README's API names already
get.  Every backticked snake_case token in a paragraph that is TALKING
ABOUT sketches must name a file under the sketches directory, and a token
that does not is either exempted here by name and reason or the run fails.

WHAT COUNTS AS A STEM.  `^[a-z][a-z0-9]*(_[a-z0-9]+)+$` inside backticks:
lower case, at least one underscore, no dots and no slashes, which is
exactly how the registry addresses a sketch and how a `--sketch` argument
spells one.  A single word is never a stem — `sketch`, `plate`, `raster`
are prose — and neither is anything carrying a path separator or an
extension, which names a file rather than a registry entry.

WHAT MAKES A PARAGRAPH ABOUT SKETCHES.  The paragraph — the block between
blank lines, which is the unit a reader takes a claim from — mentions a
sketch, a study or studies.  A snake_case token anywhere else is a field, a
CMake variable, a shader uniform or an environment name, and this check has
nothing to say about it.

WHAT IS EXEMPT, AND WHY IT IS NAMED.  A test, bench or probe target ends in
a suffix this file lists, and those are build targets rather than registry
entries.  Anything else that cannot resolve must be written down in
EXEMPT below with the reason it is not a sketch; the report prints every
exemption by name, so what the check deliberately does not resolve stays
visible instead of disappearing into a count.

THE COUNT RULE.  A cardinal that states the LENGTH of a list of studies
goes stale the moment the list grows, and it is maintained by hand in
lockstep with the list beside it — so it is refused, and the count is
deleted rather than maintained, since the list itself is the count.  A
cardinal immediately qualifying `studies`, `sketches` or `scenes` within
two lines of a named stem is refused when it is at least the number of
stems the paragraph names: equal to it, it is the length claim; above it,
the paragraph already promises more than it lists.  BELOW it the cardinal
is describing a subset — "two studies that hang an overlay", among four
named — which is prose about the items, not a count of them, and stays.

`--self-test` runs the checker against small in-script fixtures — one
document per behaviour it must keep: resolve, fail on a misspelt stem,
ignore a snake_case token outside a sketch paragraph, exempt a bench
target, and refuse a cardinal beside a list.
"""

import argparse
import os
import re
import sys

# A registry stem: lower case, at least one underscore, digits allowed.
STEM = re.compile(r"^[a-z][a-z0-9]*(?:_[a-z0-9]+)+$")

# Every backticked span in a line, code fences included: a stem is named in
# prose and in ```sh blocks alike.
BACKTICKED = re.compile(r"`([^`\n]+)`")

# What makes a paragraph one this check reads.
ABOUT = re.compile(r"\b(sketch|sketches|study|studies)\b", re.IGNORECASE)

# A build target rather than a registry entry.
TARGET_SUFFIXES = ("_test", "_bench", "_probe", "_probes", "_ledger")

# A cardinal qualifying a list of studies — the claim the tree contradicts
# as soon as the list grows.
CARDINAL = re.compile(
    r"\b(one|two|three|four|five|six|seven|eight|nine|ten|eleven|twelve|"
    r"thirteen|fourteen|fifteen|sixteen|seventeen|eighteen|nineteen|twenty|"
    r"\d+)\s+(studies|sketches|scenes)\b",
    re.IGNORECASE,
)

# Tokens that read as stems inside a sketch paragraph and name something
# else.  Each carries the reason it is not a registry entry.
EXEMPT = {
    "sketch_readme_stems": "a ctest case",
    "sketch_reload_runs_the_file": "a ctest case",
    "sketch_reload_materials": "a ctest case",
    "sketch_reload_surface": "a ctest case",
    "sigil_sketch": "the registration macro",
    "sigil_sketch_only": "a CMake cache variable",
    "sigil_sketch_dir": "a CMake cache variable",
    "sigil_sketch_asset_dir": "a CMake cache variable",
    "window_bench": "a host flag",
    "plate_ledger": "a script",
    "app_fps_ledger": "a script",
    "bench_ledger": "a script",
}


CARDINAL_WORDS = [
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
    "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
    "sixteen", "seventeen", "eighteen", "nineteen", "twenty",
]


def cardinal(word):
    """The number a cardinal spells, written or in digits."""
    word = word.lower()
    if word.isdigit():
        return int(word)
    return CARDINAL_WORDS.index(word)


def sketch_stems(sketch_dir):
    """Every stem the registry can address: one .cpp file, or one directory
    holding a sketch's own unit files."""
    stems = set()
    if not os.path.isdir(sketch_dir):
        return stems
    for entry in os.listdir(sketch_dir):
        path = os.path.join(sketch_dir, entry)
        if entry.endswith(".cpp"):
            stems.add(entry[: -len(".cpp")])
        elif os.path.isdir(path) and not entry.startswith("."):
            stems.add(entry)
    return stems


def paragraphs(text):
    """The document as (first line number, lines) blocks split on blank
    lines — the unit a reader takes one claim from."""
    out = []
    start = 1
    block = []
    for number, line in enumerate(text.splitlines(), start=1):
        if line.strip():
            if not block:
                start = number
            block.append((number, line))
        elif block:
            out.append((start, block))
            block = []
    if block:
        out.append((start, block))
    return out


class Check:
    """One run over a corpus: what resolved, what did not, what was
    exempted, and which counts were refused."""

    def __init__(self, stems):
        self.stems = stems
        self.resolved = []  # (doc, line, stem)
        self.unresolved = []  # (doc, line, stem)
        self.excluded = []  # (doc, line, token, reason)
        self.counts = []  # (doc, line, text)

    def read(self, doc, text):
        for _, block in paragraphs(text):
            if not ABOUT.search("\n".join(line for _, line in block)):
                continue
            named = []  # line numbers that carry a stem
            for number, line in block:
                for token in BACKTICKED.findall(line):
                    token = token.strip()
                    if not STEM.match(token):
                        continue
                    if token.endswith(TARGET_SUFFIXES):
                        self.excluded.append(
                            (doc, number, token, "a test, bench or probe target")
                        )
                        continue
                    if token in EXEMPT:
                        self.excluded.append((doc, number, token, EXEMPT[token]))
                        continue
                    if token in self.stems:
                        self.resolved.append((doc, number, token))
                        named.append(number)
                    else:
                        self.unresolved.append((doc, number, token))
                        named.append(number)
            if not named:
                continue
            for number, line in block:
                if not any(abs(number - at) <= 2 for at in named):
                    continue
                match = CARDINAL.search(line)
                if match and cardinal(match.group(1)) >= len(named):
                    self.counts.append((doc, number, match.group(0)))

    def failures(self):
        return len(self.unresolved) + len(self.counts)


def report(check, out=sys.stdout):
    def w(text):
        out.write(text + "\n")

    w("sketch stems: %d resolved, %d exempt" % (len(check.resolved), len(check.excluded)))
    for doc, line, token, reason in check.excluded:
        w("  exempt   %s:%d  `%s` — %s" % (doc, line, token, reason))
    for doc, line, token in check.unresolved:
        w(
            "  MISSING  %s:%d  `%s` names no sketch under the sketches "
            "directory" % (doc, line, token)
        )
    for doc, line, text in check.counts:
        w(
            '  COUNT    %s:%d  "%s" beside a list of studies — delete the '
            "count, the list is the count" % (doc, line, text)
        )
    return check.failures() == 0


FIXTURE_STEMS = {"blur_falloff", "tile_map", "eva_magi_defense"}


def fixture_check(text):
    check = Check(FIXTURE_STEMS)
    check.read("fixture.md", text)
    return check


def self_test():
    failures = []

    def check(ok, what):
        print("  %s  %s" % ("ok " if ok else "FAIL", what))
        if not ok:
            failures.append(what)

    print("readme_sketch_stems --self-test")

    c = fixture_check("The `blur_falloff` study draws the falloff.\n")
    check(
        [s for _, _, s in c.resolved] == ["blur_falloff"] and not c.unresolved,
        "a named stem in a sketch paragraph resolves",
    )

    c = fixture_check("The `blur_faloff` study draws the falloff.\n")
    check(
        [s for _, _, s in c.unresolved] == ["blur_faloff"] and c.failures() == 1,
        "a misspelt stem is reported and the run fails",
    )

    c = fixture_check("Set `max_sigma` on the effect and it blurs.\n")
    check(
        not c.resolved and not c.unresolved,
        "a snake_case token outside a sketch paragraph is not a stem",
    )

    c = fixture_check("The `compose_core_test` case covers the sketch.\n")
    check(
        any(t == "compose_core_test" for _, _, t, _ in c.excluded)
        and not c.unresolved,
        "a test target is exempted with a recorded reason",
    )

    c = fixture_check("The two studies `blur_falloff` and `tile_map` draw it.\n")
    check(
        len(c.counts) == 1 and c.failures() == 1,
        "a cardinal qualifying a list of studies is refused",
    )
    check(
        "two studies" in "\n".join(t for _, _, t in c.counts),
        "the refused count is quoted in the report",
    )

    c = fixture_check("A `tile_map` study.\n\nThe four scenes are chunked.\n")
    check(
        not c.counts,
        "a cardinal in another paragraph is not beside the list",
    )

    c = fixture_check(
        "Of `blur_falloff`, `tile_map` and `eva_magi_defense`, the two "
        "studies that bake are the last.\n"
    )
    check(
        not c.counts and len(c.resolved) == 3,
        "a cardinal below the list's length describes a subset and stays",
    )

    print("%d failure(s)" % len(failures))
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--doc", action="append", default=[],
                        help="a markdown document to read; repeatable")
    parser.add_argument("--sketch-dir", help="the directory the registry addresses")
    parser.add_argument("--self-test", action="store_true",
                        help="run the in-script fixtures and exit")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    if not args.doc or not args.sketch_dir:
        parser.error("--doc and --sketch-dir are required without --self-test")

    stems = sketch_stems(args.sketch_dir)
    if not stems:
        print("no sketches found under %s" % args.sketch_dir, file=sys.stderr)
        return 1
    check = Check(stems)
    for doc in args.doc:
        with open(doc, encoding="utf-8") as f:
            check.read(os.path.relpath(doc), f.read())
    return 0 if report(check) else 1


if __name__ == "__main__":
    sys.exit(main())
