/** @file
 * Three resources before and after their files change under a Hub.
 * The matrix compares a held text value, caller-decoded Cloud and ImageAsset
 * with a second request after poll(). Existing shared views keep their
 * values. The registered decoder belongs to the cached view, so reloading
 * can repeat the decode without consumer-specific file watching.
 */

// TAGS: Runtime/Resources

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Pen.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Sink.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace img = sigil::image;
namespace io = sigil::io;
namespace draw = sigil::draw;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 700};

const char* kMount = "res://";
const char* kFirst = "the first state on disk";
const char* kSecond = "the second, after the write";

/** THE CALLER'S OWN TYPE — a run of points in a two-number-per-line
 *  text, which is exactly the shape of thing a hub has no opinion
 *  about. */
struct Cloud {
  std::vector<SkPoint> points;
};

std::optional<Cloud> parseCloud(const io::Bytes& bytes, std::string_view) {
  Cloud cloud;
  const std::string text(reinterpret_cast<const char*>(bytes.bytes.data()),
                         bytes.bytes.size());
  float x = 0, y = 0;
  size_t at = 0;
  while (at < text.size()) {
    if (std::sscanf(text.c_str() + at, "%f %f", &x, &y) == 2)
      cloud.points.push_back({x, y});
    const size_t next = text.find('\n', at);
    if (next == std::string::npos) break;
    at = next + 1;
  }
  return cloud;
}

/** A small picture with a stated number of bars, so the two states of
 *  the same file are told apart at a glance. */
sk_sp<SkData> chart(int bars, SkColor4f ink) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 80));
  draw::on(*surface->getCanvas(), {120, 80}, [bars, ink](draw::Pen& pen) {
    pen.background(SkColor4f{0.10f, 0.11f, 0.13f, 1});
    pen.noStroke();
    pen.fill(ink);
    for (int i = 0; i < bars; ++i)
      pen.rect(8.0f + (float)i * 14.0f, 70.0f - (float)(i + 1) * 7.0f, 10.0f,
               (float)(i + 1) * 7.0f + 2.0f);
  });
  return img::encodeImage(*surface->makeImageSnapshot(), img::Format::Png);
}

}  // namespace

struct HubReload {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // both readings have already been taken
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    const sketch::kit::Theme& look = sketch::kit::theme();

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "sigil-hub-reload";
    std::filesystem::create_directories(dir);
    const auto put = [&](const char* name, const std::string& text) {
      io::writeBytes(dir / name, text.data(), text.size());
    };

    // THE FIRST STATE on disk.
    put("notes.txt", kFirst);
    put("cloud.pts", "10 20\n40 64\n86 30\n120 78\n150 44\n");
    if (sk_sp<SkData> png = chart(3, look.palette.figure))
      io::writeBytes(dir / "chart.png", png->data(), png->size());

    io::Hub hub;
    hub.mount(kMount, dir);
    hub.registerDecoder<Cloud>(parseCloud);

    const std::string notesUri = std::string(kMount) + "notes.txt";
    const std::string cloudUri = std::string(kMount) + "cloud.pts";
    const std::string chartUri = std::string(kMount) + "chart.png";

    const std::optional<std::string> firstText = hub.text(notesUri);
    const std::shared_ptr<const Cloud> firstCloud = hub.load<Cloud>(cloudUri);
    const std::shared_ptr<const img::ImageAsset> firstChart =
        hub.image(chartUri);

    // …and the SECOND, written under the hub's feet.
    put("notes.txt", kSecond);
    put("cloud.pts", "16 70\n52 26\n96 62\n128 22\n158 68\n");
    if (sk_sp<SkData> png = chart(6, {0.46f, 0.74f, 0.94f, 1}))
      io::writeBytes(dir / "chart.png", png->data(), png->size());
    const bool moved = hub.poll();

    const std::optional<std::string> secondText = hub.text(notesUri);
    const std::shared_ptr<const Cloud> secondCloud = hub.load<Cloud>(cloudUri);
    const std::shared_ptr<const img::ImageAsset> secondChart =
        hub.image(chartUri);

    const auto snapshot =
        [&](const std::optional<std::string>& words,
            const std::shared_ptr<const Cloud>& cloud,
            const std::shared_ptr<const img::ImageAsset>& picture) {
          return box().column().gap(18).width(330).children(
              {sketch::kit::well({.width = 330, .height = 96, .padding = 20})
                   .children({text(words.value_or("No text"))
                                  .width(290)
                                  .font({.size = 18})}),
               sketch::kit::well({.width = 330,
                                  .height = 130,
                                  .content = sketch::kit::Well::Content{}},
                                 clouds(nullptr, cloud).width(180).height(100)),
               sketch::kit::well(
                   {.width = 330,
                    .height = 130,
                    .content = sketch::kit::Well::Content{}},
                   (picture ? image(picture) : box()).width(180).height(120))});
        };
    Element resourceIndex = box().column().gap(18).width(330).children(
        {box().height(96).column().gap(10).padding(0, 16).children(
             {document::label("TEXT"), text("notes.txt").styleClass("readout"),
              document::caption("hub.text(uri)")}),
         box().height(130).column().gap(10).padding(0, 16).children(
             {document::label("CALLER-DEFINED TYPE"),
              text("cloud.pts").styleClass("readout"),
              document::caption("registerDecoder<Cloud>\nload<Cloud>(uri)")}),
         box().height(130).column().gap(10).padding(0, 16).children(
             {document::label("IMAGE"), text("chart.png").styleClass("readout"),
              document::caption("hub.image(uri)")})});
    ctx.composer.render(sketch::kit::page(
        {.title = "A file changes. A held value does not.",
         .subtitle = "Mounted files are replaced, poll() invalidates their "
                     "views, and a second request gets the new values.",
         .footer =
             "The hub keeps each view's decoder. Consumers receive new shared "
             "values by asking again; existing holders remain valid."},
        box().column().gap(26).children(
            {sketch::kit::comparison(
                 {.cases = {{.title = "RESOURCE",
                             .control = "res:// → mounted folder",
                             .figure = std::move(resourceIndex)},
                            {.title = "HELD BEFORE THE WRITE",
                             .control = "first load",
                             .figure =
                                 snapshot(firstText, firstCloud, firstChart)},
                            {.title = "REQUESTED AFTER POLL",
                             .control = kit::formatted(
                                 "poll() = %s", moved ? "true" : "false"),
                             .figure = snapshot(secondText, secondCloud,
                                                secondChart)}},
                  .measure = 1020,
                  .gap = 15}),
             sketch::kit::readout(
                 {{"Before / after cloud",
                   kit::formatted(
                       "%zu / %zu points",
                       firstCloud ? firstCloud->points.size() : 0,
                       secondCloud ? secondCloud->points.size() : 0)},
                  {"Held image",
                   kit::formatted("%d × %d",
                                  firstChart ? firstChart->width() : 0,
                                  firstChart ? firstChart->height() : 0)},
                  {"Resolved file", hub.resolve(notesUri).filename().string()}},
                 {.measure = 480, .ruled = true})})));
  }

  /** A point snapshot at the same origin and scale as its comparison. */
  Element clouds(const std::shared_ptr<const Cloud>& before,
                 const std::shared_ptr<const Cloud>& after) {
    const std::vector<SkPoint> a =
        before ? before->points : std::vector<SkPoint>{};
    const std::vector<SkPoint> b =
        after ? after->points : std::vector<SkPoint>{};
    // A paint program runs after the describe scope has closed, so both
    // inks are read here and carried in by value.
    const SkColor4f ash = sketch::kit::theme().palette.ash;
    const SkColor4f figure = sketch::kit::theme().palette.figure;
    return pen("hub.clouds", [a, b, ash, figure](sigil::draw::Pen& pen) {
      pen.noStroke();
      const auto dots = [&](const std::vector<SkPoint>& points,
                            SkColor4f colour, float diameter) {
        pen.fill(colour);
        for (const SkPoint& point : points)
          pen.circle(point.fX, point.fY, diameter);
      };
      dots(a, ash, 14);
      dots(b, figure, 8);
    });
  }
};

SIGIL_SKETCH(HubReload, "Kit · API",
             "one mounted folder read as text, as a type this file taught "
             "the hub to decode, and as an image — each before "
             "and after the files changed under it and poll() ran")
