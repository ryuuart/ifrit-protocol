#!/usr/bin/env python3
"""Compile the documentation's names against the headers that own them.

Prose goes stale silently.  A document can name a type no header
declares, a member that never existed, or an initialiser that does not
compile, and nothing catches it — the reader does, later, by copying it
and failing to build.  A hand-written guard does not fix that: one that
has to be extended for each new passage covers only the passages someone
remembered to transcribe.

So this is the mechanical route.  It reads the markdown files it is
given, extracts every name an author could copy, and emits a C++
translation unit of probes that only builds if the headers still spell
those names that way.  Nothing is registered by hand: new documentation
joins the guard on the next build, and a rename the prose misses becomes
a build break rather than a confident wrong answer.  What each of the
four spellings is and how each is probed stands in the docprobes package
beside this file, a module per subject.

STATED LIMITATIONS — what this guard structurally cannot see:

  Unqualified names in prose and in code blocks.  A document that writes
      `padding(24_px)` — a bad argument to a real function — or invents
      a free function `px(float)` outside a header listing spells no
      qualified name, so the extractor has nothing to match and both
      errors pass unprobed.  Closing this would mean resolving an
      unqualified call the way a C++ compiler does (scopes,
      using-directives, ADL), i.e. writing a C++ parser, which this
      script deliberately is not.  Reviewers own that class of error;
      where practical, documents should spell names qualified so the
      guard can see them.

  Operator names.  The qualified-name pattern stops at the first
      character that cannot appear in an identifier, so `Spans::operator|`
      is captured only as far as `Spans::operator` and exempted by the
      operator-id rule — no member or free operator is ever probed.  The
      exemption is reported by name so the gap stays visible per
      document.

`--self-test` runs the generator against the small fixtures in
`docprobes/selftest.py`, without touching the real corpus — one name per
behaviour it must keep: resolve, fail, exempt-and-report, the bare names
of a header listing, a supplied alias, and the class-scope probe for an
external class.
"""

import argparse
import sys

from docprobes.generate import Generator, report_text
from docprobes.names import EXCLUDED_SPELLED
from docprobes.selftest import self_test


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--md", action="append")
    ap.add_argument("--include", action="append")
    ap.add_argument("--out")
    ap.add_argument("--report", default=None)
    ap.add_argument(
        "--library",
        default="",
        help="the include directory whose headers the probe TU includes "
        "wholesale, by its last path segment (sigilcompose, sigilmaterial); "
        "empty when the documents' library has no include root of its own, "
        "and then the preludes are all the TU opens",
    )
    ap.add_argument(
        "--namespace",
        default="sigil::docs",
        help="the namespace the probes are emitted inside, which is what "
        "lets a document spell a name the way its own readers do",
    )
    ap.add_argument(
        "--suite",
        default="Docs",
        help="the gtest suite the visible case is registered under",
    )
    ap.add_argument(
        "--exclude",
        action="append",
        help="a name the documents spell that no header can resolve, as "
        "name=reason: a symbol from a library whose include root is not "
        "given, or a spelling that is not C++ at all. The reason is "
        "printed in the coverage report, so an exemption stays visible",
    )
    ap.add_argument(
        "--prelude",
        action="append",
        help="a header the probe TU includes before the library's own, "
        "spelled as it would be written in an #include (with its quotes "
        "or angle brackets); defaults to gtest's, which the visible case "
        "needs",
    )
    ap.add_argument(
        "--alias",
        action="append",
        help="a namespace alias the probe TU opens with, as name=target, "
        "for a document that spells names through one",
    )
    ap.add_argument(
        "--skip-header",
        action="append",
        help="a header of the library, by file name, that the probe TU "
        "does NOT include: one behind an SDK or a UI toolkit, which "
        "compiles only where that dependency is",
    )
    ap.add_argument(
        "--floors",
        default=None,
        help="the five probe-count floors the visible case asserts, as "
        "usings,members,indexed,listed,designators — a corpus that yields "
        "none of a kind passes 0 for it",
    )
    ap.add_argument(
        "--self-test",
        action="store_true",
        help="run the generator's in-script fixtures (no --md/"
        "--include/--out needed): a real name must probe, "
        "an unreal one must fail, an operator spelling must "
        "be exempted and reported, and an EXTERNAL_CLASSES "
        "member must take the class-scope probe path",
    )
    args = ap.parse_args()

    if args.self_test:
        return self_test()
    for entry in args.exclude or []:
        name, _, reason = entry.partition("=")
        if not reason:
            ap.error("--exclude takes name=reason; %s states no reason" % name)
        EXCLUDED_SPELLED[name] = reason
    for entry in args.alias or []:
        name, separator, target = entry.partition("=")
        if not (separator and name and target):
            ap.error("--alias takes name=target; %s names no target" % entry)
    if not (args.md and args.include and args.out):
        ap.error("--md, --include and --out are required unless --self-test")

    floors = None
    if args.floors:
        floors = tuple(int(n) for n in args.floors.split(","))
        if len(floors) != 5:
            ap.error(
                "--floors takes five counts: usings,members,indexed,listed,designators"
            )
    gen = Generator(
        args.md,
        args.include,
        library=args.library,
        namespace=args.namespace,
        suite=args.suite,
        preludes=args.prelude,
        floors=floors,
        aliases=args.alias,
        skip_headers=args.skip_header,
    )
    gen.collect()
    with open(args.out, "w", encoding="utf-8") as f:
        gen.emit(f)

    text = report_text(gen, args.md)
    if args.report:
        open(args.report, "w", encoding="utf-8").write(text + "\n")
    print(text)
    if gen.unresolved:
        sys.stderr.write(
            "\nThe docs name %d thing(s) no header declares.  Either the doc is\n"
            "wrong (fix the doc — HEADERS WIN) or the name is documented on\n"
            "purpose, in which case add it to EXCLUDED with a reason.\n"
            % len(gen.unresolved)
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
