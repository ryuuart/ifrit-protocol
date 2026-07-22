#pragma once
// The zellige wall (REFERENCES.md §4 — girih Hankin PIC), ported from
// sketch/sketches/zellige_study.cpp: patterns::girih8 runs Hankin's
// polygons-in-contact rule on the real 4.8.8 tiling (contact angle θ=45°
// → the {8/2} khatam through every octagon's edge midpoints), colored
// with zellige palettes. ONE generator, parameterized, applied to three
// panels at different scales and rotations, REGENERATED at runtime — the
// wall re-tiles itself every few seconds; each swap is exactly one
// changed recipe, one new bake.
//
// Porting notes (per the ScenesKinetic.h exemplar):
//  - ctx.canvas(960, 640) → the registry's 900x640 kSceneSize (insets
//    tightened), ctx.background(kPlaster) → the root stack's fill
//  - Pattern objects stay SCENE MEMBERS (the header's identity contract:
//    patterns bake once per recipe — re-describing a fresh Pattern each
//    frame would re-bake per render); update() re-rolls them on a timer
//    and gates composer.render() on that discrete swap
//  - carved depth is the Photoshop route (inner shadow + inner glow),
//    not a shader

#include "GalleryCore.h"

#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Patterns.h>

#include <cstdio>
#include <string>

namespace compose_gallery {

namespace zellige_wall {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;
constexpr SkColor4f kPlaster{0.885f, 0.850f, 0.775f, 1};
constexpr SkColor4f kInk{0.180f, 0.129f, 0.106f, 1};
constexpr SkColor4f kSub{0.42f, 0.36f, 0.30f, 1};
constexpr double kSwapPeriod = 3.0; // seconds between re-tilings

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

inline std::string caption(const char *palette, float edge, bool rotated) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), rotated ? "%s \xc2\xb7 a=%.0f \xc2\xb7 rotated"
                                          : "%s \xc2\xb7 a=%.0f",
                palette, (double)edge);
  return buf;
}

} // namespace zellige_wall

struct ZelligeScene final : Scene {
  // Three panels of one generator at different parameters — held as
  // members (the Pattern identity contract), re-rolled in update().
  Pattern left = patterns::girih8(16, patterns::fezPalette());
  Pattern middle = patterns::girih8(26, patterns::nasridPalette());
  Pattern right = patterns::girih8(20, patterns::fezPalette());
  Pattern grain = patterns::speckle(96, 60, 0.4f, 1.1f,
                                    {{0.35f, 0.30f, 0.24f, 0.25f}});
  std::string captions[3];
  double nextSwap = 0.0;
  int phase = 0;

  const char *name() const override { return "zellige"; }

  void setup(Composer &composer, sigil::motion::Ticker &) override {
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

  Element panel(const Pattern &p, const std::string &label) {
    namespace zw = zellige_wall;
    return box().column().grow(1).gap(8)
        .child(box().grow(1).corners({3})
                   .fill(p.material())
                   // Carved-plaster depth: the Photoshop route, no shader.
                   .foreground(styles::InnerShadow{{0, 0, 0, 0.38f},
                                                   {0, 4},
                                                   9})
                   .foreground(styles::innerGlow({1, 1, 1, 0.18f}, 5))
                   .stroke(sigil::compose::util::stroke(
                       2.5f, Fill::color(zw::kInk))))
        .child(text(toU8(label), zw::type(13, zw::kInk, 1.2f)));
  }

  Element describe() {
    namespace zw = zellige_wall;
    return stack()
        .fill(Fill::color(zw::kPlaster))
        // Speckled plaster grain over the ground — its own full-bleed
        // layer (the root fill and the pattern can't share one slot).
        .child(box().absolute().inset(0, 0, 0, 0).fill(grain.material()))
        .child(
            box().column().absolute().inset(50, 44, 50, 44).gap(14)
                .child(box().row().alignItems(Align::Baseline).gap(14)
                           .child(text(toU8("ZELLIJE"),
                                       zw::type(34, zw::kInk, 3)))
                           .child(text(
                               toU8("Hankin PIC \xc2\xb7 4.8.8 \xc2\xb7 "
                                    "\xce\xb8 = 45\xc2\xb0"),
                               zw::type(14, zw::kSub, 1))))
                .child(box().row().grow(1).gap(22)
                           .child(panel(left, captions[0]))
                           .child(panel(middle, captions[1]))
                           .child(panel(right, captions[2]))));
  }

  void update(double elapsed, Composer &composer) override {
    namespace zw = zellige_wall;
    if (elapsed < nextSwap)
      return;
    nextSwap = elapsed + zw::kSwapPeriod;
    ++phase;
    // Runtime regeneration: new parameters → new recipes → each panel
    // re-bakes exactly once and the reconciler sees one changed fill.
    const float edges[3] = {14.0f + 4 * (float)(phase % 3),
                            22.0f + 6 * (float)((phase + 1) % 2),
                            18.0f + 3 * (float)(phase % 4)};
    const bool swapPalettes = (phase % 2) != 0;
    left = patterns::girih8(edges[0], swapPalettes
                                          ? patterns::nasridPalette()
                                          : patterns::fezPalette());
    middle = patterns::girih8(edges[1], swapPalettes
                                            ? patterns::fezPalette()
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

} // namespace compose_gallery
