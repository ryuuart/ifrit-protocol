/** @file
 * hub_reload — a mounted folder, a resource decoded as a type of the
 * caller's own, and what `poll()` does when the file underneath changes.
 *
 * A hub maps URIs onto directories: `mount(prefix, dir)` sends every URI
 * starting with that prefix under it, longest matching prefix wins, and
 * re-mounting a prefix replaces it. Every ask is CACHED, and a failed
 * lookup is not: a missing file loads as soon as it appears.
 *
 * `registerDecoder<T>` says how a T is made out of bytes, and `load<T>`
 * answers through it. The decode a view was made with rides along with
 * the view, so `poll()` can re-run exactly it — which is what makes hot
 * reload a property of the hub rather than of each consumer.
 * `ImageAsset` and `ChannelData` are registered by the constructor, and
 * `image(uri)` is `load<ImageAsset>(uri)` sharing one view.
 *
 * `poll()` re-checks every previously loaded resource, reloads what
 * changed, drops entries whose files vanished, and answers whether
 * anything moved. Views already handed out keep their values — a
 * `shared_ptr` a caller is holding is not rewritten under it — so a
 * consumer picks the new value up by ASKING AGAIN, which this sheet does
 * beside the readings it took before.
 *
 * The files are written by this sketch into a directory of its own under
 * the system's temporary one, changed there, and asked for again, so the
 * whole reload happens inside one describe and the sheet shows both
 * readings.
 *
 * EDIT THESE FIRST
 *   kMount — the prefix the folder is mounted under.
 *   kFirst, kSecond — the two states the text file is written in.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilimage/encode/Encode.h>
#include <sigilloader/hub/Hub.h>
#include <sigilloader/source/Sink.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace img = sigil::image;
namespace loader = sigil::loader;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 206;
constexpr float kPicture = 176;

const char* kMount = "res://";
const char* kFirst = "the first state on disk";
const char* kSecond = "the second, after the write";

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

/** THE CALLER'S OWN TYPE — a run of points in a two-number-per-line
 *  text, which is exactly the shape of thing a hub has no opinion
 *  about. */
struct Cloud {
  std::vector<SkPoint> points;
};

std::optional<Cloud> parseCloud(const loader::Bytes& bytes, std::string_view) {
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
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SkColor4f{0.10f, 0.11f, 0.13f, 1}.toSkColor());
  SkPaint paint;
  paint.setColor4f(ink);
  for (int i = 0; i < bars; ++i)
    canvas->drawRect({8.0f + (float)i * 14.0f, 70.0f - (float)(i + 1) * 7.0f,
                      18.0f + (float)i * 14.0f, 72.0f},
                     paint);
  return img::encodeImage(*surface->makeImageSnapshot(), img::Format::Png);
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

struct HubReload final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // both readings have already been taken

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "sigil-hub-reload";
    std::filesystem::create_directories(dir);
    const auto put = [&](const char* name, const std::string& text) {
      loader::writeBytes(dir / name, text.data(), text.size());
    };

    // THE FIRST STATE on disk.
    put("notes.txt", kFirst);
    put("cloud.pts", "10 20\n40 64\n86 30\n120 78\n150 44\n");
    if (sk_sp<SkData> png = chart(3, kFigure))
      loader::writeBytes(dir / "chart.png", png->data(), png->size());

    loader::Hub hub;
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
      loader::writeBytes(dir / "chart.png", png->data(), png->size());
    const bool moved = hub.poll();

    const std::optional<std::string> secondText = hub.text(notesUri);
    const std::shared_ptr<const Cloud> secondCloud = hub.load<Cloud>(cloudUri);
    const std::shared_ptr<const img::ImageAsset> secondChart =
        hub.image(chartUri);

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("A MOUNTED FOLDER \xc2\xb7 Hub::mount, text, "
                           "registerDecoder / load, poll"),
             .subtitle = toU8("dials \xc2\xb7 the prefix the folder is "
                              "mounted under \xc2\xb7 the two states each "
                              "file is written in \xc2\xb7 what a T is "
                              "decoded from bytes by"),
             .footer = toU8("the decode a view was made with rides along "
                            "with the view, so poll() re-runs exactly it "
                            "\xe2\x80\x94 which is what makes hot reload a "
                            "property of the hub rather than of every "
                            "consumer of it"),
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
                     {cell("hub.text(\"res://notes.txt\")",
                           "the UTF-8 convenience over blob() \xc2\xb7 read "
                           "once before the file changed and once after, "
                           "with poll() between them",
                           lines({kit::format(
                                      "first  \xc2\xb7 %s",
                                      firstText ? firstText->c_str() : "-"),
                                  kit::format("poll() \xc2\xb7 %s",
                                              moved ? "true" : "false"),
                                  kit::format("second \xc2\xb7 %s",
                                              secondText ? secondText->c_str()
                                                         : "-")})),
                      cell("hub.load<Cloud>(\"res://cloud.pts\")",
                           "a type the hub has no opinion about, decoded by "
                           "a function this file registered \xc2\xb7 both "
                           "readings drawn over one another",
                           clouds(firstCloud, secondCloud)),
                      cell("hub.image(\"res://chart.png\")",
                           "the decoder the constructor registered \xc2\xb7 "
                           "image(uri) IS load<ImageAsset>(uri) and shares "
                           "one view of the entry",
                           charts(firstChart, secondChart)),
                      cell("what a reload costs a holder",
                           "nothing: a view already handed out keeps its "
                           "value, so the first reading is still the first "
                           "reading and the new one arrives by asking again",
                           lines({kit::format(
                                      "first  cloud \xc2\xb7 %zu points",
                                      firstCloud
                                          ? firstCloud->points
                                                .size()
                                          : 0),
                                  kit::format(
                                      "second cloud \xc2\xb7 %zu points",
                                      secondCloud ? secondCloud
                                                        ->points.size()
                                                  : 0),
                                  kit::format("first  chart \xc2\xb7 "
                                              "%d\xc3\x97%d",
                                              firstChart ? firstChart->width()
                                                         : 0,
                                              firstChart ? firstChart->height()
                                                         : 0),
                                  kit::format("mount  \xc2\xb7 %s", hub.resolve(notesUri)
                                                                        .filename()
                                                                        .string()
                                                                        .c_str())}))},
                 .gap = 14}))
            .absolute()
            .inset(0));
  }

  Element lines(std::vector<std::string> rows) {
    Element column = box().column().gap(8);
    for (const std::string& row : rows)
      column.child(text(toU8(row), mono(10, kFigure)).width(Dim(kCell - 20)));
    return column;
  }

  /** Both readings of the point file, the first in ash and the second in
   *  ink, so the reload is one picture. */
  Element clouds(const std::shared_ptr<const Cloud>& before,
                 const std::shared_ptr<const Cloud>& after) {
    const std::vector<SkPoint> a =
        before ? before->points : std::vector<SkPoint>{};
    const std::vector<SkPoint> b =
        after ? after->points : std::vector<SkPoint>{};
    return custom("hub.clouds",
                  [a, b](SkCanvas& canvas, const PaintContext&) {
                    SkPaint paint;
                    paint.setAntiAlias(true);
                    const auto draw = [&](const std::vector<SkPoint>& points,
                                          SkColor4f colour, float radius) {
                      paint.setColor4f(colour);
                      for (const SkPoint& p : points)
                        canvas.drawCircle(p.fX, p.fY, radius, paint);
                    };
                    draw(a, kAsh, 7);
                    draw(b, kFigure, 4);
                  })
        .absolute()
        .inset(0);
  }

  Element charts(const std::shared_ptr<const img::ImageAsset>& before,
                 const std::shared_ptr<const img::ImageAsset>& after) {
    Element column = box().column().gap(8);
    for (const std::shared_ptr<const img::ImageAsset>& asset : {before, after})
      column.child(asset ? image(asset).width(Dim(120)).height(Dim(80))
                         : box().width(Dim(120)).height(Dim(80)));
    return column;
  }
};

SIGIL_SKETCH(HubReload, "Kit \xc2\xb7 API",
             "one mounted folder read as text, as a type this file taught "
             "the hub to decode, and as an image \xe2\x80\x94 each before "
             "and after the files changed under it and poll() ran")
