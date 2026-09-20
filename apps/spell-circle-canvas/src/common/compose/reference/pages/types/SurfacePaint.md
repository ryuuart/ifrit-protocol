---
kind: type
library: SigilCompose
name: SurfacePaint
qualified: sigil::compose::SurfacePaint
group: Paint
status: stable
---

# SurfacePaint

THE WIDEST COLOURING VALUE IN THE TREE, and the one name an author never
has to learn a second time: a flat fill, a fill that moves, a material
paint, or a recipe instance. Every other colouring value converts into
it, and it is what a component prop asks for when it means "colour this
surface, however the caller likes".

It is a two-branch variant. The left branch is a
`motion::Animatable<Fill>` — the cheap, comparable, prunable slot. The
right branch is a `material::skia::Paint` — the shader tree, which a
`material::Material` enters through `material::skia::Paint::recipe`. The
containment runs that way and only that way: a material is one kind of
paint, and a paint is one branch of a surface paint.

## Anatomy

`SurfacePaint::none` answers whether it holds nothing, which is what a
default-constructed one holds, and what makes an unnamed prop leave the
element's own fill alone rather than clearing it.

`SurfacePaint::apply` visits the variant onto the element: it is the one
place that decides which `Element::fill` overload a given branch reaches,
which is why a component that takes a surface paint needs no overload
set of its own.

`SurfacePaint::resolve` answers the `Fill` for one paint context — what a
decoration reads, since a decoration paints with a context and no
instance, and therefore runs no transition of its own.

`SurfacePaint::isAnimated` is the volatility declaration, spelled the same
word every value in this tree spells it with.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `SurfacePaint()` | C++ | nothing — leaves the element's own fill alone |
| `SurfacePaint(fill)` | C++ | implicit from a `Fill`, so `Fill::currentInk()` and every colour spelling reach it |
| `SurfacePaint(animatable)` | C++ | implicit from a `motion::Animatable<Fill>` |
| `SurfacePaint(output)` | C++ | implicit from a `choreograph::Output<Fill>*` — the live binding |
| `SurfacePaint(transitioned)` | C++ | implicit from a `motion::Transitioned<Fill>` |
| `SurfacePaint(paint)` | C++ | implicit from a `material::skia::Paint` |
| `SurfacePaint(recipe)` | C++ | implicit from a `material::Material`, wrapped through `material::skia::Paint::recipe` |
| `"#1f2933"`, a tuple, `material.Color(...)` | Python | every colour spelling, implicitly |
| `material.Paint.linear(...)` | Python | a material paint, implicitly |
| `material.kit.unlit()` | Python | a recipe instance, implicitly — the kit's recipe functions are Python's door to a material |
| `motion.bind(...)`, an output, a transition | Python | the moving forms, implicitly |
| `compose.SurfacePaint(...)` | Python | direct, when a name for the value is wanted |

In Python the whole of that column is the union `SurfacePaintLike`. Every
parameter that decides what colour pixels are takes the widest union its
slot can honour; a narrower one is a documented decision with its reason
on the line, never an omission.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Element::fill` | verb | SigilCompose |
| `PathFormat::strokeFill` | field | SigilCompose |
| `compose::stroke` | function | SigilCompose — the stroke decoration's paint |
| `Board::ground` | field | SigilCompose |
| `Well::ground` | field | SigilCompose |
| `Sheet::ground` | field | SigilCompose |
| `Reading::swatch` | field | SigilCompose — the patch standing before a row's name |
| `Bars::bar`, `Bars::rest` | field | SigilCompose — the bar and the track behind it |
| `kit::dot` | function | SigilCompose |

A kit ground is a `SurfacePaint` wherever the thing it grounds can carry
a gradient. Where a kit field is a plain `Fill` — a rule, a hairline
companion — that is the narrower slot stating that a rule is a colour and
not a surface.

## Also returned by

Nothing returns one: it is an argument type. A consumer holding one reads
it back as a `Fill` through `SurfacePaint::resolve`, or applies it with
`SurfacePaint::apply`.

## Description

The variant's two branches are the two volatility stories a colour can
have, and keeping them apart is what makes the caching work.

A fill compares in five scalars, so a node painted with one prunes
without a memo. A material paint compares by its own recipe and may need
a frame to answer at all. `Element::fill` routes between them: a STATIC
paint collapses to a `Fill` on the node's paint slot with the paint kept
only as the prune signature, and a LIVE or geometry-dependent paint is
kept whole on the node's material slot so the painter resolves it against
the frame.

That routing is why the top of the lattice is here, in Compose's kernel,
rather than in the paint model. SigilCompose holds no paint vocabulary of
its own — a paint, an effect, a signed-distance surface, a tile and a
field are SigilMaterial's, and are spelled from it — and what is Compose's
is where a paint LANDS and what it costs the tree.

## See also

- `core/SurfacePaint.h` — the header: `SurfacePaint`
- [Fill](value:sigil::compose::Fill) — the left branch, and what a
  colour boils down to
- The colour chapter on the [SigilCompose](doxygen:SigilCompose) site —
  the lattice whole
- [Paint](value:sigil::material::skia::Paint) — the right branch, in
  SigilMaterial
- [Material](value:sigil::material::Material) — a recipe instance,
  which enters through a recipe paint
