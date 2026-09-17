/** @file
 * Four independent first reads over a seeded and a missing cache entry.
 * The policy table shows the actual returned image or absence. CacheFirst
 * serves the seed directly; Offline serves only cache hits; Refresh attempts
 * a fetch and falls back to the seed. The reserved host makes that fetch
 * fail. Each row creates a fresh Hub so no in-memory view bypasses policy.
 */

// TAGS: Runtime/Resources

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Network.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace img = sigil::image;
namespace io = sigil::io;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 650};

// A reserved name that cannot resolve: every fetch here fails at once
// and none of them leaves the machine.
const char* kSeeded = "https://sigil.invalid/plate.png";
const char* kMissing = "https://sigil.invalid/absent.png";

/** What the seed holds — drawn here so the cell that serves it from the
 *  cache is showing bytes this file wrote and nothing else. */
sk_sp<SkData> seedBytes() {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(150, 100));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SkColor4f{0.11f, 0.13f, 0.17f, 1}.toSkColor());
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor4f(sketch::kit::theme().palette.figure);
  for (int i = 0; i < 5; ++i)
    canvas->drawCircle(24.0f + (float)i * 26.0f,
                       50.0f + (i % 2 ? 18.0f : -18.0f), 11.0f, paint);
  return img::encodeImage(*surface->makeImageSnapshot(), img::Format::Png);
}

Element decision(const char* policy, const char* state, const char* route,
                 const std::shared_ptr<const img::ImageAsset>& result) {
  return sketch::kit::well({.width = 728, .height = 94, .padding = 16})
      .row()
      .gap(20)
      .alignItems(Align::Center)
      .children(
          {box().column().gap(8).width(130).children(
               {text(policy).styleClass("captionLabel"),
                text(state).styleClass("captionNote")}),
           text(route).width(234),
           result
               ? image(result).width(90).height(60)
               : box()
                     .width(90)
                     .height(60)
                     .fill(Fill::color({0.18f, 0.10f, 0.12f, 1}))
                     .padding(8)
                     .children({text("NO\nIMAGE").styleClass("captionLabel")}),
           text(result ? kit::formatted("%d × %d\nserved", result->width(),
                                        result->height())
                       : "null\nreturned")
               .width(182)
               .styleClass("readout")});
}

}  // namespace

struct NetPolicy {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // every ask has already been answered
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // Seed the URL before any hub reads it.
    const std::filesystem::path cacheDir =
        std::filesystem::temp_directory_path() / "sigil-net-policy";
    if (sk_sp<SkData> bytes = seedBytes())
      io::seedNetworkCache(
          kSeeded,
          {static_cast<const std::byte*>(bytes->data()), bytes->size()},
          cacheDir);

    /** One hub, one policy, one ask — a hub of its own each time,
     *  because the policy governs the FIRST ask and an entry already
     *  loaded stays as it is. */
    const auto ask = [&](io::NetworkPolicy policy, const char* url) {
      io::Hub hub;
      hub.setNetworkCacheDirectory(cacheDir);
      hub.setNetworkPolicy(policy);
      return hub.image(url);
    };

    const auto cacheFirst = ask(io::NetworkPolicy::CacheFirst, kSeeded);
    const auto offlineHit = ask(io::NetworkPolicy::Offline, kSeeded);
    const auto offlineMiss = ask(io::NetworkPolicy::Offline, kMissing);
    const auto refresh = ask(io::NetworkPolicy::Refresh, kSeeded);

    ctx.composer.render(sketch::kit::page(
        {.title = "When may a resource use the network?",
         .subtitle = "The same seeded image, four independent first reads. A "
                     "reserved host makes every attempted fetch fail.",
         .footer = "Policies govern the first load. A resource already held by "
                   "a hub remains loaded until invalidated."},
        box().row().gap(32).children(
            {box().column().gap(16).width(260).children(
                 {sketch::kit::sectionHeader(
                      {.label = "THE CACHE INPUT",
                       .note = "Seeded before each read"}),
                  sketch::kit::well(
                      {.width = 260,
                       .height = 200,
                       .content = sketch::kit::Well::Content{}},
                      cacheFirst ? image(cacheFirst).width(225).height(150)
                                 : text("Seed unavailable")),
                  text("plate.png  ·  seeded\nabsent.png ·  missing")
                      .styleClass("readout"),
                  text("Each row creates a fresh hub. The policy and the "
                       "presence of a cache file are the only inputs that "
                       "change.")
                      .width(260)}),
             box().column().gap(16).width(728).children(
                 {sketch::kit::sectionHeader(
                      {.label = "POLICY / CACHE",
                       .note = "DECISION → OBSERVED RESULT"}),
                  decision("CacheFirst", "Seeded",
                           "Use the cached file without fetching.", cacheFirst),
                  decision("Offline", "Seeded",
                           "Only inspect the cache; the file is present.",
                           offlineHit),
                  decision("Offline", "Missing",
                           "Only inspect the cache; there is no fallback.",
                           offlineMiss),
                  decision("Refresh", "Seeded",
                           "Try the network, then use the cached file after "
                           "failure.",
                           refresh)})})));
  }
};

SIGIL_SKETCH(NetPolicy, "Kit · API",
             "one pre-seeded cache asked for under each network policy, and "
             "one miss, on a host name that cannot resolve so no cell "
             "leaves the machine")
