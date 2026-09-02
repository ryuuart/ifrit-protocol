// frame_asset.cpp — an ASSET sketch: authors a 96px carved nine-slice
// frame at exact pixel size, for headless export:
//
//   Sketchbook sketches/frame_asset.cpp \
//       --frame frame.png --at 0
//
// The canvas IS the asset: ctx.canvas(96, 96) with --scale 1 (the
// default) captures exactly 96×96 pixels, ready for
// SigilLoader + Slice{xDivs {32,64}, yDivs {32,64}} in any demo.
// Edit-and-save still live-previews in the windowed host; Cmd+S drops
// numbered captures next to the sketch.
//
// WHAT IT DRAWS IS THE KIT'S, not this file's: the template teaches the
// export workflow — declare the exact canvas, give it a transparent
// ground, draw, capture — and re-deriving art the ornament kit already
// owns would teach the drawing instead. Swap the palette, swap the
// generator, or put your own tree in its place; the four lines around
// it are what the template is.
//
// EDIT THESE FIRST
//   ctx.canvas(96, 96) — the asset's exact pixel size. It is also the
//                        size the frame is generated at, so the two
//                        cannot drift.
//   oakPalette()       — azure, crimson and emerald stand beside it in
//                        sigilcompose/kit/Ornament.h.

#include <sigilcompose/kit/Ornament.h>
#include <sigilsketch/canvas/Sketch.h>

#include <memory>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using namespace sigil::compose::kit::ornament;

namespace {
constexpr int kSide = 96;
}  // namespace

struct FrameAsset : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSide, kSide);
    ctx.background({0, 0, 0, 0});  // transparent: it's a texture

    ctx.composer.render(image(std::make_shared<sigil::image::ImageAsset>(
                                  sigil::image::ImageAsset::wrap(
                                      makeCarvedFrame(oakPalette(), kSide))))
                            .inset(0));
  }
};

SIGIL_SKETCH(
    FrameAsset, "Kit",
    "The asset template \xe2\x80\x94 an exact canvas, a transparent ground, "
    "a nine-slice frame exported at its own size")
