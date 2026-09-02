/** @file
 * panel_console — 2D content standing in a 3D room.
 *
 * Four screens on a console: three flat cards on an arc and one curved
 * band under them, each wearing a compose tree of its own that is
 * reconciled and painted every frame. What crosses from 2D to 3D is an
 * ordinary `material::Texture` in a surface's base-colour slot; there is
 * no panel element and no door of its own, which is why the curved band
 * takes the same content as the flat cards without either side knowing
 * about the other's shape.
 *
 * THE SCREENS ARE THEIR OWN LIGHT. A screen is `kit::unlit`, so what it
 * shows is exactly what its compose tree painted and the room's emitters
 * do not reach it — which is the honest reading of a display, and also
 * what makes the picture a statement about the 2D content rather than
 * about the shading over it. The console's frame beside them is lit, so
 * both readings stand in one plate.
 *
 * Each screen keeps a scene ACROSS frames and is remade when the clock
 * goes backwards, which is what starts a sweep: a plate is a function of
 * the declared moment and of the number of steps taken to reach it, so a
 * second sweep in one process must not begin where the first left off.
 */

#include <sigilcompose/core/Factories.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilcompose/typography/Type.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/fonts/FontContext.h>
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

using namespace sigil::world;

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr float kTwoPi = 6.283185307179586f;
/** The console's own geometry: how wide a card stands, how far out the
 *  arc bows, and how far round it the three of them are spread. */
constexpr float kCardWidth = 210.0f;
constexpr float kCardHeight = 140.0f;
constexpr float kArcRadius = 300.0f;
constexpr float kArcSpreadDeg = 44.0f;

weave::TextStyle type(float size, SkColor colour) {
  return compose::type({.size = size,
                        .color = SkColor4f::FromColor(colour),
                        .antiAlias = false});
}

/** A LEVEL SCREEN: a row of bars whose heights ride one wave, so the
 *  whole row moves together and no single bar has to be watched. */
compose::Element levels(float seconds, SkColor4f accent) {
  compose::Element root = compose::box()
                              .width(compose::pct(100))
                              .height(compose::pct(100))
                              .column()
                              .gap(8.0f)
                              .padding(16.0f)
                              .fill(SkColor4f{0.07f, 0.09f, 0.13f, 1.0f});
  root.child(compose::text(u8"LEVELS", type(22.0f, 0xFFBFD4EF)));
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
                              .fill(SkColor4f{0.06f, 0.08f, 0.11f, 1.0f});
  root.child(compose::text(u8"TRACE", type(22.0f, 0xFFBFD4EF)));
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
  root.child(
      compose::text(u8"one wave, fourteen cells", type(19.0f, 0xFF7E93B4)));
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
                              .fill(SkColor4f{0.08f, 0.07f, 0.12f, 1.0f});
  root.child(compose::text(u8"DIAL", type(22.0f, 0xFFBFD4EF)));
  const float reading = 0.5f + 0.5f * std::sin(seconds * 1.15f);
  compose::Element track = compose::box()
                               .width(compose::pct(100))
                               .height(26.0f)
                               .fill(SkColor4f{0.14f, 0.16f, 0.22f, 1.0f});
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
                              .fill(SkColor4f{0.05f, 0.07f, 0.10f, 1.0f});
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

/** ONE SCREEN'S SCENE, kept across the frames of a sweep and remade when
 *  the clock goes backwards. The tree it paints is handed in, so one
 *  type serves every screen on the console. */
struct Screen {
  SkISize size{256, 160};
  weave::FontContext* fonts = nullptr;
  std::shared_ptr<compose::TextureScene> scene;
  float lastSeconds = -1.0f;

  material::Texture at(float seconds, const compose::Element& content) {
    if (!scene || seconds <= lastSeconds)
      scene = compose::TextureScene::make(size, *fonts);
    lastSeconds = seconds;
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

struct PanelConsole final : sketch::Set {
  std::array<Screen, 3> cards;
  Screen strip;

  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(900, 580);
    ctx.background({0.025f, 0.028f, 0.038f, 1.0f});
    ctx.captureAt(1.35);
    for (Screen& card : cards) {
      card.fonts = &ctx.fonts;
      card.size = {320, 214};
    }
    strip.fonts = &ctx.fonts;
    strip.size = {1024, 128};
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
    Element console = Element().key("console").rotateY(90.0f);
    for (int i = 0; i < 3; ++i) {
      // The arc: each card stands one radius out along its own bearing
      // and is turned to face the middle, which is two numbers rather
      // than a matrix.
      const float bearingDeg = ((float)i - 1.0f) * kArcSpreadDeg;
      const float bearing = bearingDeg * kTwoPi / 360.0f;
      console.child(
          Element()
              .key("card" + std::to_string(i))
              .at({kArcRadius * std::sin(bearing), 78.0f,
                   kArcRadius * std::cos(bearing) - kArcRadius})
              .rotateY(bearingDeg)
              .mesh(gm::quad(kCardWidth, kCardHeight))
              .fill(screenOf(cards[(size_t)i].at(seconds, content[(size_t)i])))
              .tag("screen"));
    }

    // The band: the same kind of content on a panel that is not flat.
    console.child(Element()
                      .key("band")
                      .at({0.0f, -40.0f, 0.0f})
                      .mesh(gm::cylinderPanel(560.0f, 76.0f, kArcRadius, 72, 6))
                      .fill(screenOf(strip.at(seconds, band(seconds))))
                      .tag("screen"));

    // …and one lit body in front of them, so the plate carries both
    // readings at once: a surface the emitters reach, and screens they
    // do not.
    console.child(
        Element()
            .key("shelf")
            .at({0.0f, -96.0f, 96.0f})
            .mesh(gm::superellipsoid({330.0f, 9.0f, 74.0f}, 6.0f, 48, 16))
            .fill(material::kit::surface(
                {.baseColor = {0.42f, 0.45f, 0.53f, 1.0f}, .roughness = 0.3f}))
            .tag("frame"));

    kit::Set set;
    set.rig.extent = 260.0f;
    set.rig.bearing = -22.0f;
    set.rig.elevation = 26.0f;
    set.rig.intensity = 0.85f;
    set.ground = 3.2f;
    set.drop = 0.62f;
    set.table.radius = 720.0f;
    set.table.height = 165.0f;
    set.table.period = 40.0f;
    set.table.fovYDeg = 40.0f;
    return Frame(kit::litSet(std::move(console), set, seconds));
  }
};

}  // namespace

SIGIL_SKETCH(PanelConsole, "Set",
             "Four live 2D scenes as screens in a 3D room \xe2\x80\x94 three "
             "flat cards on an arc and one curved band, each an ordinary "
             "texture in a surface's base-colour slot")
