/** @file
 * zellige — girih tiling by construction: a Hankin polygons-in-contact
 * derivation that regenerates rather than a traced pattern.
 */

// A wall of zellige panels, generated rather than drawn.
//
// patterns::girih8 runs Hankin's polygons-in-contact construction on the
// real 4.8.8 tiling: at a 45° contact angle the interlace through every
// octagon's edge midpoints is the {8/2} khatam star, which is how these
// patterns are actually built. One generator, parameterized, applied to
// three panels at different scales and rotations and coloured from
// historical zellige palettes.
//
// The wall re-tiles itself every few seconds. Each swap is exactly one
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
#include <sigilcompose/core/Patterns.h>
#include <sigilcompose/typography/Type.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cstdio>
#include <string>

namespace sketch = sigil::sketch;

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
constexpr SkColor4f kInk{0.180f, 0.129f, 0.106f, 1};
constexpr SkColor4f kSub{0.42f, 0.36f, 0.30f, 1};
constexpr double kSwapPeriod = 3.0;  // seconds between re-tilings

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0) {
  return sigil::compose::type(
      {.size = size, .color = color, .track = tracking, .color8 = true});
}

inline std::string caption(const char* palette, float edge, bool rotated) {
  char buf[64];
  std::snprintf(
      buf, sizeof(buf),
      rotated ? "%s \xc2\xb7 a=%.0f \xc2\xb7 rotated" : "%s \xc2\xb7 a=%.0f",
      palette, (double)edge);
  return buf;
}

}  // namespace zellige_wall

struct Zellige final : sketch::Sketch {
  // Three panels from one generator at different parameters. Held as members
  // rather than built in describe(): a Pattern bakes once per recipe, so a
  // fresh one each render would re-bake every frame.
  Pattern left = patterns::girih8(16, patterns::fezPalette());
  Pattern middle = patterns::girih8(26, patterns::nasridPalette());
  Pattern right = patterns::girih8(20, patterns::fezPalette());
  Pattern grain =
      patterns::speckle(96, 60, 0.4f, 1.1f, {{0.35f, 0.30f, 0.24f, 0.25f}});
  std::string captions[3];
  double nextSwap = 0.0;
  int phase = 0;

  // The wall re-rolls all three recipes every kSwapPeriod seconds, so the
  // still has to name its moment or it can land a frame either side of a
  // re-roll. This is the midpoint of the first hold, which shows the authored
  // setup recipes — the ones the captions describe.

  void setup(sketch::SketchContext& ctx) override {
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(1.5);
    Composer& composer = ctx.composer;
    namespace zw = zellige_wall;
    nextSwap = zw::kSwapPeriod;
    phase = 0;
    left = patterns::girih8(16, patterns::fezPalette());
    middle = patterns::girih8(26, patterns::nasridPalette());
    right = patterns::girih8(20, patterns::fezPalette());
    captions[0] = zw::caption("fez", 16, false);
    captions[1] = zw::caption("nasrid", 26, false);
    captions[2] = zw::caption("fez", 20, false);
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
                   // Carved-plaster depth: the Photoshop route, no shader.
                   .foreground(styles::InnerShadow{{0, 0, 0, 0.38f}, {0, 4}, 9})
                   .foreground(styles::innerGlow({1, 1, 1, 0.18f}, 5))
                   .stroke(sigil::compose::stroke(2.5f, Fill::color(zw::kInk))))
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
                                         "\xce\xb8 = 45\xc2\xb0"),
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
    const float edges[3] = {14.0f + 4 * (float)(phase % 3),
                            22.0f + 6 * (float)((phase + 1) % 2),
                            18.0f + 3 * (float)(phase % 4)};
    const bool swapPalettes = (phase % 2) != 0;
    left = patterns::girih8(edges[0], swapPalettes ? patterns::nasridPalette()
                                                   : patterns::fezPalette());
    middle =
        patterns::girih8(edges[1], swapPalettes ? patterns::fezPalette()
                                                : patterns::nasridPalette());
    right = patterns::girih8(edges[2], patterns::fezPalette());
    const bool rotated = (phase % 8) != 0;
    right.rotate((float)(phase % 8) * 22.5f);
    captions[0] = zw::caption(swapPalettes ? "nasrid" : "fez", edges[0], false);
    captions[1] = zw::caption(swapPalettes ? "fez" : "nasrid", edges[1], false);
    captions[2] = zw::caption("fez", edges[2], rotated);
    composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH_AS(Zellige, "zellige", "Catalog \xc2\xb7 Tiling",
                "girih Hankin PIC \xe2\x80\x94 regenerating")
