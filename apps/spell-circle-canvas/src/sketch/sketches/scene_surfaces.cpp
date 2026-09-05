/** @file
 * scene_surfaces — a compose scene is an ordinary texture, and every
 * sampling dial applies to it.
 *
 * There is no panel element here and no door of its own. What crosses
 * from 2D to 3D is a `material::Texture` in a surface's base-colour
 * slot, which is why one kind of content dresses three different
 * surfaces without either side knowing about the other's shape:
 *
 *   FLAT — three cards on an arc, each a quad wearing its own scene.
 *   CURVED — one `cylinderPanel` band under them, taking the same kind
 *     of content with no change on either side of the slot.
 *   TILED — one swept ribbon, whose card repeats along the band's v and
 *     stands once across its u, which is `Texture::tile` and `uv()`
 *     doing the work a panel type would otherwise have to invent.
 *
 * THE SCREENS ARE THEIR OWN LIGHT. A screen is `kit::unlit`, so what it
 * shows is exactly what its compose tree painted and the room's emitters
 * do not reach it — the honest reading of a display, and what makes the
 * picture a statement about the 2D content rather than about the shading
 * over it. The ribbon is `kit::surface` instead: it is printed tape, not
 * a display, so the same texture read through a lit body is the third
 * reading and the shelf beside them is the fourth.
 *
 * THE CARDS AND BANDS ARE OPEN SURFACES. Each keeps its backface visible,
 * so the turntable can pass behind it without the geometry disappearing;
 * a closed shelf keeps the ordinary hidden-backface default.
 *
 * EACH SURFACE'S SCENE IS ASKED FOR ONCE, while the study is declaring
 * itself: `ctx.textureScene(size)` hands back a scene the session keeps,
 * and `describe` only renders the tree of the moment into it and reads
 * its texture. A scene asked for per frame would be a scene held per
 * frame, which the session's counters would report as a number that
 * climbs.
 *
 * EDIT THESE FIRST
 * kArcSpreadDeg — how far round the arc the three flat cards spread
 * kRepeats — how many times the ribbon's card repeats along its length
 * kRibbonRadius — how wide the tape loop stands round the console
 */

#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Sections.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/style/Type.h>
#include <sigilworld/kit/Kit.h>

#include <array>
#include <cmath>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace compose = sigil::compose;
namespace weave = sigil::weave;
namespace gm = sigil::geometry::mesh;
namespace sections = sigil::geometry::sections;

namespace {

constexpr float kTwoPi = 6.283185307179586f;
/** The console's own geometry: how wide a card stands, how far out the
 *  arc bows, and how far round it the three of them are spread. */
constexpr float kCardWidth = 210.0f;
constexpr float kCardHeight = 140.0f;
constexpr float kArcRadius = 300.0f;
constexpr float kArcSpreadDeg = 52.0f;
/** The tape loop under the console: how far out it runs, and how many
 *  times its card repeats round it. */
constexpr float kRibbonRadius = 360.0f;
constexpr float kRibbonDrop = -150.0f;
constexpr float kRepeats = 6.0f;
constexpr int kTapeWidth = 512;
constexpr int kTapeHeight = 160;

/** A LEVEL SCREEN: a row of bars whose heights ride one wave, so the
 *  whole row moves together and no single bar has to be watched. */
compose::Element levels(float seconds, SkColor4f accent) {
  compose::Element root = compose::box()
                              .width(compose::pct(100))
                              .height(compose::pct(100))
                              .column()
                              .gap(8.0f)
                              .padding(16.0f)
                              .fill(compose::hex(0x12171f));
  root.child(
      compose::text(u8"LEVELS", weave::textStyle({.size = 22.0f,
                                               .color = compose::hex(0xbfd4ef),
                                               .antiAlias = false})));
  compose::Element row =
      compose::box().row().gap(7.0f).height(compose::pct(100));
  for (int i = 0; i < 9; ++i) {
    const float phase = seconds * 2.1f + (float)i * 0.55f;
    const float height = 18.0f + 62.0f * (0.5f + 0.5f * std::sin(phase));
    compose::Element column = compose::box().width(14.0f).column();
    column.child(
        compose::box().width(compose::pct(100)).height(92.0f - height));
    column.child(
        compose::box().width(compose::pct(100)).height(height).fill(accent));
    row.child(std::move(column));
  }
  root.child(std::move(row));
  return root;
}

/** A TRACE SCREEN: one line of blocks whose brightness travels, which is
 *  a moving picture made without moving anything. */
compose::Element trace(float seconds, SkColor4f accent) {
  compose::Element root = compose::box()
                              .width(compose::pct(100))
                              .height(compose::pct(100))
                              .column()
                              .gap(10.0f)
                              .padding(16.0f)
                              .fill(compose::hex(0x0f141c));
  root.child(
      compose::text(u8"TRACE", weave::textStyle({.size = 22.0f,
                                              .color = compose::hex(0xbfd4ef),
                                              .antiAlias = false})));
  constexpr int kCells = 14;
  compose::Element row = compose::box().row().gap(5.0f).height(44.0f);
  for (int i = 0; i < kCells; ++i) {
    const float at =
        std::fmod(seconds * 0.45f + (float)i / (float)kCells, 1.0f);
    const float lit = 0.16f + 0.84f * std::pow(1.0f - at, 3.0f);
    row.child(compose::box()
                  .width(compose::pct(100))
                  .height(44.0f)
                  .fill(SkColor4f{accent.fR * lit, accent.fG * lit,
                                  accent.fB * lit, 1.0f}));
  }
  root.child(std::move(row));
  root.child(compose::text(u8"one wave, fourteen cells",
                           weave::textStyle({.size = 19.0f,
                                          .color = compose::hex(0x7e93b4),
                                          .antiAlias = false})));
  return root;
}

/** A DIAL SCREEN: a needle laid out rather than drawn — a bar whose
 *  offset is the reading, which is the layout vocabulary standing in for
 *  a gauge. */
compose::Element dial(float seconds, SkColor4f accent) {
  compose::Element root = compose::box()
                              .width(compose::pct(100))
                              .height(compose::pct(100))
                              .column()
                              .gap(10.0f)
                              .padding(16.0f)
                              .fill(compose::hex(0x14121f));
  root.child(
      compose::text(u8"DIAL", weave::textStyle({.size = 22.0f,
                                             .color = compose::hex(0xbfd4ef),
                                             .antiAlias = false})));
  const float reading = 0.5f + 0.5f * std::sin(seconds * 1.15f);
  compose::Element track = compose::box()
                               .width(compose::pct(100))
                               .height(26.0f)
                               .fill(compose::hex(0x242938));
  compose::Element needle =
      compose::box().width(10.0f).height(26.0f).fill(accent);
  needle.absolute().left(6.0f + reading * 150.0f).top(0.0f);
  track.child(std::move(needle));
  root.child(std::move(track));
  root.child(compose::box()
                 .width(compose::pct(100))
                 .height(12.0f + 46.0f * reading)
                 .fill(SkColor4f{accent.fR * 0.35f, accent.fG * 0.35f,
                                 accent.fB * 0.35f, 1.0f}));
  return root;
}

/** THE BAND under them: one wide strip of cells, so the curved panel has
 *  content whose repetition the curve is read by. */
compose::Element band(float seconds) {
  compose::Element root = compose::box()
                              .width(compose::pct(100))
                              .height(compose::pct(100))
                              .row()
                              .gap(6.0f)
                              .padding(12.0f)
                              .fill(compose::hex(0x0d121a));
  constexpr int kCells = 26;
  for (int i = 0; i < kCells; ++i) {
    const float phase = seconds * 1.4f - (float)i * 0.34f;
    const float lit =
        0.12f + 0.88f * std::pow(0.5f + 0.5f * std::sin(phase), 4.0f);
    root.child(
        compose::box()
            .width(compose::pct(100))
            .height(compose::pct(100))
            .fill(SkColor4f{0.30f * lit, 0.95f * lit, 0.70f * lit, 1.0f}));
  }
  return root;
}

/** THE CARD ON THE TAPE: type over a plate, with three marks swinging
 *  under it, so the repeat has something in it that moves and something
 *  that stands still. */
compose::Element tape(float seconds) {
  compose::Element root = compose::box()
                              .width(compose::pct(100))
                              .height(compose::pct(100))
                              .column()
                              .gap(6.0f)
                              .padding(14.0f)
                              .fill(compose::hex(0x1f2430));
  root.child(
      compose::text(u8"WOVEN", weave::textStyle({.size = 46.0f,
                                              .color = compose::hex(0xf2ebdc),
                                              .antiAlias = false})));
  root.child(compose::text(u8"a scene, sampled",
                           weave::textStyle({.size = 20.0f,
                                          .color = compose::hex(0x9eb8d9),
                                          .antiAlias = false})));
  compose::Element marks =
      compose::box().row().gap(10.0f).height(18.0f).absolute();
  marks.left(16.0f).bottom(14.0f);
  for (int i = 0; i < 3; ++i) {
    const float phase = seconds * 1.7f + (float)i * 0.7f;
    const float length = 46.0f + 34.0f * (0.5f + 0.5f * std::sin(phase));
    marks.child(
        compose::box().width(length).height(6.0f).fill(compose::hex(0xeb8c40)));
  }
  root.child(std::move(marks));
  return root;
}

/** THE TILING, written as the texture's own placement.
 *
 *  A swept band's u runs ACROSS it and its v along it, while a card
 *  reads along its own WIDTH — so the two axes are exchanged, and the
 *  card repeats @p repeats times down the length. Placement puts image
 *  pixels into the space they are sampled in; a sampler reads the other
 *  way, so what a face sees is this matrix undone. */
SkMatrix alongTheBand(SkISize card, float repeats) {
  const float width = (float)card.width();
  const float height = (float)card.height();
  return SkMatrix::MakeAll(0.0f, width / height, 0.0f,
                           height / (repeats * width), 0.0f, 0.0f, 0.0f, 0.0f,
                           1.0f);
}

/** The ribbon the tape rides: the world kit's wave, a closed loop
 *  standing high and low by turns, so the band reads as a curve in space
 *  rather than as a ring. */
gm::curve::Spline3 ribbon() {
  return world::kit::wave({.at = {0.0f, kRibbonDrop, 0.0f},
                           .radius = kRibbonRadius,
                           .inner = kRibbonRadius * 0.64f,
                           .high = 34.0f,
                           .low = -30.0f});
}

/** ONE SURFACE'S SCENE: the session's, asked for once and held. The
 *  tree it paints is handed in at the moment, so one type serves every
 *  surface in the room. */
struct Screen {
  std::shared_ptr<compose::TextureScene> scene;

  material::Texture at(float seconds, const compose::Element& content) {
    scene->render(content, (double)seconds);
    return scene->texture();
  }
};

/** A screen's surface: the texture in a base-colour slot on a body that
 *  says light does not reach it. */
material::Material screenOf(material::Texture texture) {
  material::Material surface =
      material::kit::unlit({.baseColor = {1, 1, 1, 1}});
  surface.child(material::kit::kBaseColorSlot, std::move(texture));
  return surface;
}

}  // namespace

namespace {

struct SceneSurfaces final : sketch::Set {
  std::array<Screen, 3> cards;
  Screen strip;
  Screen loop;
  /** The tape's rail, swept once: the ribbon never changes, so sweeping
   *  it per frame would be work the description already did. */
  gm::Mesh rail;

  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(960, 620);
    ctx.background({0.025f, 0.028f, 0.038f, 1.0f});
    ctx.captureAt(1.35);
    for (Screen& card : cards) card.scene = ctx.textureScene({320, 214});
    strip.scene = ctx.textureScene({1024, 128});
    loop.scene = ctx.textureScene({kTapeWidth, kTapeHeight});
    rail =
        gm::pop::sweep(ribbon(), sections::line(),
                         {.segments = 240,
                          .scale = 46.0f,
                          .normals = gm::pop::SweepOptions::Normals::Frame});
  }

  world::Frame describe(float seconds) override {
    const std::array<SkColor4f, 3> accents = {
        SkColor4f{0.30f, 0.82f, 1.00f, 1.0f},
        SkColor4f{1.00f, 0.62f, 0.24f, 1.0f},
        SkColor4f{0.70f, 0.52f, 1.00f, 1.0f}};
    const std::array<compose::Element, 3> content = {
        levels(seconds, accents[0]), trace(seconds, accents[1]),
        dial(seconds, accents[2])};

    // The console faces the turntable's FIRST STATION, which stands on
    // the +x axis: a set whose subject is a screen has to be built
    // toward where the camera starts, or the plate is a picture of its
    // edge.
    world::Element console = world::Element().key("console").rotateY(90.0f);
    for (int i = 0; i < 3; ++i) {
      // The arc: each card stands one radius out along its own bearing
      // and is turned to face the middle, which is two numbers rather
      // than a matrix.
      const float bearingDeg = ((float)i - 1.0f) * kArcSpreadDeg;
      const float bearing = bearingDeg * kTwoPi / 360.0f;
      console.child(
          world::Element()
              .key("card" + std::to_string(i))
              .at({kArcRadius * std::sin(bearing), 78.0f,
                   kArcRadius * std::cos(bearing) - kArcRadius})
              .mesh(gm::quad(kCardWidth, kCardHeight))
              .backface(world::Backface::Visible)
              .fill(screenOf(cards[(size_t)i].at(seconds, content[(size_t)i])))
              .tag("flat"));
    }

    // CURVED: the same kind of content on a panel that is not flat.
    console.child(world::Element()
                      .key("band")
                      .at({0.0f, -40.0f, 0.0f})
                      .mesh(gm::cylinderPanel(560.0f, 76.0f, kArcRadius, 72, 6))
                      .backface(world::Backface::Visible)
                      .fill(screenOf(strip.at(seconds, band(seconds))))
                      .tag("curved"));

    // …and one lit body in front of them, so the plate carries both
    // readings at once: a surface the emitters reach, and screens they
    // do not.
    console.child(
        world::Element()
            .key("shelf")
            .at({0.0f, -96.0f, 96.0f})
            .mesh(gm::superellipsoid({330.0f, 9.0f, 74.0f}, 6.0f, 48, 16))
            .fill(material::kit::surface(
                {.baseColor = {0.42f, 0.45f, 0.53f, 1.0f}, .roughness = 0.3f}))
            .tag("frame"));

    // TILED: the third sampling of the same kind of scene. The card
    // repeats along the ribbon and stands once across it, and the tape
    // is lit, so the texture is read here through a shading model
    // rather than emitted straight out of the slot.
    material::Texture printed = loop.at(seconds, tape(seconds));
    printed.tile(SkTileMode::kRepeat)
        .uv(alongTheBand({kTapeWidth, kTapeHeight}, kRepeats));
    material::Material printedTape =
        material::kit::surface({.baseColor = {1, 1, 1, 1}, .roughness = 0.4f});
    printedTape.child(material::kit::kBaseColorSlot, std::move(printed));

    world::Element room = world::Element().key("room");
    room.child(std::move(console));
    room.child(world::Element()
                   .key("ribbon")
                   .mesh(rail)
                   .backface(world::Backface::Visible)
                   .fill(std::move(printedTape))
                   .tag("tiled"));

    world::kit::Set set;
    set.rig.extent = 300.0f;
    set.rig.bearing = -22.0f;
    set.rig.elevation = 26.0f;
    set.rig.intensity = 0.85f;
    set.ground = 4.2f;
    set.drop = 0.92f;
    set.table.radius = 1020.0f;
    set.table.height = 345.0f;
    set.table.period = 40.0f;
    set.table.fovYDeg = 40.0f;
    return world::Frame(world::kit::litSet(std::move(room), set, seconds));
  }
};

}  // namespace

SIGIL_SKETCH(SceneSurfaces, "Set",
             "One kind of 2D scene on three surfaces \xe2\x80\x94 flat cards, "
             "a curved band and a tiled ribbon, each an ordinary texture in "
             "a base-colour slot")
