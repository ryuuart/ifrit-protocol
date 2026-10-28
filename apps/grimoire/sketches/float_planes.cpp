/** @file
 * float_planes — values past one, kept apart by name and kept by the
 * storage that can hold them.
 *
 * Four named float planes are written into one EXR and read back by name.
 * The planes have nothing to do with one another, which is the whole
 * reason a format keeps them apart: R is a radiance falloff that peaks
 * past one, G a roughness field, B a hard mask. The strip measures each
 * plane's peak; the composite reads R, G and B as one picture, and the G
 * plane alone goes into the slot a surface reads its roughness from and is
 * read back out of it. The metadata is probed without decoding a pixel.
 * This fixture has one part and no named layers.
 *
 * Then the same decoded picture is read back twice, through
 * `halfFloatPixels` and through `bytePixels`: the halves keep the radiance
 * past one, the bytes store the clip, and the centre texel says so.
 *
 * EDIT THESE FIRST
 *   kPeak — the red plane's authored peak.
 */

// TAGS: Media/Images, Materials/Color

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Resource.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/image/Channels.h>
#include <sigilmedia/image/Decode.h>
#include <sigilmedia/image/Encode.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilskia/graphite/Pixels.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace media = sigil::media;
namespace material = sigil::material;
namespace skia = sigil::skia;

using namespace sigil::compose;

namespace {

constexpr int kSize = 192;     // the fixture's edge, px
constexpr float kPeak = 4.0f;  // the red plane's authored peak
constexpr SkSize kCanvas = {1100, 1060};

constexpr material::Color kCellGround{0.12f, 0.12f, 0.14f, 1};

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
  for (int row = 0; row < kSize; ++row) {
    for (int column = 0; column < kSize; ++column) {
      const float u = ((float)column + 0.5f) / kSize;
      const float v = ((float)row + 0.5f) / kSize;
      const float across = u - 0.5f, down = v - 0.5f;
      const float radius = std::sqrt(across * across + down * down) * 2.0f;

      const float red = kPeak * std::exp(-radius * radius * 3.4f);
      const float rough = 0.5f +
                          0.34f * std::sin(u * 9.4f) * std::cos(v * 7.1f) +
                          0.12f * std::sin((u + v) * 88.0f);
      const float mask = (radius < 0.78f ? 1.0f : 0.0f) *
                         (v > 0.42f && v < 0.5f ? 0.25f : 1.0f);

      float* texel = &pixels[((size_t)row * kSize + column) * 4];
      texel[0] = red;
      texel[1] = std::clamp(rough, 0.0f, 1.0f);
      texel[2] = mask;
      texel[3] = 1.0f;
    }
  }
  return pixels;
}

/** The fixture's bytes, or null where this build has no EXR encoder. */
std::vector<std::byte> writeExr() {
  const std::vector<float> pixels = fields();
  const SkImageInfo info = SkImageInfo::Make(
      kSize, kSize, kRGBA_F32_SkColorType, kUnpremul_SkAlphaType);
  const SkPixmap map(info, pixels.data(), (size_t)kSize * 4 * sizeof(float));
  return media::encode(map, media::Format::Exr);
}

/** One plane, drawn at the cell's own size. The picture is the plate's
 *  GROUND — a comparable image paint, which prunes on the image it names,
 *  where a canvas call can be compared to nothing. */
Element plane(const media::PixelSource& picture, float width = 237) {
  return sketch::kit::well(
      {.width = width,
       .height = 200,
       .ground = Fill::color(kCellGround),
       .content = sketch::kit::Well::Content{}},
      sigil::compose::image(picture, material::Fit::Contain)
          .width(180)
          .height(180));
}

}  // namespace

struct FloatPlanes {
  /** WHAT THIS BUILD MUST HAVE. The sheet writes its own fixture, so it
   *  needs the EXR encoder as well as the decoder — a build without
   *  OpenImageIO's encode side has neither. */
  static bool available(std::string* why) {
    if (!writeExr().empty()) return true;
    if (why)
      *why =
          "no EXR encoder in this build (SIGILIMAGE_HAS_OIIO_ENCODE), so "
          "the sheet cannot write the float fixture it reads back";
    return false;
  }

  void setup(sketch::Scene2DContext& ctx) {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(
        ctx,
        {.theme = sketch::kit::featureTheme(sketch::kit::Density::Spacious),
         .sheet = rule(":root").var("spacing-caption-gap", 6),
         .size = kCanvas,
         .captureAt = 0.05});

    const auto bytes = writeExr();
    if (bytes.empty()) {
      ctx.composer.render(missing("the EXR encoder wrote nothing"));
      return;
    }
    const std::optional<media::Metadata> probed =
        media::probe(bytes, "fixture.exr");
    const auto planes = media::decode<media::Channels>(bytes, "fixture.exr");
    if (!planes) {
      ctx.composer.render(missing("decodeChannels read no planes back"));
      return;
    }
    ctx.composer.render(sheet(*planes, probed, bytes.size()));
  }

  /** The green plane in the slot a surface reads its roughness from, and
   *  the same texture read back out of it. */
  static media::Picture throughRoughnessSlot(const media::Channels& planes) {
    const int green = planes.index("G");
    if (green < 0) return {};
    material::Material stone = material::surface::program(
        {.baseColor = {0.62f, 0.60f, 0.56f, 1}, .roughness = 1.0f});
    stone.slot(material::surface::kRoughnessSlot,
               material::Texture(
                   planes.image(media::ChannelPick{green, green, green, -1})));
    const material::Texture* placed =
        material::surface::map(stone, material::surface::kRoughnessSlot);
    return placed ? placed->frameAt({}).image : media::Picture{};
  }

  Element sheet(const media::Channels& planes,
                const std::optional<media::Metadata>& probed,
                size_t byteSize) const {
    std::vector<sketch::kit::ComparisonCase> channels;
    for (size_t channel = 0; channel < planes.names.size(); ++channel) {
      float peak = 0;
      for (int row = 0; row < planes.height; ++row)
        for (int column = 0; column < planes.width; ++column)
          peak = std::max(peak, planes.at(column, row, (int)channel));
      channels.push_back(
          {.title = planes.names[channel] + " PLANE",
           .control =
               kit::formatted("channel %zu · peak %.2f", channel, (double)peak),
           .figure = plane(planes.image(media::ChannelPick{
               (int)channel, (int)channel, (int)channel, -1})),
           .note = planes.names[channel] == "R"
                       ? "Radiance exceeds the display range."
                   : planes.names[channel] == "G"
                       ? "The roughness field is independent of the colour."
                   : planes.names[channel] == "B"
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
                    .control = "image()",
                    .figure = plane(planes.image(), 318),
                    .note = "The selected R, G and B planes become one "
                            "displayable image."},
                   {.title = "ROUGHNESS INPUT",
                    .control = "G → kRoughnessSlot",
                    .figure = plane(throughRoughnessSlot(planes), 318),
                    .note = "The G plane is put in a material slot, then read "
                            "back through kit::map."}},
         .measure = 660,
         .gap = 24});
    // THE SAME DECODED PICTURE READ BACK TWICE: as halves, which keep
    // what is past one, and as bytes, which store the clip.
    const sk_sp<SkImage> decoded =
        media::toSk(media::PixelSource(planes.image()).frameAt({}).image);
    const std::vector<uint16_t> halves = skia::halfFloatPixels(decoded);
    const std::vector<uint8_t> bytes = skia::bytePixels(decoded);
    const int middle = kSize / 2;
    const size_t centre = ((size_t)middle * kSize + middle) * 4;
    const std::string storage = kit::formatted(
        "isFloatImage · %s\nhalves · %zu words of 16 bits\nbytes · %zu "
        "words of 8 bits\nR at the centre, decoded · %.2f\nR at the centre, "
        "as a byte · %.2f",
        skia::isFloatImage(decoded) ? "true" : "false", halves.size(),
        bytes.size(), (double)planes.at(middle, middle, 0),
        bytes.size() > centre ? bytes[centre] / 255.0 : 0.0);

    return sketch::kit::page(
        {.title = "An image is not always a picture",
         .subtitle = "One generated EXR stores four named float planes. Their "
                     "range and meaning survive decoding, and survive "
                     "storage only where the storage can hold them.",
         .footer = "The fixture is a single-part EXR. Named layer selection "
                   "needs a source with named layers."},
        box().column().gap(26).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  FOUR NAMED PLANES",
                  .note = "Each picked by name, its peak measured"}),
             sketch::kit::comparison(
                 {.cases = std::move(channels), .measure = 1020, .gap = 24}),
             sketch::kit::sectionHeader(
                 {.label = "02  CHOOSE WHAT EACH PLANE MEANS"}),
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
                               .width(286)})}),
             sketch::kit::sectionHeader(
                 {.label = "03  STORAGE DECIDES WHAT SURVIVES",
                  .note = "The composite read back as halves and as bytes"}),
             sketch::kit::well({.width = 1020, .padding = 22})
                 .row()
                 .gap(40)
                 .children(
                     {text(storage).styleClass("readout").width(420),
                      text("halfFloatPixels keeps the radiance past one in "
                           "sixteen-bit floats, the form a device texture "
                           "is uploaded from; bytePixels stores the clip, "
                           "and nothing read from it afterwards brings the "
                           "falloff back.")
                          .width(500)})}));
  }

  static Element missing(const std::string& why) {
    return box()
        .cover()
        .column()
        .gap(10)
        .padding(40)
        .ink(var("palette-ink"))
        .children({text("no float source here").font({.size = 20}),
                   text(why)
                       .font({.size = 12})
                       .ink(var("palette-ash"))
                       .width(620.0f)});
  }
};

SIGIL_SKETCH(FloatPlanes, "Kit · API",
             "an EXR written and read back plane by plane, each picked by "
             "name, the green one landing in a roughness slot, then the "
             "decoded picture read back as halves, which keep what is past "
             "one, and as bytes, which store the clip")
