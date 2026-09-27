# SigilMaterial — the program model

The chapter on what a shader material is made of. `README.md` beside this
file is the library, and its `material::shader(source, Parameters{…})` is
the one line a sketch writes; this chapter is for a consumer that needs
the pieces that line is built from — a renderer that resolves a material
against a frame, a definition with a body in two languages or a slot an
executor fills from the layer, a leaf a backend binds, a bank of seeded
instances, a stack composed for a target that cannot sample its operands.

Every header named here lives under `<sigilmaterial/advanced/…>`, and a
consumer includes it by name: neither `<sigilmaterial/core/Material.h>`
nor `<sigilmaterial/program/Shader.h>` includes any of them.

- `advanced/Recipe.h` — `Recipe`, `FrameInput`, `LayerFilter`,
  `LayerSlot`, `uniformName`
- `advanced/Program.h` — `Program`, `ProgramCache`, `Compiler`,
  `WarmupRequest`, `WarmupResult`, `registerCompiler`, `program`,
  `warmup`, `reportOnce`
- `advanced/Leaf.h` — `Leaf`
- `advanced/Bank.h` — `Bank`
- `advanced/Terms.h` — `termsSource`, `skSLFromSlang`
- `advanced/Combine.h` — `OverParameters`, `overRecipe`, `stackName`,
  `over`, `under`, `stackDepth`
- `advanced/UniformBlock.h` — `UniformBlock`
- `advanced/FrameData.h` — `FrameData`

## A recipe by hand

```cpp
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Target.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

using namespace sigil::material;

// The ABI: a plain aggregate of uniform fields. Names are read off the
// type; there is nothing to register.
struct Glow {
  float uScale;
  Color uTint;
  std::array<float, 8> uBars;
};

// A shader YOU authored stays a shader file, so editors and shader tools see
// the language, and reaches the program through SigilIO from wherever you
// keep it. The Hub caches it; the lease makes its residency promise explicit
// for as long as this material catalogue lives. The shaders this library
// SHIPS are not read at run time at all — see "Where the stock shaders live".
sigil::io::Hub shaders;
shaders.mount("shader://", shaderDirectory);
auto retainedShaders = shaders.retain("shader://");
retainedShaders.preload();
auto glowSource = shaders.text("shader://Glow.sksl");
if (!glowSource) throw std::runtime_error("Glow.sksl is missing");

// The definition, made once and shared. The loaded body follows the generated
// declarations — the uniforms above, then uTime, then the slot.
auto glow = std::make_shared<const Recipe>(
    Recipe::of<Glow>("glow")
        .frame(FrameInput::Time)
        .slot("uSrc")
        .body(Target::SkSL, std::move(*glowSource)));

// An instance: values now, a bound clock and a live table later.
Material m(glow, Glow{1.0f, {1, 0.8f, 0.2f, 1}, {}});
m.bind("uScale", scale);              // a live motion::Animatable<float>
m.bind("uBars", spectrumBlock);        // a shared_ptr<UniformBlock>, 8 floats
m.slot("uSrc", Material(gradientRecipe, GradientParameters{...}));

// A renderer, per frame:
FrameData frame{.seconds = clock.now(), .resolution = {w, h}};
sk_sp<SkShader> shader = skia::shader(m, frame);
```

`skia::shader` is the whole Skia path: it resolves the material, builds
over the program's effect with every uniform set from the resolved bytes,
binds each slot — a material resolved recursively, a texture
leaf as its image shader — and makes the shader. The Skia backend prepares
its compiler on first use; drawing needs no registration step. A renderer
that fills some slots itself uses `skia::builder(m, frame, variant,
leave)`, which prepares the same program and leaves the named slots for
the caller. `skia::fill(canvas, path, m)`
is the one-call draw: clip to the path, paint the shader across it.

A renderer applying a recipe-backed paint to a layer calls
`skia::resolvePass` with `skia::PassInputs` from
`<sigilmaterial/skia/Pass.h>`. The inputs supply the content shader, unit
rectangles, progress and seeds. The paint owns shader specialization and
program reuse for each unit count.

The surface program reads the same way, its slots filled with textures:

```cpp
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/surface/Surface.h>

// The map in the base-colour slot is multiplied by the factor, so a white
// factor shows the image as it is.
Material wall = surface::unlit({.baseColor = {1, 1, 1, 1}});
wall.slot(surface::kBaseColorSlot, Texture(bricks));
skia::fill(canvas, outline, wall);   // per frame; the program is cached
```

## Mental model

**A parameter struct is the ABI, and the bytes are the upload.** Every field
type is some count of floats with float alignment — `float`, `glm::vec2`,
`glm::vec4`, `std::array<float, N>`, `Color` — so a struct of them has no
padding and its memory image is exactly the uniform data in declaration
order. `schema<P>()` proves this at compile time and refuses a struct with
any other field type or with padding. The same walk emits the uniform
declarations (`declare<P>(target)`), so the names in the shader are the
names in the struct and cannot drift. A struct with NO fields is legal and
is a recipe with no ABI of its own — a body over slots and frame
inputs alone.

**A layout can also be assembled while the library runs.**
`packedSchema(fields)` lays a field list out by the rule `schema<P>()`
applies to a struct — each float count read off the kind, each offset the
running sum — and `Recipe::of(name, schema)` defines over the result. That
is the door for an ABI no C++ type stands behind: a definition composed out
of other definitions' fields, or one authored from outside C++ altogether.
A repeated name and an array of no floats are each reported once and left
out, so what comes back is a layout `find()` answers unambiguously and
`declare()` can emit.

**Writing to a field no body reads is reported at the write.** A dial
that does nothing looks, from the call site, exactly like a dial set to
the wrong value: the bytes go up and the picture does not change. So
`Material::set(name, …)` asks the recipe — `Recipe::readsField(name)`,
which is whether any body of it SPELLS the name as a whole identifier —
and names the recipe and the field on stderr once per pair, beside the
reports for an unknown field and a wrong float count. The value is still
written; the report is about the picture, not the bytes.

It is asked at the WRITE and not at the compile because a parameter struct
carrying a field this recipe's kind has no use for is a shared ABI and
not a mistake — the three `sdf` silhouettes are one struct whose `uP0..2`
mean something different in each — and a struct poured in whole says
nothing. What the compile side can still say is that a BACKEND discarded
a uniform: `Program::keeps(name)` is that question, and the program cache
names each dropped field once per (recipe, target). Skia's reflection
keeps every declared uniform, so on SkSL that answer is always yes and
the write-side check is the one that speaks.

**A recipe's identity is the object.** Two recipes built from the same
text are two definitions with two sets of programs; `operator==` compares
definitions and is for tests, while the program cache and a material's
equality use the pointer. Define a recipe once and hold it in a
`shared_ptr<const Recipe>` beside the code that owns it.

A definition a renderer can only finish at draw — a body rewritten around
an array size or a constant nothing knew earlier — is a SPECIALIZATION:
`m.withRecipe(r)` is the same instance over a second recipe of the same
parameters layout, so the values, bindings and slots carry over and the two
definitions compile and cache apart. Hold the specializations, one per
distinct constant, or the cache fills with a definition per draw.

**One body per target, and asking for a missing one is an error once.**
`Recipe::body(Target, source)` stores the body for a language;
`Recipe::source(target)` is the generated declarations followed by it.
The two targets ask a body for the same thing in their own words:

| target | what a body is | how it reads a slot |
|---|---|---|
| `Target::SkSL` | `half4 main(float2 p)`, returning premultiplied colour | `uniform shader NAME`, evaluated as `NAME.eval(p)` |
| `Target::Slang` | `float4 surface(float2 uv)`, returning STRAIGHT colour — the renderer that compiles it puts the lighting and the premultiply around it | `uniform Sampler2D NAME`, read as `NAME.Sample(uv)` |

A Slang body may also say what a colour cannot carry. A renderer that
shades declares four variables the body MAY write —
`gSurfaceNormal` (tangent space), `gSurfaceGloss` (a Blinn exponent),
`gSurfaceMetal`, and `gSurfacePerPixel` to say it wrote any of them —
and evaluates its shading again where those can be seen. A body that
writes none of them costs nothing and changes nothing. It is an
OPTIONAL half of the contract: a body that says only a colour is a
complete body, and the four exist because a MAP that varies a surface
across a face is a per-pixel answer no per-vertex shading can carry. A
material resolved for a target its recipe has no body for — or one no
compiler is registered for, or one whose body fails to compile — yields a
null program, and the cache reports it to stderr exactly once per (recipe,
target), naming both, so the mistake surfaces at the first describe rather
than scrolling past every frame. A body that compiles but leaves a parameters
field unread is reported the same way.

**One program cache.** `ProgramCache::shared()` holds every compiled
program in the process, keyed by (recipe identity, target, variant). A
backend registers its compiler with `registerCompiler(Target, Compiler)`
and the cache compiles on first use. Every Skia lowering entry prepares
the built-in SkSL compiler automatically, including `skia::builder`,
`skia::shader`, `skia::fill`, recipe-backed paints and recipe-backed effects.
An explicitly registered SkSL compiler takes precedence for subsequent
compilation, whether registered before or after the first draw. A device renderer registers the
Slang compiler, since only that renderer knows the scaffold a body is
appended to. `Variant` is a small ordered key the
backend owns the meaning of — a premultiplied build, a debug view — and
the default variant is the plain build.

**Bindings are live, and equality is by identity.** `bind(name, animatable)`
makes a float field read a `motion::Animatable<float>`'s current value at
every resolve;
`bind(name, shared_ptr<UniformBlock>)` does the same for an array field
and a caller-owned table. A bound material `isRunning()`;
`isBound(name)` is the other question — whether a field carries a binding
at all, a live value or a number or a block, rather than only the bytes
`set()` last wrote. Two materials
bound to the same live value or block compare equal; bound to different ones,
unequal; the values behind them never enter the comparison. A `UniformBlock`
carries a revision (`commit()` advances it) so a caller can tell an edited
frame from an untouched one, and its values are read live whether or not
they were committed.

**Frame inputs are declared, then injected.** `Recipe::frame(FrameInput)`
declares that the body reads `uTime`, `uResolution`, `uContentScale` or
`uWorld`; the declaration adds the uniform after the parameters and
`resolve()` fills it from the `FrameData`. Time and content scale make a
material `isRunning()`; resolution and the world transform make it
`geometryDependent()`. `quantizeTime(rate)` snaps the time a material sees
to a step, so a material that need not move every frame resolves only
when the snapped clock advances.

**Slots ride everything.** A recipe declares slots (`slot("uSrc")`,
exposed to SkSL as `uniform shader uSrc`); a material fills them with other
materials or with leaves. A live source makes the parent live, a
geometry-dependent source makes it geometry-dependent, and a different
child makes it unequal — which is required, not incidental: a child left
out of equality would let a node prune while its second source had
changed.

**A slot an EXECUTOR fills comes from the layer.** A recipe run over a
rendered layer reads that layer in a slot the executor fills; declaring
one with `Recipe::slot(name, LayerFilter::Blurred, amountField)` says it
is filled from the SAME layer put through a filter first, at the amount
the named parameter carries. `Recipe::layerSlots()` is the list, and the
declaration is additive: the slot is generated to a target exactly as
any other is, and an author who fills the name himself keeps his source,
which is how a recipe with one still paints as an ordinary fill. A body
that needs a blurred copy of its input therefore reads one tap of it
instead of gathering the blur itself, per pixel, for as long as the
picture is on screen — and because the filter is Skia's, the cost of a
wide reach is Skia's reduction rather than a fixed tap count.

**A slot is declared to the target that samples it, and to no other.**
A slot belongs to the recipe, but each target's generated declarations
carry only the slots ITS body spells — `Recipe::samples(target, slot)` is
that reading, the one `readsField` takes of a parameter field, and a target
with no body answers yes. The two sets differ where one language reaches
a child material and another cannot: a composed stack declares a slot per
operand's own slot for the language handed one body per material, and the
language whose slot is a shader samples the three operands
themselves and needs none of them. It is not tidiness. A declared slot is
an IMAGE SAMPLER in the compiled program whether anything reads it or
not, a GPU backend inlines the whole tree of effects into one fragment
program, and Metal binds fragment textures at sixteen indices — so a
program carrying another language's slots spends a device's whole budget
on samplers it never reads. `skia::samplerCount(material)` is what one
lowering asks for, `skia::kSamplerLimit` is what a program may declare,
and a tree over it is refused with both counts named rather than built
into a pipeline the driver silently rejects.

**A leaf is a child no recipe computes.** `Leaf` is the core's seam for
an image with its sampling, a rendered frame, anything a backend binds
into a slot directly: it compares by value (same dynamic type, then the
type's own equality) and says whether it moves between frames.
`Texture` is the leaf every renderer binds: the Skia executor as the
image shader `skia::shader(texture, frame)` builds, a device renderer as
a texture. `skia::ShaderLeaf` is the Skia-facing refinement for
everything else — a leaf that yields the `SkShader` to bind, such as a
renderer's own native sources (a gradient it built, a Perlin generator)
— and the Skia executor binds any of them. A slot holds a material or a leaf,
never both, and `Material::slot(name)` and `Material::leaf(name)` each
answer null for the other kind.

## Stacking

**`over(base, top, mask, blend, amount)` is a material.** The three
operands
fill its slots, so the stack compares, animates and resolves as one
value, and applying `over` again builds a taller one. The MASK is any
material whose red channel is read as a scalar; `blend` is `Mix`, `Add`
or `Multiply`, one recipe each so a body carries no branch; `amount` is
how strongly the top shows where the mask is fully on, which is the
stack's own strength rather than a second answer about where it applies.
It is a parameter of the call because a COMPOSED stack has no parameters
struct to write afterwards — its ABI is its operands' fields — so a
caller who did not know to write the field by name would get a stack at
full strength and read it as a wrong mask. `under(m)`
is the material a stack stands on — one step down, so walking it reaches
the bottom — and `stackDepth(m)` counts the steps. A consumer that can
only express one material (`UsdPreviewSurface`, say) writes the bottom
and records the depth.

**Two kinds of target read a stack, and only one of them can reach the
operands.** A target whose slot is a SHADER — SkSL's is — samples
each operand's own program, so one body over the three slots `base`,
`top` and `mask` is the whole story. A target handed exactly ONE body per
material cannot reach a child material at all; for it a stack is
COMPOSED. `over()` builds a recipe out of its operands' own definitions:
the parameters are theirs under a prefix per operand (`base_`, `top_`,
`mask_`), the sampled slots are theirs under the same prefixes, the frame
inputs are the union of theirs, and the body inlines all three of their
bodies and mixes what they return. The renaming is the shading
language's own preprocessor rather than a rewrite of the text — a body
names its parameters and its slots exactly as its recipe declares them,
and a macro maps each — and each operand's helpers stand in a namespace
of its own, so three operands over one recipe are three bodies. **The one
thing a composable body may not do is give a local the name of one of its
own parameters.**

A composed stack is the same material otherwise: the same three operands
in its slots, the same walk down, and the same recipe NAME — which is what
says a material is a stack, since a composed one carries a recipe built
for its own operands rather than the shared one. The operands' values and
their sampled slots are copied in at the moment of the call, so a later
edit to an operand is not seen and a live binding on one does not reach
the composed body; the operand still rides every query as a child, so the
stack still reports itself animated. The composition costs one recipe and
one program per distinct triple of definitions and buys nothing for a
target that samples its operands, so it is built only where a compiler
that needs it is installed. The triple is identified by the definitions
themselves and the cache HOLDS them, so a definition that has been
composed stands for as long as its composition does — which is what
keeps a later recipe built at a freed one's address from inheriting a
body it never wrote. `Target::Slang` is the one such target,
and `stackName(blend)` is the name every stack of a blend carries.

A composed stack therefore carries slots for two languages at once, and
what keeps that from costing the sampling target anything is that a slot
is declared only to the target whose body spells it: the composed
recipe's SkSL program declares `base`, `top` and `mask` and none of the
prefixed ones, so a stack asks a device for exactly its operands'
samplers however many slots its operands DECLARE.

## The Slang backend

`slang::compileModule(source, vertexEntry, fragmentEntry, lit, &out,
&error)` compiles a whole module — imports, both entry points and all —
and hands back a `slang::Compiled`: the two stages' SPIR-V words, the
sampled slots in their declared order, and a `slang::UniformSlot` per
uniform saying where its bytes go. NOTHING GUESSES A LAYOUT: every offset
is the one the compiler reported for the program it just built, so a body
that declares one more parameter moves nothing a renderer has to be told
about. `slang::Uniforms` is the buffer a draw writes into at those
offsets — a matrix row by row and an array element by element where the
layout put them apart, and a name the program does not carry skipped,
since an optimiser that dropped an unused uniform is not a mistake to
report.

Both stages are linked as one program, because the layout is a property
of the linked program: linking them apart would let an unused uniform be
dropped from one and not the other, and the two would then read one
buffer at two sets of offsets.

Every session carries two modules by name, from the text the build embedded
in the library each belongs to, so a shader's `import` resolves against the
session rather than opening a file during compilation. `Portable` is the subset
whose transcendentals a host and a device answer alike — a kernel compiled for
both cannot afford two spellings of a square root. `Shading` is
`termsSource`'s own text, so a renderer's shading and every material body
compiled beside it call one definition of a term rather than a copy apiece.

`lit` is the one axis a session specialises on: it defines `SIGIL_LIT`,
so a renderer's scaffold can carry its lighting uniforms in one build and
not the other. There are therefore two sessions, and a module is loaded
into whichever one the caller asked for under a name no other module
has — a session remembers a module by its name, so two recipes under one
name would be one module and every material after the first would be
drawn with the first one's program.

## Warming every program

**Warming a list of materials.** `skia::warmup(materials)` prepares the
backend and compiles the distinct programs of the materials a host
hands it before the first draw — a host passes the materials it uses;
omitting warm-up leaves compilation to first use.
`material::warmup(materials, target, variant)` folds identical recipe,
target and variant keys and compiles distinct keys concurrently into the
shared cache; `ProgramCache::warmup` takes the requests themselves. A request arriving while the same key is compiling shares that
in-flight result, and the cache's synchronization remains an implementation
detail. A registered compiler can therefore receive concurrent calls for
different keys; a backend with thread-affine work must marshal that work at
its own executor seam.

**Every body an effect is built out of, as one list.**
`skia::everyFilterProgram()` in `<sigilmaterial/skia/Filter.h>` answers a
`std::span<const sk_sp<SkRuntimeEffect>>` holding the compiled SkSL an
`Filter` runs — the bright pass and phosphor halo a bloom gathers,
the tap that lays that halo back, a light's deepening and whitening, and
a parametric blur's mix. The recipes a `Paint` runs are not among
them; those are reached through the program cache, one per recipe.

They are the very objects the effects go on to use, not copies. A device
backend can be asked to give a runtime effect a name that outlives the run,
so a device program built over one can be written down and rebuilt at the
next launch instead of compiled again — and the name is given to the OBJECT,
so an effect compiled a second time from the same source is a stranger to
it. An effect's place in the list is part of its name, which is why the
order is fixed and why a body that would not compile is absent from the list
rather than null in it: the list is what is really there, and a machine that
loses a body offers a shorter one. Reach for this when declaring effects to
such a backend. Asking compiles all of them, which is a handful of small
programs beside the device programs they are inlined into.
