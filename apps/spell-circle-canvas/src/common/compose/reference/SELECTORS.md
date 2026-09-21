# Selectors

A chapter of [SigilCompose's README](../README.md), beside
[the cascade](CASCADE.md).

A SELECTOR says which elements a rule speaks about. It is CSS's own
grammar over the element tree, as one immutable, comparable value:
`compose::ElementSelector`. There are two front doors onto it and they
produce the same value — the CSS text most authors write, and the typed
builders under `compose::select` for the caller who would rather not
write a string.

```cpp
using namespace sigil::compose;

const ElementSelector fromText = selector(".card > .title:first-child");
const ElementSelector fromBuilders =
    select::styleClass("card").child(select::styleClass("title").firstChild());
// fromText == fromBuilders
```

## What the text says

A bare word is a ROLE — `Element::role`, which is what a type selector
selects here. `.name` is a style class — one of the names
`Element::styleClass` writes. `*` is any element at all. The four
combinators are CSS's: `a b` a descendant, `a > b` a child, `a + b` the
sibling immediately after, `a ~ b` any later sibling. A comma is a
selector list. The SUBJECT — the element a rule actually styles — is the
last compound of the chain, which is why `ElementSelector::child` reads
"this, then its child" and returns a selector whose subject is the
argument.

The structural pseudo-classes are `:first-child`, `:last-child`,
`:only-child`, `:nth-child(an+b)`, `:nth-last-child(an+b)`, `:empty`,
`:root`, the whole of-type family — `:first-of-type`, `:last-of-type`,
`:only-of-type`, `:nth-of-type(an+b)`, `:nth-last-of-type(an+b)` — and
CSS's filtered count, `:nth-child(an+b of S)`. A count is written as a
number, as `an+b`, or as `odd` or `even`. `:is(...)`, `:where(...)` and
`:not(...)` take selector lists.

**Type means ROLE**, and an element with no role has no type, so no
of-type pseudo-class matches it.

Not read: the state pseudo-classes, attribute selectors, `#id` and the
pseudo-elements. A text this library does not read warns once and yields
a selector that matches nothing, so a misprint loses one rule rather
than the sheet around it — `ElementSelector::matchesNothing` answers for
one. An `an+b` number too large to hold, and bracketed lists nested
deeper than the parser reads, are refused the same way rather than
clamped.

Not yet: `:has()`, `calc()` on a length, a transition stated in a rule,
and the box half of what a rule can state. Each of those is wanted and
decided; none is in the grammar or the rule yet.

## The set algebra

The operators are the house's, the same three `weave::Selector` takes
over text ranges: `a | b` is a selector list, `a & b` a compound on ONE
element, `!a` a negation. `select::is`, `select::where` and
`select::notAnyOf` spell the same three pseudo-classes by name. The
combinators stay named methods — `ElementSelector::child`,
`ElementSelector::descendant`, `ElementSelector::next` and
`ElementSelector::sibling` — because they are relations between two
elements rather than set operations on one.

A comma does not nest, so `(a | b) | c` and `a | (b | c)` are the same
flat list. A list compounded onto one element is exactly CSS's `:is()`,
so `(a | b) & c` is `:is(a, b)c`.

A selector that matches nothing is the ZERO of this algebra, not its
identity: compounding it, negating it, qualifying it with a
pseudo-class or chaining it with a combinator all match nothing, so a
misprint can only ever narrow a rule away. `a | b` is the one exception
— the alternatives that do read still read.

## What a selector weighs

`ElementSelector::specificity` answers with `compose::Specificity`, the
pair CSS counts where nothing carries an id: `Specificity::classes` —
the style classes and the pseudo-classes — then `Specificity::roles`.
The pair compares left to right, so one class outweighs any number of
roles, and combinators and `*` weigh nothing at all.

`:is()` and `:not()` weigh as their heaviest argument; `:where()` weighs
nothing, which is what makes it the way to state a default anything can
override. The filtered `:nth-child(an+b of S)` adds its heaviest
argument to the one class the pseudo-class is worth itself. A list
reports its heaviest alternative; a matcher weighs the alternative that
actually matched.

| Selector | classes | roles |
| --- | --- | --- |
| `*` | 0 | 0 |
| `heading` | 0 | 1 |
| `.card` | 1 | 0 |
| `.card > heading` | 1 | 1 |
| `.row:nth-child(2n+1)` | 2 | 0 |
| `:is(.a, heading)` | 1 | 0 |
| `:where(.a, .b.c)` | 0 | 0 |

## What a rule states

A RULE is a selector and what it states about the elements that
selector speaks about: `compose::Rule`, which `compose::rule` builds
from either front door.

```cpp
const StyleSheet house{
    rule(".card").font({.size = 18}),
    rule(".card > .title").ink(SkColor4f{0, 0, 0, 1}),
    rule("heading:first-of-type").var("gutter", 24.0f),
};
```

`Rule::font` and `Rule::block` take the partials `Element::font` and
`Element::block` take and fold them the same way — later wins field by
field — so a property is spelled once whichever side says it.
`Rule::ink` takes a colour or a custom property exactly as
`Element::ink` does, and `Rule::var` sets a property on every element
the rule matches, for that element and everything under it. What a
rule leaves unsaid the element inherits. A rule holds STATIC values: a
live binding, an entrance and an animation stay verbs on the element.

## The sheet, and applying it

`compose::StyleSheet` is those rules in order, as one immutable value:
declared once, applied at as many subtrees as the author likes. Copies
share one stored form and compare by pointer before they compare by
value. A literal may hold another sheet where a rule would stand,
whose rules then stand in its place in order, and `+` joins two sheets
the same way — `house + darkTheme + local`.

`Element::applyStyleSheet` puts a sheet in force at a node and
everything under it. Calling it again applies another, later in order;
nothing removes one, because a tree that should stop applying a sheet
is described without it. `StyleSheet::rules` reads back what a sheet
holds.

**An applied sheet sees ONLY ITS OWN SUBTREE.** Every compound of a
selector — the subject, which is the last one, and every ancestor or
sibling named before it — must match the applying node itself or an
element below it. Nothing above that node and nothing beside it, its
parent and its siblings included, can satisfy any part of a rule of
that sheet: a sheet applied deep in a page that says `.page .swatch`
reaches a `.swatch` only where the `.page` it names is the applying
node or stands under it. A sheet that must see an outer element is
applied at or above that element instead. Custom properties still
inherit across the boundary, as everything inherited does.

The applying node is INSIDE the sheet it applies, and is the root of
the only tree that sheet sees. `rule(".card")` in a sheet applied on
the `.card` node styles that node, and `.card .swatch` reaches the
swatches under it. Its own siblings and its parent stand outside, so
for that sheet it is the only child of nothing and it is the `:root`,
exactly as the tree's own root is.

## Which rule wins

At every element the cascade folds these layers, each over the one
before:

1. the defaults of its role (`Element::role`),
2. the rule the name-keyed `weave::StyleSheet` in force carries under
   that role's name,
3. the classes that sheet carries under the names `Element::styleClass`
   lists,
4. the rules of the applied sheets whose selectors matched, and
5. the node's own `font`, `block`, `ink` and `var`.

Among the matched rules the order is CSS's: the heavier
`ElementSelector::specificity` first; then scope proximity, the nearer
applying node winning; then the order the sheets were applied in; then
the order the rules stand in their sheet. A rule weighs the
ALTERNATIVE that matched it, so `rule(".a, heading")` weighs one class
where `.a` matched and one role where `heading` did.

A relative size still resolves ONCE against the parent's font: the
matched rules fold into the same partial the classes and the node's
own verbs fold into, and that partial is laid over the parent's font
at one point, so `1.5_em` in a rule means what it means on a verb.

Where a node stands among its siblings is counted once per parent,
each time its children are resolved — by position and by role — so a
structural pseudo-class is a lookup, and a changed child list
re-resolves that parent's children.

## Where it stands

- `sigilcompose/core/Selector.h` — `ElementSelector`, `Specificity`,
  `selector`, and the builders `styleClass`, `role`, `any`, `is`,
  `where`, `notAnyOf`.
- `sigilcompose/core/StyleSheet.h` — `compose::Rule`,
  `compose::StyleSheet`, `compose::rule`.

`Element::applyStyleSheet` is the verb that puts a sheet in force, and
[the cascade](CASCADE.md) is where the matched rules are folded in.
The name-keyed `weave::StyleSheet` stands beside them, unchanged:
`Element::styleSheet` still states it, and a class still resolves
through it.
