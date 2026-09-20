"""THE PROBES ONE CORPUS YIELDS, and the report of what was probed and
what was not.

Three probe forms, chosen by what the name is:

  namespace-scope entity   `shapes::polygon`   -> `using shapes::polygon;`
      A using-declaration is the one spelling that works uniformly for
      functions (including overload sets), types, variables, namespaces
      and enumerators, and it is a hard error when the name does not
      exist.

  class member             `PathFormat::effect` -> a concept disjunction
      `using` cannot name a non-static data member outside a derived
      class, so members are probed through `requires`, which covers
      enumerators, static and non-static data, nested types and member
      functions.  An OVERLOADED member function is the one thing no
      `requires` spelling can name — every way of writing it has to
      resolve the overload set, and is therefore ill-formed — so those
      fall back to the header index, in Python, at build time.

  designated initialiser   `PathFormat{.effect = …}` -> `T{.effect = Any{}}`
      This form needs its own probe for two independent reasons.  It
      never spells `PathFormat::effect` anywhere, so the qualified-name
      scan does not see it at all.  And it asks a STRICTER question than
      existence: the field must be a DATA member.  `PathFormat{.paint =
      …}` names the real member FUNCTION `paint`, so every existence
      probe answers yes while the initialiser still does not compile.

A bare name in a header listing takes no C++ probe at all: the bullet
says which header it belongs to, so it is checked here against the
identifiers that header carries, and then against the rest of the
scanned headers.

Names that CANNOT resolve are not silently dropped.  Every one is either
exempted — by a table in names.py or by the operator-id rule, both of
which record a reason — or it is reported as an unresolved documented
name and the generator FAILS.  That inversion is the whole point: a new
passage of prose, or a type renamed out from under it, breaks the build
rather than quietly leaving the guard.
"""

import os

from .documents import code_regions, doc_labels
from .headers import reachable_from, scan_headers, strip_comments
from .names import (
    BARE_NAME,
    DESIG,
    DESIG_MORE,
    EXCLUDED,
    EXCLUDED_MEMBERS,
    EXCLUDED_SPELLED,
    EXTERNAL_CLASSES,
    NS_EXTERNAL,
    QUAL,
    UNPROBEABLE_MEMBERS,
    resolve_type,
)


class Generator:
    def __init__(
        self,
        mds,
        incdirs,
        library="",
        namespace="sigil::docs",
        suite="Docs",
        preludes=None,
        floors=None,
        aliases=None,
        skip_headers=None,
    ):
        self.mds = mds
        self.library = library
        self.namespace = namespace
        self.suite = suite
        self.preludes = preludes or ["<gtest/gtest.h>"]
        self.floors = floors or (0, 0, 0, 0, 0)
        # A namespace alias the documents write names through — a
        # document writing `ch::Output` for choreograph's — so a probe
        # spells the name the way the prose does.
        self.aliases = aliases or []
        (
            self.namespaces,
            self.types,
            self.ns_paths,
            self.funcs,
            self.spelled_in,
            templates,
        ) = scan_headers(incdirs)
        self.spelled_anywhere = set()
        for names in self.spelled_in.values():
            self.spelled_anywhere |= names
        self.namespaces |= NS_EXTERNAL
        # A supplied alias names a namespace this scanner may never see —
        # the whole reason a document spells names through one — so its name
        # leads a qualified name exactly as a scanned namespace does.
        self.namespaces |= {alias.partition("=")[0] for alias in self.aliases}
        self.headers = []  # as an #include spells them
        self.header_files = []  # the same headers, on disk
        for incdir in incdirs:
            base = os.path.basename(os.path.normpath(incdir))
            if base != self.library:
                continue
            for root, _, files in os.walk(incdir):
                for name in sorted(files):
                    if not name.endswith(".h"):
                        continue
                    # A header behind an SDK or a UI toolkit compiles only
                    # where that dependency is; the TU that includes every
                    # OTHER header still probes every name it declares.
                    if name in (skip_headers or ()):
                        continue
                    path = os.path.join(root, name)
                    rel = os.path.relpath(path, os.path.dirname(incdir))
                    self.headers.append(rel.replace(os.sep, "/"))
                    self.header_files.append(path)
        # The probe includes every header of the library, so what those
        # reach is what a probe may name.  A type only another library's
        # UNREACHABLE header declares is dropped from the candidates rather
        # than written into a static_assert that cannot compile.
        seen_files = reachable_from(self.header_files, incdirs)
        self.types = {
            name: [q for q, f in cands if f in seen_files and q not in templates]
            or [q for q, _ in cands if q not in templates]
            or [q for q, _ in cands]
            for name, cands in self.types.items()
        }
        self.usings = []  # (qualified, line, kind)
        self.class_usings = []  # (class, member, spelled, line, kind)
        self.members = []  # (candidates, chain, spelled, line, kind)
        self.designators = []  # (candidates, field, spelled, line, kind)
        self.excluded = []  # (spelled, line, reason)
        self.unresolved = []  # (spelled, line, why)
        self.index_checked = []  # (spelled, line, kind) — member fns
        self.listed = []  # (spelled, line, header) — bare names in a listing

    def collect(self):
        seen_q, seen_m, seen_b = {}, {}, {}
        labels = doc_labels(self.mds)
        for md in self.mds:
            doc = labels[md]
            for line, text, kind, headers in code_regions(md):
                where = "%s:%d" % (doc, line)
                text = strip_comments(text)
                for m in QUAL.finditer(text):
                    self.qualified(m.group(1), where, kind, seen_q)
                for m in DESIG.finditer(text):
                    tail = text[m.start() :]
                    fields = [m.group(2)] + DESIG_MORE.findall(tail)
                    for field in fields:
                        self.designated(m.group(1), field, where, kind, seen_m)
                if kind == "listing" and BARE_NAME.match(text):
                    self.bare(text, where, headers, seen_b)

    def expand(self, prefix):
        """Leaf namespace spelling -> the header's own full path for it."""
        if not prefix:
            return []
        head = prefix[0]
        if any(head == alias.partition("=")[0] for alias in self.aliases):
            return list(prefix)  # the document's own alias names the path
        paths = self.ns_paths.get(head)
        if paths:
            # A leaf two libraries spell is the DOCUMENTING library's: a
            # scanned dependency declaring the same word says nothing about
            # what this document's reader would reach. Ambiguity WITHIN the
            # library is the document's own, and is left to fail.
            own = [p for p in paths if p.startswith(self.namespace + "::")]
            if len(own) == 1:
                paths = own
            if len(paths) == 1 and paths[0] != head:
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

    def bare(self, spelled, line, headers, seen):
        """A bare name in a header-listing bullet, resolved against the
        headers the bullet names.

        The bullet is the only place a bare name says which header owns it,
        which is what makes this resolvable at all.  A name the named
        headers carry is the claim the bullet makes; one they do not, but
        another header of the library does, is a misfiled entry rather than
        a missing API and is reported without failing the run; one that
        appears in no scanned header at all is a name that does not exist,
        and the run fails on it exactly as it fails on a qualified name.
        """
        if spelled in seen or spelled in self.namespaces:
            return
        seen[spelled] = line
        if self.excluded_hit(spelled, line):
            return
        for header in headers:
            for path, names in self.spelled_in.items():
                if (path == header or path.endswith("/" + header)) and spelled in names:
                    self.listed.append((spelled, line, header))
                    return
        if spelled in self.spelled_anywhere:
            self.excluded.append(
                (spelled, line, "spelled by another header than the one listed")
            )
            return
        self.unresolved.append(
            (spelled, line, "no header of this library spells " + spelled)
        )

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
        w("// GENERATED by src/test/docs/api_doc_probes.py — DO NOT EDIT.\n")
        w(
            "// Sources: %s.  Rebuilt whenever they change.\n"
            % ", ".join(doc_labels(self.mds).values())
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
            "//   listed-names   : %d (bare names in a header listing, checked "
            "in Python)\n" % len(self.listed)
        )
        w(
            "//   excluded       : %d (exclusion tables and operator-ids)\n"
            % len(self.excluded)
        )
        for prelude in self.preludes:
            w("#include %s\n" % prelude)
        w(
            "// Every header of the library, so a name is never reported missing\n"
            "// merely because the harness did not include the file that owns it.\n"
        )
        for header in self.headers:
            w("#include <%s>\n" % header)
        for cls in sorted({c for c, *_ in self.class_usings}):
            w("#include <%s>  // EXTERNAL_CLASSES probe base\n" % EXTERNAL_CLASSES[cls])
        w("\n")
        w("namespace %s {\nnamespace docs_probe {\n" % self.namespace)
        for alias in self.aliases:
            name, _, target = alias.partition("=")
            w("namespace %s = %s;\n" % (name, target))
        w("\n")
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
        w("} // namespace docs_probe\n} // namespace %s\n\n" % self.namespace)
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
            "constexpr int kListedNames = %d;\n"
            "constexpr int kDesignatorProbes = %d;\n} // namespace\n\n"
            % (
                len(self.usings) + len(self.class_usings),
                len(self.members),
                len(self.index_checked),
                len(self.listed),
                len(self.designators),
            )
        )
        usings, members, indexed, listed, designators = self.floors
        w("TEST(%s, EveryNameInTheDocsResolvesAgainstTheHeaders) {\n" % self.suite)
        w(
            "  // The probes above are compile-time; this case exists so the\n"
            "  // guard is VISIBLE in the suite, and so an extractor that\n"
            "  // matched nothing fails loudly instead of passing vacuously.\n"
        )
        w(
            "  EXPECT_GE(kUsingProbes, %d)\n" % usings
            + '      << "the docs\' namespace-scope names stopped being extracted";\n'
        )
        w(
            "  EXPECT_GE(kMemberProbes, %d)\n" % members
            + '      << "the docs\' Type::member names stopped being extracted";\n'
        )
        w(
            "  EXPECT_GE(kIndexChecked, %d)\n" % indexed
            + '      << "the docs\' member-function names stopped being extracted";\n'
        )
        if listed:
            w(
                "  EXPECT_GE(kListedNames, %d)\n"
                % listed
                + '      << "the header listings\' bare names stopped being checked "\n'
                '         "against the headers they are listed under";\n'
            )
        if designators:
            w(
                "  EXPECT_GE(kDesignatorProbes, %d)\n"
                % designators
                + '      << "no designated initialiser is covered — that form names "\n'
                '         "no member directly, so the qualified-name scan cannot "\n'
                '         "see it and only this probe can";\n'
            )
        w("}\n")


def report_text(gen, mds):
    """The coverage report: counts, then every exemption BY NAME, then every
    unresolved name.  An exemption folded into a bare count is invisible —
    a reader auditing the guard could not tell what it deliberately skips —
    so each one is listed with its reason."""
    lines = [
        "Documented-name coverage (%s)" % ", ".join(doc_labels(mds).values()),
        "  using-probes  : %d (+ %d class-scope)"
        % (len(gen.usings), len(gen.class_usings)),
        "  member-probes : %d" % len(gen.members),
        "  designators   : %d" % len(gen.designators),
        "  index-checked : %d" % len(gen.index_checked),
        "  listed-names  : %d (bare, in a header listing)" % len(gen.listed),
        "  excluded      : %d" % len(gen.excluded),
    ]
    for spelled, line, reason in gen.excluded:
        lines.append("    exempt  %s  %s  (%s)" % (line, spelled, reason))
    lines.append("  unresolved    : %d" % len(gen.unresolved))
    for spelled, line, why in gen.unresolved:
        lines.append("    %s  %s  (%s)" % (line, spelled, why))
    return "\n".join(lines)
