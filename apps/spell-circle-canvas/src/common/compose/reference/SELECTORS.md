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

## Where it stands

- `sigilcompose/core/Selector.h` — `ElementSelector`, `Specificity`,
  `selector`, and the builders `styleClass`, `role`, `any`, `is`,
  `where`, `notAnyOf`.

Nothing matches against the tree with a selector yet: the cascade pass
still resolves a node against its role and its class names alone, as
[the cascade](CASCADE.md) describes, and `Element::styleSheet` still
takes the name-keyed `weave::StyleSheet`. This value is what the rule
and the sheet that do the matching are written in.
