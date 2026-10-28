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

A keyed node that states `Element::exit` leaves as it entered. When a
later description leaves it out, it keeps its place in the layout, so its
siblings do not move until it has gone; it answers no hit; and every
entrance under it plays backwards: each `motion::animate({.from = a, .to
= b})` from where its value stands back to `a`, over the same duration,
on its curve run backwards, and a text entrance's track back to the start
of its schedule. The nodes one description leaves out together leave as
one run reversed, so a stagger that brought them in one after another
takes them out last first. The node is dropped on the frame the last of
those motions lands, whether or not anything is described in between.
Named again before then, it turns round and plays forward from where it
stands.

```cpp
compose::Element row(const Channel& channel) {
  return compose::box()
      .key(channel.id)
      .opacity(motion::animate({.from = 0.0f,
                                .to = 1.0f,
                                .duration = std::chrono::milliseconds(300),
                                .delay = motion::stagger(std::chrono::milliseconds(60))}))
      .children({compose::text(channel.label)})
      .exit();
}
```

An unkeyed node, a node with nothing under it that entered, and a node in
a capture that draws settled values, such as `compose::snapshot`, drop at
once. On a memo, `.exit()` is stated on the shell beside `.key()`.

`memo(props, describe)` skips the component call while the properties
compare equal. It is optional: ordinary value properties already compare
structurally. Use memo when describing a subtree is expensive, and include
every input the component reads in its properties; a value the sheet in
force sets, such as a custom property `var()` names, is resolved by the
cascade after the call and needs no place in them.

A memo also follows the live values its component reads. A
`motion::Animatable` made by `motion::animatable()` or `motion::bind()`
whose `value()` the component reads becomes one of the memo's inputs
beside its properties: when the number changes, the next draw calls the
component again and reconciles that memo's subtree alone, so a readout, a
count or a flag that changes at its own pace needs no dirty flag, no
compared copy and no call to `render`. Several changes before one draw
are one call, and a change made before a draw shows in that draw.
`Animatable::peek()` reads the number as it stands without following it.
A component that writes a live value it has read keeps the number it
read: the write is refused and reported once, so the memo never calls
itself again. Only a memo's component follows: a value read in a paint
program, a timer or a describe outside any memo is read once.
`Composer::beatsOf` asked inside a memo's component is followed the same
way: the memo is described again when the queried node first appears,
when a layout moves it or sets its text again, and when its track's
progress changes.

```cpp
motion::Animatable<float> gold = motion::animatable(0.0f);

compose::Element purse() {
  return compose::memo(0, [](int) {
    return compose::text(std::to_string((int)gold.value()));
  }).key("purse");
}

gold = 120.0f;  // the next draw shows 120
```

An `Element` is a shared, copy-on-write description. Copying it is cheap;
changing a shared copy clones its payload. It owns no Yoga node or paint
cache. The composer owns the mutable retained instances behind those
values.

## Updating and drawing

There are three update paths:

| Change | What the host does |
|---|---|
| Structure, text, layout or discrete model state | Build the next tree and call `Composer::render`. |
| A paint value that changes between descriptions | Keep a `motion::Animatable` in the model and put its live value or binding in the description. |
| A number, word or flag one part of the tree is described from | Keep a live value in the model and read it with `value()` inside a `memo`'s component; the draw after it changes describes that memo again. |

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

A host that keeps the pixels its last draw left can repaint only what
changed. `Composer::changedRegion()` answers the rects, in canvas units,
outside which the next draw paints exactly what the last one did: every node
whose own paint changed, moves on the clock, moved, or gained, lost or
reordered a child adds where its ink stood and where it stands, its
decorations' bleed and its layer effects' reach included. Clip the canvas to
those rects, clear them and draw; if `drewWithinChangedRegion()` then
answers false — a text leaf of a stated size is set at paint and can reach
past the ink it had — draw again whole. It answers none, meaning the whole
canvas, before the draw that first keeps its extents, after a new size,
view, map or cache purge, on a composer that bakes pixels, and where a
changed node cannot be bounded on its plane (a backdrop, a plane turned in
depth, a custom program). A page painted as surface maps and every
`TextureScene` painting one do this themselves.

Paint bindings do not run layout. Change text or dimensions by describing
again. Make a live cell once: making a new one on every description gives
it a new identity and defeats pruning.

Content that changes with a live value is a memo reading it, which the
draw describes again by itself. `Composer::render` and slots are the
manual doors, for a change no live value carries: declare `slot("name")`
and call `Composer::renderSlot` with its content. The slot integrates into
the same layout and cascade. Its name is its key, so a later `.key()`
renames it. An unknown slot name leaves the tree unchanged and warns once.

The engine and font context must outlive the composer. Use the font
context on its owning layout thread. On device loss, call
`Composer::purgeCaches` before drawing through the replacement context.

## Layout and resolved queries

`box()` is a column by default; `.row()` changes its main axis.
`compose::row(a, b)` and `compose::column(a, b)` start a box with its
line stated and its children given in the call. Dimensions,
flex properties, gaps, padding, margins and baseline alignment describe
layout. `stack()` overlaps its children and makes them absolute.
`positioned()` accepts nested explicit rectangles and skips Yoga below
that container; flex properties and geometry-reading layout features do
not apply inside it.

Text leaves measure their contents. An `image()` leaf's fit holds in
whatever box it stands in: a free leaf under `Contain` or `Cover` takes the
picture's proportions, and a leaf whose box is stated in another shape
draws `Cover` as the centred crop that covers the box and `Contain`
letterboxed and centred in it, applied to its `imageRegion()` where one is
stated. A `custom()` program has no intrinsic
size, so give it dimensions or absolute insets. Bare numbers are pixels;
`percent()` reads the parent's extent. Font-relative lengths use SigilWeave's
units, while canvas-relative lengths read the viewport.

Layout runs when drawing needs it. Read `Composer::bounds`,
`Composer::paragraphLayout` and `Composer::hitTest` after a draw or another
operation that runs layout. Unknown keys return empty answers. The
paragraph pointer is valid until the next layout. Bounds are layout boxes;
hit testing also reads paint order, shapes and transforms. A keyed
transparent container can receive hits over its whole box; disable its
own hit region with `Element::hitTestable(false)` to keep its keyed
children interactive. Its keyless contents go with it, as CSS's
`pointer-events: none` is inherited, so the point falls through them to
what lies beneath.

A text leaf in a column keeps the height of its lines when the column
runs short of room, as CSS's automatic minimum floors a flex item at its
content: a leaf whose lines run horizontally, that does not clip them and
that stands in the column's flex line holds its lines unless it says
otherwise. `minHeight(0)`, or any stated minimum height or basis, lets the
column shrink it; a stated height or maximum shorter than its lines is
its floor instead. Yoga has no automatic minimum, so the floor is the leaf
declining to shrink, and one thing differs from CSS: a leaf with a stated
height taller than its lines holds that height where CSS would give the
column the room back down to its lines. A box in a column and a text leaf
in a row still shrink to nothing, where CSS floors them at their content.

Layout reports what it would otherwise draw without a word, through the
reporting door SigilCore keeps, so a host's installed reporter hears it.
A single-line flex container whose in-flow children run past its
box along its main axis is named with how far they run and the first
child past its edge, unless it clips them with `Overflow::Clip`, an
arranging operator places them, or a clipping ancestor answers for the
spill: its box holds the whole spill, or its clip hides all of it. A
spill a clip cuts in part is named, as the cut shows. A text leaf that a stated minimum lets a column shrink
below its lines is named too, with the minimum or basis that let it, as
its lines paint over what follows. Each is said once when it starts, and
again only after the node has fit; a redraw, or a layout that leaves it
standing, says nothing.

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
An operator declaring `readsChildMinimumSizes` receives content minima;
`resolvesChildPercentages` supplies stated sizes. Each `Child` record
resolves them with `sizeIn(box)` against the box the operator gives it.
A scheme's own distances are lengths read where the node lays out:
`Arrangement::pixels` resolves one against the node's font, the canvas and
the custom properties in force there, so a `layouts::Grid` gap written
`layouts::Gap::both(var("spacing-cell-gap"))` is the gutter the sheet in
force states, and a change to that property lays the node out again.

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

Font, ink, paragraph settings, sheets, custom properties and image sampling
inherit through the tree. Layout and paint declarations such
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

`textShadow` casts a shadow from each glyph, CSS's `text-shadow`: it is
drawn with its glyph, beneath it, so a `textFx()` track that fades, tints
or deforms the glyph takes the shadow along, and a blurred shadow is
rasterised once per glyph and size rather than over the passage's box. A
shadow naming no colour is the colour its glyph is painted in; one naming a
custom property, `var(name)`, reads it where the shadow is
stated. It inherits as the font does. A shadow, glow or bevel among an
ink material's effects dresses the node's outline instead, its glyphs
under `Boundary::Glyphs`, in one colour.

`boxShadow` is CSS's `box-shadow` on the node's own outline: a `Shadow`
moved by its `offset`, grown by its `spread` and softened by its `blur`
beneath the fill, or with `inset` a band hugging the inner edge over the
fill and under the content; a list paints its first entry on top. The
three shadows share CSS's words — `offset`, `blur`, `spread`, `color`,
`inset` — on `Shadow`, `TextShadow` and `material::ShadowOptions`.
`readShadow(text)` and `readTextShadow(text)` read CSS's own lists —
`readShadow("0 4px 12px 2px rgba(0 0 0 / .3) inset")`,
`readTextShadow("3px 3px red, yellow -3px 3px 2px")` — onto those values,
the first on top, and answer nothing for text no shadow holds.

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
such as shadows and bevels. These are layer styles, not lighting: each
states its own highlight direction, colours and, for chrome, the picture
its face reflects, and paints the same with no scene and no light. `Element::decorationOutline` chooses the
shape, placed glyph contours or rendered coverage. Glyph contours are
taken where the glyphs are drawn: a `textFx()` track that moves, scales,
turns, shears or hides a glyph this frame moves its contour with it, and a
passage no track deviates keeps the outline its layout cut. Coverage tracing uses
a bounded raster, produces stepped edges, follows device scale and has an
opacity threshold. A silhouette over children that only turn or slide is
composed from held parts, so its edges are each child's own steps carried
with it rather than device-aligned steps. It excludes the node's own decorations, includes its
content and children, and falls back to the shape when empty.

A shadow, bevel or inner shadow cast from a silhouette of held parts keeps
each part's blurred cast as a mask, drawn on the device the page is drawn on
and again only for a new silhouette, cast or density. A bevel or a blurred
inner shadow on any other outline keeps its blurred band the same way and
cuts it by the outline where it is drawn, so an ink changing beside it
blurs nothing again.

**Compose does not shade.** A fill, ink or stroke whose material states a
surface (`material::SurfaceOptions`) paints that material's colour stack on
a page, flat, exactly as a material that states no surface paints it; its
normal, roughness, metallic, occlusion and emission channels change nothing
there. A relief paints its material's colours flat over the outline, and
glyph and word ink restart those colours on each unit. Light belongs to
SigilWorld: a page is lit by a World scene through its surface maps.

**A World scene lights a page through its surface maps.** `SurfaceScene` renders
one retained tree as the maps of a surface — base colour, normal,
roughness, metallic, occlusion and emissive — and `maps()` answers them as
a `material::texture::TextureMaps`. Hand that to
`material::surface::program(maps, parameters)` and put the result on a
World surface: the scene's lights, environment and camera shade the page.
Every lit fill, ink (Glyph and Word units included), material stroke and
relief paints the map its material states; content that states no surface
— a plain colour, a gradient, an image, flat text, a layer style — is a
stock surface: its colour in the base colour map, a normal facing the
viewer, roughness one half, occlusion one and metallic and emission zero,
so it takes the scene's light, shadows and environment like the rest of
the body. Only a material that states emission glows, and one stated
unlit — `material::SurfaceOptions::unlit` or `material::surface::unlit` — is
its own colour, in the emissive map over a black base. Where nothing paints,
every map reads a transparent base colour over a normal facing the viewer,
roughness and occlusion one and metallic and emission zero, and an
antialiased edge composites each map by its coverage, normals included as
their encoding. Normals are encoded green up the picture. A material that
states a normal of its clear coat's own adds a `ClearcoatNormal` map, as a
stated optional lobe adds its maps: a sheen, a specular, a brushing, a thin
film, a transmission with the thickness, index, dispersion and attenuation
of its volume, a distortion.

**A page keeps the range its content states, or the one stated.** The maps are eight bits, so
the emissive map's white stands for the brightest emission any material on
the page has stated — one at least — and `maps()` says that strength:
content at or below one paints exactly as stated, and an emission of four
paints white and is read back as four, so it reaches a bloom's threshold.
The thickness map's white stands for the deepest volume stated and the
distortion map's full swing for the strongest distortion, both in scene
units, and each region's volume is in its own maps. A `SurfaceRange` handed to
`SurfaceScene::make` states those readings from the first paint instead —
a page whose emission moves on the clock states the brightest it will
reach, nothing below it clips, and the page never repaints because its
content grew brighter. A body wearing the page transmits,
bends and displaces what stands behind it exactly where the page's maps say
it does, and shows its own colour everywhere else.

**Every channel stands on the unit the colour does.** A fill or an ink
names a unit with its `PaintBox` — the element's box, its padding or
content box, the subtree's, the canvas, or a glyph, cluster, word, line or
sentence of a passage — and the material's normal, numbers, coat, emission
and lobes are placed on that unit and restart on it exactly as its colour
stack does. A material may also read the unit as a source, resolved by
the painter when it paints: `material::Material::relief` raises or impresses
the unit's own coverage softened over the shoulder into the normal map, and
`material::Material::brushed` brushes along the unit — a keyline's tangent,
a box's long axis, a glyph's box's long axis, in the page. A span's
ink that names no unit reads the stretch of glyphs it paints as one, and a
material ribbon reads its band, brushed along its spine's tangent wherever
it bends. Every normal a page's maps carry is written in the page: a node
turned a quarter turns the lean of its normals with it, as its brushing
turns. So
`.ink(stone.relief({.shoulder = 3, .depth = 1}), PaintBox::Glyph)` cuts each
letter and the same ink with `PaintBox::Canvas` cuts the passage as one
mark; under `PaintBox::Word` neighbouring letters share one height, so the
shoulders between them fall where the word's do. A material holds no
element: a body wearing it with no page behind it has no unit to read and
says so.

```cpp
#include <sigilcompose/texture/SurfaceScene.h>

auto page = compose::SurfaceScene::make({1024, 640}, fonts);
page->render(tree, seconds);
material::Material worn = material::surface::program(page->maps());
```

Here `tree`, `fonts` and `seconds` are the caller's. Each map is its own
`TextureScene` over the tree, so nothing a composer recorded for one map
is replayed for another, a still tree paints none of them again, and a
tree that changed repaints only the region it changed in each map.
`page->maps(region)` answers the maps of one rectangle of the page, in page
pixels, each a cut of the page's map (`TextureScene::cut`) kept in step
with every paint, so several bodies wear one page's parts. The
mode underneath is `Composer::setSurfaceMap`, with
`Composer::setSurfaceMapScale` for what a map's white stands for; a
`TextureScene` takes one map through `setSurfaceMap` too. A page painted as a map takes no pixel
bakes. A layer effect that makes colour of its own over lit content — a
shadow or glow filter on a group — writes that colour into every map.

`relief(material, options)` is the same relief said over an outline: a
decoration that paints `material.relief(options)` with the outline it is
given as the unit — a shape, placed glyph contours, traced coverage —
combining the relief with any normal map in the material, in the normal map
a World scene lights the page by. Shoulder is measured in logical pixels;
positive depth raises the outline and negative depth impresses it. Attach it
as a shape background, or as a glyph foreground with the ordinary ink set
transparent so the glyphs are not painted twice:

```cpp
#include <sigilcompose/brush/Relief.h>

auto lettering = compose::text("AURUM").font({.size = 64})
    .ink(material::Color{0, 0, 0, 0})
    .decorationOutline(compose::Boundary::Glyphs)
    .foreground(compose::relief(gold, {.shoulder = 2, .depth = 1}));
```

Here `gold` is the caller's material. Link `SigilComposeBrush`. Relief keeps
its layers, effects and live surface channels. Its options are
`material::ReliefOptions`; a fill or an ink states the same relief on its
own unit with `material::Material::relief`.

A material Ribbon uses the same surface map and effect path over its swept
band. `brush::ribbon(profile, material)` keeps the full material; its `Fill`
overload paints only the colour stack. Layer brushes with `brush::layers`
when several marks need to share one outline.

A material keyline is the stroke said over the node's own outline:
`stroke(material, options)` widens the outline by `compose::StrokeOptions`
— `width` on the side `position` names, an open contour ended with `cap`,
corners met with `join` and cut back past `miterLimit` — in the geometry
library's words, and the band is the silhouette Skia strokes. That band is
the unit its material reads: `relief` raises or impresses it, caps and
corners included, and `brushed` runs along the outline's tangent through
every bend. A width of zero or less draws nothing.

```cpp
auto trace = compose::box()
    .shape(compose::heldPath(spine))
    .stroke(copper.relief({.shoulder = 4, .depth = 0.6f}).brushed({}),
            {.width = 14,
             .position = material::StrokePosition::Center,
             .cap = geometry::path::Cap::Round,
             .join = geometry::path::Join::Round});
```

Here `spine` and `copper` are the caller's; the options are
`<sigilcompose/core/verbs/Decoration.h>`'s and the cap and join
`<sigilgeometry/path/Stroke.h>`'s.

Elements can also supply information to a material instead of appearing
as separate visible layers. Bake an independent tree with `texture()` or
hold a `TextureScene` for an editable tree. Paint white marks on an opaque
black ground for a height map, including any blur that shapes their edges.
Pass `material::image(scene->texture())` to
`material::surface::normalFromHeight`, then put that result in the shared
background material's normal channel. The same texture can mask colour,
roughness or metallic changes. Lettering then changes how the background
takes a scene's light, including where marks overlap. Link `SigilComposeTexture`
and `SigilMaterialSurface` and include their own headers. Keep the source
tree independent of the material consuming it to avoid a feedback loop.
Retain the texture scene; its settled tree does not repaint as the scene's
lights move.

ONE RULE DECIDES WHAT IS PAINTED AGAIN: a node records again when
something it paints has changed, and an author's cache statement is the
only bake. Leave `Cache::Auto` in place for ordinary scenes: the composer
records static content as pictures and replays them until a declared
input moves. A custom paint program that reads time, input or external
state without re-describing must use `Cache::None`. Custom decoration
schemes must forward their animation, overflow, mark width, backdrop,
root-space sampling and borrowing capabilities when composed;
`DecorationStack` and the stock adaptors do that for their members. A
scheme that samples root coordinates declares `usesWorldSpace()` so a
moved ancestor invalidates its held paint. A scheme whose ink stands only
on the outline it is handed, within its bleed of it, declares
`staysOnOutline()`: a node that declares a shape smaller than its box and
paints only its fill and such marks is then sized to the shape, so the
layer under its opacity covers the stroke rather than the box its
coordinates came from; a filter over it and a bake of it still take the
box, the extent their pixels are exact to. Every mark is one or the
other: the stock strokes, stamps, swept art, shadows, washes, hatches,
reliefs and bevels, and the adaptors and composites over them, declare it
wherever their bleed holds their whole reach from the outline, and a
stroke's bleed holds its mitred corners and square caps at the
renderer's miter limit; a `ContourWalk` declares it once its `bleedPx`
states how far its bodies stand off the outline; `Slice`,
`styles::Brackets` and `styles::TickRail` are laid against the box and
keep it. A described
tree may be
painted by several composers at once, so a scheme keeps nothing it
prepares in its own value: what a mark keeps between paints stands on the
instance painting it, handed to the mark through `PaintContext::markState`
and `PaintContext::stamps`, and a mark painted outside a composer prepares
what it needs for that one paint.

`Cache::Picture`, `Cache::Texture` and `Cache::Group` are the author's own
reuse statements: a recording kept even for a leaf, a pixel bake, and a
pixel bake of a subtree whose children animate. The one bake an author
states without a cache statement is a live `filter()` over still content:
the content is rasterized once with the effect left out, and the effect
runs over that image every frame. A composer told its draws are watched
live (`Composer::setLive`, which a window's session says and a capture
never does) holds more: a node whose own transform moves from one draw
to the next while nothing it paints moves inside it is painted once into
pixels on the device at the scale it is shown at and composited under each
new matrix, as a browser's compositor turns a layer; a node whose opacity
alone moves is painted once in place, the pixels the layer a fade opens
would hold, and composited at each new opacity with no layer opened. Each
is painted live again the draw it holds still. Nothing else is baked to
pixels. Bake
density controls the resolution of those bakes. Those
controls are useful when a profile identifies work to change; they are not
needed to start a scene. `Composer::setProfiling` and `Composer::profile`
report per-node paint cost and how each node produced its pixels.

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
| `SigilComposeKit` | Stock layouts, components, document roles, drawing recipes and the text paints (`kit::textPaint`). |
| `SigilComposeDraw` | A SigilDraw pen hosted in a tree, and retained elements drawn by a pen. |
| `SigilComposeTexture` | A composer and owned surface exposed as a texture value, and a tree rendered as a surface's maps. |
| `SigilComposeWeb` | Web content as a leaf, when the Ultralight SDK is available. |
| `SigilComposeTesting` | Consumer test helpers. |
| `SigilCompose` | Convenience target over Kit, Brush and Typography, including Core. |

Hosted draw, texture and web features are explicit opt-ins. Public headers
live under `include/sigilcompose/<feature>/`; the
[header map](reference/HEADERS.md) lists their vocabulary. The
[reference catalogue](reference/README.md) indexes factories, verbs and
accepted values, with examples beside each entry.

Values retain their library of origin: outlines and geometry are
SigilGeometry's, paints and surfaces SigilMaterial's, pixels SigilMedia's,
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
and clears its pixels and caches on success. On a device the scene paints
level zero of a texture the device makes with the whole chain its size
holds, and whoever wears it fills the levels below; a Metal context over a
Vulkan device whose driver runs over Metal paints the Metal texture the
driver keeps behind that image, which is how a page stands on Apple. Fonts,
device and context must outlive the scene and its texture values. A device
`image()` belongs to its Graphite context; it is not a portable raster
readback, so the scene's texture names its pixels on the device and answers
a recorder of that same context with the image as it stands and any other
recorder — another context on the same device included — or none, with a
host copy read once per revision (`TextureScene::recorder` names the
scene's own). `SceneSource` reads the latest scene image and compares by
its captured revision, rather than preserving historical pixels.
`compose::Textures` is the door a tree that does not move comes in by: it
paints a tree at a size over a background once and hands the same texture
to every later ask for an equal one, equal as `sameDescription` judges, so
a setup that builds the same map twice paints it once. A tree holding a
paint program, keyed or not, equals only itself: the same value asked for
again is handed back and one built again is painted again, because a
custom node's key names its program across one place's describes and the
trees the door compares share no place.
`Textures::sweep` lets go of what was not asked for since the sweep before,
which is how a host that declares a body again keeps the maps it still
asks for.

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
Use the benchmark for scaling and `Composer::profile` for a costly node.
Compose's warnings go through SigilCore's one reporting door under the
name SigilCompose, so a reporter a host installs there hears them in
place of the standard error; repeated causes are said once.
