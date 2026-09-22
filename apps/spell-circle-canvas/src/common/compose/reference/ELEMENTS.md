# The elements

Every factory that starts a tree. An element is a free function
returning a node — a cheap value built fresh every frame and thrown
away — and everything else in this library is either something you SAY
to one (a verb) or something you PASS to a verb (a value).

Most factories return an `Element`. The three that make a LEAF with
verbs of its own return that leaf's own type — `text` and `frame` a
`Text`, `image` an `Image`, `band` a `Band` — which is a node plus what
only that leaf can be told, and which CONVERTS to an `Element` wherever
a node is wanted. Hold the leaf's own type for as long as its own verbs
are still to be written; a wrong-kind verb on a box does not compile.

```cpp
box().column().gap(12).children({
    text(u8"A heading", heading),
    image(plate).height(180),
});
```

The kit tier composes these same factories into whole components
(`kit::well`, `kit::plate`, the document family) and is a catalogue of
its own; nothing on this page needs it.

---

## The kernel

`core/Factories.h` holds the factories a scene is normally built from.

| Element | What it is | Children |
|---|---|---|
| [`box`](pages/elements/box.md) | A flex container, and a leaf when it has no children. | Laid out in a row or a column. |
| [`stack`](pages/elements/stack.md) | An overlap container: every child shares the box. | Absolute, painted in zIndex then declaration order. |
| `positioned` | A container whose children carry their own rects and skip flexbox entirely. | Placed by their own insets, Yoga-free. |
| [`point`](pages/elements/point.md) | A node with no extent that carries a key and facts and draws nothing. | None; it is out of the flow and out of hit testing. |
| [`text`](pages/elements/text.md) | A text leaf, in four content forms, as a `Text`. | Its marks and its slot mounts. |
| `frame` | One frame of a story, as a `Text`, which `key` names and `textThreadTo` links to the next. | The same. |
| [`image`](pages/elements/image.md) | An image asset, or a picture already rendered under one of three fits, as an `Image`. | None. |
| `picture` | A recorded picture as a leaf — the door out of a bake. | None. |
| `pathFigure` | A leaf the shape of a path already in canvas coordinates. | None. |
| [`custom`](pages/elements/custom.md) | A box whose content is one paint program, keyed or not. | None; it sizes like an empty box. |
| `layout` | A container whose children are placed by a scheme instead of flexbox. | Measured, then placed by the scheme. |
| `slot` | A named mount point whose content is supplied through the composer. | Rendered independently of the tree around it. |
| `memo` | Deferred description: the function runs only when its properties changed. | Whatever the function produced. |
| `each` | The children a range or a count describes, for a `children` block. | Not an element: a list of them. |

`material::skia::Fit` is the enum `image` takes: `Stretch`,
`Contain`, `Cover` or `Native`.
`Children` is what a `children({…})` block is made of — an element, or
the list `each` made, so one block mixes both.

## Derived from other nodes

`core/Derive.h` holds the factories whose geometry is READ OFF the tree
after layout, by the keys they name.

| Element | What it is |
|---|---|
| `connector` | A line from one keyed node to another, routed after both are placed. |
| `rail` | A line through a list of anchors, routed the same way. |
| `band` | A ribbon of stated width along a spine, as a `Band`, placed on it by `bandAlignment`. |

## Fields, feeds and the other leaves

| Element | Header | What it is |
|---|---|---|
| `instances` | `core/Instances.h` | One node drawing a whole pool of sprites from an atlas. |
| `feed` | `core/Feed.h` | A column of arrivals from a ring, oldest cut as new ones land. |
| `pen` | `draw/Draw.h` | A node running a pen program — the door to the immediate-mode pen. |
| `graphics` | `draw/Draw.h` | The same door with its own coordinate space, sized by the node. |
| `video` | `video/Video.h` | A video frame sampled from the composer's motion clock. |
| `web` | `web/Web.h` | A live web page as a leaf. |

## What Python spells differently

The children go in the CALL, not only in a verb: `compose.box(a, b)` and
`compose.box([a, b])` are both the block that C++ writes as
`box().children({a, b})`, and `.children(...)` is there as well. Every
container factory takes them that way.

`compose.picture(picture, width, height)` takes the recorded size as two
numbers where C++ takes one size value. `compose.pen` and
`compose.graphics` require the key, where C++ offers an unkeyed form
beside it.

Not bound: `each` — a Python comprehension is the same list — and
`custom`, `connector`, `rail`, `band`, `feed`, `instances`, `video` and
`web`. `compose.pen` is the Python door for a node that draws its own
content.

## Where they live

- `core/Factories.h` — `box`, `stack`, `positioned`, `point`, `text`,
  `frame`, `image`, `picture`, `pathFigure`, `custom`, `layout`, `slot`, `memo`
  and `each`; the fit a picture meets its box under is
  `material::skia::Fit`.
- `core/Derive.h` — `connector`, `rail` and `band`, with the `Anchor` a
  rail is strung through and the `RailRouter` that routes it.
- `core/Instances.h` — `instances`, and `pick`, which answers which
  instance a point is over.
- `core/Feed.h` — `feed` over a `Ring`, with the `Options` it is shaped
  by.
- `draw/Draw.h` — `pen` and `graphics`, which run a `PenProgram`.
- `video/Video.h` — `video`, with the `VideoOptions` it plays under.
- `web/Web.h` — `web`.
