# SigilMaterial

Materials as recipe instances. A **recipe** is a material's definition: a
plain C++ struct of uniform-typed fields that is its ABI, one shader body
per shading language, the child slots it samples and the per-frame values
it reads. A **material** is one instance of a recipe: the field values,
mirrored as the bytes the shader will receive; live bindings that overwrite
fields every frame; other materials filling its child slots; and the
settings a renderer reads off the instance. A renderer asks a material to
**resolve** against a frame and receives the compiled **program** for its
shading language plus the bytes to upload — the same answer, memoised,
until an input changes.

The library links glm (for the vector types a struct may hold),
choreograph (for the animation output a field may bind to) and Boost.PFR
(for the reflection that reads a struct's field names off the type). The
core has no renderer in it: compilers arrive from backend features, and
there is one, for Skia's SkSL.

Namespace `sigil::material`. Two feature libraries, one per directory,
each a static archive that links only what sits beneath it:

| target | holds | links |
|--------|-------|-------|
| `SigilMaterialCore` | the value model: `Target`, `Params`, `Recipe`, `Program` and the cache, `Material`, `UniformBlock`, `FrameData`, `Color` | SigilGeometryPath, SigilMotionBind, Boost::pfr |
| `SigilMaterialSkia` | the SkSL compiler and `SkiaProgram`, whose builder uploads resolved bytes | SigilMaterialCore, Skia |

`SigilMaterial` is the umbrella, an interface over both. Headers live under
`include/sigilmaterial/<feature>/` and are spelled that way —
`<sigilmaterial/core/Recipe.h>`, `<sigilmaterial/skia/SkiaCompiler.h>` —
and `<sigilmaterial/Material.h>` includes the whole core.

## Using it

```cpp
#include <sigilmaterial/Material.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

using namespace sigil::material;

// The ABI: a plain aggregate of uniform fields. Names are read off the
// type; there is nothing to register.
struct Glow {
  float uScale;
  Color uTint;
  std::array<float, 8> uBars;
};

// The definition, made once and shared. The body is what follows the
// generated declarations — the uniforms above, then uTime, then the
// child slot.
auto glow = std::make_shared<const Recipe>(
    Recipe::of<Glow>("glow")
        .frame(FrameInput::Time)
        .child("uSrc")
        .body(Target::SkSL, R"(
          half4 main(float2 p) {
            half4 src = uSrc.eval(p);
            return src * half4(uTint) * half(uScale + 0.5 * sin(uTime));
          })"));

// An instance: values now, a bound clock and a live table later.
Material m(glow, Glow{1.0f, {1, 0.8f, 0.2f, 1}, {}});
m.bind("uScale", &scaleOutput);        // a choreograph::Output<float>
m.bind("uBars", spectrumBlock);        // a shared_ptr<UniformBlock>, 8 floats
m.child("uSrc", Material(gradientRecipe, GradientParams{...}));

// A renderer, once:
skia::install();                        // registers the SkSL compiler

// A renderer, per frame:
FrameData frame{.seconds = clock.now(), .resolution = {w, h}};
sk_sp<SkShader> shader = skia::shader(m, frame);
```

`skia::shader` is the whole Skia path: it resolves the material, builds
over the program's effect with every uniform set from the resolved bytes,
resolves and binds each child slot recursively, and makes the shader. A
renderer that needs the pieces takes them apart the same way —
`m.resolve(Target::SkSL, frame)` returns the `Program` and the bytes, and
`program->as<skia::SkiaProgram>()->upload(builder, bytes)` fills a builder
the renderer made over `program->effect()`.

## Mental model

**A params struct is the ABI, and the bytes are the upload.** Every field
type is some count of floats with float alignment — `float`, `glm::vec2`,
`glm::vec4`, `std::array<float, N>`, `Color` — so a struct of them has no
padding and its memory image is exactly the uniform data in declaration
order. `schema<P>()` proves this at compile time and refuses a struct with
any other field type or with padding. The same walk emits the uniform
declarations (`declare<P>(target)`), so the names in the shader are the
names in the struct and cannot drift.

**A recipe's identity is the object.** Two recipes built from the same
text are two definitions with two sets of programs; `operator==` compares
definitions and is for tests, while the program cache and a material's
equality use the pointer. Define a recipe once and hold it in a
`shared_ptr<const Recipe>` beside the code that owns it.

**One body per target, and asking for a missing one is an error once.**
`Recipe::body(Target, source)` stores the body for a language;
`Recipe::source(target)` is the generated declarations followed by it. A
material resolved for a target its recipe has no body for — or one no
compiler is registered for, or one whose body fails to compile — yields a
null program, and the cache reports it to stderr exactly once per (recipe,
target), naming both, so the mistake surfaces at the first describe rather
than scrolling past every frame.

**One program cache.** `ProgramCache::shared()` holds every compiled
program in the process, keyed by (recipe identity, target, variant). A
backend registers its compiler with `registerCompiler(Target, Compiler)`
and the cache compiles on first use. `Variant` is a small ordered key the
backend owns the meaning of — a premultiplied build, a debug view — and
the default variant is the plain build.

**Bindings are live, and equality is by identity.** `bind(name, Output*)`
makes a float field read the output's current value at every resolve;
`bind(name, shared_ptr<UniformBlock>)` does the same for an array field
and a caller-owned table. A bound material `isAnimated()`. Two materials
bound to the same output or block compare equal; bound to different ones,
unequal; the values behind them never enter the comparison. A `UniformBlock`
carries a revision (`commit()` advances it) so a caller can tell an edited
frame from an untouched one, and its values are read live whether or not
they were committed.

**Frame inputs are declared, then injected.** `Recipe::frame(FrameInput)`
declares that the body reads `uTime`, `uResolution`, `uContentScale` or
`uWorld`; the declaration adds the uniform after the params and
`resolve()` fills it from the `FrameData`. Time and content scale make a
material `isAnimated()`; resolution and the world transform make it
`geometryDependent()`. `quantizeTime(hz)` snaps the time a material sees
to a step, so a material that need not move every frame resolves only
when the snapped clock advances.

**Children ride everything.** A recipe declares slots (`child("uSrc")`,
exposed to SkSL as `uniform shader uSrc`); a material fills them with other
materials. A live child makes the parent live, a geometry-dependent child
makes it geometry-dependent, and a different child makes it unequal —
which is required, not incidental: a child left out of equality would let
a node prune while its second source had changed.

**Resolve is memoised on its inputs.** `resolve()` samples the bindings,
snaps and injects the frame values, and compares the resulting bytes plus
the target and variant against the previous call's; when they match, the
previous program and bytes come back with no cache lookup.

## Colour

`Color` is four straight (not premultiplied) sRGB floats, uploaded as one
float4. `Color.h` also holds the sRGB transfer function both ways and the
OKLab round trip — `toOklab`, `fromOklab`, `lerpOklab` — which every
perceptual interpolation in the codebase runs through.

## Building and testing

Two tests and two benchmarks, one pair per feature:

```sh
ctest --test-dir build -C Debug -R material
python3 scripts/bench_ledger.py --benches material_core_bench material_skia_bench
```

`material_core_test` links the core alone, so a link edge that pulled a
renderer into the model would fail there. `material_skia_test` compiles a
two-uniform recipe through the cache and checks the raster it shades is
byte-identical to the same SkSL compiled and filled by hand.
