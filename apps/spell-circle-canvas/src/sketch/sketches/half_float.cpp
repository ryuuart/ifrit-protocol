/** @file
 * The same HDR ramp read back through halfFloatPixels and bytePixels.
 * Rows select the storage format and columns select the exposure applied
 * after readback. The upper band exceeds one; the lower is clipped before
 * storage. A hot texel is read from both buffers to distinguish data loss
 * from a highlight merely exceeding the display range.
 */

// TAGS: Materials/Color, Media/Images

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilskia/graphite/Pixels.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace img = sigil::image;
namespace skia = sigil::skia;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 820};
constexpr float kCell = 192;

constexpr int kSide = 96;                   // the source's side, texels
constexpr float kPeak = 6.0f;               // how far past one the ramp runs
constexpr float kStops[2] = {1.0f, 0.18f};  // the two exposures

/** IEEE half back to float, written out because reading the words is the
 *  whole point of asking for them: a packed half is data until somebody
 *  decodes it. */
float halfToFloat(uint16_t bits) {
  const uint32_t sign = (uint32_t)(bits >> 15) & 1u;
  const uint32_t exponent = (uint32_t)(bits >> 10) & 0x1fu;
  const uint32_t mantissa = (uint32_t)bits & 0x3ffu;
  float value = 0.0f;
  if (exponent == 0) {
    value = (float)mantissa * 5.9604645e-8f;  // 2^-24, the subnormal step
  } else if (exponent == 0x1f) {
    value = mantissa ? 0.0f : 65504.0f;  // NaN reads as 0, infinity clamps
  } else {
    value = (float)(mantissa + 1024u) * std::pow(2.0f, (float)exponent - 25.0f);
  }
  return sign ? -value : value;
}

/** The upper ramp runs from zero to kPeak; the lower ramp is clipped
 *  to one before either readback. */
sk_sp<SkImage> hdrSource() {
  sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::Make(
      kSide, kSide, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SkColor4f{0, 0, 0, 1}.toSkColor());
  SkPaint paint;
  for (int x = 0; x < kSide; ++x) {
    const float t = (float)x / (float)(kSide - 1);
    const float v = t * kPeak;
    paint.setColor4f({v, v * 0.82f, v * 0.55f, 1}, nullptr);
    canvas->drawRect({(float)x, 0, (float)x + 1, kSide * 0.62f}, paint);
    paint.setColor4f({std::min(v, 1.0f), std::min(v, 1.0f) * 0.82f,
                      std::min(v, 1.0f) * 0.55f, 1},
                     nullptr);
    canvas->drawRect({(float)x, kSide * 0.62f, (float)x + 1, (float)kSide},
                     paint);
  }
  return surface->makeImageSnapshot();
}

}  // namespace

struct HalfFloat {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // both readbacks have already been taken
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const sk_sp<SkImage> source = hdrSource();
    const bool isFloat = skia::isFloatImage(source);
    const std::vector<uint16_t> halves = skia::halfFloatPixels(source);
    const std::vector<uint8_t> bytes = skia::bytePixels(source);

    // The same tone map over each readback, so the only difference
    // between the two rows is what the readback still had to give.
    const auto fromHalves = [&](float exposure) {
      return mapped((int)halves.size() / 4, [&](size_t texel, int channel) {
        return halfToFloat(halves[texel * 4 + (size_t)channel]) * exposure;
      });
    };
    const auto fromBytes = [&](float exposure) {
      return mapped((int)bytes.size() / 4, [&](size_t texel, int channel) {
        return (float)bytes[texel * 4 + (size_t)channel] * (1.0f / 255.0f) *
               exposure;
      });
    };

    const size_t hot = (size_t)(kSide / 4) * (size_t)kSide + (size_t)kSide - 4;
    const std::string readout = kit::formatted(
        "isFloatImage · %s\nhalves %zu words · bytes %zu\n"
        "hot texel R · half %.2f · byte %.2f\n"
        "peak asked for · %.2f",
        isFloat ? "true" : "false", halves.size(), bytes.size(),
        halves.size() > hot * 4 ? halfToFloat(halves[hot * 4]) : 0.0f,
        bytes.size() > hot * 4 ? (float)bytes[hot * 4] / 255.0f : 0.0f, kPeak);

    const auto exposureRow = [&](bool floating) {
      return sketch::kit::comparison(
          {.cases =
               {{.title = "DISPLAY EXPOSURE",
                 .control = "1.00 ×",
                 .figure = sketch::kit::well(
                     {.width = 332,
                      .height = 182,
                      .content = sketch::kit::Well::Content{}},
                     floating ? fromHalves(kStops[0]) : fromBytes(kStops[0])),
                 .note = "Bright values meet the display ceiling."},
                {.title = "REDUCED EXPOSURE",
                 .control = "0.18 ×",
                 .figure = sketch::kit::well(
                     {.width = 332,
                      .height = 182,
                      .content = sketch::kit::Well::Content{}},
                     floating ? fromHalves(kStops[1]) : fromBytes(kStops[1])),
                 .note =
                     floating
                         ? "Highlight structure survives in the stored values."
                         : "Lowering exposure darkens the clipped plateau."}},
           .measure = 688,
           .gap = 24});
    };
    Element matrix = box().column().gap(20).width(688).children(
        {sketch::kit::sectionHeader(
             {.label = "16-BIT FLOAT", .note = "halfFloatPixels"}),
         exposureRow(true),
         sketch::kit::sectionHeader(
             {.label = "8-BIT CHANNELS", .note = "bytePixels"}),
         exposureRow(false)});
    Element readings =
        sketch::kit::well({.width = 300, .padding = 22})
            .column()
            .gap(22)
            .children(
                {document::label("SAME SOURCE"),
                 text("0 → 6")
                     .font({.size = 44})
                     .ink(sketch::kit::theme().palette.figure),
                 text("The upper ramp exceeds one. The lower band is limited "
                      "to one before either readback.")
                     .width(256),
                 document::label("BUFFER EVIDENCE"),
                 text(readout).styleClass("readout").width(256),
                 text("A display can hide a highlight that is still in the "
                      "file. An 8-bit readback removes that information.")
                     .width(256)});
    ctx.composer.render(sketch::kit::page(
        {.title = "A highlight, kept or clipped",
         .subtitle = "Storage format runs down the page; exposure runs across "
                     "it. Every picture starts with the same HDR ramp.",
         .footer = "The exposure is applied after readback. Both rows use the "
                   "same clamp only when making a displayable image."},
        box().row().gap(32).children(
            {std::move(matrix), std::move(readings)})));
  }

  /** A readback laid back out as a displayable picture: the sampler is
   *  handed a texel and a channel and answers a linear value, which is
   *  clamped once here — the only clamp on the sheet. */
  template <typename Sampler>
  Element mapped(int texels, Sampler sampler) {
    if (texels < kSide * kSide) return box();
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(kSide, kSide));
    for (int y = 0; y < kSide; ++y)
      for (int x = 0; x < kSide; ++x) {
        const size_t texel = (size_t)y * kSide + (size_t)x;
        const auto channel = [&](int c) {
          return (uint32_t)std::clamp(sampler(texel, c) * 255.0f, 0.0f, 255.0f);
        };
        *bitmap.getAddr32(x, y) =
            0xff000000u | (channel(2) << 16) | (channel(1) << 8) | channel(0);
      }
    bitmap.setImmutable();
    return image(bitmap.asImage(), Fit::Stretch)
        .width(kCell - 20)
        .height(kCell - 20);
  }
};

SIGIL_SKETCH(HalfFloat, "Kit · API",
             "one HDR ramp read back as halves and as bytes, each tone-"
             "mapped at the same two exposures, with the words of a hot "
             "texel printed from both")
