// substance_swatches.cpp — a procedural material archive cooked, and its
// channels laid out as cards.
//
// A .sbsar is a graph with named parameters and named outputs, each
// output tagged with the material channel it feeds. This renders one at
// a fixed resolution and shows what came back: one card per channel,
// keyed by the usage the archive declared, which is the map a texture
// set is built from.
//
// The archive is the SDK's own sample rather than anything in this
// repository — the sample ships with the engine that renders it, so the
// sketch has nothing to carry and nothing to keep in step. That is also
// why it can be UNAVAILABLE on a machine whose SDK arrived without its
// assets: the probe says which file it looked for, and no plate is
// taken.
//
// EDIT THESE FIRST
//   kCookLog2 — the cook's resolution as a power of two, both axes. It
//               is the number the archive is rendered at, so it is also
//               what every card shows under its usage word.
//   kPerRow   — cards across, which sets the canvas: the sheet is sized
//               from the count rather than a size being chosen and the
//               cards fitted into it.

#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsubstance/Substance.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace substance = sigil::substance;

using namespace sigil::compose;

namespace {

/** The channels are cooked at this many pixels a side and shown at
 *  `kCard`, so the cards are a downsample of real output rather than a
 *  magnification of a thumbnail. */
constexpr int kCookLog2 = 8;
constexpr float kCard = 200;
constexpr float kGap = 16;
constexpr float kMargin = 40;
constexpr float kHeaderHeight = 104;
constexpr float kCaptionHeight = 46;
constexpr int kPerRow = 4;

constexpr SkColor4f kInk = hex(0xf0ece4);
constexpr SkColor4f kDim = hex(0xb4a894);

/** The archive's own look: warm ink on a cooled ground, and the card's
 *  two lines under its picture rather than around it. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.ink = kInk;
  look.palette.ash = kDim;
  look.type.title = {.size = 15, .track = 2.4f};
  look.type.subtitle = {.size = 12};
  look.type.captionLabel = {.size = 14};
  look.type.captionNote = {.size = 11.5f};
  look.captionWhere = kit::Caption::Where::Below;
  look.spacing.captionGap = 7;
  look.spacing.captionNoteGap = 7;
  look.spacing.cellGap = kGap;
  return look;
}

std::filesystem::path archive() {
  return std::filesystem::path(SIGIL_SUBSTANCE_SDK_DIR) / "assets" /
         "Autumn_Leaves.sbsar";
}

/** One cooked channel: the usage the archive tagged it with, and the
 *  image itself wrapped so the layout can draw it like any other. */
struct Swatch {
  std::string usage;
  int width = 0;
  int height = 0;
  std::shared_ptr<const sigil::image::ImageAsset> asset;
};

Element card(const Swatch& swatch) {
  const std::string size =
      kit::formatted("%d \xc3\x97 %d", swatch.width, swatch.height);
  return sketch::kit::caption(
             kCard, toU8(swatch.usage), toU8(size),
             image(swatch.asset)
                 .width(kCard)
                 .height(kCard)
                 .corners({10})
                 .clip()
                 .foreground(stroke(1.0f, Fill::color(hex(0xffffff, 0.16f)))))
      .width(kCard);
}

Element notice(std::u8string heading, const std::string& detail) {
  return box()
      .inset(kMargin, kMargin, kMargin, kMargin)
      .corners({16})
      .padding(28)
      .fill(Fill::color(hex(0x241c14, 0.9f)))
      .foreground(stroke(1.0f, Fill::color(hex(0xffb46b, 0.24f))))
      .column()
      .gap(10)
      .child(text(std::move(heading), weave::textStyle({.size = 22, .color = kInk})))
      .child(text(toU8(detail), weave::textStyle({.size = 13, .color = kDim})));
}

}  // namespace

struct SubstanceSwatchesSketch final : sketch::Sketch {
  /** WHAT THIS MACHINE MUST HAVE. The library is only built where the
   *  SDK is, and the SDK's sample archives are a separate part of that
   *  install — an SDK without them renders nothing, which is a piece
   *  this machine cannot show rather than a piece that is broken. */
  static bool available(std::string* why) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(archive(), ec)) return true;
    if (why)
      *why = "the Substance SDK's sample archive is not installed (" +
             archive().string() + ")";
    return false;
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.background(hex(0x140f0a));
    ctx.captureAt(0.5);

    std::string error;
    std::unique_ptr<substance::Package> package =
        substance::Package::load(archive(), &error);
    if (!package || package->graphCount() == 0) {
      ctx.canvas(940, 320);
      ctx.composer.render(
          stack()
              .fill(Fill::color(hex(0x140f0a)))
              .child(notice(u8"the archive did not load",
                            error.empty() ? archive().string() : error)));
      return;
    }

    substance::Graph& graph = package->graph(0);
    graph.setResolution(kCookLog2, kCookLog2);
    if (!graph.render()) {
      ctx.canvas(940, 320);
      ctx.composer.render(
          stack()
              .fill(Fill::color(hex(0x140f0a)))
              .child(notice(u8"the graph did not cook", archive().string())));
      return;
    }

    const sketch::kit::Provide look(sheetTheme());
    std::vector<Swatch> swatches;
    for (const auto& [usage, cooked] : graph.outputsByUsage()) {
      if (!cooked) continue;
      swatches.push_back({usage, cooked->width(), cooked->height(),
                          std::make_shared<const sigil::image::ImageAsset>(
                              sigil::image::ImageAsset::wrap(cooked))});
    }

    // The canvas follows the archive: a piece whose content is a cooked
    // package cannot declare a size before it knows how many channels
    // came back.
    const int rows = ((int)swatches.size() + kPerRow - 1) / kPerRow;
    const float cardHeight = kCard + 7 + 18 + 7 + 16;
    ctx.canvas(kMargin * 2 + kPerRow * kCard + (kPerRow - 1) * kGap,
               kHeaderHeight + (float)rows * cardHeight +
                   (float)(rows - 1) * kGap + kCaptionHeight);

    // THE ENGINE'S VERSION GOES TO stderr, NOT ONTO THE PLATE. It is a
    // number this machine's SDK decides, so a plate carrying it would
    // change under an SDK upgrade that changed no pixel anyone authored,
    // and a byte-identity sweep would report that as a mover.
    std::fprintf(stderr, "[substance] engine %s \xc2\xb7 %s\n",
                 substance::Package::engineVersion().c_str(),
                 graph.label().c_str());

    const std::string caption = kit::formatted(
        "%s \xc2\xb7 %zu parameters \xc2\xb7 %zu channels",
        graph.label().c_str(), graph.parameters().size(), swatches.size());

    std::vector<Element> cards;
    cards.reserve(swatches.size());
    for (const Swatch& swatch : swatches) cards.push_back(card(swatch));
    // The grid is as wide as its own cards, so a share IS a card: the
    // canvas above was sized from the same arithmetic.
    Element grid = box()
                       .left(kMargin)
                       .top(kHeaderHeight)
                       .width(kPerRow * kCard + (kPerRow - 1) * kGap)
                       .child(sketch::kit::panelGrid(
                           {.cells = std::move(cards), .columns = kPerRow}));

    ctx.composer.render(
        stack()
            .child(sketch::kit::backdrop(
                {.over = ctx.size,
                 .ground = linearGradient({0, 0}, {0, ctx.size.height()},
                                          {hex(0x1a120b), hex(0x0f0d10)})}))
            .child(sketch::kit::titleCard(
                       {.title = u8"A PROCEDURAL ARCHIVE, COOKED",
                        .subtitle = toU8(caption)})
                       .left(kMargin)
                       .top(34))
            .child(std::move(grid)));
  }
};

SIGIL_SKETCH(SubstanceSwatchesSketch, "Start & fixtures",
             "A .sbsar cooked through the Substance engine, one card per "
             "channel it declares")
