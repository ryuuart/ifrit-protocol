/** @file
 * half_float — the copy that makes an HDR image drawable, and what it
 * keeps that the ordinary one throws away.
 *
 * A decoded HDR panorama lands as 32-bit float RGBA, which keeps the
 * range a sun needs and is NOT FILTERABLE on Apple GPUs: a sampler asked
 * to interpolate between two F32 texels there answers nothing. The copy
 * that makes such an image drawable is a HALF-float one, and it belongs
 * beside the device rather than beside the decoder, because it is a
 * property of the hardware the pixels are going to and not of the file
 * they came from.
 *
 * `isFloatImage` is the question a caller asks first — the form an HDR
 * decode produces, and the one a sampler may refuse. `halfFloatPixels`
 * answers tightly packed half RGBA, four values a texel, row after row
 * with no padding, which is what a 16-bit float texture is uploaded
 * from. `bytePixels` is the ordinary path, and it stands beside the
 * float one so a caller choosing between them reads both in one place.
 *
 * VALUES ABOVE ONE SURVIVE the halves, which is the whole point of
 * asking for them rather than for bytes. This sheet writes a ramp that
 * runs past one, reads it back both ways, and tone-maps each readback at
 * the same two exposures: the half copy still has the highlights to
 * bring down, and the byte copy clipped them on the way out.
 *
 * EDIT THESE FIRST
 *   kPeak — how far past one the source ramp runs.
 *   kStops — the two exposures each readback is tone-mapped at.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilskia/graphite/Pixels.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace img = sigil::image;
namespace skia = sigil::skia;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 206;
constexpr float kPicture = 176;

constexpr int kSide = 96;                   // the source's side, texels
constexpr float kPeak = 6.0f;               // how far past one the ramp runs
constexpr float kStops[2] = {1.0f, 0.18f};  // the two exposures

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

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

/** The source: a ramp that runs from 0 to `kPeak` across the image, with
 *  a band at the top held at exactly 1 so the clip has an edge to be
 *  read against. */
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

Element cell(const char* call, const char* note, Element body) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   kit::well({.width = kCell,
                              .height = kPicture,
                              .ground = Fill::color(kCellGround),
                              .padding = 10})
                       .child(std::move(body)));
}

}  // namespace

struct HalfFloat final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // both readbacks have already been taken

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
    const std::string readout = kit::format(
        "isFloatImage \xc2\xb7 %s\nhalves %zu words \xc2\xb7 bytes %zu\n"
        "hot texel R \xc2\xb7 half %.2f \xc2\xb7 byte %.2f\n"
        "peak asked for \xc2\xb7 %.2f",
        isFloat ? "true" : "false", halves.size(), bytes.size(),
        halves.size() > hot * 4 ? halfToFloat(halves[hot * 4]) : 0.0f,
        bytes.size() > hot * 4 ? (float)bytes[hot * 4] / 255.0f : 0.0f, kPeak);

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("THE DRAWABLE COPY \xc2\xb7 skia::isFloatImage, "
                           "halfFloatPixels, bytePixels"),
             .subtitle = toU8("dials \xc2\xb7 how far past one the ramp runs "
                              "(6.0) \xc2\xb7 the two exposures each "
                              "readback is tone-mapped at (1.0 and 0.18) "
                              "\xc2\xb7 the source's side"),
             .footer = toU8("values above one survive the halves, which is "
                            "the whole point of asking for them rather than "
                            "for bytes \xe2\x80\x94 and the byte copy clipped "
                            "them on the way out, so no exposure brings them "
                            "back"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {cell("halfFloatPixels \xc2\xb7 exposure 1.0",
                           "the half readback shown straight \xc2\xb7 "
                           "everything past one is off the top of the "
                           "display, which is what a display is",
                           fromHalves(kStops[0])),
                      cell("halfFloatPixels \xc2\xb7 exposure 0.18",
                           "the same words brought down \xc2\xb7 the "
                           "highlights are still there to bring, because a "
                           "half held them",
                           fromHalves(kStops[1])),
                      cell("bytePixels \xc2\xb7 exposure 1.0",
                           "the ordinary readback \xc2\xb7 the same picture, "
                           "and the top band and the ramp above one are now "
                           "one colour",
                           fromBytes(kStops[0])),
                      cell("bytePixels \xc2\xb7 exposure 0.18",
                           "brought down by the same amount \xc2\xb7 nothing "
                           "comes back: the clip happened in the readback "
                           "and not in the display",
                           fromBytes(kStops[1])),
                      cell("what each readback answered",
                           "the question a caller asks first, the two buffer "
                           "sizes, and one hot texel read out of each",
                           text(toU8(readout), mono(10, kFigure))
                               .width(Dim(kCell - 20)))},
                 .gap = 12}))
            .absolute()
            .inset(0));
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
    return image(std::make_shared<const img::ImageAsset>(
                     img::ImageAsset::wrap(bitmap.asImage())))
        .width(Dim(kCell - 20))
        .height(Dim(kCell - 20));
  }
};

SIGIL_SKETCH(HalfFloat, "Kit \xc2\xb7 API",
             "one HDR ramp read back as halves and as bytes, each tone-"
             "mapped at the same two exposures, with the words of a hot "
             "texel printed from both")
