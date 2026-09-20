"""WHAT THE HEADERS DECLARE, scraped from the headers themselves so the
guard's own idea of what exists follows them automatically — the same
rule the documents are held to.

One walk yields the namespaces, the types under the scope they are
really in, the member functions each class declares, and every
identifier each header's code carries, which is what a header listing's
bare names are checked against.  A second walk answers what a
translation unit including the library's own headers actually SEES,
because a candidate the probe cannot see is a compile error wherever it
is named.
"""

import os
import re


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
