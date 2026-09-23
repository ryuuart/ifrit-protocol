/** @file
 * Four named float planes are encoded into one EXR and decoded by name.
 * The channel strip measures their peaks; the lower comparison composites
 * RGB and places G in a material roughness slot. Metadata is probed without
 * decoding pixels. This fixture has one part and no named layers.
 */

// TAGS: Media/Images

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilimage/decode/ChannelData.h>
#include <sigilimage/decode/Decode.h>
#include <sigilimage/encode/Encode.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/kit/Pbr.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace image = sigil::image;
namespace material = sigil::material;
namespace mskia = sigil::material::skia;

using namespace sigil::compose;

namespace {

constexpr int kSize = 192;     // the fixture's edge, px
constexpr float kPeak = 4.0f;  // the red plane's authored peak
constexpr SkSize kCanvas = {1100, 820};

constexpr material::Color kCellGround{0.12f, 0.12f, 0.14f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.type.captionLabel = {.size = 11, .track = 0.4f};
  look.spacing.captionGap = 6;
  return look;
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

/** One plane, drawn at the cell's own size. The picture is the plate's
 *  GROUND — a comparable image paint, which prunes on the image it names,
 *  where a canvas call can be compared to nothing. */
Element plane(const sk_sp<SkImage>& picture, float width = 237) {
  return sketch::kit::well(
      {.width = width,
       .height = 200,
       .ground = Fill::color(kCellGround),
       .content = sketch::kit::Well::Content{}},
      sigil::compose::image(picture, mskia::Fit::Contain).width(180).height(180));
}

}  // namespace

struct ExrChannels {
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

  void setup(sketch::SketchContext& ctx) {
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
    stone.slot(material::kit::kRoughnessSlot,
               material::Texture::of(planes.makeImage(g, g, g, -1)));
    const material::Texture* placed =
        material::kit::map(stone, material::kit::kRoughnessSlot);
    return placed ? placed->image() : nullptr;
  }

  Element sheet(const image::ChannelData& planes,
                const std::optional<image::ImageProbe>& probed,
                size_t byteSize) const {
    std::vector<sketch::kit::ComparisonCase> channels;
    for (size_t i = 0; i < planes.names.size(); ++i) {
      float peak = 0;
      for (int y = 0; y < planes.height; ++y)
        for (int x = 0; x < planes.width; ++x)
          peak = std::max(peak, planes.at(x, y, (int)i));
      channels.push_back(
          {.title = planes.names[i] + " PLANE",
           .control =
               kit::formatted("channel %zu · peak %.2f", i, (double)peak),
           .figure = plane(planes.makeImage((int)i, (int)i, (int)i, -1)),
           .note = planes.names[i] == "R"
                       ? "Radiance exceeds the display range."
                   : planes.names[i] == "G"
                       ? "The roughness field is independent of the colour."
                   : planes.names[i] == "B"
                       ? "A hard mask with a reduced-coverage band."
                       : "Opaque coverage throughout."});
    }
    std::string metadata =
        probed ? kit::formatted(
                     "%s · %d × %d\n%d channels · %s\n%zu bytes",
                     probed->format.c_str(), probed->width, probed->height,
                     probed->channels,
                     probed->floatingPoint ? "floating point" : "integer",
                     byteSize)
               : "Metadata probe unavailable";
    if (probed && !probed->layers.empty()) {
      metadata += "\nLayers:";
      for (const auto& layer : probed->layers) metadata += " " + layer;
    }
    Element outputs = sketch::kit::comparison(
        {.cases = {{.title = "COMPOSITE",
                    .control = "makeImage()",
                    .figure = plane(planes.makeImage(), 318),
                    .note = "The selected R, G and B planes become one "
                            "displayable image."},
                   {.title = "ROUGHNESS INPUT",
                    .control = "G → kRoughnessSlot",
                    .figure = plane(throughRoughnessSlot(planes), 318),
                    .note = "The G plane is put in a material slot, then read "
                            "back through kit::map."}},
         .measure = 660,
         .gap = 24});
    return sketch::kit::page(
        {.title = "An image is not always a picture",
         .subtitle = "One generated EXR stores four named float planes. Their "
                     "range and meaning survive decoding.",
         .footer = "The fixture is a single-part EXR. Named layer selection "
                   "needs a source with named layers."},
        box().column().gap(26).children(
            {sketch::kit::comparison(
                 {.cases = std::move(channels), .measure = 1020, .gap = 24}),
             sketch::kit::sectionHeader(
                 {.label = "CHOOSE WHAT EACH PLANE MEANS"}),
             box().row().gap(30).children(
                 {std::move(outputs),
                  sketch::kit::well({.width = 330, .padding = 22})
                      .column()
                      .gap(18)
                      .children(
                          {document::label("PROBE BEFORE DECODE"),
                           text(metadata).styleClass("readout").width(286),
                           text("The metadata query reads the format, "
                                "dimensions and channel names without decoding "
                                "pixels.")
                               .width(286)})})}));
  }

  static Element missing(const std::string& why) {
    const sketch::kit::Theme& sheet = sketch::kit::theme();
    return box()
        .cover()
        .fill(Fill::color(sheet.palette.ground))
        .column()
        .gap(10)
        .padding(40)
        .ink(sheet.palette.ink)
        .children({text("no float source here").font({.size = 20}),
                   text(why)
                       .font({.size = 12, .color = sheet.palette.ash})
                       .width(620.0f)});
  }
};

SIGIL_SKETCH(ExrChannels, "Kit · API",
             "an EXR written and read back plane by plane, each picked by "
             "name, the green one landing in a roughness slot")
