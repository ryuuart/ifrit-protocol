/** @file
 * resource_hub — one `io::Hub` asked for resources two ways: files in a
 * mounted folder before and after they change, and an image under each
 * network policy.
 *
 * RELOAD AFTER POLL. A folder is mounted at `res://` and read three ways:
 * as text, as a type this file teaches the hub to decode (`Cloud`, a run
 * of points), and as an image. The files are then rewritten under the
 * hub's feet and `poll()` invalidates their views; a second request gets
 * the new values while a holder of the first keeps what it holds. The
 * registered decoder belongs to the cached view, so a reload repeats the
 * decode without any consumer watching a file.
 *
 * NETWORK POLICY. One image is seeded into a network cache, and four
 * hubs — a fresh one per ask, because a policy governs the FIRST load
 * and an entry already held stays as it is — each ask once. CacheFirst
 * serves the seed without fetching; Offline serves only what the cache
 * holds, so the missing entry answers nothing; Refresh tries the network
 * and falls back to the seed. The host is a reserved name that cannot
 * resolve, so every fetch fails at once and none leaves the machine.
 *
 * The fixtures — the two charts and the seed — are pen programs drawn
 * here and encoded, so every picture on the sheet is bytes this file
 * wrote.
 *
 * EDIT THESE FIRST
 *   kMount — the scheme the folder is mounted at.
 *   kSeeded, kMissing — the two addresses the policies are asked for.
 */

// TAGS: Runtime/Resources

#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Pen.h>
#include <sigilio/advanced/Decoding.h>
#include <sigilio/advanced/Network.h>
#include <sigilio/advanced/Places.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Network.h>
#include <sigilio/source/Sink.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmedia/advanced/Resource.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/Image.h>
#include <sigilmedia/image/Encode.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace media = sigil::media;
namespace io = sigil::io;
namespace draw = sigil::draw;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 930};
constexpr float kMeasure = 1020;

const char* kMount = "res://";
// A reserved name that cannot resolve: every fetch here fails at once.
const char* kSeeded = "https://sigil.invalid/plate.png";
const char* kMissing = "https://sigil.invalid/absent.png";

/** THE CALLER'S OWN TYPE — a run of points in a two-number-per-line
 *  text, which is exactly the shape of thing a hub has no opinion
 *  about. */
struct Cloud {
  std::vector<SkPoint> points;
};

/** The name the hub registers and asks for a cloud under. */
std::string_view meaningName(std::type_identity<Cloud>) {
  return "resource_hub.Cloud";
}

std::optional<Cloud> parseCloud(const io::Bytes& bytes, std::string_view) {
  Cloud cloud;
  const std::string text(reinterpret_cast<const char*>(bytes.data()),
                         bytes.size());
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

/** A FIXTURE: @p program drawn on a @p size raster and encoded as PNG. */
std::vector<std::byte> fixture(SkSize size,
                               const std::function<void(draw::Pen&)>& program) {
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul((int)size.width(), (int)size.height()));
  if (!surface) return {};
  draw::on(*surface->getCanvas(), size, program);
  return media::encode(surface->makeImageSnapshot(), media::Format::Png);
}

/** A chart of @p bars bars, so the two states of one file are told apart
 *  at a glance. */
std::vector<std::byte> chart(int bars, material::Color ink) {
  return fixture({120, 80}, [bars, ink](draw::Pen& pen) {
    pen.background(material::Color{0.10f, 0.11f, 0.13f, 1});
    pen.noStroke();
    pen.fill(ink);
    for (int i = 0; i < bars; ++i)
      pen.rect(8.0f + (float)i * 14.0f, 70.0f - (float)(i + 1) * 7.0f, 10.0f,
               (float)(i + 1) * 7.0f + 2.0f);
  });
}

/** The seed the network cache holds: five discs on a dark plate. */
std::vector<std::byte> seed(material::Color ink) {
  return fixture({150, 100}, [ink](draw::Pen& pen) {
    pen.background(material::Color{0.11f, 0.13f, 0.17f, 1});
    pen.noStroke();
    pen.fill(ink);
    for (int i = 0; i < 5; ++i)
      pen.circle(24.0f + (float)i * 26.0f, 50.0f + (i % 2 ? 18.0f : -18.0f),
                 22.0f);
  });
}

/** A point snapshot at the cloud's own origin and scale. */
Element clouds(const std::shared_ptr<const Cloud>& cloud) {
  const std::vector<SkPoint> points =
      cloud ? cloud->points : std::vector<SkPoint>{};
  return pen("hub.clouds",
             [points](draw::Pen& pen, const PaintContext& context) {
               pen.noStroke();
               pen.fill(resolveColor(var("palette-figure"), context));
               for (const SkPoint& point : points)
                 pen.circle(point.fX, point.fY, 8);
             });
}

/** What a request came back with: the image, or the absence of one. */
Element answered(const std::shared_ptr<const media::Image>& result) {
  return result ? Element(image(result).width(150).height(100))
                : Element(box()
                              .width(150)
                              .height(100)
                              .padding(10)
                              .fill(Fill::color({0.18f, 0.10f, 0.12f, 1}))
                              .children({document::label("NO IMAGE")}));
}

}  // namespace

struct ResourceHub {
  void setup(sketch::Scene2DContext& ctx) {
    // every request has already been answered
    sketch::kit::stage(
        ctx,
        {.theme = sketch::kit::featureTheme(sketch::kit::Density::Spacious),
         .size = kCanvas,
         .captureAt = 0.05});
    const sketch::kit::Theme& look =
        sketch::kit::featureTheme(sketch::kit::Density::Spacious);

    // RELOAD AFTER POLL — the first state on disk, read, then the second.
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "sigil-resource-hub";
    std::filesystem::create_directories(dir);
    const auto put = [&](const char* name, std::string_view bytes) {
      io::writeBytes(dir / name, bytes.data(), bytes.size());
    };
    const auto putImage = [&](const char* name,
                              const std::vector<std::byte>& png) {
      if (!png.empty()) io::writeBytes(dir / name, png.data(), png.size());
    };
    put("notes.txt", "the first state on disk");
    put("cloud.pts", "10 20\n40 64\n86 30\n120 78\n150 44\n");
    putImage("chart.png", chart(3, look.palette.figure));

    io::Hub hub;
    io::mount(hub, kMount, dir);
    media::registerDecoders(hub);
    io::registerDecoder<Cloud>(hub, parseCloud);
    const std::string notesUri = std::string(kMount) + "notes.txt";
    const std::string cloudUri = std::string(kMount) + "cloud.pts";
    const std::string chartUri = std::string(kMount) + "chart.png";

    const std::optional<std::string> firstText = hub.text(notesUri);
    const auto firstCloud = hub.load<Cloud>(cloudUri);
    const auto firstChart = hub.load<media::Image>(chartUri);

    put("notes.txt", "the second, after the write");
    put("cloud.pts", "16 70\n52 26\n96 62\n128 22\n158 68\n");
    putImage("chart.png", chart(6, {0.46f, 0.74f, 0.94f, 1}));
    const bool moved = io::poll(hub);

    const std::optional<std::string> secondText = hub.text(notesUri);
    const auto secondCloud = hub.load<Cloud>(cloudUri);
    const auto secondChart = hub.load<media::Image>(chartUri);

    // NETWORK POLICY — the seed is in the cache before any hub reads it.
    const std::filesystem::path cacheDirectory =
        std::filesystem::temp_directory_path() / "sigil-resource-hub-network";
    if (auto bytes = seed(look.palette.figure); !bytes.empty())
      io::NetworkCache(cacheDirectory).put(kSeeded, bytes);
    const auto ask = [&](io::NetworkPolicy policy, const char* url) {
      io::Hub fresh(
          {.network = {.policy = policy, .cacheDirectory = cacheDirectory}});
      media::registerDecoders(fresh);
      return fresh.load<media::Image>(url);
    };

    const float third = (kMeasure - 30) / 3, quarter = (kMeasure - 45) / 4;
    const auto row = [&](float height, Element body) {
      return sketch::kit::well({.width = third,
                                .height = height,
                                .content = sketch::kit::Well::Content{}},
                               std::move(body));
    };
    const auto snapshot =
        [&](const std::optional<std::string>& words,
            const std::shared_ptr<const Cloud>& cloud,
            const std::shared_ptr<const media::Image>& chart) {
          return box().column().gap(14).children(
              {row(80, text(words.value_or("No text"))
                           .width(third - 40)
                           .font({.size = 18})),
               row(110, clouds(cloud).width(180).height(100)),
               row(110,
                   (chart ? image(chart) : box()).width(150).height(100))});
        };
    const auto named = [&](float height, const char* role, const char* file,
                           const char* call) {
      return box().height(height).column().gap(8).padding(14, 0).children(
          {document::label(role), text(file).styleClass("readout"),
           document::caption(call)});
    };
    const auto policy = [&](const char* name, const char* cache,
                            const char* route, io::NetworkPolicy which,
                            const char* url) {
      const std::shared_ptr<const media::Image> result = ask(which, url);
      return sketch::kit::ComparisonCase{
          .title = name,
          .control = cache,
          .figure = sketch::kit::well({.width = quarter,
                                       .height = 130,
                                       .content = sketch::kit::Well::Content{}},
                                      answered(result)),
          .note = std::string(route) +
                  (result ? kit::formatted(" · %d × %d served",
                                           result->size().x, result->size().y)
                          : std::string(" · null returned"))};
    };

    ctx.composer.render(sketch::kit::page(
        {.title = "One hub, asked for resources",
         .subtitle = "Files that change under a mounted folder, then an "
                     "image under each network policy",
         .footer = "The hub keeps each view's decoder. Consumers receive new "
                   "shared values by asking again; existing holders remain "
                   "valid."},
        box().column().gap(30).children(
            {sketch::kit::section(
                 {.label = "RELOAD AFTER POLL",
                  .note = "res:// → a mounted folder; the files are rewritten "
                          "and poll() invalidates their views"},
                 sketch::kit::comparison(
                     {.cases =
                          {{.title = "RESOURCE",
                            .control = "three ways to read a file",
                            .figure =
                                box().column().gap(14).width(third).children(
                                    {named(80, "TEXT", "notes.txt",
                                           "hub.text(uri)"),
                                     named(110, "CALLER-DEFINED TYPE",
                                           "cloud.pts",
                                           "registerDecoder<"
                                           "Cloud>\nload<Cloud>"
                                           "(uri)"),
                                     named(110, "IMAGE", "chart.png",
                                           "hub.load<Image>"
                                           "(uri)")})},
                           {.title = "HELD BEFORE THE WRITE",
                            .control = "first load",
                            .figure =
                                snapshot(firstText, firstCloud, firstChart)},
                           {.title = "REQUESTED AFTER POLL",
                            .control = kit::formatted("poll() = %s",
                                                      moved ? "true" : "false"),
                            .figure = snapshot(secondText, secondCloud,
                                               secondChart)}},
                      .measure = kMeasure,
                      .gap = 15})),
             sketch::kit::section(
                 {.label = "NETWORK POLICY",
                  .note = "one seeded entry and one missing, each asked once "
                          "of a fresh hub on a host that cannot resolve"},
                 sketch::kit::comparison(
                     {.cases = {policy("CacheFirst", "seeded",
                                       "the cached file, without fetching",
                                       io::NetworkPolicy::CacheFirst, kSeeded),
                                policy("Offline", "seeded",
                                       "only the cache; the file is there",
                                       io::NetworkPolicy::Offline, kSeeded),
                                policy("Offline", "missing",
                                       "only the cache; no fallback",
                                       io::NetworkPolicy::Offline, kMissing),
                                policy("Refresh", "seeded",
                                       "the network fails, then the cache",
                                       io::NetworkPolicy::Refresh, kSeeded)},
                      .measure = kMeasure,
                      .gap = 15}))})));
  }
};

SIGIL_SKETCH(ResourceHub, "Kit · API",
             "one hub asked for resources — a mounted folder read as text, "
             "as a type this file taught it to decode and as an image, "
             "before and after the files changed and poll() ran, then one "
             "seeded image under each network policy on a host that cannot "
             "resolve")
