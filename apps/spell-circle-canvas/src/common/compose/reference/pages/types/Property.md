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
| Placement | `Absolute` `Left` `Top` `Right` `Bottom` `CenterAt` `GridCells` `GridArea` |
| Shape | `BorderRadius` `Shape` `Overflow` |
| Paint | `Fill` `Opacity` `BlendMode` `BackgroundOrigin` `ZIndex` |
| Transform | `TranslateX` `TranslateY` `Rotate` `Scale` `ScaleX` `ScaleY` `SkewX` `SkewY` `TransformOrigin` |
| Depth | `RotateX` `RotateY` `TranslateZ` `ScaleZ` `Perspective` `PerspectiveOrigin` `TransformOriginZ` `Preserve3d` `Backface` |
| Decoration | `DecorationOutline` |
| Cascade | `Font` `Block` `Ink` `CustomProperties` `ImageRendering` |

`compose::propertyName` answers a property's authored spelling, which is
what a diagnostic prints.

## What inherits

`compose::inheritsByDefault` is the one table, and its answer is CSS's
set as this library spells it: `Font`, `Block`, `Ink`,
`CustomProperties` and `ImageRendering`. Everything else is a statement
about ONE box — a padding taken from the parent would be applied again at
every depth — so nothing in the box, the flex line, the placement, the
fill, the silhouette, the transforms or the plane inherits.

A property that does not inherit can still be told to, one node at a
time, with `inherit`.

## The three keywords

`compose::Keyword` is `Inherit`, `Initial` and `Unset`, and
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
pruning against its old answer.

@trap Written after a value on the same node, the keyword is what stands:
`width(120).initial(Property::Width)` is auto. The two are the same
layer, and there the later statement wins.

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
inherited value and to the prune. Every declaring verb sets its bit in
the same statement that writes its field, and the reconciler compares the
mask before it compares a single number.

## See also

- `core/Property.h` — the header: `Property`, `PropertyMask`, `Keyword`,
  `inheritsByDefault`, `resolveKeyword`, `KeywordTable`, `propertyName`
- The cascade chapter on the [SigilCompose](doxygen:SigilCompose) site —
  what the five inheriting properties are and how a rule reaches them
