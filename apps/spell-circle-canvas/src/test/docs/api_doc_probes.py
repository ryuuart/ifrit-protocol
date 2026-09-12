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

  bare name in a header listing   a `core/Paint.h` bullet naming `hex`,
                                  `mul` -> the header's own text
      A bullet that opens with a header path is an INDEX of that header:
      every bare backticked name in it claims to be something that header
      spells.  That claim is checked in Python against the identifiers the
      named headers actually carry, and then against the rest of the
      scanned headers, so a name the library does not have anywhere fails
      the run.  This is the one place bare names can be resolved without a
      C++ parser, because the bullet says which header they belong to.

Names that CANNOT resolve are not silently dropped.  Every one is either
exempted — by an exclusion table below or by the operator-id rule, both of
which record a reason — or it is reported as an unresolved documented name
and the generator FAILS.  That inversion is the whole point: a new passage
of prose, or a type renamed out from under it, breaks the build rather than
quietly leaving the guard.  Every exemption is listed by name and reason in
the coverage report, so what the guard deliberately does not check is
visible rather than folded into a count.

STATED LIMITATIONS — what this guard structurally cannot see:

  Unqualified names in prose and in code blocks.  A document that writes
      `padding(24_px)` — a bad argument to a real function — or invents a
      free function `px(float)` outside a header listing spells no
      qualified name, so the extractor has nothing to match and both
      errors pass unprobed.  Closing this would mean resolving an
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
name per behaviour it must keep: resolve, fail, exempt-and-report, the
bare names of a header listing, and the class-scope probe for
EXTERNAL_CLASSES — without touching the real corpus.
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
# A bullet that OPENS with one or more backticked header paths is an index
# of those headers: `- `core/Paint.h` — the paint values: …`.  Everything
# backticked in it is claimed to be a name those headers carry, which is
# what makes bare names resolvable here and nowhere else.
LISTING_OPENER = re.compile(
    r"^\s*[-*]\s+(`[A-Za-z0-9_./]+\.h`"
    r"(?:\s*(?:,|and|/)\s*`[A-Za-z0-9_./]+\.h`)*)"
)
HEADER_PATH = re.compile(r"`([A-Za-z0-9_./]+\.h)`")
BARE_NAME = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")

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
    # The vector maths every params struct is written in, exposed in this
    # tree's own signatures.
    "glm",
    # Boost.Asio's executor vocabulary is declared in dependency .hpp
    # headers, outside the library .h roots this scanner reads.
    "boost",
    "asio",
    # Skia's actual namespaces, which are Sk-prefixed like its types and
    # would otherwise be probed as if they were classes.
    "SkSurfaces",
    "SkShaders",
    "SkImages",
    "SkPathEffects",
    # SigilMaterial's root and its feature namespaces, whose include paths
    # are deliberately NOT all given: scanning the core would put every
    # type it declares into the candidate set a member probe ORs over, and
    # one of those (`ProgramCache::Key`) is a PRIVATE nested type, which
    # this scanner cannot see and which is a hard error the moment it is
    # named.  Only the Skia feature's headers are scanned, for the paint
    # value the documents name members of; the rest are listed here so a
    # document naming one bare is passed over rather than probed as a
    # type.
    "material",
    "sdf",
    "pattern",
    "field",
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

# A dependency's OVERLOAD SET reached through a type this generator does
# not index: no `requires` spelling can name one, and the index path cannot
# answer either, since the index is built from the scanned headers alone.
EXCLUDED_SPELLED.update(
    {
        "SkRuntimeEffect::MakeForShader": "Skia's own overloaded static factory: no requires-spelling can "
        "name an overload set, and this generator indexes only the "
        "libraries whose include roots it is given",
    }
)


def code_regions(path):
    """[(line, text, kind, headers)] — every ```cpp line and every inline
    `span`, each span carrying the header paths of the listing bullet it
    stands in (empty everywhere else)."""
    out = []
    fence = None
    listing = ()  # the headers of the bullet being read, if any
    for i, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
        if line.startswith("```"):
            fence = None if fence is not None else line[3:].strip()
            continue
        if fence is not None:
            if fence.startswith("cpp"):
                out.append((i, line, "block", ()))
            continue
        opener = LISTING_OPENER.match(line)
        if opener:
            listing = tuple(HEADER_PATH.findall(opener.group(1)))
        elif not line.strip() or re.match(r"^\s*[-*]\s", line) or line.startswith("#"):
            listing = ()  # the bullet ended
        for m in re.finditer(r"`([^`]+)`", line):
            out.append((i, m.group(1), "listing" if listing else "inline", listing))
    return out


# Both comment forms in ONE alternation, block first. Stripping line
# comments separately would cut a `http://` inside a block comment down to
# the end of its line and take the closing `*/` with it, and everything
# after it in the file would then read as commented-out — which is how a
# type a document names goes missing without the document being wrong.
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)


def strip_comments(text):
    return COMMENT.sub("", text)


HEADER_TOKENS = re.compile(
    r"(?P<ns>\bnamespace\s+(?P<nsname>[A-Za-z_][A-Za-z0-9_:]*)\s*\{)"
    r"|(?P<alias>\bnamespace\s+(?P<aname>[A-Za-z_][A-Za-z0-9_]*)\s*=)"
    r"|(?P<agg>\b(?:struct|class|enum\s+class|enum\s+struct|enum)\s+"
    r"(?P<aggname>[A-Za-z_][A-Za-z0-9_]*)\b(?P<tail>[^;{\n]*)(?P<open>\{)?)"
    r"|(?P<access>\b(?:public|private|protected)\s*:)"
    r"|(?P<fn>\b[A-Za-z_][A-Za-z0-9_]*)\s*\("
    r"|(?P<open2>\{)|(?P<close>\})"
)


def opens_a_template(text, at):
    """Whether the aggregate at `at` is preceded by a template-parameter
    list.

    A class TEMPLATE cannot stand where a probe names a type: `M<T>` over
    one is ill-formed, and one whose simple name a document never meant —
    a `From` in a keyframe header beside the `From` a schedule spells —
    would otherwise take the whole disjunction down with it.
    """
    before = text[:at].rstrip()
    if not before.endswith(">"):
        return False
    depth, i = 0, len(before) - 1
    while i >= 0:
        if before[i] == ">":
            depth += 1
        elif before[i] == "<":
            depth -= 1
            if depth == 0:
                break
        i -= 1
    return i >= 0 and before[:i].rstrip().endswith("template")


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
    templates = set()  # qualified names of class templates
    ns_paths = {}
    funcs = {}  # simple type name -> names declared with a ( in its body
    # path suffix -> every identifier that header's CODE carries, which is
    # what a header-listing bullet's bare names are checked against.
    spelled_in = {}
    for incdir in incdirs:
        for root, _, files in os.walk(incdir):
            for name in sorted(files):
                if not name.endswith(".h"):
                    continue
                text = strip_comments(
                    open(os.path.join(root, name), encoding="utf-8").read()
                )
                rel = os.path.relpath(os.path.join(root, name), incdir)
                # Accumulated, not assigned: two include roots can carry a
                # header of the same relative name, and a listing that
                # names one must not be answered from whichever was
                # scanned last.
                spelled_in.setdefault(rel.replace(os.sep, "/"), set()).update(
                    re.findall(r"[A-Za-z_][A-Za-z0-9_]*", text)
                )
                text = re.sub(r'"(?:[^"\\]|\\.)*"', '""', text)
                depth = 0
                # [depth_at_open, [name parts], is_a_class, members_public]
                scope = []
                for m in HEADER_TOKENS.finditer(text):
                    if m.group("alias"):
                        namespaces.add(m.group("aname"))
                    elif m.group("ns"):
                        parts = m.group("nsname").split("::")
                        namespaces.update(parts)
                        scope.append([depth, parts, False, True])
                        depth += 1
                        outer = [p for _, ps, _c, _a in scope for p in ps]
                        for i in range(len(outer)):
                            ns_paths.setdefault(outer[i], set()).add(
                                "::".join(outer[: i + 1])
                            )
                    elif m.group("access"):
                        # A section label belongs to the class it stands in.
                        if scope and scope[-1][2] and scope[-1][0] == depth - 1:
                            scope[-1][3] = m.group("access").startswith("public")
                    elif m.group("agg"):
                        if m.group("open"):  # a definition
                            qual = "::".join(p for _, ps, _c, _a in scope for p in ps)
                            name_ = m.group("aggname")
                            spelling = (qual + "::" + name_) if qual else name_
                            # A type declared in a class's private or
                            # protected section is a hard error the moment a
                            # probe names it, and this scanner is the only
                            # thing that can keep it out of a candidate set:
                            # the compiler's answer is a diagnostic, not a no.
                            visible = not scope[-1][2] if scope else True
                            visible = visible or scope[-1][3]
                            if visible:
                                types.setdefault(name_, set()).add(
                                    (spelling, os.path.join(root, name))
                                )
                                if opens_a_template(text, m.start()):
                                    templates.add(spelling)
                            # A class is a scope too: `Composer::CacheState`
                            # must not come out as `sigil::compose::CacheState`.
                            # `struct` opens public, `class` private.
                            scope.append(
                                [
                                    depth,
                                    [name_],
                                    True,
                                    not m.group("agg").startswith("class"),
                                ]
                            )
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
        {k: sorted(v) for k, v in types.items()},  # (qualified, declaring file)
        {k: sorted(v) for k, v in ns_paths.items()},
        {k: sorted(v) for k, v in funcs.items()},
        spelled_in,
        templates,
    )


INCLUDE_LINE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)


def reachable_from(roots, incdirs):
    """Every header a translation unit including `roots` actually sees.

    A candidate the probe cannot see is a compile error wherever it is
    named, even when the header that declares it exists: another library
    on the scanner's include path is not automatically on the probe's.
    Resolution is by path suffix against the scanned directories, which is
    how the includes are spelled.
    """
    files = {}
    for incdir in incdirs:
        for root, _, names in os.walk(incdir):
            for name in names:
                if name.endswith(".h"):
                    path = os.path.join(root, name)
                    files.setdefault(
                        os.path.relpath(path, os.path.dirname(incdir)).replace(
                            os.sep, "/"
                        ),
                        path,
                    )
    seen, queue = set(), list(roots)
    while queue:
        path = queue.pop()
        if path in seen or not os.path.exists(path):
            continue
        seen.add(path)
        text = open(path, encoding="utf-8", errors="ignore").read()
        for spelled in INCLUDE_LINE.findall(text):
            spelled = spelled.replace(os.sep, "/")
            for rel, full in files.items():
                if rel == spelled or rel.endswith("/" + spelled):
                    queue.append(full)
                    break
            else:
                sibling = os.path.join(os.path.dirname(path), spelled)
                if os.path.exists(sibling):
                    queue.append(sibling)
    return seen


def doc_labels(mds):
    """{path: a short name unique among them}.

    A library whose chapters are a nested README apiece — a kit's beside
    the page that links it — has more than one `README.md`, and a report
    that names them both the same cannot say which line it means.  So a
    label grows leftwards along the path until it is the only one.
    """

    def tail(md, n):
        return "/".join(os.path.normpath(md).split(os.sep)[-n:])

    labels = {}
    for md in mds:
        n = 1
        while n < len(os.path.normpath(md).split(os.sep)):
            if sum(1 for other in mds if tail(other, n) == tail(md, n)) == 1:
                break
            n += 1
        labels[md] = tail(md, n)
    return labels


def resolve_type(name, types):
    """Fully qualified candidates for a type name as a document spells it."""
    if name in types:
        return ["::" + q for q in types[name]]
    if name.startswith("Sk") or name.startswith("Gr"):
        return ["::" + name]  # Skia lives at global scope
    return []


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
