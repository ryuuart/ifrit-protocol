/** @file
 * woven_card — a live 2D scene riding a 3D ribbon.
 *
 * A compose tree — a title, a caption and a few brush marks — is
 * rendered into a texture by a composer of its own, and that texture is
 * what a swept band is made of. The band is `curve::sweep` over a
 * two-point profile, so its u runs across the ribbon and its v along it;
 * the card repeats along v, which is what makes the ribbon read as a
 * length of printed tape rather than as one stretched picture.
 *
 * THERE IS NO PANEL HERE and no door of its own: what crosses from 2D to
 * 3D is an ordinary `material::Texture` in a surface's base-colour slot,
 * so the tiling, the placement and the repeat are the dials every
 * texture has. The scene is reconciled once per frame and paints only
 * when something moved, which is why the marks that swing cost a paint
 * and the type that stands still does not.
 *
 * The 2D scene here paints into a RASTER surface, which is what a plate
 * on either tier can be taken from. The same scene given a device paints
 * straight into a texture on it, and a renderer standing on that device
 * binds those pixels where they are.
 */

#include <sigilcompose/core/Factories.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilworld/kit/Kit.h>

#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "Studies.h"

namespace sigil::world::testing {

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr int kCardWidth = 512;
constexpr int kCardHeight = 160;
/** How many times the card repeats along the ribbon. */
constexpr float kRepeats = 5.0f;

/** THE CARD LAID ALONG THE RIBBON.
 *
 *  A swept band's u runs ACROSS it and its v along it, while a card
 *  reads along its own WIDTH — so the two axes are exchanged here, and
 *  the card repeats @p repeats times down the length. It is written as
 *  the texture's own placement, which puts image pixels into the space
 *  they are sampled in; a sampler reads the other way, so what a face
 *  sees is this matrix undone. */
SkMatrix alongTheBand(SkISize card, float repeats) {
  const float width = (float)card.width();
  const float height = (float)card.height();
  return SkMatrix::MakeAll(0.0f, width / height, 0.0f,
                           height / (repeats * width), 0.0f, 0.0f, 0.0f, 0.0f,
                           1.0f);
}

/** One font context for the process: shaping caches warm once, and a
 *  study measures the picture rather than the font manager. */
sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

/** One type style: a size and a colour, which is all the card asks of
 *  the shaping vocabulary. */
weave::TextStyle type(float size, SkColor colour) {
  weave::TextStyle style;
  style.shaping.fontSize = size;
  style.paint.foreground.setColor(colour);
  return style;
}

/** THE CARD: type over a plate, with three marks swinging under it at a
 *  rate the scene's own clock sets. */
compose::Element cardAt(float seconds) {
  compose::Element root = compose::box()
                              .width(compose::pct(100))
                              .height(compose::pct(100))
                              .column()
                              .gap(6.0f)
                              .fill(SkColor4f{0.12f, 0.14f, 0.19f, 1.0f})
                              .padding(14.0f);

  root.child(compose::text(u8"WOVEN", type(46.0f, 0xFFF2EBDC)));
  root.child(compose::text(u8"a scene, sampled", type(20.0f, 0xFF9EB8D9)));

  // The marks: three bars whose lengths swing, so the scene has
  // something in it that actually moves and the version has something
  // to count.
  compose::Element marks =
      compose::box().row().gap(10.0f).height(18.0f).absolute();
  marks.left(16.0f).bottom(14.0f);
  for (int i = 0; i < 3; ++i) {
    const float phase = seconds * 1.7f + (float)i * 0.7f;
    const float length = 46.0f + 34.0f * (0.5f + 0.5f * std::sin(phase));
    marks.child(compose::box().width(length).height(6.0f).fill(
        SkColor4f{0.92f, 0.55f, 0.25f, 1.0f}));
  }
  root.child(std::move(marks));
  return root;
}

/** The ribbon the card rides: a closed loop that rises and falls, so
 *  the band reads as a curve in space. */
Spline3 ribbon() {
  Spline3 spline;
  constexpr float kTwoPi = 6.283185307179586f;
  for (int i = 0; i < 6; ++i) {
    const float angle = (float)i * kTwoPi / 6.0f;
    const float radius = (i % 2 == 0) ? 220.0f : 140.0f;
    const float height = (i % 2 == 0) ? 60.0f : -50.0f;
    spline.points.emplace_back(radius * std::cos(angle), height,
                               radius * std::sin(angle));
  }
  spline.closed = true;
  return spline;
}

/** THE SCENE BEHIND THE TEXTURE, kept across the frames of one sweep.
 *
 *  It is remade whenever the clock goes backwards, which is what starts
 *  a sweep: a plate is a function of the declared moment and of the
 *  number of steps taken to reach it, so a second sweep in one process
 *  must not begin where the first one left off. */
struct Card {
  std::shared_ptr<compose::TextureScene> scene;
  float lastSeconds = -1.0f;

  material::Texture at(float seconds) {
    if (!scene || seconds <= lastSeconds)
      scene = compose::TextureScene::make({kCardWidth, kCardHeight}, fonts());
    lastSeconds = seconds;
    scene->render(cardAt(seconds), (double)seconds);
    material::Texture value = scene->texture();
    // The card repeats along the ribbon and stands once across it.
    value.tile(SkTileMode::kRepeat)
        .uv(alongTheBand({kCardWidth, kCardHeight}, kRepeats));
    return value;
  }
};

}  // namespace

Study wovenCard() {
  Study study;
  study.name = "woven_card";
  study.canvas = {820, 560};
  study.captureSeconds = 1.25f;
  study.background = {0.03f, 0.034f, 0.045f, 1.0f};

  const std::shared_ptr<Card> card = std::make_shared<Card>();
  study.describe = [card](float seconds) {
    const gm::Mesh band =
        gm::curve::sweep(ribbon(), gm::curve::profile::line(),
                         {.segments = 240,
                          .scale = 54.0f,
                          .normals = gm::curve::SweepOptions::Normals::Frame});

    material::Material tape =
        material::kit::surface({.baseColor = {1, 1, 1, 1}, .roughness = 0.4f});
    tape.child(material::kit::kBaseColorSlot, card->at(seconds));

    kit::Set set;
    set.rig.extent = 150.0f;
    set.rig.bearing = -40.0f;
    set.rig.elevation = 30.0f;
    set.ground = 5.0f;
    set.drop = 0.9f;
    set.table.radius = 520.0f;
    set.table.height = 190.0f;
    set.table.period = 12.0f;
    set.table.fovYDeg = 44.0f;

    return Frame(kit::litSet(
        Element().key("ribbon").mesh(band).fill(std::move(tape)).tag("tape"),
        set, seconds));
  };
  return study;
}

}  // namespace sigil::world::testing
