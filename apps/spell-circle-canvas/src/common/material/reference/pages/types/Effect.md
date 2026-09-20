---
kind: type
library: SigilMaterial
name: Effect
qualified: sigil::material::skia::Effect
group: The Skia paint
status: stable
---

# Effect

POST-PROCESSING OVER A LAYER THAT IS ALREADY RENDERED, as a comparable
value. A paint shades a shape; an effect takes the picture a consumer has
drawn and runs a filter over it — a blur, a glow, a bloom, a colour map,
a displacement, or an SkSL program whose `content` slot IS that layer.

The one-sentence difference from a paint: a paint answers "what colour is
this pixel of this shape", an effect answers "what happens to the pixels
already there".

## Anatomy

Every effect is built by a static factory and then narrowed with the
modifiers. `Effect::isAnimated` is the volatility declaration — one word
across the whole library — and it is true while any uniform is bound or
any child material is live. `Effect::usesWorldSpace` asks whether any
child paint anchors to the root frame.

`Effect::imageFilter` is the built filter; `Effect::colorFilter` is the
colour lane, set only by the colour-filter factory and by a lowered
recipe, and the two are never both present. `Effect::resolvedImageFilter`
is the filter with bound uniforms resolved NOW, against the painting
node's frame — which is what the paint phase applies.

`Effect::then` chains: `next` runs after this one. `Effect::emit` makes a
LIGHT from the same input this effect reads and blends it over this
effect's own output, so lights stack rather than compound — a second
emit adds a light of the layer, never a light of the first light.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `Effect::filter(imageFilter)` | C++ | any Skia image filter — blur, displacement, lighting, compose chains |
| `Effect::filter(colorFilter)` | C++ | a per-pixel colour map with no neighbourhood, applied to the layer's paint as a blit rather than a pass |
| `Effect::recipe(material)` | C++ | a recipe as the effect: its program runs over the layer, which arrives in the slot named `content` |
| `Effect::recipe(material, sampleRadius)` | C++ | the same, bounding the largest local-coordinate source offset |
| `Effect::recipe(material, surface)` | C++ | the same recipe LOWERED for the surface it will land on — a channelwise recipe on an eight-bit surface becomes a table and no program at all |
| `Effect::shader(effect)` | C++ | an SkSL runtime effect whose content slot is the rendered layer |
| `Effect::glow(colour, sigma)` | C++ | the layer re-emitted blurred beneath itself, which keeps the content on top |
| `Effect::blur(sigma)` | C++ | the plain blur |
| `Effect::blur(sigmaMap, maxSigma)` | C++ | a blur whose radius is read from a paint, per pixel |
| `Effect::directionalBlur(sigma, angleDeg)` | C++ | the blur along one axis |
| `Effect::brightPass(threshold, knee)` | C++ | the layer with everything but its light taken out — the first half of a bloom |
| `Effect::phosphorBloom(radius, threshold)` | C++ | the whole bloom in one gather |
| `Effect::dilate(pixels)`, `Effect::deepen(amount)`, `Effect::whiten(amount)` | C++ | the small tonal passes |
| `material.skia.Effect.blur(...)` and the rest | Python | the same factories under the same names |

Then the modifiers: `Effect::uniform` sets or binds a named uniform —
including a `motion::Animatable<float>`, which makes the effect live —
and `Effect::slot` fills a declared `uniform shader` with a paint.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Effect::then` | member | SigilMaterial — as the next link of a chain |
| `Effect::emit` | member | SigilMaterial — as the light |

Outside this library an effect is what a node's own layer filter and its
backdrop filter take, and what a composer's view transform is stated in.
Those slots belong to the libraries that own them and each spells this
library's name for the value.

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| every factory above | function | SigilMaterial |
| `Effect::then`, `Effect::emit` | member | SigilMaterial |
| `skia::bloom` | function | SigilMaterial |

## Description

An effect is a value so that a consumer caching a filtered layer can
prove two frames asked for the same one. A static shader effect compares
by RECIPE — the runtime effect pointer plus its constant uniforms — so a
re-described effect prunes as long as the caller holds ONE runtime effect
and rebuilds only the wrapper around it. A filter effect compares by
filter pointer, because an already-built image filter carries no recipe
to compare. A live effect never compares equal, conservatively, like a
live material.

A recipe effect reads its material's bindings ONCE, at construction, so
animation happens by re-describing rather than by the material moving
underneath it. That is the difference from a recipe PAINT, which
re-resolves per frame, and it is why the two exist side by side.

Where an effect is applied is a consumer's decision: it is attached at a
stacking-context boundary, and where the consumer caches that layer an
expensive filter over static content is paid once.

## See also

- `skia/Effect.h` — the header: `Effect`
- `skia/Bloom.h` — the header: `bloom`, `BloomParameters`
- [Paint](value:sigil::material::skia::Paint) — what shades a shape, as
  against what filters a layer
- [Material](value:sigil::material::Material) — the recipe an effect can
  run
- The colour chapter on the [SigilCompose](doxygen:SigilCompose) site —
  the lattice whole
