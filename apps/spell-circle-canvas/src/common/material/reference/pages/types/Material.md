---
kind: type
library: SigilMaterial
name: Material
qualified: sigil::material::Material
header: sigilmaterial/core/Material.h
group: Core
python: sigil.material.Material
status: stable
---

# Material

A RECIPE INSTANCE: the recipe, its parameter values held as the bytes the
shader receives, the live bindings that overwrite fields at every
resolve, the materials or leaves filling its slots, and the instance-side
settings a renderer reads. Comparable by value, so a scene prunes;
resolvable against a frame into the program plus the bytes to upload,
memoised on the last inputs.

A material is not the top of the colouring lattice and is not a colour
with extra parts. It is one KIND of paint — the recipe-backed kind — and
it reaches a surface by being wrapped in a paint, through
`skia::Paint::recipe`. Nothing goes the other way: a material has no leaf
that holds a paint.

## Anatomy

VALUES are bytes. `Material::set` writes one field by name, or rewrites
every field from a parameter struct; `Material::get` reads a field back
with the type it was declared with; `Material::bytes` is the whole block
in the recipe's own layout. A name the recipe does not declare, or a
value whose kind does not match the field, is reported once and ignored.

BINDINGS replace a field's bytes at every resolve. `Material::bind` takes
a `motion::Animatable<float>` for a float field or a
`shared_ptr<const UniformBlock>` for an array field; `Material::unbind`
drops one, leaving whatever was last written. A field bound to a LIVE
animatable is live; one bound to a plain number is a value like any
other. A material holds no clock, so an animatable carrying its own
transition has nothing to run it and reads as its target.

SLOTS are the recipe's declared sockets. `Material::slot` fills one with
another material or with a `Leaf` — an image and its sampling, bound by
the backend rather than compiled — and reads one back;
`Material::slots` lists the filled ones in recipe order.
`Material::leaf` reads back the leaf case. A live child makes the parent
live and a different child makes the parent unequal.

SETTINGS are the instance's own: `Material::amount` is the strength a
renderer blends it in at, `Material::quantizeTime` snaps the time it sees
to a step rate, `Material::worldSpace` anchors it to the root frame
rather than the node's.

QUERIES are the two the rest of the tree asks. `Material::isAnimated` is
whether the upload can change with no edit — a bound output or block, a
recipe reading time or content scale, or an animated child.
`Material::geometryDependent` is whether it depends on where and how
large the node is. `Material::resolve` answers the program and the bytes
for one frame, and `Material::withRecipe` is the same instance over a
specialization of the same parameter layout.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `Material(recipe, parameters)` | C++ | an instance with the field values of a parameter struct, whose type must be the struct the recipe was defined over |
| `Material(recipe)` | C++ | an instance whose fields all start at zero |
| `Material::withRecipe(recipe)` | C++ | the same values, bindings, children and settings over a second definition of the same layout |
| the kit's recipe functions | C++ | a stock look already instanced — the kit composes, it decides nothing |
| `material.Material(recipe, parameters)` | Python | the same two constructors |

A material is a VALUE: every setter copies on write, so binding on a copy
never affects the material it was copied from.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `skia::Paint::recipe` | function | SigilMaterial — the wrap that makes it a paint |
| `skia::Effect::recipe` | function | SigilMaterial — the same recipe run over an already-rendered layer |
| `Material::slot` | member | SigilMaterial — a material fills another's slot |
| `skia::Paint::slot` | member | SigilMaterial — through a paint |

Every consumer that takes a paint takes a material where the conversion
is spelled for it. In Python a material is a member of `SurfacePaintLike`,
so a slot that colours a surface takes one directly.

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| `Material::withRecipe` | member | SigilMaterial |
| `skia::Paint::recipeMaterial` | member | SigilMaterial — the instance behind a recipe paint, or null |
| the kit's recipe functions | function | SigilMaterial |

## Description

EQUALITY is by value: recipe identity, bytes, bindings under SigilMotion's
rule for an animatable — a live binding by the Output's IDENTITY, never
the number behind it — a block by pointer, children by value, and the
instance settings. Two materials describing the same thing compare equal,
which is what lets a node prune.

That comparator is also the reason a flat colour is not a degenerate
material. A colour compares in a handful of scalars and a material
compares by all of the above; putting every flat fill on this path would
move every node's paint prune from the first cost to the second, and
would need either a program per flat colour or a special case
re-inventing the solid short-circuit one layer up.

`Material::resolve` is memoised on the sampled values, the target and the
variant, so a frame that changed nothing returns the previous result
without a cache lookup.

## See also

- `core/Material.h` — the header: `Material`
- `core/Leaf.h` — the header: `Leaf`
- [Paint](Paint.md) — the paint model a material is one leaf of
- [Effect](Effect.md) — the same recipe over an already-rendered layer
- [Color](Color.md) — what a material's colour-typed fields hold
- [Colour, fill, paint and material](../../../../compose/reference/COLOURING.md)
  — the lattice whole, and why the containment runs this way
