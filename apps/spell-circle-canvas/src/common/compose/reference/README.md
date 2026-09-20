# The SigilCompose reference

The discovery surface over this library: what the elements ARE, what can
be said to one, and what to pass where. It answers the two questions the
literal API cannot, because an alphabetical list of members per class
cannot be sorted by concern and says nothing about where a value comes
from.

- [ELEMENTS.md](ELEMENTS.md) — every factory that starts a tree.
- [VERBS.md](VERBS.md) — every verb an `Element` takes, in fourteen
  concern groups.
- [VALUES.md](VALUES.md) — what those verbs accept, with where one comes
  from and what takes it.
- [COLOURING.md](COLOURING.md) — the chapter over the four values that
  all mean "what colour is this", and which one a verb wants.

Beside them are the model's own chapters, each linked from the section
of the library README it was written under:

- [CASCADE.md](CASCADE.md) — what flows down the tree, and how a role or
  a class resolves against a sheet.
- [DEPTH.md](DEPTH.md) — the depth lanes, the shared space and the
  projection.
- [HEADERS.md](HEADERS.md) — every public header, feature by feature,
  and the names it owns.
- [CACHING.md](CACHING.md) — the declared-volatility contract.
- [TRAPS.md](TRAPS.md) — the silent no-ops, lifetime, pruning and
  ordering.

And the type chapter's own chapters, indexed by
[TYPOGRAPHY.md](../TYPOGRAPHY.md): [TEXT_FX.md](TEXT_FX.md),
[TEXT_PATH.md](TEXT_PATH.md), [RICH_TEXT.md](RICH_TEXT.md),
[PARAGRAPHS.md](PARAGRAPHS.md), [BESIDE_TEXT.md](BESIDE_TEXT.md) and
[VERTICAL_TEXT.md](VERTICAL_TEXT.md).

Under `pages/` is one page per entity — `elements/`, `verbs/` and
`types/` — and under `examples/` the drawn example each page shows. The
library's own `README.md`, one directory up, stays the canon for the
MODEL — the phases, the write paths, the boundaries, and where each
chapter above is. This is
the catalogue over it, and a sentence here that disagrees with the
README is a defect in this directory.

## What a page looks like

One file per entity, `pages/<kind>/<name>.md`, in the order a reader
needs it:

1. **Front matter** — `kind`, `library`, `name`, `qualified`, `header`,
   `group`, `python`, `status`, `example`. The site assembler reads it;
   a reader does not. `accepts`, `returns` and `doxygen` are written by
   the assembler from the compiled surface and are never hand-edited.
2. **The name, and a summary** — two or three sentences saying what the
   thing IS and what it is for, with no list in them.
3. **The drawn example**, referenced as `<!-- example: <stem> -->`,
   where `<stem>` names a pair under `examples/`.
4. **Syntax**, C++ then Python, in two fenced blocks that the assembler
   renders as one tabbed widget.
5. **Parameters**, one row per value, each linking to the row in
   [VALUES.md](VALUES.md) that says where one comes from. A Python
   paragraph under the table expands the accepted union where it is
   wider than the C++ one.
6. **Description** — the behaviour that is not in the signature. State
   the rule; do not narrate the API.
7. **Examples** — the files under `examples/`, named.
8. **See also** — the neighbouring pages, the verbs most often written
   beside this one.

A value that has no page of its own is linked to its row in
[VALUES.md](VALUES.md); when the generated page for that value exists,
the assembler resolves the link to it.

## The examples

Each example is a PAIR: `examples/<stem>.cpp`, a sketch-shaped C++ file,
and `examples/<stem>.py`, the same picture in Python.

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
    src/common/compose/reference/examples/fill_verb.cpp --frame fill_verb.png
sigil render src/common/compose/reference/examples/fill_verb.py \
    --output fill_verb.python.png
```

They stand OUTSIDE the sketch registry, which walks the compiled-in
table, so an example is photographed with `--frame` and never enters the
plate sweep: nothing here can make a scene a mover.

Three rules hold for every pair. It is **small** — one picture, one
subject, no kit components and no theme, so the page shows the verb
rather than a house style. It is **deterministic** — a still captured at
a stated moment, with nothing read from the clock that the capture does
not pin. And the two files **draw the same picture**, which is what
makes "here is the Python spelling" a claim rather than a hope.

Where Python cannot spell the C++ one at all, the page says so in its
Python column and the twin reaches the same picture through the door
Python does have. `custom` is the case in this slice: the Python twin
draws through `compose.pen`.

## How this stays true

Every document here is listed in the compose library's
`sigil_doc_probes()` call — the three indexes, the colour chapter, the
model's chapters and the type chapter's, every
page, and this file — so each qualified name it spells is compiled
against the header that owns it, and the bare names of a bullet that
opens with a header path are checked against that header. A rename the
prose misses is a build break, and a page added without that one line is
a page nothing checks.

Two things the probe cannot see, and the site assembler owns: a Python
spelling, which lives in a fenced `python` block, and an entity with no
page at all. The assembler reads the compiled surface and the stubs and
reports both, along with a page whose entity no longer exists.

What neither of those two reads is whether an example DRAWS. That is the
assembler's `--example-images` pass, which renders each pair in both
languages and fails on the one that did not: a twin can name only real
attributes and still die on the first frame, so a pair is not checked
until it has been rendered.

## Python spellings

The names on these pages are the ones an author types:

```python
from sigil import compose, material
```

`material.Color` is the one colour class. The paint and the effect sit
one level down, in the `skia` submodule that carries what compiles to a
Skia shader: `material.skia.Paint` and `material.skia.Effect`. That
submodule is the spelling the stubs annotate and the spelling an author
types, so it is what every page and every twin here writes.
