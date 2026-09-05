/** @file
 * net_policy — when a hub may touch the network, and what it does when
 * it cannot.
 *
 * Three policies, and the difference between them is entirely about the
 * FIRST ask for a resource: an entry already loaded stays as it is.
 * `CacheFirst`, the default, serves a present cache file with no traffic
 * at all and fetches on a miss, which is what makes an offline run work
 * out of the box once a resource has been seen. `Refresh` asks the
 * network first to pick up upstream changes and FALLS BACK to the cached
 * copy when the fetch fails, so a flaky network degrades to CacheFirst
 * instead of failing. `Offline` never touches the network: a cache hit,
 * or nothing.
 *
 * The cache is a directory of files named by `networkCacheKey(url)` —
 * hex of the URL's hash plus the URL path's extension, so a decoder's
 * path hints keep working — and it is exposed precisely so a caller can
 * PRE-SEED it. That is what this sheet does: it encodes a picture,
 * writes it under the key one URL maps to, and then asks four hubs for
 * two URLs, one seeded and one not.
 *
 * So nothing here reaches the network, and the sheet is the same picture
 * on a machine with a connection and on one without: the host used is a
 * reserved name that cannot resolve, which is what makes the `Refresh`
 * cell a fetch that genuinely failed rather than one that was skipped.
 *
 * EDIT THESE FIRST
 *   kSeeded, kMissing — the two URLs, one of which is pre-seeded.
 *   kCacheDir — the directory the seed is written into.
 */

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
#include <sigilio/source/Sink.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace img = sigil::image;
namespace io = sigil::io;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 254;
constexpr float kPicture = 190;

// A reserved name that cannot resolve: every fetch here fails at once
// and none of them leaves the machine.
const char* kSeeded = "https://sigil.invalid/plate.png";
const char* kMissing = "https://sigil.invalid/absent.png";

constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};

/** What the seed holds — drawn here so the cell that serves it from the
 *  cache is showing bytes this file wrote and nothing else. */
sk_sp<SkData> seedBytes() {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(150, 100));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SkColor4f{0.11f, 0.13f, 0.17f, 1}.toSkColor());
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor4f(kFigure);
  for (int i = 0; i < 5; ++i)
    canvas->drawCircle(24.0f + (float)i * 26.0f,
                       50.0f + (i % 2 ? 18.0f : -18.0f), 11.0f, paint);
  return img::encodeImage(*surface->makeImageSnapshot(), img::Format::Png);
}

Element cell(const char* call, const char* note,
             const std::shared_ptr<const img::ImageAsset>& asset,
             const std::string& readout) {
  Element art = asset ? image(asset).width(Dim(150)).height(Dim(100))
                      : box().width(Dim(150)).height(Dim(100)).fill(
                            Fill::color({0.13f, 0.10f, 0.11f, 1}));
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well({.width = kCell, .height = kPicture, .padding = 12})
          .column()
          .gap(10)
          .child(std::move(art))
          .child(text(toU8(readout), sketch::kit::theme().mono(10, kFigure))));
}

}  // namespace

struct NetPolicy final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // every ask has already been answered
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // THE PRE-SEED: the cache is a directory of files named by the key a
    // URL maps to, and the key is exposed for exactly this.
    const std::filesystem::path cacheDir =
        std::filesystem::temp_directory_path() / "sigil-net-policy";
    std::filesystem::create_directories(cacheDir);
    const std::string key = io::networkCacheKey(kSeeded);
    if (sk_sp<SkData> bytes = seedBytes())
      io::writeBytes(cacheDir / key, bytes->data(), bytes->size());

    /** One hub, one policy, one ask — a hub of its own each time,
     *  because the policy governs the FIRST ask and an entry already
     *  loaded stays as it is. */
    const auto ask = [&](io::NetworkPolicy policy, const char* url) {
      io::Hub hub;
      hub.setNetworkCacheDir(cacheDir);
      hub.setNetworkPolicy(policy);
      return hub.image(url);
    };

    const auto cacheFirst = ask(io::NetworkPolicy::CacheFirst, kSeeded);
    const auto offlineHit = ask(io::NetworkPolicy::Offline, kSeeded);
    const auto offlineMiss = ask(io::NetworkPolicy::Offline, kMissing);
    const auto refresh = ask(io::NetworkPolicy::Refresh, kSeeded);

    const auto verdict = [](const char* name,
                            const std::shared_ptr<const img::ImageAsset>& a) {
      return kit::format(
          "%s \xc2\xb7 %s", name,
          a ? kit::format("served %d\xc3\x97%d", a->width(), a->height())
                  .c_str()
            : "null");
    };

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("THE NETWORK POLICIES \xc2\xb7 Hub::"
                       "setNetworkPolicy over a pre-seeded cache"),
         .subtitle = toU8("dials \xc2\xb7 the policy \xc2\xb7 which URL "
                          "is seeded \xc2\xb7 the cache directory "
                          "\xc2\xb7 the key a URL maps to, which is what "
                          "makes seeding possible at all"),
         .footer = toU8("the host is a reserved name that cannot "
                        "resolve, so nothing here leaves the machine "
                        "\xe2\x80\x94 which is what makes the Refresh "
                        "cell a fetch that genuinely failed and fell "
                        "back rather than one that was skipped")},
        kit::cells(
            {.cells = {cell("CacheFirst \xc2\xb7 seeded",
                            "the default \xc2\xb7 a present cache file is "
                            "served with no traffic at all, which is what "
                            "makes an offline run work once a resource has "
                            "been seen",
                            cacheFirst, verdict("CacheFirst", cacheFirst)),
                       cell("Offline \xc2\xb7 seeded",
                            "never touches the network \xc2\xb7 a cache hit "
                            "answers exactly as CacheFirst did, because "
                            "neither of them asked anything",
                            offlineHit, verdict("Offline", offlineHit)),
                       cell("Offline \xc2\xb7 not seeded",
                            "…and a miss is a miss \xc2\xb7 nothing is "
                            "fetched and nothing is invented, which is what "
                            "a hermetic run wants",
                            offlineMiss, verdict("Offline", offlineMiss)),
                       cell("Refresh \xc2\xb7 seeded",
                            "asks the network FIRST to pick up upstream "
                            "changes \xc2\xb7 the fetch failed here, and a "
                            "failed fetch falls back to the cached copy",
                            refresh, verdict("Refresh", refresh))},
             .gap = 14})));
  }
};

SIGIL_SKETCH(NetPolicy, "Kit \xc2\xb7 API",
             "one pre-seeded cache asked for under each network policy, and "
             "one miss, on a host name that cannot resolve so no cell "
             "leaves the machine")
