# The overview-and-reference layer

The Doxygen sites are the literal API: every overload, every member,
every inheritance edge, cross-linked. They answer *what exactly does
this member do* for a reader who already knows its name.

This layer answers the two questions they cannot.

**What is there to reach for?** A catalogue, split by what a thing IS
— something you create, something you say to one, a value you pass —
with one page per entity and an index per kind that can be read top to
bottom. `reference/index.html` is every library and every kind on one
screen.

**What do I make to pass to it?** A page per value, tree-wide, with
three generated tables: what makes one, everywhere one is taken, and
everything that hands one back. `values/index.html` is every value in
the tree with those three counts beside it. This is the half that no
amount of prose produces, because the answer crosses libraries: a
`Fill` is made in SigilCompose, widened by SigilMaterial and taken by
SigilDraw and SigilSketch.

`docs/README.md` is the canon for the Doxygen pipeline below this.

## Building it

```sh
cmake --build build --target docs                  # sites, then this layer
python3 scripts/sigil.py docs --manifest build/docs-manifest.txt \
        --reference-only                           # this layer alone
open build/docs/reference/index.html
```

The `docs` target writes both. `--reference-only` skips Doxygen
entirely and reads the XML inventory an earlier run left in
`build/docs-build/`, which is the loop to use while prose is moving.
`--no-reference` writes the Doxygen sites and nothing above them.

| Flag | What it does |
| --- | --- |
| `--reference-only` | skip Doxygen; read the inventory already written |
| `--no-reference` | the Doxygen sites alone |
| `--example-images` | render every page's example in both languages |
| `--report` | print every entity with no page, and keep the ledger |
| `--strict` | with `--report`, fail when coverage has DROPPED |

`--library` narrows the Doxygen sites and nothing here: a value's
answer crosses libraries, so this layer reads every one of them
whatever that flag names. With no inventory to read — `--no-xml` on a
tree that has never had the third pass run — the layer says so and is
skipped rather than failing the verb.

`--example-images` is off by default because a C++ example compiles
before it draws. `--strict` belongs in the one refinement pass before a
push, beside `sigil.py check`, and nowhere else: a documentation gate
that broke a library build would be exactly what the repository's
code-fast rule forbids.

## What is generated and what is written

| On a page | Where it comes from |
| --- | --- |
| the title, and the `differs` / `C++ only` / `Python only` badge | the binding scan |
| **Syntax**, both languages | the Doxygen XML and the `.pyi` tree |
| **Values** / **Parameters** | the parameter types, each linked to its value page |
| the Python role-union paragraph | `apps/python/sigil/typing/_types.pyi` |
| **Members** on an enumeration | the Doxygen XML |
| **Make one** / **Pass it to** / **Also returned by** | the type graph |
| the include line, the Python spelling, the literal-API link | the readers |
| the lede, **Description**, **Examples**, **See also** | **a person** |

A page with no Markdown file still renders: every generated section
appears and the lede is the header's own brief. A hand-written page is
how prose the header cannot carry gets onto the page.

## Writing a page

```
src/<library>/reference/pages/<kind>/<Name>.md
src/<library>/reference/examples/<stem>.cpp
src/<library>/reference/examples/<stem>.py
docs/overview/<name>.md
docs/guides/<name>.md
docs/glossary.md
```

`<kind>` is `elements`, `verbs`, `types`, `kit`, `enums` or
`functions`. `<Name>` is the entity's own simple name as C++ spells it,
with `::` written as `.` for a nested one.

Front matter is `key: value`, one per line, a scalar or a bracketed
list, no nesting. Three keys are required — `kind`, `library`, `name` —
and the useful rest are `group` (the concern the kind's index sorts it
under), `qualified` (when the simple name is ambiguous), `example` (a
stem beside the library), `status`, `common_verbs`, `summary`, `title`,
`canonical_python` and `python_exclude`.

**Do not write `accepts:`, `returns:`, `doxygen:`, `python:` or
`header:`.** They are generated, a hand-held copy can only go stale,
and the report prints a `stale front matter` line for a page that
spells one.

Write the lede, `## Description`, `## Examples` and `## See also`, and
let the rest arrive. Writing a generated section anyway keeps yours:
the machine is right about the shapes and can still be wrong about a
page. `Parameters` and `Values` are one slot — a verb's parameters ARE
the values it takes — so writing either suppresses both.

### The grammar

Headings, paragraphs, fenced code, tables, links, lists, blockquotes,
inline code, emphasis, rules, and two directives. Anything else renders
as the text it is.

A fenced `cpp` block immediately followed by a fenced `python` block
becomes one two-tab widget, and the reader's choice of tab is
remembered across every page of the site.

`<!-- example: fill_verb -->` names a stem under the library's
`reference/examples/`. The picture stands above the two sources, which
are folded.

A link target may be written as a shorthand, which resolves to a
correct relative path from wherever the page ends up:

| Target | Goes to |
| --- | --- |
| `value:sigil::compose::Fill` | that value's page |
| `page:SigilCompose/verbs/fill` | that reference page |
| `guide:first-drawing` | `guides/first-drawing.html` |
| `overview:libraries` | `overview/libraries.html` |
| `glossary:element` | the glossary, at that term |
| `doxygen:SigilCompose` | that library's Doxygen site |

An absolute site path is rewritten relative, so the tree works opened
off the disk as well as served.

## The six kinds, and how a declaration is sorted into one

Sorting is by SHAPE, not by a list, so a library that grows a verb gets
it catalogued with nothing registered by hand.

| Kind | Rule |
| --- | --- |
| element | a free function returning the library's node type |
| verb | a member of the node type that hands the node back: by reference, which continues the chain, or by value, which ends it |
| type | a class, struct or union |
| kit component | an element factory declared under a `kit/` header |
| enumeration | an `enum` or `enum class` |
| function | a free function returning anything else |

**The node type is found, not declared.** A library's node is the class
most of whose members hand it back for chaining. Below ten such members
a library has no node, and then only types, enumerations and functions
apply.

The rule names whatever chains, which in a library with no element tree
is its one fluent builder: that builder's factory is then catalogued as
the library's single element and its setters as the library's verbs.
The catalogue says what the library is shaped like, not what its author
called things.

**A verb a class inherits from a mixin over itself is that class's
own.** `class Element : public BoxVerbs<Element>` writes `width` on the
mixin and means it on `Element`: the member hands back the template
parameter, and the parameter is the inheritor. So the reader files the
member under the inheritor — `Element::width`, returning `Element&`,
counted toward the node — while the header and the link into the
literal API stay the mixin's, which is where it is declared and where
Doxygen wrote it. The mixin itself is not catalogued as a type: nothing
takes one and nothing hands one back.

**A name only Python has is an entity too.** A binding that composes
two C++ calls into one, or takes a Python object where no header does,
has nothing on the C++ side to match. Where the stubs export it, it is
catalogued under the library whose vocabulary it extends — a verb when
it lands on that library's node, an element when it hands one back, a
function otherwise — and its page wears the `Python only` badge. A
dependency's own type, handed to Python as it stands, is not
catalogued at all: it has a reference of its own.

A name that two namespaces of one library spell is two pages, and the
one that has to say more says it: `functions/grid` and
`functions/mesh.points.grid`. Two entities are never written to one
file — the run stops and names both rather than putting one page over
the other.

## Three constraints the layer is built around

**A value has one page, not two.** A type's three tables are the same
wherever they are read from, so the page is `values/<qualified>.html`,
tree-wide, and the declaring library's `types/` and `enums/` indexes
link to it rather than carrying a second copy.

**Cross-library types are matched on their tails.** A signature carries
Doxygen's own identifier for a type its own library declares, and that
identifier is exact. The XML pass reads no tag files, so a type from
another library is plain text with no identifier at all: those are
matched on their tails, the enclosing namespace decides a tie, and a
tail that still fits several resolves to nothing rather than a guess.

**A page's generated sections are generated, not transcribed.** Syntax,
Parameters, Values, Members and the three type tables are read off the
XML on every build; a copy written by hand into a page can only go
stale, and the report prints one when it finds it.

## Keeping it true

```sh
python3 scripts/sigil.py docs --manifest build/docs-manifest.txt \
        --reference-only --report
```

prints every entity with no page, by name, with the header and line it
was declared at and the Python name it is bound under. The findings:

| Finding | What it means |
| --- | --- |
| `no page` | an entity nobody has written prose for |
| `no summary` | a page and a header that both say nothing |
| `no example` | a page with no `example:` stem |
| `orphan page` | a Markdown file naming no entity |
| `contradiction` | a binding naming something neither side declares |
| `unresolved link` | a `sigil.…` path a page spells that the stubs do not carry |
| `stale front matter` | a page spelling a key the generator writes |
| `unbound python` | a declaration in the stubs that reaches no page |

What pybind11 writes onto every class it binds is not a gap: Python's
own protocol methods and the `name` and `value` properties that come
with every bound enumeration are left out, because listing them would
bury the names a reader really has no page for.

`docs/reference_coverage.json` is the ledger, one row per library, so a
narrowed run raises its own library's count without discarding what it
never looked at. A rise is written; a DROP fails `--strict` and is not
written, so it stays visible until it is fixed or deliberately adopted.

C++ names inside a page are checked by the existing documentation
probe, unchanged: list the page in its library's
`sigil_doc_probes(… READMES …)` and every backticked qualified name in
it is compiled. Python names are checked here, against the stub tree.

## The parts

```
scripts/sigil/reference/
  __init__.py       generate(manifest, options)
  model.py          Entity, Signature, Parameter, Site, Page — the shared records
  doxygen_xml.py    reader one: the C++ surface, out of the XML inventory
  python_stubs.py   reader two: the Python surface and the role unions
  bindings.py       reader three: how a C++ name reaches Python
  catalogue.py      the three readers joined, and every kind decided
  graph.py          make one / pass it to / also returned by
  pages.py          one entity plus its prose, as one page's Markdown
  markdown.py       the restricted renderer and the front matter
  site.py           the shell, the search index, the stylesheet staging
  report.py         the coverage report and the ledger
  build.py          the order they go in
  test/             the generator's own fixtures, one ctest entry
docs/reference.css  the layer's styling, beside custom.css for Doxygen
docs/reference.js   the language tabs and the typed search
```

`ctest -R reference_generator` runs the fixtures: a small tree carrying
every shape the readers handle, so a reader that quietly stops matching
fails there rather than writing a thinner site that still looks like a
site. It needs no build tree and no Doxygen.

## The search is typed

The search a reader of this site needs is not full text over prose. It
is `accepts:Fill`, `kind:verb`, `library:SigilCompose`, `group:Paint`,
`python:no` — the fields `search-index.json` carries, which the
generator already holds because it built the type graph. Free words are
matched against the name first and the summary second. `/` focuses the
box.
