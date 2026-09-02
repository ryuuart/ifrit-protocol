#!/usr/bin/env python3
"""Compile the documentation's names against the headers that own them.

Prose goes stale silently.  A document can name a type no header declares, a
member that never existed, or an initialiser that does not compile, and
nothing catches it — the reader does, later, by copying it and failing to
build.  A hand-written guard does not fix that: one that has to be extended
for each new passage covers only the passages someone remembered to
transcribe.

So this is the mechanical route.  It reads the markdown files it is given,
extracts every qualified name an author could copy — from the ```cpp blocks
AND from the inline `code` spans, because prose carries as many names as
the blocks do — and emits a C++ translation unit of probes that only builds
if the headers still spell those names that way.  Nothing is registered by
hand: new documentation joins the guard on the next build, and a rename the
prose misses becomes a build break rather than a confident wrong answer.

Three probe forms, chosen by what the name is:

  namespace-scope entity   `shapes::polygon`   -> `using shapes::polygon;`
      A using-declaration is the one spelling that works uniformly for
      functions (including overload sets), types, variables, namespaces and
      enumerators, and it is a hard error when the name does not exist.

  class member             `PathFormat::effect` -> a concept disjunction
      `using` cannot name a non-static data member outside a derived class,
      so members are probed through `requires`, which covers enumerators,
      static and non-static data, nested types and member functions.  An
      OVERLOADED member function is the one thing no `requires` spelling can
      name — every way of writing it has to resolve the overload set, and is
      therefore ill-formed — so those fall back to the header index this
      file builds, in Python, at build time.

  designated initialiser   `PathFormat{.effect = …}` -> `T{.effect = Any{}}`
      This form needs its own probe for two independent reasons.  It never
      spells `PathFormat::effect` anywhere, so the qualified-name scan does
      not see it at all.  And it asks a STRICTER question than existence:
      the field must be a DATA member.  `PathFormat{.paint = …}` names the
      real member FUNCTION `paint`, so every existence probe answers yes
      while the initialiser still does not compile.  Only probing the
      initialiser itself answers what the document claims.

Names that CANNOT resolve are not silently dropped.  Every one is either
exempted — by an exclusion table below or by the operator-id rule, both of
which record a reason — or it is reported as an unresolved documented name
and the generator FAILS.  That inversion is the whole point: a new passage
of prose, or a type renamed out from under it, breaks the build rather than
quietly leaving the guard.  Every exemption is listed by name and reason in
the coverage report, so what the guard deliberately does not check is
visible rather than folded into a count.

STATED LIMITATIONS — what this guard structurally cannot see:

  Unqualified names.  A document that writes `padding(24_px)` — a bad
      argument to a real function — or invents a free function `px(float)`
      spells no qualified name, so the extractor has nothing to match and
      both errors pass unprobed.  Closing this would mean resolving an
      unqualified call the way a C++ compiler does (scopes, using-directives,
      ADL), i.e. writing a C++ parser, which this script deliberately is
      not.  Reviewers own that class of error; where practical, documents
      should spell names qualified so the guard can see them.

  Operator names.  The qualified-name pattern stops at the first character
      that cannot appear in an identifier, so `Spans::operator|` is captured
      only as far as `Spans::operator` and exempted by the operator-id rule
      — no member or free operator is ever probed.  The exemption is
      reported by name so the gap stays visible per document.

`--self-test` runs the generator against small in-script fixtures — one
name per behaviour it must keep: resolve, fail, exempt-and-report, and the
class-scope probe for EXTERNAL_CLASSES — without touching the real corpus.
"""

import argparse
import io
import os
import re
import sys
import tempfile

ID = r"[A-Za-z_][A-Za-z0-9_]*"
QUAL = re.compile(r"\b(" + ID + r"(?:::" + ID + r")+)")
# `Type{.field = …` / `Type{.field,` — a designated initialiser.  It needs
# its own pattern because it names no member in qualified form, so QUAL
# never sees it.
DESIG = re.compile(r"\b(" + ID + r"(?:::" + ID + r")*)\s*\{\s*\.(" + ID + r")")
DESIG_MORE = re.compile(r"[,{]\s*\.(" + ID + r")\s*(?:=|,|\})")

# Namespace names this generator cannot scrape, because the headers that
# declare them are not on the include path it is given.  Every other
# namespace comes from the scanned headers themselves.
#
# A name whose components are ALL namespaces has nothing to probe and is
# passed over; a missing entry here instead makes the first unrecognised
# component look like a type, and the name is reported unresolved.
NS_EXTERNAL = {
    # The standard library, its sub-namespaces the documents spell, and the
    # animation library compose exposes in its own signatures.  `ch` is the
    # short alias, which the generated translation unit declares to match.
    # Nested inline namespaces count: a document writing `using namespace
    # std::chrono_literals` names one, and a using-DECLARATION probe on a
    # namespace is ill-formed, so it has to be recognised here.
    "std",
    "chrono",
    "chrono_literals",
    "literals",
    "ranges",
    "views",
    "choreograph",
    "ch",
    # Skia's actual namespaces, which are Sk-prefixed like its types and
    # would otherwise be probed as if they were classes.
    "SkSurfaces",
    "SkShaders",
    "SkImages",
    "SkPathEffects",
    # SigilMaterial, whose include path is deliberately NOT given: scanning
    # it would put every type it declares into the candidate set a member
    # probe ORs over, and one of those (`ProgramCache::Key`) is a PRIVATE
    # nested type, which this scanner cannot see and which is a hard error
    # the moment it is named.  The documents spell only namespace-scope
    # names from it, and those probe correctly from here.
    "material",
}

# Skia's static-factory aggregates that READ like namespaces but are CLASSES
# (`class SK_API SkImageFilters { static … }`).  This table is how such a
# name gets probed at all, not a way of skipping it: a namespace-scope
# using-declaration cannot name a class member and is ill-formed if it tries,
# while a DERIVED-CLASS using-declaration names one uniformly, overload sets
# included — the same one-spelling rule the namespace probe follows, one
# scope over.  Each maps to the Skia header that declares it, included only
# when a probe needs it.
EXTERNAL_CLASSES = {
    "SkImageFilters": "include/effects/SkImageFilters.h",
    "SkColorFilters": "include/core/SkColorFilter.h",
    "SkGradientShader": "include/effects/SkGradientShader.h",
    "SkFontMgr": "include/core/SkFontMgr.h",
}

# Whole spellings a document names on purpose that no header resolves —
# a symbol owned by a library this generator does not scan, or a name a
# document writes in order to say it does NOT exist.  Keyed by the exact
# qualified name, never by a component, so an exclusion can never widen
# silently to a sibling that SHOULD be probed.  Each entry states why that
# name cannot resolve; an entry with no such reason is a hole in the guard.
EXCLUDED_SPELLED = {}

# Single components that make any qualified name containing them
# unprobeable: an example's own host type, a placeholder standing in for a
# type the reader supplies, a template parameter recited from a header.
# Matched per component, which is why these spellings must be distinctive —
# a common word here silently exempts every name that contains it.
EXCLUDED = {}

# (type, member) pairs no probe form can SEE, as distinct from members that
# do not exist: overloaded member functions of types outside the scanned
# headers.  No `requires` spelling can name an overload set, and the header
# index below only covers headers this generator scans, so neither route
# reaches them.
UNPROBEABLE_MEMBERS = {}

# (type, member) pairs where the type is real, is scanned, and genuinely has
# no such member — a spelling a document writes to record that it is gone.
#
# NOTHING here may name a WRONG spelling of something that still exists in
# another form.  Such an entry disarms the guard for exactly the mistake it
# exists to catch: `PathFormat{.paint = …}` is wrong because `paint` is a
# member FUNCTION, and an entry for ("PathFormat", "paint") would turn that
# defect into a pass.  Prose that warns against a spelling should write it
# unqualified (`paint`), which is not a probed form and needs no entry at
# all.
EXCLUDED_MEMBERS = {}


def code_regions(path):
    """[(line, text, kind)] — every ```cpp line and every inline `span`."""
    out = []
    fence = None
    for i, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        if line.startswith("```"):
            fence = None if fence is not None else line[3:].strip()
            continue
        if fence is not None:
            if fence.startswith("cpp"):
                out.append((i, line, "block"))
        else:
            for m in re.finditer(r"`([^`]+)`", line):
                out.append((i, m.group(1), "inline"))
    return out


def strip_comments(text):
    return re.sub(r"/\*.*?\*/", "", re.sub(r"//.*", "", text))


HEADER_TOKENS = re.compile(
    r"(?P<ns>\bnamespace\s+(?P<nsname>[A-Za-z_][A-Za-z0-9_:]*)\s*\{)"
    r"|(?P<alias>\bnamespace\s+(?P<aname>[A-Za-z_][A-Za-z0-9_]*)\s*=)"
    r"|(?P<agg>\b(?:struct|class|enum\s+class|enum\s+struct|enum)\s+"
    r"(?P<aggname>[A-Za-z_][A-Za-z0-9_]*)\b(?P<tail>[^;{\n]*)(?P<open>\{)?)"
    r"|(?P<fn>\b[A-Za-z_][A-Za-z0-9_]*)\s*\("
    r"|(?P<open2>\{)|(?P<close>\})"
)


def scan_headers(incdirs):
    """(namespace leaf names, {type name: [fully qualified spellings]}).

    Scraped from the headers so the guard's own idea of what exists follows
    the headers automatically — the same rule the documents are held to.
    Scope is tracked by real brace depth, not by the `} // namespace`
    convention: a header that closes a namespace without the comment would
    otherwise mis-qualify every type after it.
    """
    namespaces = set()
    types = {}
    ns_paths = {}
    funcs = {}  # simple type name -> names declared with a ( in its body
    for incdir in incdirs:
        for root, _, files in os.walk(incdir):
            for name in sorted(files):
                if not name.endswith(".h"):
                    continue
                text = strip_comments(
                    open(os.path.join(root, name), encoding="utf-8").read()
                )
                text = re.sub(r'"(?:[^"\\]|\\.)*"', '""', text)
                depth = 0
                scope = []  # (depth_at_open, [namespace parts])
                for m in HEADER_TOKENS.finditer(text):
                    if m.group("alias"):
                        namespaces.add(m.group("aname"))
                    elif m.group("ns"):
                        parts = m.group("nsname").split("::")
                        namespaces.update(parts)
                        scope.append((depth, parts, False))
                        depth += 1
                        outer = [p for _, ps, _c in scope for p in ps]
                        for i in range(len(outer)):
                            ns_paths.setdefault(outer[i], set()).add(
                                "::".join(outer[: i + 1])
                            )
                    elif m.group("agg"):
                        if m.group("open"):  # a definition
                            qual = "::".join(p for _, ps, _c in scope for p in ps)
                            name_ = m.group("aggname")
                            types.setdefault(name_, set()).add(
                                (qual + "::" + name_) if qual else name_
                            )
                            # A class is a scope too: `Composer::CacheState`
                            # must not come out as `sigil::compose::CacheState`.
                            scope.append((depth, [name_], True))
                            depth += 1
                    elif m.group("fn"):
                        # Only names declared DIRECTLY in a class body — not
                        # calls inside an inline function — so the index means
                        # "this class declares a member function of that name".
                        if scope and scope[-1][2] and scope[-1][0] == depth - 1:
                            funcs.setdefault(scope[-1][1][0], set()).add(m.group("fn"))
                    elif m.group("open2"):
                        depth += 1
                    else:
                        depth -= 1
                        while scope and scope[-1][0] >= depth:
                            scope.pop()
    return (
        namespaces,
        {k: sorted(v) for k, v in types.items()},
        {k: sorted(v) for k, v in ns_paths.items()},
        {k: sorted(v) for k, v in funcs.items()},
    )


def resolve_type(name, types):
    """Fully qualified candidates for a type name as a document spells it."""
    if name in types:
        return ["::" + q for q in types[name]]
    if name.startswith("Sk") or name.startswith("Gr"):
        return ["::" + name]  # Skia lives at global scope
    return []


class Generator:
    def __init__(self, mds, incdirs):
        self.mds = mds
        (self.namespaces, self.types, self.ns_paths, self.funcs) = scan_headers(incdirs)
        self.namespaces |= NS_EXTERNAL
        self.headers = []
        for incdir in incdirs:
            base = os.path.basename(os.path.normpath(incdir))
            if base != "sigilcompose":
                continue
            for root, _, files in os.walk(incdir):
                for name in sorted(files):
                    if not name.endswith(".h") or name == "Web.h":
                        continue  # Web.h is Ultralight-gated
                    rel = os.path.relpath(
                        os.path.join(root, name), os.path.dirname(incdir)
                    )
                    self.headers.append(rel.replace(os.sep, "/"))
        self.usings = []  # (qualified, line, kind)
        self.class_usings = []  # (class, member, spelled, line, kind)
        self.members = []  # (candidates, chain, spelled, line, kind)
        self.designators = []  # (candidates, field, spelled, line, kind)
        self.excluded = []  # (spelled, line, reason)
        self.unresolved = []  # (spelled, line, why)
        self.index_checked = []  # (spelled, line, kind) — member fns

    def collect(self):
        seen_q, seen_m = {}, {}
        for md in self.mds:
            doc = os.path.basename(md)
            for line, text, kind in code_regions(md):
                where = "%s:%d" % (doc, line)
                text = strip_comments(text)
                for m in QUAL.finditer(text):
                    self.qualified(m.group(1), where, kind, seen_q)
                for m in DESIG.finditer(text):
                    tail = text[m.start() :]
                    fields = [m.group(2)] + DESIG_MORE.findall(tail)
                    for field in fields:
                        self.designated(m.group(1), field, where, kind, seen_m)

    def expand(self, prefix):
        """Leaf namespace spelling -> the header's own full path for it."""
        if not prefix:
            return []
        head = prefix[0]
        paths = self.ns_paths.get(head)
        if paths and len(paths) == 1 and paths[0] != head:
            return paths[0].split("::") + prefix[1:]
        return list(prefix)

    def excluded_hit(self, spelled, line):
        if spelled in EXCLUDED_SPELLED:
            self.excluded.append((spelled, line, EXCLUDED_SPELLED[spelled]))
            return True
        for part in spelled.split("::"):
            if part in EXCLUDED:
                self.excluded.append((spelled, line, EXCLUDED[part]))
                return True
        return False

    def qualified(self, spelled, line, kind, seen):
        if spelled in seen:
            return
        seen[spelled] = line
        parts = spelled.split("::")
        if any(p == "operator" for p in parts):
            self.excluded.append((spelled, line, "operator-id, not a probeable name"))
            return
        if self.excluded_hit(spelled, line):
            return
        i = 0
        while i < len(parts) and parts[i] in self.namespaces:
            i += 1
        if i >= len(parts):
            return  # a namespace, nothing to probe
        if i == len(parts) - 1:
            # Documents write the LEAF namespace (`shapes::polygon`), which
            # is how it reads under `using namespace sigil::compose`; the
            # probe has to spell the path the headers actually put it on.
            self.usings.append(
                (self.expand(parts[:i]) + [parts[i]], spelled, line, kind)
            )
            return
        if parts[i] in EXTERNAL_CLASSES:
            # Probed through a derived-class using-declaration (see
            # EXTERNAL_CLASSES): the first member level is what the doc's
            # reader would copy, and the one thing a class-scope using can
            # name uniformly — overload sets included.
            self.class_usings.append((parts[i], parts[i + 1], spelled, line, kind))
            return
        # parts[i] names a type; the rest is a member chain.
        cands = resolve_type(parts[i], self.types)
        if not cands:
            self.unresolved.append(
                (spelled, line, "no header declares type " + parts[i])
            )
            return
        self.member(cands, parts[i], "::".join(parts[i + 1 :]), spelled, line, kind)

    def designated(self, typename, field, line, kind, seen):
        key = (typename, field)
        if key in seen:
            return
        seen[key] = line
        leaf = typename.split("::")[-1]
        if (leaf, field) in EXCLUDED_MEMBERS:
            self.excluded.append(
                (typename + "{." + field, line, EXCLUDED_MEMBERS[(leaf, field)])
            )
            return
        if self.excluded_hit(typename, line) or field in EXCLUDED:
            return
        if not leaf[0].isupper():
            return  # `ns::fn({.a = …})` etc.
        cands = resolve_type(leaf, self.types)
        if not cands:
            self.unresolved.append(
                (typename + "{." + field, line, "no header declares type " + leaf)
            )
            return
        self.designators.append(
            (cands, field, typename + "{." + field + " = …}", line, kind)
        )

    def member(self, cands, typename, chain, spelled, line, kind):
        """Route one Type::member reference to the probe form that can see it.

        A `requires` expression cannot name an OVERLOADED member function —
        every spelling of it (`T::m`, `&T::m`, `v.m`) has to resolve the
        overload set and is therefore ill-formed.  Those names are checked
        against the header index instead, which is the same headers-win rule
        enforced one layer up, in Python, at build time.
        """
        if (typename, chain) in EXCLUDED_MEMBERS:
            self.excluded.append((spelled, line, EXCLUDED_MEMBERS[(typename, chain)]))
            return
        if "::" not in chain and chain in self.funcs.get(typename, ()):
            self.index_checked.append((spelled, line, kind))
            return
        if (typename, chain) in UNPROBEABLE_MEMBERS:
            self.excluded.append(
                (spelled, line, UNPROBEABLE_MEMBERS[(typename, chain)])
            )
            return
        self.members.append((cands, chain, spelled, line, kind))

    def emit_designators(self, w):
        """A designator must name a DATA member, which is a stricter claim
        than `Type::field` resolving: `PathFormat{.paint = …}` names the
        real member FUNCTION `paint`, so every member-existence form says
        yes and the initialiser still does not compile.  Probing the
        initialiser itself is the only form that answers the question the
        document asks."""
        if not self.designators:
            return
        w(
            "// A universal source value, so the probe tests the DESIGNATOR and\n"
            "// not the type of whatever the document assigned to it.\n"
            "struct AnyInit { template <class U> operator U() const; };\n\n"
        )
        for n, (cands, field, spelled, line, kind) in enumerate(self.designators):
            w(
                "template <class T> concept DI%d = requires { T{.%s = AnyInit{}}; };\n"
                % (n, field)
            )
            expr = " || ".join("DI%d<%s>" % (n, c) for c in cands)
            msg = "%s spells `%s`; the header has no such data member" % (line, spelled)
            w('static_assert(%s,\n              "%s");\n\n' % (expr, msg))

    def emit(self, out):
        w = out.write
        w("// GENERATED by test/docs/api_doc_probes.py — DO NOT EDIT.\n")
        w(
            "// Sources: %s.  Rebuilt whenever they change.\n"
            % ", ".join(os.path.basename(m) for m in self.mds)
        )
        w(
            "//\n// Every qualified name and designated-initialiser field these\n"
            "// docs spell, compiled against the headers that own them.  A failure\n"
            "// here is a documentation defect: HEADERS WIN.\n//\n"
        )
        w(
            "//   using-probes   : %d (namespace-scope) + %d (class-scope, "
            "EXTERNAL_CLASSES)\n" % (len(self.usings), len(self.class_usings))
        )
        w("//   member-probes  : %d\n" % len(self.members))
        w("//   designator-probes : %d\n" % len(self.designators))
        w(
            "//   index-checked  : %d (overloaded member fns, checked in Python)\n"
            % len(self.index_checked)
        )
        w(
            "//   excluded       : %d (exclusion tables and operator-ids)\n"
            % len(self.excluded)
        )
        w('#include "support/DocsTestSupport.h"\n')
        w(
            "// Every compose header, so a name is never reported missing merely\n"
            "// because the harness did not include the file that owns it.\n"
        )
        for header in self.headers:
            w("#include <%s>\n" % header)
        for cls in sorted({c for c, *_ in self.class_usings}):
            w("#include <%s>  // EXTERNAL_CLASSES probe base\n" % EXTERNAL_CLASSES[cls])
        w("\n")
        w(
            "namespace sigil::compose {\nnamespace docs_probe {\nnamespace ch = choreograph;\n\n"
        )
        for n, (path, spelled, line, kind) in enumerate(self.usings):
            w(
                "namespace u%d { using %s; }  // %s %s (%s)\n"
                % (n, "::".join(path), line, spelled, kind)
            )
        for n, (cls, member, spelled, line, kind) in enumerate(self.class_usings):
            w(
                "namespace c%d { struct Probe : %s { using %s::%s; }; }"
                "  // %s %s (%s)\n" % (n, cls, cls, member, line, spelled, kind)
            )
        w("\n")
        for n, (cands, chain, spelled, line, kind) in enumerate(self.members):
            single = "::" not in chain
            forms = [
                "requires { T::%s; }" % chain,
                "requires { typename T::%s; }" % chain,
            ]
            if single:
                forms.append("requires(const T &v) { v.%s; }" % chain)
                forms.append("requires { &T::%s; }" % chain)
            w("template <class T> concept M%d = %s;\n" % (n, "\n    || ".join(forms)))
            expr = " || ".join("M%d<%s>" % (n, c) for c in cands)
            msg = "%s spells `%s`; no header declares it" % (line, spelled)
            w(
                'static_assert(%s,\n              "%s");\n\n'
                % (expr, msg.replace('"', "'"))
            )
        self.emit_designators(w)
        w("} // namespace docs_probe\n} // namespace sigil::compose\n\n")
        # A guard whose extractor silently matches NOTHING compiles perfectly
        # and proves nothing — the same failure this generator exists to
        # prevent, one level up. So the counts are asserted. The floors are
        # set below the corpus's real yield, which means they catch a broken
        # extractor rather than ordinary edits, and lowering one is a
        # deliberate act someone has to write down.
        w(
            "namespace {\nconstexpr int kUsingProbes = %d; "
            "// namespace-scope + class-scope\n"
            "constexpr int kMemberProbes = %d;\n"
            "constexpr int kIndexChecked = %d;\n"
            "constexpr int kDesignatorProbes = %d;\n} // namespace\n\n"
            % (
                len(self.usings) + len(self.class_usings),
                len(self.members),
                len(self.index_checked),
                len(self.designators),
            )
        )
        w(
            "TEST(ComposeDocs, EveryNameInTheDocsResolvesAgainstTheHeaders) {\n"
            "  // The probes above are compile-time; this case exists so the\n"
            "  // guard is VISIBLE in the suite, and so an extractor that\n"
            "  // matched nothing fails loudly instead of passing vacuously.\n"
            "  EXPECT_GE(kUsingProbes, 40)\n"
            '      << "the docs\' namespace-scope names stopped being extracted";\n'
            "  EXPECT_GE(kMemberProbes, 3)\n"
            '      << "the docs\' Type::member names stopped being extracted";\n'
            "  EXPECT_GE(kIndexChecked, 12)\n"
            '      << "the docs\' member-function names stopped being extracted";\n'
            "  EXPECT_GE(kDesignatorProbes, 1)\n"
            '      << "no designated initialiser is covered — that form names "\n'
            '         "no member directly, so the qualified-name scan cannot "\n'
            '         "see it and only this probe can";\n'
            "}\n"
        )


def report_text(gen, mds):
    """The coverage report: counts, then every exemption BY NAME, then every
    unresolved name.  An exemption folded into a bare count is invisible —
    a reader auditing the guard could not tell what it deliberately skips —
    so each one is listed with its reason."""
    lines = [
        "Documented-name coverage (%s)" % ", ".join(os.path.basename(m) for m in mds),
        "  using-probes  : %d (+ %d class-scope)"
        % (len(gen.usings), len(gen.class_usings)),
        "  member-probes : %d" % len(gen.members),
        "  designators   : %d" % len(gen.designators),
        "  index-checked : %d" % len(gen.index_checked),
        "  excluded      : %d" % len(gen.excluded),
    ]
    for spelled, line, reason in gen.excluded:
        lines.append("    exempt  %s  %s  (%s)" % (line, spelled, reason))
    lines.append("  unresolved    : %d" % len(gen.unresolved))
    for spelled, line, why in gen.unresolved:
        lines.append("    %s  %s  (%s)" % (line, spelled, why))
    return "\n".join(lines)


# --------------------------------------------------------------------------
# Self-test fixtures.  Each is one markdown snippet run against one small
# header, pinning a behaviour of the GENERATOR itself: a real name yields a
# probe, an unreal one fails the run, an operator spelling is exempted AND
# reported, and an EXTERNAL_CLASSES member yields a class-scope probe.  The
# generator is the layer that decides what gets probed at all, so a
# regression here would not fail any C++ build — it would silently narrow
# the guard, which is exactly the failure the guard exists to prevent.

FIXTURE_HEADER = """\
namespace fix {
struct Widget {
  int knob;
};
void spin();
}
"""


def fixture_generator(md_text):
    """A Generator run over one in-memory markdown fixture and the fixture
    header, in a temp dir so nothing on disk is touched."""
    with tempfile.TemporaryDirectory() as tmp:
        incdir = os.path.join(tmp, "fixinc")
        os.makedirs(incdir)
        with open(os.path.join(incdir, "Fixture.h"), "w", encoding="utf-8") as f:
            f.write(FIXTURE_HEADER)
        md = os.path.join(tmp, "fixture.md")
        with open(md, "w", encoding="utf-8") as f:
            f.write(md_text)
        gen = Generator([md], [incdir])
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

    if failures:
        print("self-test: %d failure(s)" % len(failures))
        return 1
    print("self-test: all fixtures pass")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--md", action="append")
    ap.add_argument("--include", action="append")
    ap.add_argument("--out")
    ap.add_argument("--report", default=None)
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
    if not (args.md and args.include and args.out):
        ap.error("--md, --include and --out are required unless --self-test")

    gen = Generator(args.md, args.include)
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
