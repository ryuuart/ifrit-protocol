/** @file
 * nine slice — a carved hall: frame textures generated on intermediate
 * canvases and stretched over panels of every size, with one panel
 * relaid out every frame so the stretch is watched rather than assumed.
 */

#include <include/core/SkMaskFilter.h>
#include <sigilcompose/kit/Ornament.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;
using namespace sigil::compose::kit::ornament;

namespace {
/** A text style at one size and colour — the two things every label in
 *  this piece varies. */

/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

struct NineSlice final : sketch::Sketch {
  std::shared_ptr<sigil::image::ImageAsset> oakFrame, azureFrame, crimsonFrame;
  float stretch = 0.0f;

  /** The frame texture, and the factor every consumer of it must repeat:
   *  drawn at twice the size it is used at, so the corner bands stay sharp
   *  on a 2x device, and handed to the slice as a density of 2 so they come
   *  out at the on-page width the panel padding is measured against. */
  static constexpr float kFrameDensity = 2.0f;

  static std::shared_ptr<sigil::image::ImageAsset> generate(
      const Palette& pal) {
    // The intermediate canvas: draw the carved frame once, wrap the
    // snapshot, stretch it everywhere below.
    return std::make_shared<sigil::image::ImageAsset>(
        sigil::image::ImageAsset::wrap(
            makeCarvedFrame(pal, (int)(96 * kFrameDensity))));
  }

  Element describe() {
    auto panel = [&](const std::shared_ptr<sigil::image::ImageAsset>& f,
                     float l, float t, float w, float h) {
      return box()
          .width(w)
          .height(h)
          .inset(l, t, kSceneSize.width() - l - w, kSceneSize.height() - t - h)
          .background(carvedFrameSlice(f, kFrameDensity))
          // 24 clears the carved corner bosses, which reach 0.215 of the
          // 96-unit band in from the edge; a wider band would put type
          // under them.
          .padding(24);
    };

    const float breathW = 250 + 66 * stretch;
    const float breathH = 130 + 34 * stretch;

    return stack()
        .fill(sigil::compose::linearGradient(
            {0, 0}, {0, 640},
            {{0.09f, 0.07f, 0.10f, 1}, {0.05f, 0.06f, 0.09f, 1}}))
        // The source texture at natural size, labeled.
        .child(box()
                   .inset(24, 24, kSceneSize.width() - 24 - 200,
                          kSceneSize.height() - 24 - 150)
                   .column()
                   .gap(8)
                   .child(image(oakFrame).width(96).height(96))
                   .child(text(u8"the source texture — drawn 2x on an "
                               u8"offscreen canvas, wrapped, nine-sliced",
                               type({.size = 12, .color = hex(0x9aa4bb)}))
                              .width(190)))
        // Button: oak, small.
        .child(panel(oakFrame, 24, 210, 220, 84)
                   .alignItems(Align::Center)
                   .justify(Justify::Center)
                   .child(text(u8"BEGIN QUEST",
                               type({.size = 19, .color = hex(0x2b1c0b)}))))
        // Banner: azure, wide.
        .child(panel(azureFrame, 268, 24, 600, 108)
                   .justify(Justify::Center)
                   .child(text(u8"THE HALL OF STRETCHED FRAMES",
                               type({.size = 23, .color = hex(0x14243a)})))
                   .child(text(u8"one texture per palette — any size "
                               u8"without distortion, corners stay carved",
                               type({.size = 13.5f, .color = hex(0x3a4a63)}))))
        // Tall dialog: crimson, itemized.
        .child(panel(crimsonFrame, 560, 168, 300, 330)
                   .column()
                   .gap(12)
                   .child(text(u8"CELLAR MANIFEST",
                               type({.size = 19, .color = hex(0x3a1410)})))
                   .child(text(u8"◈  six barrels of pitch",
                               type({.size = 15, .color = hex(0x4a2018)})))
                   .child(text(u8"◈  the copper bowls",
                               type({.size = 15, .color = hex(0x4a2018)})))
                   .child(text(u8"◈  rope, forty fathoms",
                               type({.size = 15, .color = hex(0x4a2018)})))
                   .child(text(u8"◈  one coal, still warm",
                               type({.size = 15, .color = hex(0x4a2018)})))
                   .child(box().grow(1))
                   .child(text(u8"signed, the quartermaster",
                               type({.size = 13, .color = hex(0x6a3a30)}))))
        // The breathing panel: relaid out every frame — the lattice
        // stretches live while the carved corners hold their shape.
        .child(
            panel(oakFrame, 60, 380 - (breathHalf(breathH)), breathW, breathH)
                .alignItems(Align::Center)
                .justify(Justify::Center)
                .child(text(u8"stretch me",
                            type({.size = 17, .color = hex(0x2b1c0b)}))));
  }

  static float breathHalf(float h) { return (h - 130.0f) * 0.5f; }

  void setup(sketch::SketchContext& ctx) override {
    ctx.background({0, 0, 0, 1});
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    oakFrame = generate(oakPalette());
    azureFrame = generate(azurePalette());
    crimsonFrame = generate(crimsonPalette());
    stretch = 0.0f;
    ticker.add([this](double) { return true; });  // keep clock alive
    composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    Composer& composer = ctx.composer;
    stretch = 0.5f + 0.5f * (float)std::sin(elapsed * 1.4);
    composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH_AS(NineSlice, "nine slice", "Catalog \xc2\xb7 Scale",
                "#9 texture-gen")
