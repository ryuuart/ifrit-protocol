---
kind: type
library: SigilCompose
name: Property
qualified: sigil::compose::Property
group: Cascade
status: stable
---

# Property

EVERY VALUE A NODE OR A RULE MAY STATE, as one name. It is what the
three wide keywords — `inherit`, `initial`, `unset` — take as their
argument, what the declared mask records a bit per, and the whole of the
answer to "which properties inherit".

Things a node does that are NOT properties have no enumerator here and
never will: the decoration lists, the masks and the stroke passes APPEND
rather than state, and the key, the role, the classes, the applied sheets
and the cache mode are what the node IS. A keyword said about one of
those would mean nothing.

## Anatomy

The per-side names are the properties, and `padding`, `margin` and
`inset` are shorthands that state four of them at once — CSS's own
arrangement, so `Property::PaddingLeft` is a thing you can inherit while
`padding` is not a thing you can name.

| Group | Properties |
| --- | --- |
| Box | `Display` `BoxSizing` `Gap` `PaddingTop` `PaddingRight` `PaddingBottom` `PaddingLeft` `MarginTop` `MarginRight` `MarginBottom` `MarginLeft` `Width` `Height` `MinWidth` `MaxWidth` `MinHeight` `MaxHeight` `AspectRatio` |
| Flex | `FlexDirection` `FlexWrap` `FlexBasis` `FlexGrow` `FlexShrink` `AlignItems` `AlignSelf` `JustifyContent` |
| Placement | `Absolute` `Left` `Top` `Right` `Bottom` `CenterAt` `GridCells` `GridCellAlign` `GridArea` |
| Shape | `BorderRadius` `Shape` `Overflow` |
| Paint | `Fill` `Opacity` `BlendMode` `BackgroundOrigin` `ZIndex` |
| Transform | `TranslateX` `TranslateY` `Rotate` `Scale` `ScaleX` `ScaleY` `SkewX` `SkewY` `TransformOrigin` |
| Depth | `RotateX` `RotateY` `TranslateZ` `ScaleZ` `Perspective` `PerspectiveOrigin` `TransformOriginZ` `Preserve3d` `Backface` |
| Decoration | `DecorationOutline` |
| Cascade | `Font` `Paragraph` `Ink` `CustomProperties` `ImageRendering` |

`compose::propertyName` answers a property's authored spelling, which is
what a diagnostic prints. `compose::answersKeyword` answers whether a
keyword said about a property resolves anywhere: the plane a node turns
in, the silhouette's generator, the grid area and the outline the
decorations dress are kept on the description, which no fold reads, so a
keyword about one of them is refused at the verb and said once rather
than taken and dropped.

## What inherits

`compose::inheritsByDefault` is the one table, and its answer is CSS's
set as this library spells it: `Font`, `Paragraph`, `Ink`,
`CustomProperties` and `ImageRendering`. Everything else is a statement
about ONE box — a padding taken from the parent would be applied again at
every depth — so nothing in the box, the flex line, the placement, the
fill, the silhouette, the transforms or the plane inherits.

`compose::kInherited` is the list that table builds at compile time, and
the fold walks it: moving a property into the set is the one line in
`inheritsByDefault`, with nothing else to write.

A property that does not inherit can still be told to, one node at a
time, with `inherit`.

## The three keywords

The three words are the text engine's — `sigil::weave::Keyword` is
`Inherit`, `Initial` and `Unset`, so a field of a text-style partial and
a property of an element are written as the same thing — and
`compose::resolveKeyword` is what `unset` asks: inherit where the
property inherits, initial where it does not.

| Verb | What the property becomes |
| --- | --- |
| `Element::inherit(p)` | the PARENT's computed value for `p`, whether or not `p` inherits. At the root, which has no parent, this reads as `initial`. |
| `Element::initial(p)` | the value `p` has where nothing anywhere states it. This is how an inheriting property is stopped. |
| `Element::unset(p)` | whichever of the two `p`'s own behaviour asks for — "as if I had not written this", which is not the same as "leave it alone" when a rule might have. |

A keyword IS a declaration: it sits in the node's own layer, over a rule
and over anything inherited, and two descriptions that differ only in a
keyword are unequal, so a node that gains one re-lays out rather than
pruning against its old answer. That layer is the reason `inherit` says
something about a property that inherits ANYWAY: the value arriving from
the parent is the weakest layer of the five, and the keyword drops the
role default, the rule and the node's own verb that were folded over it.

A keyword and a value about the same property are ONE layer, and the
statement written second is the one that stands:
`width(120).initial(Property::Width)` is auto, and
`initial(Property::Width).width(120)` is a hundred and twenty.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Element::inherit`, `Element::initial`, `Element::unset` | verb | SigilCompose |
| `compose::inheritsByDefault`, `compose::propertyName` | function | SigilCompose |

## Description

The mask beside the values is the reason this enumeration exists at all.
A field holds its type's default until a verb writes it, so the numbers
alone cannot tell a node that STATES the default from one that says
nothing about it — and those are different nodes to a rule, to an
inherited value and to the prune. Inside the library every property has
one writer, and the writer is the declaration: it sets the property's bit
as it hands back the field. For the properties the computed style carries
— the box, the flex line, the placement, the corners, the clip, the paint
and the 2D transform — nothing else reaches the field, so a verb that
skipped the bit would not compile. The seventeen kept on the element
itself — the depth lanes, the shape, the grid area, the decoration outline
and the five the cascade resolves — have writers that set their bits the
same way, but their storage can still be reached around the writer. The
reconciler compares the mask before it compares a single number.

A few values a node starts with are defaults rather than statements, and
carry no bit: the zero size and the out-of-flow placement of a `point()`,
the flag that makes a `positioned()` container, and the z-index an
operator gives the elements it adds where they state none. Only those
sites can write a value without its bit. A rule that matches such a node
stands over them, as it stands over any default, and an element's own
statement stands over its operator's z-index even where it states the
default.

## See also

- `core/Property.h` — the header: `Property`, `PropertyMask`,
  `inheritsByDefault`, `kInherited`, `resolveKeyword`, `answersKeyword`,
  `propertyName`
- `sigil::weave::Keyword` — the three words themselves, and
  `sigil::weave::KeywordTable`, which records the fields written as one
- The cascade chapter on the [SigilCompose](doxygen:SigilCompose) site —
  what the five inheriting properties are and how a rule reaches them
