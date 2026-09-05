/** @file
 * zellige — girih tiling by construction: a Hankin polygons-in-contact
 * derivation with its one dial, the contact angle, swept across the wall.
 */

// A wall of zellige panels, generated rather than drawn.
//
// kit::girih8 runs Hankin's polygons-in-contact construction on the real
// 4.8.8 tiling: two rays leave every octagon edge's midpoint at the
// CONTACT ANGLE θ to that edge, and where neighbouring rays meet is a
// vertex of the star. θ is the whole parameter of the construction, and
// the three panels here are one θ each — 30°, the classic 45° where the
// rays through an octagon are collinear and the star is the {8/2}
// khatam, and 60°, where the straps run past first contact and interlace
// through the crossing. Everything else about the three (edge, palette)
// is held so that what the eye compares is the angle.
//
// The wall re-tiles itself every few seconds: new edges and swapped
// palettes, each panel keeping its own θ. Each swap is exactly one
// changed recipe and one new bake, which is the point of the scene.
//
// Two constraints if you edit it:
//  - The Pattern objects must stay SCENE MEMBERS. A pattern bakes once per
//    recipe and is identified by that recipe; minting a fresh Pattern inside
//    describe() would re-bake on every render. update() re-rolls them on a
//    timer and only then calls render().
//  - The carved depth is a layer-style stack (inner shadow plus inner glow),
//    not a shader.

#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/kit/Patterns.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

#include <cstdio>
#include <string>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace mkit = sigil::material::kit;
namespace mpattern = sigil::material::pattern;
namespace mskia = sigil::material::skia;
using sigil::material::skia::Paint;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

namespace zellige_wall {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;
constexpr SkColor4f kPlaster{0.885f, 0.850f, 0.775f, 1};

// THE WALL IS WHITE. Every zellige field on a Fes or Meknes wall is
// white-dominant with the cobalt, turquoise, ochre, brick-red, green and
// black inside it — the white is the GROUND the colours are cut into, and
// the kit's two default palettes put the colour on the ground and the
// bone on the strap, which is the reading turned inside out. These three
// are the wall's own: one white ground, one hue in the star and one in
// the strap, black in the joint.
inline mkit::GirihPalette fesCobalt() {
  return {{0.949f, 0.937f, 0.906f, 1},   // the white ground
          {0.118f, 0.310f, 0.627f, 1},   // cobalt star
          {0.118f, 0.557f, 0.525f, 1},   // turquoise strap
          {0.102f, 0.090f, 0.078f, 1}};  // the black joint
}
inline mkit::GirihPalette fesTurquoise() {
  return {{0.949f, 0.937f, 0.906f, 1},
          {0.118f, 0.557f, 0.525f, 1},
          {0.647f, 0.251f, 0.169f, 1},  // brick red
          {0.102f, 0.090f, 0.078f, 1}};
}
inline mkit::GirihPalette fesOchre() {
  return {{0.949f, 0.937f, 0.906f, 1},
          {0.788f, 0.541f, 0.180f, 1},  // ochre
          {0.180f, 0.431f, 0.290f, 1},  // green
          {0.102f, 0.090f, 0.078f, 1}};
}
constexpr SkColor4f kInk{0.180f, 0.129f, 0.106f, 1};
constexpr SkColor4f kSub{0.42f, 0.36f, 0.30f, 1};
constexpr double kSwapPeriod = 3.0;  // seconds between re-tilings

/** THE SWEPT DIAL: one contact angle per panel, left to right. Below
 *  half the turn between an octagon's edges the straps stop at first
 *  contact; at 45° they run straight through; above it they cross, and
 *  the panel reads as an interlace. */
constexpr float kContact[3] = {30.0f, 45.0f, 60.0f};

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0) {
  return weave::textStyle(
      {.size = size, .color = color, .track = tracking, .color8 = true});
}

/** One panel's tile: the kit's generator at this panel's contact angle,
 *  with the strap left at the generator's own width. */
inline Pattern girih(float edge, const mkit::GirihPalette& palette,
                     float contactDeg) {
  return mkit::girih8(edge, palette, 0.0f, contactDeg);
}

inline std::string caption(const char* palette, float edge, float contactDeg,
                           bool rotated) {
  const std::string buf = kit::formatted(
      rotated ? "\xce\xb8 = %.0f\xc2\xb0 \xc2\xb7 %s \xc2\xb7 a=%.0f "
                "\xc2\xb7 rotated"
              : "\xce\xb8 = %.0f\xc2\xb0 \xc2\xb7 %s \xc2\xb7 a=%.0f",
      (double)contactDeg, palette, (double)edge);
  return buf;
}

}  // namespace zellige_wall

struct Zellige final : sketch::Sketch {
  // Three panels from one generator at different parameters. Held as members
  // rather than built in describe(): a Pattern bakes once per recipe, so a
  // fresh one each render would re-bake every frame.
  Pattern left = zellige_wall::girih(12, zellige_wall::fesCobalt(),
                                     zellige_wall::kContact[0]);
  Pattern middle = zellige_wall::girih(34, zellige_wall::fesTurquoise(),
                                       zellige_wall::kContact[1]);
  Pattern right = zellige_wall::girih(22, zellige_wall::fesOchre(),
                                      zellige_wall::kContact[2]);
  Pattern grain = mpattern::speckle(96, 60, 0.4f, 1.1f,
                                    {{0.35f, 0.30f, 0.24f, 0.25f}});
  std::string captions[3];
  double nextSwap = 0.0;
  int phase = 0;

  // The wall re-rolls all three recipes every kSwapPeriod seconds, so the
  // still has to name its moment or it can land a frame either side of a
  // re-roll. This is the midpoint of the first hold, which shows the authored
  // setup recipes — the ones the captions describe.

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 1.5,
                             .background = SkColor4f{0, 0, 0, 1}});
    Composer& composer = ctx.composer;
    namespace zw = zellige_wall;
    nextSwap = zw::kSwapPeriod;
    phase = 0;
    // THREE SCALES, side by side, as a wall carries them: a fine band, a
    // coarse field, and a middle course between them.
    left = zw::girih(12, zw::fesCobalt(), zw::kContact[0]);
    middle = zw::girih(34, zw::fesTurquoise(), zw::kContact[1]);
    right = zw::girih(22, zw::fesOchre(), zw::kContact[2]);
    captions[0] = zw::caption("cobalt", 12, zw::kContact[0], false);
    captions[1] = zw::caption("turquoise", 34, zw::kContact[1], false);
    captions[2] = zw::caption("ochre", 22, zw::kContact[2], false);
    composer.render(describe());
  }

  Element panel(const Pattern& p, const std::string& label) {
    namespace zw = zellige_wall;
    return box()
        .column()
        .grow(1)
        .gap(8)
        .child(box()
                   .grow(1)
                   .corners({3})
                   .fill(p.material())
                   // GLAZED, not carved. An inner shadow with an inner glow
                   // is a bevel cut into plaster; a glazed tile is a hard
                   // gloss with a sheen running off the light and a thin
                   // wet line where the glaze pools at the joint.
                   .foreground(styles::innerGlow({1, 1, 1, 0.26f}, 2))
                   .stroke(sigil::compose::stroke(2.5f, Fill::color(zw::kInk)))
                   .child(box().inset(0).fill(
                       Paint::linear({0, 0}, {180, 260},
                                        {{0.00f, {1, 1, 1, 0.20f}},
                                         {0.42f, {1, 1, 1, 0.05f}},
                                         {0.58f, {0, 0, 0, 0.03f}},
                                         {1.00f, {0, 0, 0, 0.10f}}}))))
        .child(text(toU8(label), zw::type(13, zw::kInk, 1.2f)));
  }

  Element describe() {
    namespace zw = zellige_wall;
    return stack()
        .fill(Fill::color(zw::kPlaster))
        // Speckled plaster grain over the ground — its own full-bleed
        // layer (the root fill and the pattern can't share one slot).
        .child(box().inset(0, 0, 0, 0).fill(grain.material()))
        .child(
            box()
                .column()
                .inset(50, 44, 50, 44)
                .gap(14)
                .child(
                    box()
                        .row()
                        .alignItems(Align::Baseline)
                        .gap(14)
                        .child(text(toU8("ZELLIJE"), zw::type(34, zw::kInk, 3)))
                        .child(text(toU8("Hankin PIC \xc2\xb7 4.8.8 \xc2\xb7 "
                                         "\xce\xb8 swept 30\xe2\x80\x93" "60\xc2\xb0"),
                                    zw::type(14, zw::kSub, 1))))
                .child(box()
                           .row()
                           .grow(1)
                           .gap(22)
                           .child(panel(left, captions[0]))
                           .child(panel(middle, captions[1]))
                           .child(panel(right, captions[2]))));
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    Composer& composer = ctx.composer;
    namespace zw = zellige_wall;
    if (elapsed < nextSwap) return;
    nextSwap = elapsed + zw::kSwapPeriod;
    ++phase;
    // Runtime regeneration: new parameters → new recipes → each panel
    // re-bakes exactly once and the reconciler sees one changed fill.
    const float edges[3] = {11.0f + 3 * (float)(phase % 3),
                            30.0f + 6 * (float)((phase + 1) % 2),
                            20.0f + 3 * (float)(phase % 4)};
    const bool swapPalettes = (phase % 2) != 0;
    // Each panel keeps its own contact angle across every re-roll: the
    // sweep is what the wall is FOR, and only the edge and the palette
    // are re-rolled under it.
    left = zw::girih(edges[0],
                     swapPalettes ? zw::fesTurquoise() : zw::fesCobalt(),
                     zw::kContact[0]);
    middle = zw::girih(edges[1],
                       swapPalettes ? zw::fesCobalt() : zw::fesTurquoise(),
                       zw::kContact[1]);
    right = zw::girih(edges[2], zw::fesOchre(), zw::kContact[2]);
    const bool rotated = (phase % 8) != 0;
    right.rotate((float)(phase % 8) * 22.5f);
    captions[0] = zw::caption(swapPalettes ? "turquoise" : "cobalt", edges[0],
                              zw::kContact[0], false);
    captions[1] = zw::caption(swapPalettes ? "cobalt" : "turquoise", edges[1],
                              zw::kContact[1], false);
    captions[2] = zw::caption("ochre", edges[2], zw::kContact[2], rotated);
    composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH_AS(Zellige, "zellige", "Catalog \xc2\xb7 Tiling",
                "girih Hankin PIC \xe2\x80\x94 the contact angle swept across "
                "three regenerating panels")
