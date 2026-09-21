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

// TAGS: Patterns/Tiling

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

#include <array>
#include <string>

namespace sketch = sigil::sketch;
namespace mkit = sigil::material::kit;
namespace mpattern = sigil::material::pattern;
namespace mskia = sigil::material::skia;
using sigil::material::skia::Paint;

using namespace sigil::compose;
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

/** One panel's tile: the kit's generator at this panel's contact angle,
 *  with the strap left at the generator's own width. */
inline Pattern girih(float edge, const mkit::GirihPalette& palette,
                     float contactDeg) {
  return mkit::girih8(edge, palette, 0.0f, contactDeg);
}

inline std::string caption(const char* palette, float edge, float contactDeg,
                           bool rotated) {
  return kit::formatted(
      rotated ? "θ = %.0f° · %s · a=%.0f · rotated" : "θ = %.0f° · %s · a=%.0f",
      (double)contactDeg, palette, (double)edge);
}

/** ONE PANEL OF THE WALL: the tile a re-tiling handed it, and the line
 *  under it saying what that recipe was. */
struct Panel {
  sigil::compose::Pattern tile;
  std::string caption;
};

}  // namespace zellige_wall

struct Zellige {
  // Three panels from one generator at different parameters. Held as members
  // rather than built in describe(): a Pattern bakes once per recipe, so a
  // fresh one each render would re-bake every frame.
  std::array<zellige_wall::Panel, 3> panels;
  Pattern grain =
      mpattern::speckle(96, 60, 0.4f, 1.1f, {{0.35f, 0.30f, 0.24f, 0.25f}});
  double nextSwap = 0.0;
  int phase = 0;

  // The wall re-rolls all three recipes every kSwapPeriod seconds, so the
  // still has to name its moment or it can land a frame either side of a
  // re-roll. This is the midpoint of the first hold.

  /** THE WALL, RE-TILED: new edges and swapped palettes, each panel
   *  keeping its OWN contact angle — the sweep is what the wall is for,
   *  and only the edge and the palette are re-rolled under it. Three
   *  scales side by side as a wall carries them: a fine band, a coarse
   *  field, and a middle course between them. */
  void retile() {
    namespace zw = zellige_wall;
    const float edges[3] = {11.0f + 3 * (float)(phase % 3),
                            30.0f + 6 * (float)((phase + 1) % 2),
                            20.0f + 3 * (float)(phase % 4)};
    const bool swapped = (phase % 2) != 0;
    const char* names[3] = {swapped ? "turquoise" : "cobalt",
                            swapped ? "cobalt" : "turquoise", "ochre"};
    const mkit::GirihPalette palettes[3] = {
        swapped ? zw::fesTurquoise() : zw::fesCobalt(),
        swapped ? zw::fesCobalt() : zw::fesTurquoise(), zw::fesOchre()};
    const bool rotated = (phase % 8) != 0;
    for (size_t i = 0; i < panels.size(); ++i)
      panels[i] = {
          zw::girih(edges[i], palettes[i], zw::kContact[i]),
          zw::caption(names[i], edges[i], zw::kContact[i], i == 2 && rotated)};
    panels[2].tile.rotate((float)(phase % 8) * 22.5f);
  }

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 1.5,
                             .background = SkColor4f{0, 0, 0, 1}});
    nextSwap = zellige_wall::kSwapPeriod;
    phase = 0;
    retile();
    ctx.composer.render(describe());
  }

  Element panel(const zellige_wall::Panel& one) {
    namespace zw = zellige_wall;
    return box().column().flexGrow(1).gap(8).children(
        {box()
             .flexGrow(1)
             .borderRadius({3})
             .fill(one.tile.material())
             // GLAZED, not carved. An inner shadow with an inner glow
             // is a bevel cut into plaster; a glazed tile is a hard
             // gloss with a sheen running off the light and a thin
             // wet line where the glaze pools at the joint.
             .foreground(styles::innerGlow({1, 1, 1, 0.26f}, 2))
             .stroke(sigil::compose::stroke(2.5f, Fill::color(zw::kInk)))
             .children({box().inset(0).fill(
                 Paint::linear({0, 0}, {180, 260},
                               {{0.00f, {1, 1, 1, 0.20f}},
                                {0.42f, {1, 1, 1, 0.05f}},
                                {0.58f, {0, 0, 0, 0.03f}},
                                {1.00f, {0, 0, 0, 0.10f}}}))}),
         text(one.caption).font({.size = 13, .track = 1.2f})});
  }

  Element describe() {
    namespace zw = zellige_wall;
    return stack()
        .fill(Fill::color(zw::kPlaster))
        // The wall's lettering, stated once: one ink, sent to the paint
        // through the 8-bit ladder the palette was read in.
        .font({.color8 = true})
        .ink(zw::kInk)
        // Speckled plaster grain over the ground — its own full-bleed
        // layer (the root fill and the pattern can't share one slot).
        .children(
            {box().inset(0).fill(grain.material()),
             box()
                 .column()
                 .inset({.top = 44, .right = 50, .bottom = 44, .left = 50})
                 .gap(14)
                 .children({box()
                                .row()
                                .alignItems(Align::Baseline)
                                .gap(14)
                                .children({text("ZELLIJE").font(
                                               {.size = 34, .track = 3}),
                                           text("Hankin PIC · 4.8.8 · "
                                                "θ swept 30–60°")
                                               .font({.size = 14,
                                                      .color = zw::kSub,
                                                      .track = 1})}),
                            box().row().flexGrow(1).gap(22).children({each(
                                panels, [this](const zellige_wall::Panel& one) {
                                  return panel(one);
                                })})})});
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    if (elapsed < nextSwap) return;
    nextSwap = elapsed + zellige_wall::kSwapPeriod;
    ++phase;
    // Runtime regeneration: new parameters → new recipes → each panel
    // re-bakes exactly once and the reconciler sees one changed fill.
    retile();
    ctx.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH_AS(Zellige, "zellige", "Catalog · Tiling",
                "girih Hankin PIC — the contact angle swept across "
                "three regenerating panels")
