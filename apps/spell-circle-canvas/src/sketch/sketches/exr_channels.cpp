/** @file
 * exr_channels — every channel a float source carries, picked by name.
 *
 * `decodeChannels` is the door past the SkImage. An EXR does not hold a
 * picture, it holds NAMED PLANES, and a decode that composited them into
 * four bytes a channel would throw away both the names and the range. So
 * `ChannelData` hands back what the file actually carries: the names in
 * source order, the interleaved floats, and `index(name)` to find one.
 * `makeImage(r, g, b, a)` is the way back — explicit channel indices,
 * `-1` for a plane that is absent, which is how one plane becomes a
 * greyscale map and three become a picture.
 *
 * `probeImage` reads the format, the channel names and the floating-point
 * flag WITHOUT decoding a pixel, which is what a hub asks when it is
 * deciding what a resource is.
 *
 * WHERE A CHANNEL GOES. A picked plane is a map, and a map fills a slot:
 * `Material::child(kit::kRoughnessSlot, Texture::of(image))` puts the
 * green plane where a surface reads its roughness, and `kit::map` reads
 * it back out. The last cell is that slot's contents — what a renderer
 * standing on the surface would sample.
 *
 * THE FIXTURE IS WRITTEN HERE, in setup, and the round trip is the
 * point: three fields are laid down as floats, `encodeImage(pixels,
 * Format::Exr)` writes them, and everything on the sheet comes back
 * through `decodeChannels`. The red plane is authored ABOVE ONE on
 * purpose — the readout prints the peak that came back, which is the
 * claim that the range survived a format the eight-bit encoders would
 * have flattened.
 *
 * WHAT IS NOT SHOWN, and why: this build's encoder writes a SINGLE-PART
 * EXR with the four ordinary channel names, so there is no `diffuse.R`
 * to select with `DecodeOptions::layer` here. Named layers and
 * multi-part files decode — the layer option and the part-name prefix
 * are implemented — but nothing in this tree WRITES one, so a sheet that
 * showed layer selection would have to ship a binary fixture.
 *
 * EDIT THESE FIRST
 *   kSize  — the fixture's edge, px.
 *   kPeak  — how far above one the red plane is authored.
 *   kCell  — how large each plane is drawn.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilimage/decode/ChannelData.h>
#include <sigilimage/decode/Decode.h>
#include <sigilimage/encode/Encode.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace image = sigil::image;
namespace material = sigil::material;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr int kSize = 192;     // the fixture's edge, px
constexpr float kPeak = 4.0f;  // the red plane's authored peak
constexpr float kCell = 158;   // how large each plane is drawn
constexpr SkSize kCanvas = {1080, 396};

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.12f, 0.12f, 0.14f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.type.subtitle = {.size = 11, .track = 0.6f};
  look.type.footer = {.size = 10.5f, .track = 0.3f};
  look.type.captionLabel = {.size = 11, .track = 0.4f};
  look.type.captionNote = {.size = 10.5f, .track = 0.2f};
  look.spacing.captionGap = 6;
  return look;
}

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

/** THE FIXTURE, as floats. Three fields that have nothing to do with one
 *  another, which is the whole reason a format keeps planes apart:
 *
 *    R — a radial falloff scaled past one, so the file carries values no
 *        eight-bit encoder could hold.
 *    G — a roughness field: broad lobes crossed by fine scratches.
 *    B — a hard-edged mask, a disc and a bar.
 *    A — opaque throughout.
 */
std::vector<float> fields() {
  std::vector<float> pixels((size_t)kSize * kSize * 4, 0.0f);
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      const float u = ((float)x + 0.5f) / kSize;
      const float v = ((float)y + 0.5f) / kSize;
      const float dx = u - 0.5f, dy = v - 0.5f;
      const float r = std::sqrt(dx * dx + dy * dy) * 2.0f;

      const float red = kPeak * std::exp(-r * r * 3.4f);
      const float rough = 0.5f +
                          0.34f * std::sin(u * 9.4f) * std::cos(v * 7.1f) +
                          0.12f * std::sin((u + v) * 88.0f);
      const float mask =
          (r < 0.78f ? 1.0f : 0.0f) * (v > 0.42f && v < 0.5f ? 0.25f : 1.0f);

      float* p = &pixels[((size_t)y * kSize + x) * 4];
      p[0] = red;
      p[1] = std::clamp(rough, 0.0f, 1.0f);
      p[2] = mask;
      p[3] = 1.0f;
    }
  }
  return pixels;
}

/** The fixture's bytes, or null where this build has no EXR encoder. */
sk_sp<SkData> writeExr() {
  const std::vector<float> pixels = fields();
  const SkImageInfo info = SkImageInfo::Make(
      kSize, kSize, kRGBA_F32_SkColorType, kUnpremul_SkAlphaType);
  const SkPixmap map(info, pixels.data(), (size_t)kSize * 4 * sizeof(float));
  return image::encodeImage(map, image::Format::Exr);
}

Element cell(std::string key, sk_sp<SkImage> picture, const char* call,
             std::string note) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      custom(std::move(key),
             [picture](SkCanvas& canvas, const PaintContext&) {
               if (!picture) return;
               SkPaint paint;
               canvas.drawImageRect(picture, SkRect::MakeWH(kCell, kCell),
                                    SkSamplingOptions(SkFilterMode::kLinear),
                                    &paint);
             })
          .width(kCell)
          .height(kCell)
          .fill(Fill::color(kCellGround)));
}

}  // namespace

struct ExrChannels final : sketch::Sketch {
  /** WHAT THIS BUILD MUST HAVE. The sheet writes its own fixture, so it
   *  needs the EXR encoder as well as the decoder — a build without
   *  OpenImageIO's encode side has neither. */
  static bool available(std::string* why) {
    if (writeExr()) return true;
    if (why)
      *why =
          "no EXR encoder in this build (SIGILIMAGE_HAS_OIIO_ENCODE), so "
          "the sheet cannot write the float fixture it reads back";
    return false;
  }

  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const sk_sp<SkData> bytes = writeExr();
    if (!bytes) {
      ctx.composer.render(missing("the EXR encoder wrote nothing"));
      return;
    }
    const auto* raw = static_cast<const std::byte*>(bytes->data());
    const std::optional<image::ImageProbe> probed =
        image::probeImage(raw, bytes->size(), "fixture.exr");
    const std::optional<image::ChannelData> planes =
        image::decodeChannels(raw, bytes->size(), "fixture.exr");
    if (!planes) {
      ctx.composer.render(missing("decodeChannels read no planes back"));
      return;
    }
    ctx.composer.render(sheet(*planes, probed, bytes->size()));
  }

  /** The green plane in the slot a surface reads its roughness from, and
   *  the same texture read back out of it. */
  static sk_sp<SkImage> throughRoughnessSlot(const image::ChannelData& planes) {
    const int g = planes.index("G");
    if (g < 0) return nullptr;
    material::Material stone = material::kit::surface(
        {.baseColor = {0.62f, 0.60f, 0.56f, 1}, .roughness = 1.0f});
    stone.child(material::kit::kRoughnessSlot,
                material::Texture::of(planes.makeImage(g, g, g, -1)));
    const material::Texture* placed =
        material::kit::map(stone, material::kit::kRoughnessSlot);
    return placed ? placed->image() : nullptr;
  }

  Element sheet(const image::ChannelData& planes,
                const std::optional<image::ImageProbe>& probed,
                size_t byteSize) const {
    kit::Cells shelf{.gap = 14};
    for (size_t i = 0; i < planes.names.size(); ++i) {
      const int c = (int)i;
      float peak = 0.0f;
      for (int y = 0; y < planes.height; ++y)
        for (int x = 0; x < planes.width; ++x)
          peak = std::max(peak, planes.at(x, y, c));
      shelf.cells.push_back(
          cell("plane" + planes.names[i], planes.makeImage(c, c, c, -1),
               "makeImage(i, i, i, -1)",
               "index(\"" + planes.names[i] + "\") = " + std::to_string(c) +
                   "   peak " + kit::formatted("%.2f", (double)peak)));
    }
    shelf.cells.push_back(cell("layer", planes.makeImage(), "makeImage()",
                               "the default layer \xe2\x80\x94 R, G and B "
                               "composited, alpha filled where absent"));
    shelf.cells.push_back(
        cell("slot", throughRoughnessSlot(planes),
             "child(kRoughnessSlot, Texture::of(\xe2\x80\xa6))",
             "the green plane where a surface reads its roughness, read "
             "back with kit::map"));

    std::string foot = "probeImage \xe2\x80\x94 ";
    if (probed) {
      foot += probed->format + ", " + std::to_string(probed->width) +
              "\xc3\x97" + std::to_string(probed->height) + ", " +
              std::to_string(probed->channels) + " channels, " +
              (probed->floatingPoint ? "floating point" : "integer") + ", " +
              std::to_string(byteSize) + " bytes, no pixels decoded";
      if (!probed->channelNames.empty()) {
        foot += "   \xc2\xb7   names";
        for (const std::string& name : probed->channelNames) foot += " " + name;
      }
      if (!probed->layers.empty()) {
        foot += "   \xc2\xb7   layers";
        for (const std::string& layer : probed->layers) foot += " " + layer;
      } else {
        foot +=
            "   \xc2\xb7   one part, no named layers \xe2\x80\x94 which is "
            "what this build's encoder writes";
      }
    } else {
      foot += "nothing";
    }

    return sketch::kit::page(
        {.title = toU8("FLOAT CHANNELS \xc2\xb7 decodeChannels "
                       "+ ChannelData::index / makeImage"),
         .subtitle = toU8("dials \xc2\xb7 the channel (named on "
                          "each cell) \xc2\xb7 the slot the "
                          "picked plane fills"),
         .footer = toU8(foot)},
        kit::cells(std::move(shelf)));
  }

  static Element missing(const std::string& why) {
    return box()
        .absolute()
        .inset(0)
        .fill(Fill::color(kGround))
        .column()
        .gap(10)
        .padding(40)
        .child(text(toU8("no float source here"), label(20, kInk)))
        .child(text(toU8(why), label(12, kAsh)).width(Dim(620.0f)));
  }
};

SIGIL_SKETCH(ExrChannels, "Kit \xc2\xb7 API",
             "an EXR written and read back plane by plane, each picked by "
             "name, the green one landing in a roughness slot")
