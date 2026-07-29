#!/usr/bin/env python3
"""Compile API.md's documented names against the headers that own them.

ROADMAP §25 recorded ten documentation defects, two of which were code that
does not compile — `PathFormat{.effects = …, .paint = …}` where the header
has `effect` and `strokeFill`.  The guard that existed to make that
impossible, `ComposeDocs.EverySignatureInTheLineAndBorderDocsCompiles`, is a
HAND TRANSCRIPTION of one section: it proved the mechanism and left every
other section exactly as wrong as before.  A guard that must be extended by
hand per section is the thing that failed.

So this is the mechanical route.  It reads API.md, extracts every qualified
name an author could copy — from the ```cpp blocks AND from the inline
`code` spans, because the prose carries as many names as the blocks do — and
emits a C++ translation unit of probes that only build if the headers still
spell those names that way.  Nothing here is registered per section; adding
a section to API.md adds its names to the guard on the next build.

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
      name, so those fall back to the header index, in Python, at build time.

  designated initialiser   `PathFormat{.effect = …}` -> `T{.effect = Any{}}`
      This is the §25 form and it needs its own probe, twice over.  It never
      spells `PathFormat::effect`, so the qualified-name scan cannot see it
      — which is exactly how the defect survived.  And it asks a STRICTER
      question than existence: `PathFormat{.paint = …}` names the real member
      function `paint`, so every existence form says yes while the
      initialiser still does not compile.  Only probing the initialiser
      itself answers what the doc actually claims.

Names that CANNOT resolve are not silently dropped.  Every one of them is
either in the exclusion table below, with a reason, or it is reported as an
unresolved documented name and the generator FAILS.  That inversion is the
whole point: a new doc section, or a type that is renamed out from under
the prose, breaks the build rather than quietly leaving the guard.
"""

import argparse
import os
import re
import sys

ID = r"[A-Za-z_][A-Za-z0-9_]*"
QUAL = re.compile(r"\b(" + ID + r"(?:::" + ID + r")+)")
# `Type{.field = …` / `Type{.field,` — designated initialisers, the §25 form.
DESIG = re.compile(r"\b(" + ID + r"(?:::" + ID + r")*)\s*\{\s*\.(" + ID + r")")
DESIG_MORE = re.compile(r"[,{]\s*\.(" + ID + r")\s*(?:=|,|\})")

# Namespaces that are not declared in the compose headers but are spelled in
# API.md.  Everything else in the namespace set is scraped from the headers.
NS_EXTERNAL = {
    "std", "chrono", "choreograph", "ch", "sigil", "compose", "weave",
    "motion", "image", "scry", "loader", "filesystem", "ranges", "views",
    "literals", "this_thread",
    # Skia's namespaces, which are Sk-prefixed like its types and would
    # otherwise be probed as if they were classes.
    "SkSurfaces", "SkShaders", "SkImageFilters", "SkImages", "SkColorFilters",
    "SkPathEffects", "SkGradientShader", "SkRuntimeEffectPriv", "SkFontMgr",
}

# Whole spellings API.md names on purpose that no header resolves.  Keyed by
# the exact qualified name so an exclusion can never widen silently to a
# sibling that SHOULD be probed.
EXCLUDED_SPELLED = {
    "shapes::subtract": "API.md §mask records this as a name a study reached "
                        "for and did NOT find",
    "sigil::scry::WebView": "SigilScry is Ultralight-gated; compose_web_test "
                            "owns that surface",
    "SyncToCpu::kYes": "skgpu::graphite::SyncToCpu — STRESS_TESTS.md writes "
                       "the leaf spelling of a Graphite type",
    "sigil::weave::Choreograph": "names SigilWeave's Choreograph.h MODULE, "
                                 "not a symbol in that namespace",
    "SkiaGraphiteContext::makeRecorderOptions":
        "src/common/skia's Graphite bring-up class — outside the compose "
        "surface and off compose_test's include path",
}

# Names API.md spells on purpose that no header can resolve.  Each needs a
# reason, and the list is about DOC CONTENT (a worked example's fictional
# host type; a deliberately-recorded deleted spelling), never about a
# section — so it cannot rot the way a per-section registration does.
EXCLUDED = {
    # The worked examples' fictional host application.
    "Palette": "API.md's worked example invents this theme struct",
    "EventInfo": "poster() example's own data type",
    "RowData": "scoreboard example's own row type",
    "Model": "scoreboard example's own model type",
    "Panel": "the 'inside the existing render path' example's host class",
    "MyRule": "placeholder for the reader's own crossing rule",
    "MyScheme": "placeholder for the reader's own DecorationScheme",
    "ksp": "a prose sketch of a hypothetical namespace, not shipped",
    # Concept/template parameter names in the header recitations.
    "D": "template parameter of the DecorationScheme concept",
    "P": "template parameter of the Profile concept",
    "T": "template parameter of Animatable/Transitioned/To/From",
    # Spellings API.md records BECAUSE they were deleted (the R3 rename
    # tables).  Probing them would assert the old names still exist.
    "brushes": "namespace deleted in R3; the tables document the old names",
    "with": "deleted in R3, recorded in the rename table",
    "withFrom": "deleted in R3, recorded in the rename table",
    "withKeyframes": "deleted in R3, recorded in the rename table",
    "PropValue": "deleted in R3, recorded in the rename table",
    "atDeg": "struck rather than shipped (ROADMAP §25)",
}

# Members no probe form can see, as opposed to members that do not exist.
UNPROBEABLE_MEMBERS = {
    # Overloaded member functions of types outside the scanned headers, where
    # neither probe form can see them: no `requires` spelling can name an
    # overload set, and there is no header index for Skia.
    ("SkImageInfo", "MakeN32Premul"): "Skia overload set; no probe form names it",
    ("SkCanvas", "drawImageLattice"): "Skia overload set; no probe form names it",
}

EXCLUDED_MEMBERS = {
    # NOTHING here may name a §25 defect spelling.  The first draft of this
    # table carried ("PathFormat", "effects") and ("PathFormat", "paint") on
    # the theory that API.md names them in prose to warn against them — and
    # the positive control caught it: with those entries the guard passed
    # cleanly on the exact defect it exists to make impossible.  API.md spells
    # the warning as bare `paint`/`effects`, which is not a probed form, so no
    # exclusion was ever needed.  An exclusion that covers a WRONG spelling
    # disarms the guard for that spelling; only DELETED-but-real names belong
    # here.
    # Spellings the PROSE names because they were deleted or renamed.  These
    # are the cost of probing inline `code` spans as well as blocks, and each
    # one is verified against the header that records the removal.
    ("Rail", "offset"): "renamed to `across` in R3; API.md names the old "
                        "spelling to explain the flip (Lines.h)",
    ("Line", "offset"): "renamed to `across` in R3; same sentence",
    ("Ribbon", "widthFn"): "deleted with `widthMax` (Brushes.h: the "
                           "widthFn->Profile note)",
    ("Brush", "op"): "deleted in R3 with the `ops::` one-door ruling "
                     "(Brushes.h)",
}


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
    r"|(?P<open2>\{)|(?P<close>\})")


def scan_headers(incdirs):
    """(namespace leaf names, {type name: [fully qualified spellings]}).

    Scraped from the headers so the guard's own idea of what exists follows
    the headers automatically — the same rule the doc obeys.  Scope is
    tracked by real brace depth, not by the `} // namespace` convention: a
    header that closes a namespace without the comment would otherwise
    mis-qualify every type after it.
    """
    namespaces = set()
    types = {}
    ns_paths = {}
    funcs = {}          # simple type name -> names declared with a ( in its body
    for incdir in incdirs:
        for root, _, files in os.walk(incdir):
            for name in sorted(files):
                if not name.endswith(".h"):
                    continue
                text = strip_comments(
                    open(os.path.join(root, name), encoding="utf-8").read())
                text = re.sub(r'"(?:[^"\\]|\\.)*"', '""', text)
                depth = 0
                scope = []          # (depth_at_open, [namespace parts])
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
                                "::".join(outer[:i + 1]))
                    elif m.group("agg"):
                        if m.group("open"):                 # a definition
                            qual = "::".join(p for _, ps, _c in scope for p in ps)
                            name_ = m.group("aggname")
                            types.setdefault(name_, set()).add(
                                (qual + "::" + name_) if qual else name_)
                            # A class is a scope too: `Composer::CacheState`
                            # must not come out as `sigil::compose::CacheState`.
                            scope.append((depth, [name_], True))
                            depth += 1
                    elif m.group("fn"):
                        # Only names declared DIRECTLY in a class body — not
                        # calls inside an inline function — so the index means
                        # "this class declares a member function of that name".
                        if (scope and scope[-1][2]
                                and scope[-1][0] == depth - 1):
                            funcs.setdefault(scope[-1][1][0], set()).add(
                                m.group("fn"))
                    elif m.group("open2"):
                        depth += 1
                    else:
                        depth -= 1
                        while scope and scope[-1][0] >= depth:
                            scope.pop()
    return (namespaces,
            {k: sorted(v) for k, v in types.items()},
            {k: sorted(v) for k, v in ns_paths.items()},
            {k: sorted(v) for k, v in funcs.items()})


def resolve_type(name, types):
    """Fully qualified candidates for a type name as API.md spells it."""
    if name in types:
        return ["::" + q for q in types[name]]
    if name.startswith("Sk") or name.startswith("Gr"):
        return ["::" + name]          # Skia lives at global scope
    return []


class Generator:
    def __init__(self, mds, incdirs):
        self.mds = mds
        (self.namespaces, self.types, self.ns_paths,
         self.funcs) = scan_headers(incdirs)
        self.namespaces |= NS_EXTERNAL
        self.headers = []
        for incdir in incdirs:
            base = os.path.basename(os.path.normpath(incdir))
            if base != "sigilcompose":
                continue
            for root, _, files in os.walk(incdir):
                for name in sorted(files):
                    if not name.endswith(".h") or name == "Web.h":
                        continue        # Web.h is Ultralight-gated
                    rel = os.path.relpath(os.path.join(root, name),
                                          os.path.dirname(incdir))
                    self.headers.append(rel.replace(os.sep, "/"))
        self.usings = []        # (qualified, line, kind)
        self.members = []       # (candidates, chain, spelled, line, kind)
        self.designators = []   # (candidates, field, spelled, line, kind)
        self.excluded = []      # (spelled, line, reason)
        self.unresolved = []    # (spelled, line, why)
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
                    tail = text[m.start():]
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
            return                                     # a namespace, nothing to probe
        if i == len(parts) - 1:
            # API.md writes the LEAF namespace (`shapers::Offset`), which is
            # how it reads under `using namespace sigil::compose`; the probe
            # has to spell the path the headers actually put it on.
            self.usings.append((self.expand(parts[:i]) + [parts[i]],
                                spelled, line, kind))
            return
        # parts[i] names a type; the rest is a member chain.
        cands = resolve_type(parts[i], self.types)
        if not cands:
            self.unresolved.append((spelled, line, "no header declares type "
                                    + parts[i]))
            return
        self.member(cands, parts[i], "::".join(parts[i + 1:]), spelled, line,
                    kind)

    def designated(self, typename, field, line, kind, seen):
        key = (typename, field)
        if key in seen:
            return
        seen[key] = line
        leaf = typename.split("::")[-1]
        if (leaf, field) in EXCLUDED_MEMBERS:
            self.excluded.append((typename + "{." + field, line,
                                  EXCLUDED_MEMBERS[(leaf, field)]))
            return
        if self.excluded_hit(typename, line) or field in EXCLUDED:
            return
        if not leaf[0].isupper():
            return                                      # `ns::fn({.a = …})` etc.
        cands = resolve_type(leaf, self.types)
        if not cands:
            self.unresolved.append((typename + "{." + field, line,
                                    "no header declares type " + leaf))
            return
        self.designators.append((cands, field,
                                 typename + "{." + field + " = …}", line, kind))

    def member(self, cands, typename, chain, spelled, line, kind):
        """Route one Type::member reference to the probe form that can see it.

        A `requires` expression cannot name an OVERLOADED member function —
        every spelling of it (`T::m`, `&T::m`, `v.m`) has to resolve the
        overload set and is therefore ill-formed.  Those names are checked
        against the header index instead, which is the same headers-win rule
        enforced one layer up, in Python, at build time.
        """
        if (typename, chain) in EXCLUDED_MEMBERS:
            self.excluded.append((spelled, line,
                                  EXCLUDED_MEMBERS[(typename, chain)]))
            return
        if "::" not in chain and chain in self.funcs.get(typename, ()):
            self.index_checked.append((spelled, line, kind))
            return
        if (typename, chain) in UNPROBEABLE_MEMBERS:
            self.excluded.append((spelled, line,
                                  UNPROBEABLE_MEMBERS[(typename, chain)]))
            return
        self.members.append((cands, chain, spelled, line, kind))

    def emit_designators(self, w):
        """A designator must name a DATA member, which is a stricter claim
        than `Type::field` resolving: `PathFormat{.paint = …}` names the
        real member FUNCTION `paint`, so every member-existence form says
        yes and the initialiser still does not compile.  Probing the
        initialiser itself is the only form that answers the question the
        doc asks.  (Found by the positive control, not by reasoning.)"""
        if not self.designators:
            return
        w("// A universal source value, so the probe tests the DESIGNATOR and\n"
          "// not the type of whatever API.md happened to assign to it.\n"
          "struct AnyInit { template <class U> operator U() const; };\n\n")
        for n, (cands, field, spelled, line, kind) in enumerate(self.designators):
            w("template <class T> concept DI%d = requires { T{.%s = AnyInit{}}; };\n"
              % (n, field))
            expr = " || ".join("DI%d<%s>" % (n, c) for c in cands)
            msg = ("%s spells `%s`; the header has no such data member"
                   % (line, spelled))
            w('static_assert(%s,\n              "%s");\n\n' % (expr, msg))

    def emit(self, out):
        w = out.write
        w("// GENERATED by test/docs/api_doc_probes.py — DO NOT EDIT.\n")
        w("// Sources: %s.  Rebuilt whenever they change.\n"
          % ", ".join(os.path.basename(m) for m in self.mds))
        w("//\n// Every qualified name and designated-initialiser field these\n"
          "// docs spell, compiled against the headers that own them.  A failure\n"
          "// here is a documentation defect: HEADERS WIN.\n//\n")
        w("//   using-probes   : %d\n" % len(self.usings))
        w("//   member-probes  : %d\n" % len(self.members))
        w("//   designator-probes : %d\n" % len(self.designators))
        w("//   index-checked  : %d (overloaded member fns, checked in Python)\n"
          % len(self.index_checked))
        w("//   excluded       : %d (see EXCLUDED in the generator)\n"
          % len(self.excluded))
        w('#include "ComposeTestSupport.h"\n')
        w("// Every compose header, so a name is never reported missing merely\n"
          "// because the harness did not include the file that owns it.\n")
        for header in self.headers:
            w("#include <%s>\n" % header)
        w("\n")
        w("namespace sigil::compose {\nnamespace docs_probe {\nnamespace ch = choreograph;\n\n")
        for n, (path, spelled, line, kind) in enumerate(self.usings):
            w("namespace u%d { using %s; }  // %s %s (%s)\n"
              % (n, "::".join(path), line, spelled, kind))
        w("\n")
        for n, (cands, chain, spelled, line, kind) in enumerate(self.members):
            single = "::" not in chain
            forms = ["requires { T::%s; }" % chain,
                     "requires { typename T::%s; }" % chain]
            if single:
                forms.append("requires(const T &v) { v.%s; }" % chain)
                forms.append("requires { &T::%s; }" % chain)
            w("template <class T> concept M%d = %s;\n" % (n, "\n    || ".join(forms)))
            expr = " || ".join("M%d<%s>" % (n, c) for c in cands)
            msg = "%s spells `%s`; no header declares it" % (line, spelled)
            w('static_assert(%s,\n              "%s");\n\n' % (expr, msg.replace('"', "'")))
        self.emit_designators(w)
        w("} // namespace docs_probe\n} // namespace sigil::compose\n\n")
        # A guard whose extractor silently matches NOTHING compiles perfectly
        # and proves nothing — the exact failure mode §25 is about, one level
        # up. So the counts are asserted, and lowering a floor is a conscious
        # act someone has to write down.
        w("namespace {\nconstexpr int kUsingProbes = %d;\n"
          "constexpr int kMemberProbes = %d;\n"
          "constexpr int kIndexChecked = %d;\n"
          "constexpr int kDesignatorProbes = %d;\n} // namespace\n\n"
          % (len(self.usings), len(self.members), len(self.index_checked),
             len(self.designators)))
        w("TEST(ComposeDocs, EveryNameInTheDocsResolvesAgainstTheHeaders) {\n"
          "  // The probes above are compile-time; this case exists so the\n"
          "  // guard is VISIBLE in the suite, and so an extractor that\n"
          "  // matched nothing fails loudly instead of passing vacuously.\n"
          "  EXPECT_GE(kUsingProbes, 160)\n"
          "      << \"the docs' namespace-scope names stopped being extracted\";\n"
          "  EXPECT_GE(kMemberProbes, 45)\n"
          "      << \"the docs' Type::member names stopped being extracted\";\n"
          "  EXPECT_GE(kIndexChecked, 28)\n"
          "      << \"the docs' member-function names stopped being extracted\";\n"
          "  EXPECT_GE(kDesignatorProbes, 20)\n"
          "      << \"the docs' designated initialisers stopped being \"\n"
          "         \"extracted — the exact form the ROADMAP §25 defect took\";\n"
          "}\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--md", required=True, action="append")
    ap.add_argument("--include", required=True, action="append")
    ap.add_argument("--out", required=True)
    ap.add_argument("--report", default=None)
    args = ap.parse_args()

    gen = Generator(args.md, args.include)
    gen.collect()
    with open(args.out, "w", encoding="utf-8") as f:
        gen.emit(f)

    lines = ["Documented-name coverage (%s)"
             % ", ".join(os.path.basename(m) for m in args.md),
             "  using-probes  : %d" % len(gen.usings),
             "  member-probes : %d" % len(gen.members),
             "  designators   : %d" % len(gen.designators),
             "  index-checked : %d" % len(gen.index_checked),
             "  excluded      : %d" % len(gen.excluded),
             "  unresolved    : %d" % len(gen.unresolved)]
    for spelled, line, why in gen.unresolved:
        lines.append("    %s  %s  (%s)" % (line, spelled, why))
    text = "\n".join(lines)
    if args.report:
        open(args.report, "w", encoding="utf-8").write(text + "\n")
    print(text)
    if gen.unresolved:
        sys.stderr.write(
            "\nThe docs name %d thing(s) no header declares.  Either the doc is\n"
            "wrong (fix the doc — HEADERS WIN) or the name is documented on\n"
            "purpose, in which case add it to EXCLUDED with a reason.\n"
            % len(gen.unresolved))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
