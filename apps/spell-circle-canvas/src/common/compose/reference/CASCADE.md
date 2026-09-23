# The cascade

A chapter of [SigilCompose's README](../README.md).

Five things flow down the TREE, from a node to everything under it,
wherever the code that built a child ran: the font a passage is set in,
its colour, which is the ink, the block its paragraphs are set in, the
sheet its classes resolve through, and the custom properties. Everything
else
a node says about itself — its fill, its stroke, its padding, its
transform — stays on that node. It is CSS's own split between the
properties that inherit and the ones that do not, and the rule of thumb
transfers whole: text properties inherit, box properties do not.

```cpp
box().font({.face = serif, .size = 14}).ink(hexColor(0xe6e6ea))
    .children({text(u8"Signal")})                      // the font and ink above
    .children({text(u8"Heading").font({.size = 22})})  // the size alone; the rest inherits
    .children({box().stroke(stroke(1.5f))});           // no colour named: the ink
```

**A leaf that names no style is set in the font and ink in force where
it lands.** `text(utf8)` is that leaf. `Element::font` on any node is a
PARTIAL, a `weave::Type` whose every field is optional: the fields it
names override the inherited font and the rest inherit, so
`font({.size = 22})` is the inherited face and colour at another size,
and `font({.size = 1.5_em})` is half again the size inherited; a face
stated as `weave::defaultFace()` is the font context's default family
outright, the way back under an ancestor that named one. The font
is everything a passage inherits: the face, size, tracking, condensation
and axes; the features, language, optical kerning, word spacing, case and
vertical form a run is shaped with; and the paint around the colour — the
line decorations and the passes beneath and above the glyphs, a halo or a
shadow — where a decoration or a pass that names no colour is drawn in
the ink. What stays on a whole `weave::TextStyle` is the foreground
paint's own state beyond its colour: a shader, a blur, a blend.
`Element::ink` is the font's colour spelled alone — CSS's `color` — which
text under the node is set in, which every mark that names no colour is
painted in (`stroke(1.5f)` with no fill, a line or a ribbon written as
`Fill::currentInk()`), and which `Fill::currentInk()` reads back wherever
a slot demands a fill. A leaf given a whole `weave::TextStyle`,
`text(utf8, style)`, is set in it alone and inherits nothing, which is
what every leaf written before this chapter existed does. What a leaf
under nothing is set in is `Composer::setInherited`: black, 16 px and the
font context's family by default, the values `weave::initialType()`
carries, and what a pen's guest or a texture scene seeds from where it
stands.

**Resolution is the tree's, not the call stack's.** The cascade pass runs
over the retained tree after describe and before layout, top-down, each
node's font, ink and properties the parent's overlaid with its own
declaration. So a child built first and adopted later is set in the
adopter's ink, a slot's content inherits from where the slot stands
however it was described, and a description holds only what was written
— a partial, a colour, a property — never a resolved value, which is what
keeps the prune exact. A bake is a root: `snapshot`, an atlas cell, a
pattern tile and `compose::texture` resolve against the initial values,
as an image placed on a page inherits nothing from it.

**A block is the same kind of partial.** `Element::block` takes a
`weave::Block`, every field optional — leading and where its room goes,
alignment and the last line, justification, hyphenation, tab stops, the
first- and last-line indents, widows and orphans, balanced ragging, the
breaking strategy, the writing mode, the line-break locale and the line
tables CJK text is set by, CSS's inherited block properties — and every
text leaf under it sets its paragraphs in the block in force, a partial
on the leaf itself included. A whole `weave::ParagraphStyle` the leaf
wrote through `paragraphStyles` inherits nothing, as a whole text style does,
and a block named through `paragraphStyles(names)` is that name's partial laid
over the block in force when the leaf lays out. `block()` is the ONE
spelling of every one of those fields — `block({.alignment =
TextAlignment::kCenter})` on any node centres every line under it,
`block({.writingMode = WritingMode::kVerticalRL})` sets the text under it
in vertical columns — and a sheet's rule states the same partial under a
name, so there is one property under each name and it inherits. What a
node keeps to itself is what CSS
keeps there: its ellipsis and line count, its frame's first baseline and
distribution, its reservation, its threading, its exclusions, its initial
letter, and a whole style — and, on a whole style, the block's air before
and after, its keeps with the next block and its every-line insets, as a
margin is a box's own. Image sampling inherits the same way, as CSS's
`image-rendering` does: `Element::imageRendering` on any node reaches every
image leaf under it.

**A range and a reading take the same partial.** `Text::spanStyle`
with a `weave::Type` lays the fields it names over the style the range is
set in — the inherited font for an inheriting leaf, the leaf's own style
otherwise — and a partial naming no shaping field repaints without
re-shaping. A reading's style (`Annotation`, `kit::ruby`, `kit::kenten`),
a nested run (`kit::NestedStyle`), a list's items (`kit::bullets`) and an
initial letter's are partials over the text they belong to, so a reading
at `0.5_em` is half its base whatever the base inherits. A caption's and
a sheet's lines (`kit::Caption`, `kit::Sheet`) take the same fields by
NAME instead — the `label`, `caption`, `h1`, `lead` and
`footer` document roles of the sheet in force where the component lands, which
is what a theme states on a page's root; each such line is a `kit::Part`,
so a cell whose call must differ hands in its own leaf and the cells
under it keep the register. A bake — `snapshot`,
`intrinsicSize`, `kit::coverage` — runs the cascade over its own tree,
so a partial inside it resolves against the bake's root.

**A sheet is a value applied to a subtree, and a role or class is a name
its rules speak about.** `Element::applyStyleSheet` puts a
`compose::StyleSheet` in force for that node and everything under it, so a
subtree carries a look of its own; [the selectors chapter](SELECTORS.md)
is the whole of how a rule is written and which one wins. A rule states
a font partial and a block partial, with the verbs a tree is written
with: `rule("body").font({.size = 19.5f}).block({.leading =
Leading::multiple(1.35f)})`.
`Element::styleClass` names classes: several in one call, separated by
spaces as CSS's class attribute lists them. The cascade pass matches the
rules in force where the element LANDS, and the fields a matched rule
sets inherit down the tree like any `font()` or `block()`. The order is
CSS's: role defaults stand below every rule, a rule for a class stands
over a rule for a role by its weight, rules of one weight fall to the
nearer and later sheet and then to the later rule, and every rule loses to
the node's own `font()` or `block()`, so `styleClass("cell").font({.color
= c})` is the cell class in this cell's colour. A class no rule in force
names warns once and sets nothing. A run of a `weave::rich()` value
written with a name resolves the same way when the leaf is shaped, as a
virtual child of the leaf whose class is the name, unless the value names
a `weave::TypeSheet` of its own.
`weave::rich()` started with no base
is an inheriting passage: a run added with a partial keeps the inherited
face and size in every field it does not name, and only a run added with
a whole style keeps the style it was written with. Blocks have the same
discipline through the block half of the sheet and `Text::paragraphStyles`.

**A custom property is set on a node and read by anything under it.**
`Element::var` sets one; `var(name)` reads it as a `Dimension`,
`Fill::var(name)` as a fill, and `ink(var(name))` as the ink, the nearest
ancestor that set the name winning. It reaches exactly what the kernel
resolves — a fill, a stroke, a mark, a length, the ink — and no further:
a material, a layer style and every other value the kernel cannot see
inside take concrete values, so inside those the look is still read
where it is written, through `core::environment::Provide`. That channel
is LEXICAL, read by the code that builds an element; the cascade is
STRUCTURAL, carried by the tree the element ends up in. One sentence
holds both: the tree carries the font, the ink and the properties; code
reads the look.

**Lengths measure against the font.** `Dimension` takes SigilWeave's
`weave::Length` and its literals: `1_em` on a box property is the node's
own resolved font size, `1_em` on `weave::Type::size` is the parent's,
`0.5_lh` is half the node's line height, `1_rem` is the root's size, and
a length written as `var(name)` is whatever the property holds. The
padding, the margin and the gap take a `Dimension` now, and a bare number
is still pixels. Beside the font's units are the CANVAS's: `50_pct` is half
the parent's box, which is Yoga's own percent, while `50_pw` and `50_ph` are
half the width and half the height of the canvas the composer renders into,
however many boxes deep the node is written — resolved into pixels in the
same pass that resolves an em, because a percentage of something that is not
the containing block is not a thing Yoga can express.

**Motion stays on the node that declares it.** A node whose ink changes
under `Element::transition` eases it, and everything under it follows —
repainted while the colour moves, cached again when it settles. A bound
ink is not offered: a live value inherited from above would make the
subtree under it volatile, a direction the caching kernel does not fold.
What a change costs follows the split above: an ancestor's font change
re-materialises and relays out the inheriting leaves under it, and
rewrites every length measured in it; an ink change repaints them and
breaks no line again.

**The pen begins in the ink and the font.** A `compose::pen` or
`compose::graphics` program finds its fill and stroke in the node's ink
and its text in the node's font — `PaintContext::ink` and
`PaintContext::font` — restyles for its scope with the pen's own verbs,
and a guest tree it paints through `paintRetained` inherits from the tree
the pen stands in. `PaintContext::vars` is the same node's properties,
for a decoration that resolves a fill by hand through `resolveRef`.

**A rule a selector chose stands between the classes and the node's
own verbs.** `Element::applyStyleSheet` puts a `compose::StyleSheet` in
force at a node and everything under it, and a rule of it whose
selector matched folds over the role, its sheet rule and the classes,
and under the node's own `font`, `block`, `ink` and `var` — into the
same partial, so a relative size is still laid over the parent's font
at one point. That sheet sees only that subtree: every compound of a
selector must match the applying node or one below it, so nothing
above or beside it answers any part of a rule. Which elements a rule speaks about, how heavily it
weighs and which of two matched rules wins is CSS's selector grammar
as a value: [selectors](SELECTORS.md).

**Three keywords say what no value can.** Every property a node or a
rule may state has a name of its own — a
[`compose::Property`](pages/types/Property.md) — and
`Element::inherit`, `Element::initial` and `Element::unset` write that
property as CSS's wide keywords instead of as a value.
`inherit(Property::PaddingLeft)` gives a node the padding its PARENT
ended up with, though nothing about a box inherits on its own;
`initial(Property::Font)` stops an inherited font and sets the subtree in
the default face at the default size; `unset` asks
`compose::inheritsByDefault` and takes whichever of the two that property
calls for. A keyword is a declaration like any other — it stands over a
rule and under nothing but a running motion, and a later statement of the
same property on the same node replaces it, whichever of the two was
written first.

That layer is why `inherit` says something about a property that inherits
ANYWAY. The value arriving from the parent is the WEAKEST of the five
layers this chapter opened with, and the keyword drops the four folded
over it: `ink(blue).inherit(Property::Ink)` is the ancestor's ink, and
`unset(Property::Ink)` on a node carrying a class that states a colour is
the ancestor's ink too.

A keyword needs somewhere to resolve, and a few properties are kept on
the description, which no fold reads — the plane a node turns in, the
silhouette's generator, the grid area and the outline the decorations
dress. `compose::answersKeyword` says which, and a keyword about one of
them is refused at the verb and said once, so the node describes as one
that never wrote it.

`compose::inheritsByDefault` is the whole of the inherited set, and it is
the five things this chapter opened with: the font, the ink, the block,
the custom properties and the image sampling. Everything else is a
statement about ONE box — a padding taken from the parent would be
applied again at every depth — which is why the keyword is the only way
to say it, one node at a time.
