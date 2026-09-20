"""THE GENERATOR'S OWN FIXTURES: one markdown snippet against one small
header apiece, pinning a behaviour the generator must keep — a real name
yields a probe, an unreal one fails the run, an operator spelling is
exempted AND reported, a header listing's bare names are checked against
the header they are listed under, a supplied alias leads a qualified
name, an external class's member takes the class-scope probe path, a
class whose base clause wraps is still a class, and a member a class
inherits is checked as that class's own.

The generator is the layer that decides what gets probed at all, so a
regression here would fail no C++ build — it would silently narrow the
guard, which is exactly the failure the guard exists to prevent.
"""

import io
import os
import tempfile

from .generate import Generator, report_text

FIXTURE_HEADER = """\
namespace fix {
struct Widget {
  int knob;
};
void spin();
}
"""

# A node that takes its verbs from mixins over itself. `Knob` names its
# one base on the line of its own name; `Dial` names two, and the list
# wraps, which is how every class with more than one family is written.
MIXIN_HEADER = """\
namespace fix {
template <class Derived>
class TurnVerbs {
 public:
  Derived& turn(float degrees);
  Derived& turn(float degrees, float about);
};
template <class Derived>
class PressVerbs {
 public:
  Derived& press();
};
class Knob : public TurnVerbs<Knob> {
 public:
  int detents;
};
class Dial : public TurnVerbs<Dial>,
             public PressVerbs<Dial> {
 public:
  int detents;
};
}
"""


def fixture_generator(md_text, aliases=None, header=FIXTURE_HEADER):
    """A Generator run over one in-memory markdown fixture and one fixture
    header, in a temp dir so nothing on disk is touched."""
    with tempfile.TemporaryDirectory() as tmp:
        incdir = os.path.join(tmp, "fixinc")
        os.makedirs(incdir)
        with open(os.path.join(incdir, "Fixture.h"), "w", encoding="utf-8") as f:
            f.write(header)
        md = os.path.join(tmp, "fixture.md")
        with open(md, "w", encoding="utf-8") as f:
            f.write(md_text)
        gen = Generator([md], [incdir], aliases=aliases)
        gen.collect()
        return gen


def self_test():
    failures = []

    def check(ok, what):
        print("  %s  %s" % ("ok " if ok else "FAIL", what))
        if not ok:
            failures.append(what)

    print("api_doc_probes --self-test")

    # A qualified name the headers declare produces a probe.
    gen = fixture_generator("Call `fix::spin` to spin the widget.\n")
    check(
        any(s == "fix::spin" for _, s, _, _ in gen.usings) and not gen.unresolved,
        "existing namespace-scope name -> using-probe emitted",
    )
    gen = fixture_generator("Read `Widget::knob` before spinning.\n")
    check(
        any(s == "Widget::knob" for _, _, s, _, _ in gen.members)
        and not gen.unresolved,
        "existing member name -> member probe emitted",
    )

    # A qualified name no header declares makes the run FAIL (main exits
    # non-zero on any unresolved name).
    gen = fixture_generator("Then call `Nonexistent::field` at will.\n")
    check(
        any(s == "Nonexistent::field" for s, _, _ in gen.unresolved),
        "unknown type -> reported unresolved, generator fails",
    )

    # An operator spelling cannot be probed: the identifier pattern stops at
    # `|`, so the name arrives truncated and is exempted by the operator-id
    # rule.  The exemption must be REPORTED by name, not silently counted.
    gen = fixture_generator("Union them with `Spans::operator|`.\n")
    check(
        any(s == "Spans::operator" and "operator-id" in r for s, _, r in gen.excluded)
        and not gen.unresolved,
        "member-operator spelling -> exempted with a recorded reason",
    )
    check(
        "exempt" in report_text(gen, ["fixture.md"])
        and "Spans::operator" in report_text(gen, ["fixture.md"]),
        "operator exemption is listed by name in the report",
    )

    # A header-listing bullet resolves its BARE names against the header it
    # names — the one place an unqualified name says what owns it.
    gen = fixture_generator("- `Fixture.h` — the widget: `Widget`, `knob`, `spin`.\n")
    check(
        {s for s, _, _ in gen.listed} == {"Widget", "knob", "spin"}
        and not gen.unresolved,
        "bare names in a header listing -> checked against that header",
    )
    gen = fixture_generator("- `Fixture.h` — the widget: `Widget`, `wobble`.\n")
    check(
        any(s == "wobble" for s, _, _ in gen.unresolved),
        "a listed name no header spells -> reported unresolved, generator fails",
    )

    # An alias supplied on the command line names a namespace this scanner
    # may never have scanned, so it has to lead a qualified name the way a
    # scanned namespace does — and the translation unit has to declare it
    # before the probe that spells it.
    gen = fixture_generator("Call `parts::spin` to spin it.\n", aliases=["parts=fix"])
    check(
        any(s == "parts::spin" for _, s, _, _ in gen.usings) and not gen.unresolved,
        "supplied alias -> qualified name resolves through it",
    )
    buf = io.StringIO()
    gen.emit(buf)
    emitted = buf.getvalue()
    check(
        "namespace parts = fix;" in emitted
        and emitted.index("namespace parts = fix;")
        < emitted.index("using parts::spin;"),
        "the alias is declared before the probe that spells it",
    )
    gen = fixture_generator("Call `parts::spin` to spin it.\n")
    check(
        any(s == "parts::spin" for s, _, _ in gen.unresolved),
        "the same name without the alias -> reported unresolved",
    )

    # An EXTERNAL_CLASSES member takes the class-scope probe path: a derived
    # struct with a class-scope using-declaration, plus the include that
    # declares the base.
    gen = fixture_generator("Blur it with `SkImageFilters::Blur(...)`.\n")
    check(
        any(c == "SkImageFilters" and m == "Blur" for c, m, _, _, _ in gen.class_usings)
        and not gen.unresolved,
        "EXTERNAL_CLASSES member -> class-scope probe collected",
    )
    buf = io.StringIO()
    gen.emit(buf)
    emitted = buf.getvalue()
    check(
        "struct Probe : SkImageFilters { using SkImageFilters::Blur; }" in emitted
        and "#include <include/effects/SkImageFilters.h>" in emitted,
        "class-scope probe and its include are emitted",
    )

    # A base clause that wraps onto a second line still opens the class:
    # read as a forward declaration, the class would own no member and
    # every name a document spells on it would be unresolved.
    gen = fixture_generator("Count `Dial::detents` first.\n", header=MIXIN_HEADER)
    check(
        any(s == "Dial::detents" for _, _, s, _, _ in gen.members)
        and not gen.unresolved,
        "a class whose base clause wraps -> its own member is probed",
    )

    # A member a class inherits is that class's own to a document, and an
    # overloaded one cannot be named by a probe, so it is checked against
    # the index — under the class the document spelled, on either line of
    # a wrapped base clause.
    gen = fixture_generator("Turn it with `Knob::turn`.\n", header=MIXIN_HEADER)
    check(
        any(s == "Knob::turn" for s, _, _ in gen.index_checked)
        and not gen.members
        and not gen.unresolved,
        "an overloaded member from a base -> index-checked on the derived class",
    )
    gen = fixture_generator(
        "`Dial::turn`, then `Dial::press`.\n", header=MIXIN_HEADER
    )
    check(
        {s for s, _, _ in gen.index_checked} == {"Dial::turn", "Dial::press"}
        and not gen.unresolved,
        "every base of a wrapped clause hands its members to the class",
    )
    gen = fixture_generator("Then `Knob::press` it.\n", header=MIXIN_HEADER)
    check(
        not gen.index_checked,
        "a member of a base the class does not name -> not credited to it",
    )

    if failures:
        print("self-test: %d failure(s)" % len(failures))
        return 1
    print("self-test: all fixtures pass")
    return 0
