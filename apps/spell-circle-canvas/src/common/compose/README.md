# SigilCompose

SigilCompose is a C++20 library for drawing a scene from application data.
Build a tree of `Element` values, give it to a `Composer`, and draw it on
an `SkCanvas`. The composer retains layout, animations and reusable paint
between descriptions. Yoga handles flex layout; SigilWeave handles text;
SigilMotion supplies the clock and animated values.

The host owns the canvas, motion engine, font context and frame cadence.
Compose works inside an existing paint callback and preserves the canvas's
matrix and clip.

## First drawing

This complete program draws one frame into a raster bitmap. In a window,
keep the engine, font context and composer alive and draw onto the canvas
your paint callback receives instead.

```cpp
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Factories.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

namespace compose = sigil::compose;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace weave = sigil::weave;

int main() {
  weave::FontContext fonts(weave::ports::systemFontManager());
  motion::Engine engine;
  compose::Composer composer(engine, fonts);
  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(480, 180)) return 1;
  SkCanvas canvas(bitmap);

  composer.setSize({480, 180});
  composer.render(
      compose::box()
          .padding(24)
          .gap(12)
          .fill(material::hexColor(0x101820))
          .ink(material::hexColor(0xe6edf3))
          .font({.size = 24})
          .children({compose::text("Signal"),
                     compose::text("A scene described from data.")
                         .font({.size = 14})}));
  composer.draw(canvas);
  return 0;
}
```

Link `SigilComposeCore` for the element runtime. This example also links
`SigilWeavePorts` because it asks for the platform font manager:

```cmake
add_executable(example example.cpp)
target_link_libraries(example PRIVATE SigilComposeCore SigilWeavePorts)
```

Include the headers that own the values you use. `sigilcompose/Compose.h`
is the kernel umbrella; brush, typography, kit and hosted features have
their own headers and targets.

## Components and identity

A component is an ordinary function from your data to an `Element`.
Keep application state in your model; Compose needs no component base
class or component lifecycle.

```cpp
#include <sigilcompose/core/Factories.h>
#include <sigilmaterial/color/Color.h>

#include <string>
#include <vector>

struct Channel {
  std::string id;
  std::string label;
  bool alarm = false;
  bool operator==(const Channel&) const = default;
};

compose::Element channelRow(const Channel& channel) {
  return compose::box()
      .row()
      .padding(12)
      .fill(material::hexColor(channel.alarm ? 0x602020 : 0x203040))
      .children({compose::text(channel.label)});
}

compose::Element dashboard(const std::vector<Channel>& channels) {
  return compose::box().gap(8).children({
      compose::each(channels, [](const Channel& channel) {
        return compose::memo(channel, channelRow).key(channel.id);
      }),
  });
}
```

`children({…})` accepts elements, typed leaves and lists returned by
`each`. A range can also be passed directly to `children`. Repeated calls
append children in declaration order.

A key preserves a child's retained identity across reordering and is the
name used by queries. Keys must be unique among siblings; use unique keys
across the tree for unambiguous keyed queries. Unkeyed siblings match by
position. Keys on nodes are separate from a keyed drawing's comparison
key and from local names on marks or stroke passes.

`memo(props, describe)` skips the component call while the properties and
captured environment compare equal. It is optional: ordinary value
properties already compare structurally. Use memo when describing a
subtree is expensive, and include every input the component reads in its
properties or captured environment.

An `Element` is a shared, copy-on-write description. Copying it is cheap;
changing a shared copy clones its payload. It owns no Yoga node or paint
cache. The composer owns the mutable retained instances behind those
values.

## Updating and drawing

There are two update paths:

| Change | What the host does |
|---|---|
| Structure, text, layout or discrete model state | Build the next tree and call `Composer::render`. |
| A paint value that changes between descriptions | Keep a `motion::Animatable` in the model and put its live value or binding in the description. |

Call `Composer::setSize` when the viewport changes. The root fills an axis
on which it states no size; an explicit root size remains its own. An empty
viewport requests intrinsic layout.

On each animation frame, advance the motion engine once, then call
`Composer::draw`. A host can use `Composer::isRunning` to decide whether
another draw is needed. That query includes dirty layout/content, running
motions and retained external bindings; `Composer::dirty` alone does not
observe all of those inputs. The engine may be shared with other work, so
its activity can keep the host drawing even when this tree is still.

```cpp
engine.advance();
composer.draw(canvas);
const bool requestAnotherFrame = composer.isRunning();
```

Describe-time transitions use `motion::animate`; the composer retargets
from the current visual value when a new description changes the target.
A live value avoids re-describing:

```cpp
#include <sigilmotion/values/Animatable.h>

motion::Animatable<float> angle = motion::animatable(0.0f);
auto marker = compose::box().width(20).height(20)
    .fill(material::hexColor(0x80c0ff)).rotate(angle);
// Keep angle alive in the model; assigning it changes the shared live cell.
angle = 45.0f;
```

Paint bindings do not run layout. Change text or dimensions by describing
again. Make a live cell once: making a new one on every description gives
it a new identity and defeats pruning.

For independently updated content, declare `slot("name")` and call
`Composer::renderSlot` with its content. The slot integrates into the same
layout and cascade. Its name is its key, so a later `.key()` renames it.
An unknown slot name leaves the tree unchanged and warns once.

The engine and font context must outlive the composer. Use the font
context on its owning layout thread. On device loss, call
`Composer::purgeCaches` before drawing through the replacement context.

## Layout and resolved queries

`box()` is a column by default; `.row()` changes its main axis. Dimensions,
flex properties, gaps, padding, margins and baseline alignment describe
layout. `stack()` overlaps its children and makes them absolute.
`positioned()` accepts nested explicit rectangles and skips Yoga below
that container; flex properties and geometry-reading layout features do
not apply inside it.

Text leaves measure their contents. A `custom()` program has no intrinsic
size, so give it dimensions or absolute insets. Bare numbers are pixels;
`pct()` reads the parent's extent. Font-relative lengths use SigilWeave's
units, while canvas-relative lengths read the viewport.

Layout runs when drawing needs it. Read `Composer::bounds`,
`Composer::paragraphLayout` and `Composer::hitTest` after a draw or another
operation that runs layout. Unknown keys return empty answers. The
paragraph pointer is valid until the next layout. Bounds are layout boxes;
hit testing also reads paint order, shapes and transforms. A keyed
transparent container can receive hits over its whole box; disable its
own hit region with `Element::hitTestable(false)` to keep its children
interactive.

`intrinsicSize` measures a tree without a persistent composer. `snapshot`
records one into an `SkPicture`; bindings are sampled and transitions do
not run. These trees inherit only from their own roots. Set their font and
ink explicitly when they need the look of another tree.

### Custom placement and generated elements

`Element::attribute` puts typed facts on a node. `Element::operators`
applies values that read those facts:

- An arranging value implements `arrange(Arrangement&)` and places or
  turns a container's direct layout children. Text slots and marks keep
  their paragraph-owned placement.
- An adding value implements `add(Scope&)` and attaches elements after
  authored layout settles. Its scope exposes keys, facts, classes,
  bounds and outlines; nested operator scopes remain closed.

Arrangers run before adders; write them in that order. Comparable
operators prune when their parameters and inputs are unchanged. A value
without equality remains conservative and reruns when described.
`layout(scheme)` is a shorter spelling of `box().operators({scheme})`.
Stock layouts and custom arrangers use the same `Arrangement`: a borrowed
span of child records with measured sizes, baselines, cells, named areas
and facts. Write each child's `rect` with `place()` or `centreAt()`; use
`turn()` for a paint-only rotation. The records are valid during the call.
An operator declaring `readsChildMinSizes` receives content minima;
`resolvesChildPercentages` supplies stated sizes. Each `Child` record
resolves them with `sizeIn(box)` against the box the operator gives it.

Each settlement round starts from authored flex placement at the current
container size and no operator turn. Within a list, a modifier reads what
the preceding arranger left; a modifier-only list reads the flex placement.
Outer placements settle before a nested list reads its inputs. Operator
output never becomes the next round's starting position.

Additions are ordinary elements attached to their subject or their scope.
They inherit and hit-test normally, stand after the authored children and
stay out of their flow. They are excluded from operator input and
structural child counts. A copied scope is a measurement snapshot and
cannot attach to the original.


## Styling and text

Font, ink, paragraph settings, sheets, custom properties, image sampling
and lighting inherit through the tree. Layout and paint declarations such
as padding, fill and transform stay local. Inheritance follows where the
node is mounted, independent of where its C++ value was built.

`text(utf8)` inherits the font and ink; `text(utf8, weave::TextStyle)` uses
a complete style. `font()` and `paragraph()` set partial overrides.
`Element::applyStyleSheet` applies a sheet to a subtree: bare selectors
match roles and `.name` selectors match classes. Role defaults are
fallbacks, matched rules override them by specificity and order, and direct
declarations override rules.

```cpp
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>

namespace document = sigil::compose::document;
auto article = document::article({
    document::h1("Field notes"),
    document::paragraph("Content keeps its meaning while a sheet chooses its look."),
    document::caption("One tree, one inherited style."),
}).applyStyleSheet(compose::StyleSheet{
    compose::rule("h1").font({.size = 36}),
    compose::rule("caption").font({.size = 12}),
});
```

The document kit supplies roles and fallback type, including headings,
paragraphs, lists, quotations and figures. Its measure, gaps and quote
inset are inherited length properties that a theme can override.

[The cascade](reference/CASCADE.md) explains precedence, keywords and
custom properties; [selectors](reference/SELECTORS.md) explains matching.
[Typography](TYPOGRAPHY.md) covers rich spans, text effects, paths,
threaded frames, readings and vertical text.

## Painting and reuse

Within a node, painting follows this order:

```text
backgrounds and background span passes
fill and echoes
overlays
content
children
foregrounds and foreground span passes
```

Sibling order is `zIndex` then declaration order; a shared 3D space uses
depth order. Transforms, compositing and layer effects form stacking
contexts. A child's z-order remains inside that context.
`overflow(Overflow::Clip)` clips fill, content and children; decorations
dress the outline outside that clip.

A fill accepts a colour, `Fill` or SigilMaterial material. Decorations are
marks attached around a boundary; material effects supply surface looks
such as shadows and bevels. `Element::decorationOutline` chooses the
shape, placed glyph contours or rendered coverage. Coverage tracing uses
a bounded raster, produces stepped edges, follows device scale and has an
opacity threshold. It excludes the node's own decorations, includes its
content and children, and falls back to the shape when empty.

Point and spot lights place their source in root-page logical pixels, with
positive z toward the viewer. A shared source lights each fill and ink where
it sits; moving a node or its ancestor updates the response. The planar
executor supports affine placement and bump normals, without displacement
or self-shadowing.

Use `scene()` when light sources should belong to the composition itself.
It is an ordinary flex container whose `light()` leaves illuminate its own
lit paint and descendants, regardless of sibling order. Light leaves take
no layout space, paint nothing and receive no hits. Their positions are
local; element transforms place them through their ancestors.
`Display::None` on a source or its ancestor disables it; opacity and
clipping leave its illumination active.
Spot and directional axes turn with those transforms. Receivers transform
their normals into the same root-page frame, so a turned surface catches
the source from its new orientation. Out-of-plane source turns or perspective
disable its direct contribution.

Planar lighting is the whole of what Compose lights. The page is the
surface and the viewer looks straight at it: a plane turned by the depth
lanes keeps the lighting it has flat, and there is no camera, no emitter
with an extent and no shadow one node casts on another. An interface lit
as a body in space is a SigilWorld surface wearing the composition as a
`material::Texture`, where the set's camera and lights shade it.

```cpp
material::Light key{.kind = material::LightKind::Point,
                    .position = {0, 0, 180}, .range = 800};
auto panel = compose::box().width(140).height(100).fill(gold);
auto composition = compose::scene().row().gap(12).children({
    compose::light(key).translateX(120).translateY(60), panel, panel});
```

Here `gold` is the caller's material. A source belongs to its nearest scene;
a nested scene starts with no inherited sources or environment. Set a
scene's reflected surroundings with `Element::environment`. A receiver's
`Element::lighting` or its material's own lighting replaces the scene
context. Attributes remain passive facts for their readers. Emissive
appearance does not register an illuminating source.

Light color, intensity and applicable angles accept live values from
SigilMotion. Keep the cells beside the application model, then update them
without describing the tree again:

```cpp
auto keyColor = motion::animatable(material::Color{1, 0.8f, 0.6f, 1});
auto strength = motion::animatable(1.0f);
auto bearing = motion::animatable(120.0f);
material::Light key{.direction = bearing, .color = keyColor,
                    .intensity = strength, .ambient = 0.05f};
auto source = compose::light(key);
keyColor = material::Color{0.4f, 0.7f, 1, 1};
strength = 0.6f;
bearing = 30.0f;
```

The host draws after changing a cell; `Composer::isRunning` observes a
changed scene source even after its receivers settle into caches. Positions,
range, cone and ambient values are ordinary fields. Animate source placement
with its element's transforms, or describe again when those fields change.

For a caller's material `gold`, ordinary `.ink(gold)` shades the glyphs
under the lighting in force. Outline relief adds a bevel derived from
the actual contours; its glyph foreground supplies the text paint, so
set the ordinary ink transparent to avoid painting the glyphs twice.

`relief(material, options)` shades a material over that outline, combining
the contour bevel with any normal map in the material. Shoulder is measured
in logical pixels; positive depth raises the outline and negative depth
impresses it. Attach it as a shape background or a glyph foreground:

```cpp
#include <sigilcompose/brush/Relief.h>

auto lettering = compose::text("AURUM").font({.size = 64})
    .ink(material::Color{0, 0, 0, 0})
    .decorationOutline(compose::Boundary::Glyphs)
    .foreground(compose::relief(gold, {.shoulder = 2, .depth = 1}));
```

Here `gold` is the caller's material. Link `SigilComposeBrush`. Relief keeps
its layers, effects and live surface channels. It is a decoration;
ordinary material fills and inks remain available through `fill` and `ink`.

A material Ribbon uses the same lighting and effect path over its swept
band. `brush::ribbon(profile, material)` keeps the full material; its `Fill`
overload paints only the colour stack. Layer brushes with `brush::layers`
when several marks need to share one outline.

Elements can also supply information to a material instead of appearing
as separate visible layers. Bake an independent tree with `texture()` or
hold a `TextureScene` for an editable tree. Paint white marks on an opaque
black ground for a height map, including any blur that shapes their edges.
Pass `material::image(scene->texture())` to
`material::surface::normalFromHeight`, then put that result in the shared
background material's normal channel. The same texture can mask colour,
roughness or metallic changes. Lettering then changes how the background
takes light, including where marks overlap. Link `SigilComposeTexture`
and `SigilMaterialSurface` and include their own headers. Keep the source
tree independent of the material consuming it to avoid a feedback loop.
Retain the texture scene; its settled tree does not repaint as the final
surface's lighting moves.

Leave `Cache::Auto` in place for ordinary scenes. The composer records
static content and chooses reuse from declared inputs. A custom paint
program that reads time, input or external state without re-describing
must use `Cache::None`. Custom decoration schemes must forward their
animation, overflow, mark width, backdrop, lighting, root-space sampling and borrowing
capabilities when composed; `DecorationStack` and the stock adaptors do
that for their members. A scheme that samples root coordinates declares
`usesWorldSpace()` so a moved ancestor invalidates its held paint.

`Cache::Picture`, `Cache::Texture` and `Cache::Group` express explicit
reuse choices. Global texture promotion is a separate measured policy,
and bake density controls raster resolution. Those controls are useful
when a profile identifies work to change; they are not needed to start a
scene. `Composer::setProfiling` and `Composer::profile` report per-node
paint cost and cache decisions. `PromotionPolicy::Eager` is a deterministic
eligibility-testing mode, not a performance recommendation.

[The caching contract](reference/CACHING.md) states the requirements for
custom drawing and the reuse policies. [Traps](reference/TRAPS.md) covers
silent misses, callable equality, lifetime and ordering. Comparable values
prune automatically; raw callables remain conservative. A keyed callable
asserts that one comparison key always names the same drawing, so include
every parameter its body reads in that key.

## Features and boundaries

Link the feature you use:

| Target | Provides |
|---|---|
| `SigilComposeCore` | Elements, Composer, layout, transitions, basic text and paint. |
| `SigilComposeTypography` | Text effects, paths, readings and the text executor. |
| `SigilComposeBrush` | Stroke and decoration execution, brushes and masks. |
| `SigilComposeKit` | Stock layouts, components, document roles and drawing recipes. |
| `SigilComposeDraw` | A SigilDraw pen hosted in a tree, and retained elements drawn by a pen. |
| `SigilComposeTexture` | A composer and owned surface exposed as a texture value. |
| `SigilComposeWeb` | Web content as a leaf, when the Ultralight SDK is available. |
| `SigilComposeTesting` | Consumer test helpers. |
| `SigilCompose` | Convenience target over Kit, Brush and Typography, including Core. |

Hosted draw, texture and web features are explicit opt-ins. Public headers
live under `include/sigilcompose/<feature>/`; the
[header map](reference/HEADERS.md) lists their vocabulary. The
[reference catalogue](reference/README.md) indexes factories, verbs and
accepted values, with examples beside each entry.

Values retain their library of origin: outlines and geometry are
SigilGeometry's, paints and lighting SigilMaterial's, pixels SigilMedia's,
text and paragraphs SigilWeave's, clocks and animated values SigilMotion's.
SigilCore owns generic reconciliation and cache decisions. Compose owns
their execution over Yoga, text state and Skia. It does not re-export
those libraries' namespaces.

The composer borrows a canvas and owns no window, surface or render loop.
`TextureScene` owns an offscreen surface because its output must outlive a
draw. Its size factory makes an N32 raster scene. Its `SkImageInfo` factory
accepts premultiplied RGBA/BGRA byte, RGBA F16 and RGBA F32 formats and keeps
the color space. F16 retains HDR values; F32 is raster-only. Device adoption
keeps the requested format and profile, leaves the scene intact on failure,
and clears its pixels and caches on success. Fonts, device and context must
outlive the scene and its texture values. A device `image()` belongs to its
Graphite context; it is not a portable raster readback. `SceneSource` reads
the latest scene image and compares by its captured revision, rather than
preserving historical pixels.

The depth features project planes in a 2D tree; use SigilWorld for a
world scene. [Depth](reference/DEPTH.md) explains projection and hit testing.

Compose's pixel caches keep the borrowed canvas's color type and color space,
with premultiplied alpha. Float destinations retain values above one through
texture, group, atlas and brush-art bakes. Destination format changes invalidate
the bakes and recordings that contain them. One-shot snapshots have no
destination and default to N32 for embedded pixel bakes. Compose introduces no
linear compositing stage or input-space conversion. `Composer::setView` applies
a final output filter without changing the per-node caches.

Skia remains at host and executor boundaries: borrowed canvases, recorded
pictures, image inputs and the private paint seams. A paint program uses a
SigilDraw pen and can reach its canvas when it needs lower-level drawing.
A plot's domains and scale mappings belong to SigilData in its consumer;
Compose supplies the layout and marks.

## Working on the library

[core/README.md](core/README.md) maps the private implementation's ownership
and reading path. Feature targets are deliberate dependencies; the
internal description, style, runtime, layout, paint and cache directories
remain one kernel, not additional libraries.

[The shared testing contract](../../../docs/overview/testing.md) describes
builds, tests, labels and benchmarks. Compose has one `compose_test` and
one `compose_bench` assembled from its feature directories. CTest selects
individual cases. Public-header tests compile each exported header alone;
each feature probe uses only its originating target's requirements.
Documentation probes check names in this page and its registered chapters.
The first program above is also a standalone consumer example and must
compile with its stated link targets.

For retained rendering tests, include
`<sigilcompose/testing/Scene.h>` and link `SigilComposeTesting`.
`test::Scene` takes a font context and raster dimensions. Describe through
its composer, call `frame(seconds)` with an explicit time step, and inspect
`pixel(x, y)` or an owned `pixels()` snapshot. It needs no window or GPU.
Draw adapter cases reuse the pen's raster fixture and carry the
`integration` label.

Committed test fonts keep ordinary cases independent of the machine.
Device and platform-font cases carry labels for their dependencies.
Use the benchmark for scaling and `Composer::profile` for a costly node;
`COMPOSE_PROF=<ms>` reports slow paints through Skia's debug output when a
host has not enabled profiling. Compose's warnings use that same output
channel and suppress repeated causes.
