/** @file
 * The decoded frames of Space Jam's fastbreak.gif beside time queries.
 * Each decoded frame already includes the format's disposal and blend rules.
 * The top strip reports frame duration and start time; the lower strip uses
 * frameAt() across repeated playback and reports the corresponding phase.
 * The resource must be available in the network cache for a deterministic
 * capture; no replacement image stands in for it.
 */

// TAGS: Media/Images

#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace image = sigil::image;
namespace io = sigil::io;
namespace mskia = sigil::material::skia;

using namespace sigil::compose;

namespace {

constexpr const char* kSource =
    "https://www.spacejam.com/1996/img/fastbreak.gif";
constexpr SkSize kCanvas = {1120, 760};
constexpr float kScale = 2.5f;  // sheet pixels per source pixel
/** The moments the lower shelf reads. The file's own loop is 600 ms, so
 *  these run across two of them and the frames come round again. */
constexpr double kSamples[] = {0, 150, 320, 480, 640, 900, 1150, 1420};

constexpr SkColor4f kCellGround{0.12f, 0.12f, 0.14f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.type.captionLabel = {.size = 11, .track = 0.4f};
  look.spacing.captionGap = 6;
  return look;
}

Element frameFigure(const sk_sp<SkImage>& frame, float w, float h,
                    float measure = 116) {
  return sketch::kit::well(
      {.width = measure,
       .height = h + 16,
       .ground = Fill::color(kCellGround),
       .content = sketch::kit::Well::Content{}},
      sigil::compose::image(frame, mskia::Fit::Contain)
          .width(w)
          .height(h)
          .imageRendering(SkSamplingOptions(SkFilterMode::kNearest)));
}

}  // namespace

struct GifFrames {
  /** WHAT THIS MACHINE MUST HAVE. The sheet is about one real file, and a
   *  cold cache would render a different picture under the same name. */
  static bool available(std::string* why) {
    return sketch::requireCached({kSource}, why);
  }

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // every frame is on the sheet; nothing moves
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    io::Hub& hub = ctx.assets.hub();
    const std::optional<io::ResourceInfo> bytes = hub.probe(kSource);
    const std::optional<image::ImageProbe> meaning =
        hub.probe<image::ImageProbe>(kSource);
    const std::shared_ptr<const image::ImageAsset> gif = hub.image(kSource);

    ctx.composer.render(gif ? sheet(*gif, bytes, meaning) : missing());
  }

  /** The shelf of decoded frames, in file order, each with its own
   *  duration. */
  Element decoded(const image::ImageAsset& gif) const {
    const float w = (float)gif.width() * kScale;
    const float h = (float)gif.height() * kScale;
    std::vector<sketch::kit::ComparisonCase> frames;
    double start = 0;
    size_t index = 0;
    for (const auto& frame : gif.frames()) {
      frames.push_back({.title = kit::formatted("FRAME %zu", ++index),
                        .control = kit::formatted("%.0f ms duration",
                                                  (double)frame.durationMs),
                        .figure = frameFigure(frame.image, w, h, 160),
                        .note = kit::formatted("Starts at %.0f ms", start)});
      start += frame.durationMs;
    }
    return sketch::kit::comparison(
        {.cases = std::move(frames), .measure = 1040, .gap = 16});
  }

  /** The shelf of PLAYBACK: one moment per cell, read back through the
   *  loop. */
  Element sampled(const image::ImageAsset& gif) const {
    const float w = (float)gif.width() * kScale;
    const float h = (float)gif.height() * kScale;
    std::vector<sketch::kit::ComparisonCase> moments;
    for (double at : kSamples)
      moments.push_back(
          {.title = kit::formatted("%.0f ms", at),
           .control = "frameAt(time)",
           .figure = frameFigure(gif.frameAt(at).image, w, h),
           .note = kit::formatted("Loop phase %.0f ms",
                                  std::fmod(at, gif.totalDurationMs()))});
    return sketch::kit::comparison(
        {.cases = std::move(moments), .measure = 1040, .gap = 16});
  }

  Element sheet(const image::ImageAsset& gif,
                const std::optional<io::ResourceInfo>& bytes,
                const std::optional<image::ImageProbe>& meaning) const {
    std::string foot = "Hub::probe() — ";
    if (meaning && bytes)
      foot += meaning->format + ", " + std::to_string(bytes->byteSize) +
              " bytes, " + std::to_string(meaning->width) + "×" +
              std::to_string(meaning->height) + ", " +
              std::to_string(meaning->frames) +
              " frames, no pixels "
              "decoded";
    else
      foot += "nothing (the hub could not sniff this resource)";
    foot += "   ·   decoded — " + std::to_string(gif.frames().size()) +
            " frames, " +
            kit::formatted("%.0f ms", (double)gif.totalDurationMs()) +
            " a loop, " +
            (gif.repetitionCount() == image::ImageAsset::kInfinite
                 ? std::string("repeating forever")
                 : std::to_string(gif.repetitionCount()) + " repetitions");

    return sketch::kit::page(
        {.title = "Frames are not timestamps",
         .subtitle = "Space Jam's fastbreak.gif · decoded images above, "
                     "playback queries below · nearest-neighbour enlargement",
         .footer = foot},
        box().column().gap(24).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  DECODE THE DOCUMENT",
                  .note = kit::formatted("%zu composited frames",
                                         gif.frames().size())}),
             decoded(gif),
             sketch::kit::sectionHeader(
                 {.label = "02  ASK FOR A MOMENT",
                  .note = kit::formatted("%.0f ms per loop",
                                         (double)gif.totalDurationMs())}),
             sampled(gif),
             document::caption(
                 "Each frame already includes disposal and blend rules. "
                 "Playback selects among those complete images using their "
                 "durations.")
                 .width(440)}));
  }

  /** What stands here when the file decoded to nothing. The availability
   *  probe keeps a sweep away from this; the app opens whatever is
   *  selected, so it is drawn rather than left blank. */
  static Element missing() {
    const sketch::kit::Theme& sheet = sketch::kit::theme();
    return box()
        .cover()
        .fill(Fill::color(sheet.palette.ground))
        .column()
        .gap(10)
        .padding(40)
        .ink(sheet.palette.ink)
        .children({text("no animated document here").font({.size = 20}),
                   text(std::string(kSource) +
                        " did not decode: the hub reached neither the "
                        "network nor a cached copy of it")
                       .font({.size = 12, .color = sheet.palette.ash})
                       .width(620.0f)});
  }
};

SIGIL_SKETCH(GifFrames, "Kit · API",
             "every frame of a real GIF beside the moments frameAt reads "
             "them back at, with the probe that never decoded a pixel")
