# The verbs

Everything an `Element` can be told, grouped by what it is about. A verb
is a member that returns the element by reference, so a node reads as one
expression:

```cpp
box().row().gap(12).padding(18).borderRadius({10}).fill(Fill::color(ink))
```

One verb ends the chain rather than continuing it: `atRest` hands back a
SECOND element by value, because a rest pose is something to place
beside the moving copy rather than a state of it. Its row below says so,
and it is the only row that does.

One hundred and fourteen of them, in fourteen concerns. Each row says
what the verb SAYS, in one line; the page behind a linked name says what
it takes, what Python spells, and shows it drawn. An unlinked name has no
page yet, and its line here is the whole of what this reference claims
about it.

Three things to know before reading a group:

- **A verb that names a CSS-inherited property writes the lane and
  inherits.** `font`, `block` and `ink` reach every node under the one
  that said them, wherever the code that built the child ran. Everything
  else a node says about itself stays on that node.
- **Paint-only verbs never relayout.** The transforms, the masks, the
  opacity and the blends are read at paint, so animating one costs a
  repaint and no line is broken again.
- **Repeated calls in the decoration slots APPEND.** Two `stroke` calls
  are two rings, not one replaced ring. Every other verb's later call
  replaces the earlier one.

Python spells each verb exactly as C++ does unless *What Python spells
differently* at the foot of this page says otherwise.

---

## Flow and placement

How a node lays its children out, and how a node that has left the flow
is placed.

| Verb | What it says |
|---|---|
| `flexDirection` | Which way the main axis runs and from which end: `FlexDirection::Column`, `ColumnReverse`, `Row`, `RowReverse`. |
| `row` | `flexDirection(FlexDirection::Row)`: the children run left to right. |
| `column` | `flexDirection(FlexDirection::Column)`: top to bottom, which is what a node does unasked. |
| `flexWrap` | What becomes of children that overflow the main axis: `FlexWrap::NoWrap`, `Wrap`, `WrapReverse`. The bare call wraps. |
| `alignItems` | Where the children sit on the cross axis. |
| `alignSelf` | Where THIS child sits on its parent's cross axis, whatever the parent said. |
| `justify` | Where the children sit along the main axis, and how the slack is shared. |
| `absolute` | Take this node out of the flow; its insets place it. |
| [`cover`](pages/verbs/cover.md) | Fill the box it stands in — out of the flow and stretched to the parent. |
| `inset` | How far in from the parent's edges this node's own stand: one length for all four, or four. |
| `left` | Pin the left edge, implying `absolute`; the unpinned sides stay auto. |
| `top` | Pin the top edge, implying `absolute`. |
| `right` | Pin the right edge, implying `absolute`. |
| `bottom` | Pin the bottom edge, implying `absolute`. |
| [`at`](pages/verbs/at.md) | Pin the top-left to a parent-space point and let the content size the node. |
| [`rect`](pages/verbs/rect.md) | Place the node on a parent-space rectangle — the point and the box at once. |
| `centerAt` | Centre the node on a parent-space point, measured after layout. |
| [`tether`](pages/verbs/tether.md) | Hang the node off a keyed one at a stated pair of points, with fallbacks. |
| [`cells`](pages/verbs/cells.md) | Which cells of the `layout` scheme above this child it claims, and how many it covers. |
| [`area`](pages/verbs/area.md) | Which NAMED region of the scheme above it this child claims. |
| `cellAlign` | Where the child sits inside the cell box its span makes. |

## Size and spacing

What a node asks to be, and the air around and inside it. Every length
here is a `Dimension`: a bare number is pixels, a percent is of the
parent, and a font-relative length measures against the type in force.

| Verb | What it says |
|---|---|
| `width` | The flex BASIS across, not a guarantee — pair with `shrink(0)` for a promise. |
| `height` | The same down the other axis. |
| `minWidth` | The floor under the resolved width. |
| `maxWidth` | The ceiling over it. |
| `minHeight` | The floor under the resolved height. |
| `maxHeight` | The ceiling over it. |
| `aspect` | Width over height, for a node whose other axis is free. |
| `boxSizing` | What `width` and `height` measure: `BoxSizing::BorderBox`, the padding included, unless `ContentBox` is said. |
| [`display`](pages/verbs/display.md) | Whether the node has a box: `Display::Flex`; `None`, which removes it and its subtree from layout, picture and hit test; `Contents`, which hands its children to its parent's line. |
| `basis` | The flex basis outright, when it is neither the width nor the height. |
| `grow` | The share of the leftover main-axis room this child takes. |
| `shrink` | The share of the overflow this child gives back; 1 unless stated. |
| `gap` | The air BETWEEN the children, on the main axis. |
| `padding` | The air inside the node, on one, two or four sides. |
| `margin` | The air outside it, on one, two or four sides. |

## Paint

What the node is painted with, and the marks it wears. The stacking order
is a contract: backgrounds under the fill, the fill, overlays, the
content and children, then foregrounds and the unqualified strokes.

| Verb | What it says |
|---|---|
| [`fill`](pages/verbs/fill.md) | The paint of the node's own box — a colour, a paint, a transition or a live binding. |
| [`ink`](pages/verbs/ink.md) | The colour text under this node is set in, and every mark that names none. Inherits. |
| [`stroke`](pages/verbs/stroke.md) | Dress the node's boundary with a brush, whole or on the runs a span claims. |
| [`background`](pages/verbs/background.md) | A decoration painted BENEATH the fill. |
| [`overlay`](pages/verbs/overlay.md) | A decoration painted over the fill and under the content and children. |
| [`foreground`](pages/verbs/foreground.md) | A decoration painted OVER the children. |
| `style` | A whole `LayerStyle` at once: its under layers become backgrounds, its over layers foregrounds. |
| `echo` | A misprint echo: the fill shape and the text re-stamped offset and flat beneath the real pass. |
| [`textFill`](pages/verbs/textFill.md) | Paint the GLYPHS with a material mapped to text-metric space. |
| [`textStroke`](pages/verbs/textStroke.md) | Stroke the glyphs, under their fill. |
| [`boundary`](pages/verbs/boundary.md) | WHICH outline the decorations dress: the node's shape, its glyphs, or what it drew. |
| [`threshold`](pages/verbs/threshold.md) | How much paint counts as ink when the boundary is traced off coverage. |

## Shape, corners and clipping

The region the node occupies, and what is cut to it.

| Verb | What it says |
|---|---|
| [`borderRadius`](pages/verbs/borderRadius.md) | The four corner radii of the node's box. |
| [`shape`](pages/verbs/shape.md) | The node's outline as a path generator over its laid-out size; it overrides `borderRadius`. |
| [`clip`](pages/verbs/clip.md) | Cut the fill, the content and the children to that outline — the decorations keep their reach. |
| [`mask`](pages/verbs/mask.md) | Gate what the node paints, by span, edge, shape or alpha; overlapping masks intersect. |
| `centered` | A band straddles its spine — the default formation. |
| `outward` | A band takes the outer side of its spine. |
| `inward` | A band takes the inner side. |

## Effects, blending and opacity

What happens to the node's painted result.

| Verb | What it says |
|---|---|
| [`effect`](pages/verbs/effect.md) | Post-process the node's own rendered layer. |
| [`backdrop`](pages/verbs/backdrop.md) | Filter what is already painted beneath the node before it paints. |
| [`blend`](pages/verbs/blend.md) | How the node's paint meets what is under it. |
| [`opacity`](pages/verbs/opacity.md) | Fade the node and everything under it as one group. |

## Transform

The plane the node paints on, moved in two dimensions. All paint-only:
animating one never relayouts.

| Verb | What it says |
|---|---|
| `translateX` | Move the plane across, in px. |
| `translateY` | Move it down, in px. |
| [`travel`](pages/verbs/travel.md) | Ride a curve instead of two lanes — a motion path, with auto-orientation. |
| `rotate` | Turn the plane about the transform origin, in degrees. |
| `scale` | Scale both axes about the transform origin. |
| [`scaleX`](pages/verbs/scaleX.md) | Scale across, multiplied INTO `scale` — the bar, the wipe, the meter. |
| `scaleY` | Scale down the other axis, the same way. |
| `skewX` | Shear the verticals, in degrees; a negative one is the italic lean. |
| `skewY` | Shear the horizontals. |
| [`transformOrigin`](pages/verbs/transformOrigin.md) | The pivot every lane turns about: a percentage of the node's box or a length in it, and a depth. |

## Depth

The CSS 3D model over the 2D tree: a node is a plane that projects onto
its parent's. Paint-only, like the transforms.

| Verb | What it says |
|---|---|
| `rotateX` | Turn the plane about its horizontal axis; positive tips the bottom toward the viewer. |
| `rotateY` | Turn it about its vertical axis — the card-flip lane. |
| `translateZ` | Move the plane along the viewing axis; invisible without a `perspective` above it. |
| `scaleZ` | Scale the depth of the children a shared space hosts. |
| `perspective` | The view this node's CHILDREN are seen through, in px in front of the plane. |
| `perspectiveOrigin` | Where the viewer stands over the plane, written as `transformOrigin` writes its pivot. |
| [`preserve3d`](pages/verbs/preserve3d.md) | The children keep their own depth and are painted back to front by it. |
| `backface` | Whether the back of the plane is drawn once a lane has turned it away. |

## Entrances and transitions

| Verb | What it says |
|---|---|
| `appear` | The node fades in when it mounts, over the stated transition. |
| `transition` | The node's default transition for the plain constants set on it. |
| `staggerChildren` | Child *i*'s subtree enters with an extra delay, compounding through nested containers. |

## Caching

| Verb | What it says |
|---|---|
| `cache` | How the node's paint is held: a picture, a texture, a group, or nothing. |
| [`bakeScale`](pages/verbs/bakeScale.md) | The texture bake's resolution multiplier — it cheapens the bake and taxes every blit. |

## The cascade

What flows down the tree from a node to everything under it, wherever
the code that built a child ran.

| Verb | What it says |
|---|---|
| [`font`](pages/verbs/font.md) | The type everything under this node is set in, as a PARTIAL over what it inherits. |
| [`block`](pages/verbs/block.md) | The paragraph settings everything under it is set in, as a partial in the same way. |
| `styleSheet` | The sheet this subtree resolves its classes and roles through. |
| `styleClass` | The classes the sheets in force register, folded in left to right under the node's own type. |
| `role` | A semantic role with default typography, overridden by the sheet, the classes and the node. |
| `var` | A custom property set here and inherited by everything under it. |
| `varDefaults` | Fallback custom properties, which an inherited or locally set property overrides. |

The third inherited lane is `ink`, in *Paint* above: it is CSS's `color`,
and it reaches every text leaf and every unnamed mark under the node.

## The text leaf

Verbs a text, frame or rich-text leaf reads; on any other node they warn
once and do nothing.

| Verb | What it says |
|---|---|
| [`paragraphs`](pages/verbs/paragraphs.md) | How each BLOCK of the passage is set, one entry per block, by value or by name. |
| [`initialLetter`](pages/verbs/initialLetter.md) | The passage opens on a versal sized to span the lines it is given. |
| `firstBaseline` | Where the first baseline sits below the top of the leaf's box. |
| `distribute` | What becomes of the room left over down the box — nothing, split, above, or between the lines. |
| `reserve` | Room beside every line, over and above the leading, taken in the strut before breaking. |
| `maxLines` | Use at most this many lines; the rest reports as overflow. |
| `ellipsis` | The marker appended to the last line when the text overflows its geometry. |
| [`live`](pages/verbs/live.md) | An input of this passage is moving, so the layout is one of a run rather than an answer. |
| [`thread`](pages/verbs/thread.md) | The frame this one fills into — the next link of a chain over one story. |
| [`balanceChain`](pages/verbs/balanceChain.md) | This frame opens a balanced run: every frame of it resolves to one shallowest depth. |
| [`flowAround`](pages/verbs/flowAround.md) | Flow this paragraph around the keyed node, by its silhouette or its box. |
| [`annotate`](pages/verbs/annotate.md) | A reading set beside the type — furigana, emphasis dots, a gloss — reserved before breaking. |
| [`spanPaint`](pages/verbs/spanPaint.md) | Repaint the range a selector finds; never re-shapes. |
| [`spanStyle`](pages/verbs/spanStyle.md) | Restyle that range with a whole style or a partial; re-shapes only the words it covers. |
| `fx` | Append a text-fx track: which glyphs, what deviation, how the beats spread, what drives it. |
| [`variationDrive`](pages/verbs/variationDrive.md) | Drive a variable-font axis from a bound output at draw time, with no reshape. |
| [`mark`](pages/verbs/mark.md) | A sibling anchored to a unit of the text, placed on the rect that unit rests at. |
| `onPath` | Lay the run out along a path instead of a line. |
| [`atRest`](pages/verbs/atRest.md) | This leaf as it stands at rest, RETURNED BY VALUE as a second element that can stand beside it — the one verb that does not chain. |

## Content

| Verb | What it says |
|---|---|
| `region` | Image leaves: draw this sub-rect of the asset instead of the whole picture. |
| [`sampling`](pages/verbs/sampling.md) | How image leaves under this node sample their source. Inherits. |

## Identity, layering and hit testing

| Verb | What it says |
|---|---|
| `key` | The author-owned identity: what the reconciler matches by and what geometry is borrowed by. |
| `zIndex` | The paint order among siblings that share a box. |
| `hitTestable` | Take the node's own box out of hit testing; its children are still tested. |

## Children

| Verb | What it says |
|---|---|
| [`children`](pages/verbs/children.md) | What is in the node, in order, after every verb that says what is done to it. |

## What Python spells differently

Five verbs have no Python binding: `fx`, `variationDrive`, `mark`,
`mask` and `tether`.

Six spellings exist only in Python, each composing verbs that C++ writes
out: `size(width, height)` is `width` then `height`; `fontSize`,
`fontTrack` and `fontWeight` each write one field of `font`; and `copy`
(with `__copy__`) takes the value copy that C++ gets from assignment.

## Where they live

Every verb on this page is declared in one header, and the include
spelling is the feature's.

- `core/verbs/Flex.h` — the flex verbs `flexDirection`, `row`, `column`,
  `flexWrap`, `grow`, `shrink`, `basis`, `alignItems`, `alignSelf`,
  `justify`.
- `core/verbs/Box.h` — the box verbs `gap`, `padding`, `margin`,
  `width`, `height`, `minWidth`, `maxWidth`, `minHeight`, `maxHeight`,
  `aspect`, `boxSizing`, `display`.
- `core/verbs/Placement.h` — the placement verbs `absolute`, `cover`,
  `inset`, `left`, `top`, `right`, `bottom`, `centerAt`,
  `cells`, `area`, `cellAlign`, `rect`, `at`.
- `core/verbs/Shape.h` — the region verbs `borderRadius`, `shape`, `clip`.
- `core/Band.h` — the band formation `centered`, `outward`, `inward`.
- `core/verbs/Mask.h` — `mask`.
- `core/verbs/Cascade.h` — the cascade verbs `font`, `block`, `ink`,
  `var`, `varDefaults`, `sampling`.
- `core/verbs/Paint.h` — `fill`.
- `core/verbs/Decoration.h` — the decoration slots `stroke`,
  `background`, `overlay`, `foreground`, `style`, `echo`, and what they
  dress: `boundary`, `threshold`.
- `core/verbs/Effects.h` — `effect`, `backdrop`, `blend`, `opacity`,
  `appear`.
- `core/verbs/Transform.h` — the transform lanes `translateX`,
  `translateY`, `travel`, `rotate`, `scale`, `scaleX`, `scaleY`,
  `skewX`, `skewY`, `transformOrigin`, and `zIndex`.
- `core/verbs/Depth.h` — the depth lanes `rotateX`, `rotateY`,
  `translateZ`, `scaleZ`, `perspective`,
  `perspectiveOrigin`, `preserve3d`, `backface`.
- `core/verbs/TextStyle.h` — the text properties `paragraphs`,
  `initialLetter`, `firstBaseline`, `distribute`, `reserve`, `live`,
  `ellipsis`, `maxLines`, `textFill`, `textStroke`, `flowAround`.
- `core/Text.h` — the text leaf's own content `fx`, `variationDrive`,
  `mark`, `annotate`, `thread`, `balanceChain`, `onPath`, `spanPaint`,
  `spanStyle`, `atRest`.
- `core/Image.h` — `region`.
- `core/Element.h` — the cascade a node names, `styleSheet`,
  `styleClass` and `role`; `tether`, `key`, `hitTestable`, `cache`,
  `bakeScale`, `transition`, `staggerChildren`; and `children`, whose
  runs are the `Children` value.
