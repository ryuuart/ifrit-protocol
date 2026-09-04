/** @file
 * gif_frames — an animated document, one frame at a time.
 *
 * `ImageAsset` decodes a GIF (or an animated WebP or AVIF) into a list of
 * `Frame`s, each a premultiplied SkImage with the milliseconds it stays
 * on screen. EVERY FRAME IS FULLY COMPOSITED AT DECODE TIME: the source
 * format's disposal and blend rules are already applied, so drawing frame
 * N never depends on having drawn frame N-1, and a sheet may lay them out
 * in any order at all — which is what the top shelf does.
 *
 * `frameAt(milliseconds)` is the playback: the frame showing at that many
 * milliseconds since the animation started, looped according to
 * `repetitionCount()` — a finite animation that has finished holds its
 * last frame, and a still image answers its one frame at any time. The
 * lower shelf asks for one moment per cell across two loop periods, so
 * the same frames come round again with the moment printed under each.
 *
 * `Hub::probe()` reads the metadata WITHOUT decoding pixels: the format,
 * the byte size, the dimensions and the frame count. The readout at the
 * foot is that probe beside what the decode actually produced.
 *
 * THE SUBJECT IS THE REAL FILE. `fastbreak.gif` is the one thing that
 * ever moved on the 1996 Space Jam site, fetched here over https through
 * the loader's own cache; `sketch::requireCached` is the availability
 * door, so a machine that has fetched it once renders this sheet forever
 * after and offline, and one that never has stands the sketch down by
 * name rather than drawing a stand-in under it.
 *
 * EDIT THESE FIRST
 *   kSource  — the animated file. Any format the codecs carry.
 *   kSamples — the moments the lower shelf reads frameAt at, ms.
 *   kScale   — how many sheet pixels one source pixel covers.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilloader/hub/Hub.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>

#include <memory>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace image = sigil::image;
namespace loader = sigil::loader;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr const char* kSource =
    "https://www.spacejam.com/1996/img/fastbreak.gif";
constexpr SkSize kCanvas = {1120, 560};
constexpr float kScale = 3.0f;  // sheet pixels per source pixel
/** The moments the lower shelf reads. The file's own loop is 600 ms, so
 *  these run across two of them and the frames come round again. */
constexpr double kSamples[] = {0, 150, 320, 480, 640, 900, 1150, 1420};

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.12f, 0.12f, 0.14f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = label(11, kInk, 0.4f),
          .note = label(10.5f, kAsh, 0.2f),
          .gap = 6};
}

/** One frame, drawn at kScale with the texels kept hard: this file is
 *  forty pixels across and a smooth resample would invent everything the
 *  sheet is about. */
Element cell(std::string key, sk_sp<SkImage> frame, float w, float h,
             const char* call, std::string note) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   custom(std::move(key),
                          [frame, w, h](SkCanvas& canvas, const PaintContext&) {
                            if (!frame) return;
                            SkPaint paint;
                            canvas.drawImageRect(
                                frame, SkRect::MakeWH(w, h),
                                SkSamplingOptions(SkFilterMode::kNearest),
                                &paint);
                          })
                       .width(w)
                       .height(h)
                       .fill(Fill::color(kCellGround)));
}

}  // namespace

struct GifFrames final : sketch::Sketch {
  /** WHAT THIS MACHINE MUST HAVE. The sheet is about one real file, and a
   *  cold cache would render a different picture under the same name. */
  static bool available(std::string* why) {
    return sketch::requireCached({kSource}, why);
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // every frame is on the sheet; nothing moves

    loader::Hub& hub = ctx.assets.hub();
    const std::optional<loader::ResourceInfo> probed = hub.probe(kSource);
    const std::shared_ptr<const image::ImageAsset> gif = hub.image(kSource);

    ctx.composer.render(gif ? sheet(*gif, probed) : missing());
  }

  /** The shelf of decoded frames, in file order, each with its own
   *  duration. */
  Element decoded(const image::ImageAsset& gif) const {
    const float w = (float)gif.width() * kScale;
    const float h = (float)gif.height() * kScale;
    kit::Cells shelf{.gap = 12};
    for (size_t i = 0; i < gif.frames().size(); ++i)
      shelf.cells.push_back(cell(
          "frame" + std::to_string(i), gif.frames()[i].image, w, h, "frames()",
          kit::format("%.0f ms", (double)gif.frames()[i].durationMs)));
    return kit::cells(std::move(shelf));
  }

  /** The shelf of PLAYBACK: one moment per cell, read back through the
   *  loop. */
  Element sampled(const image::ImageAsset& gif) const {
    const float w = (float)gif.width() * kScale;
    const float h = (float)gif.height() * kScale;
    kit::Cells shelf{.gap = 12};
    for (double at : kSamples)
      shelf.cells.push_back(cell("at" + std::to_string((int)at),
                                 gif.frameAt(at).image, w, h, "frameAt(ms)",
                                 kit::format("%.0f ms", at)));
    return kit::cells(std::move(shelf));
  }

  Element sheet(const image::ImageAsset& gif,
                const std::optional<loader::ResourceInfo>& probed) const {
    std::string foot = "Hub::probe() \xe2\x80\x94 ";
    if (probed)
      foot += probed->image.format + ", " + std::to_string(probed->byteSize) +
              " bytes, " + std::to_string(probed->image.width) + "\xc3\x97" +
              std::to_string(probed->image.height) + ", " +
              std::to_string(probed->image.frames) +
              " frames, no pixels "
              "decoded";
    else
      foot += "nothing (the hub could not sniff this resource)";
    foot += "   \xc2\xb7   decoded \xe2\x80\x94 " +
            std::to_string(gif.frames().size()) + " frames, " +
            kit::format("%.0f ms", (double)gif.totalDurationMs()) +
            " a loop, " +
            (gif.repetitionCount() == image::ImageAsset::kInfinite
                 ? std::string("repeating forever")
                 : std::to_string(gif.repetitionCount()) + " repetitions");

    return kit::sheet(
               {.title = toU8("ANIMATED FRAMES \xc2\xb7 ImageAsset::frames() "
                              "+ frameAt(ms)"),
                .subtitle =
                    toU8(std::string("dials \xc2\xb7 the file (") + kSource +
                         ") \xc2\xb7 the moments the lower "
                         "shelf reads"),
                .footer = toU8(foot),
                .titleStyle = label(14, kInk, 2.4f),
                .subtitleStyle = label(11, kAsh, 0.6f),
                .footerStyle = label(11, kAsh, 0.3f),
                .marginX = 24,
                .marginTop = 20,
                .marginBottom = 16,
                .ground = Fill::color(kGround),
                .rule = Fill::color(kRule)},
               kit::cells({.cells = {kit::cell(header(), toU8("DECODED"),
                                               toU8("every frame, composited "
                                                    "at decode \xe2\x80\x94 "
                                                    "drawing one never needs "
                                                    "the one before it"),
                                               decoded(gif)),
                                     kit::cell(header(), toU8("PLAYED"),
                                               toU8("frameAt looks the moment "
                                                    "up in the durations and "
                                                    "loops past the last one"),
                                               sampled(gif))},
                           .column = true,
                           .gap = 26,
                           .divider = Fill::color(kRule)}))
        .absolute()
        .inset(0);
  }

  /** The voice the two shelves are titled in — a heading over the run
   *  rather than a caption under a picture. */
  static kit::Caption header() {
    return {.where = kit::Caption::Where::Above,
            .label = label(11.5f, kInk, 2.0f),
            .note = label(10.5f, kAsh, 0.2f),
            .gap = 12,
            .noteGap = 5};
  }

  /** What stands here when the file decoded to nothing. The availability
   *  probe keeps a sweep away from this; the app opens whatever is
   *  selected, so it is drawn rather than left blank. */
  static Element missing() {
    return box()
        .absolute()
        .inset(0)
        .fill(Fill::color(kGround))
        .column()
        .gap(10)
        .padding(40)
        .child(text(toU8("no animated document here"), label(20, kInk)))
        .child(text(toU8(std::string(kSource) +
                         " did not decode: the hub reached neither the "
                         "network nor a cached copy of it"),
                    label(12, kAsh))
                   .width(Dim(620.0f)));
  }
};

SIGIL_SKETCH(GifFrames, "Kit \xc2\xb7 API",
             "every frame of a real GIF beside the moments frameAt reads "
             "them back at, with the probe that never decoded a pixel")
