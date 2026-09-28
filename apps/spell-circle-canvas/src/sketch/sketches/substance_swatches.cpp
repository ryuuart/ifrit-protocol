// substance_swatches.cpp — a procedural material archive cooked, and its
// channels laid out as cards.
//
// A .sbsar is a graph with named inputs and named outputs, each output
// tagged with the material channel it feeds. `material::substance()`
// cooks one into a Material; this asks it to cook every output at a fixed
// size and shows what came back: one card per channel, keyed by the usage
// the archive declared, which is how the material fills its surface.
//
// The archive is the SDK's own sample rather than anything in this
// repository — the sample ships with the engine that renders it and its
// licence is the SDK's, so the sketch has nothing to carry and nothing to
// keep in step. That is also
// why it can be UNAVAILABLE on a machine whose SDK arrived without its
// assets: the probe says which file it looked for, and no plate is
// taken.
//
// EDIT THESE FIRST
//   kCook     — the cook's resolution in pixels, a power of two. It is
//               the size the archive is cooked at, so it is also what
//               every card shows under its usage word.
//   kPerRow   — cards across, which sets the canvas: the sheet is sized
//               from the count rather than a size being chosen and the
//               cards fitted into it.

// TAGS: Materials/Shaders

#include <sigilmedia/advanced/Skia.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/substance/Substance.h>
#include <sigilmaterial/substance/advanced/Archive.h>
#include <sigilmedia/core/Image.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace sbsar = sigil::material::sbsar;

using namespace sigil::compose;
using sigil::material::hexColor;

namespace {

/** The channels are cooked at this many pixels a side and shown at
 *  `kCard`, so the cards are a downsample of real output rather than a
 *  magnification of a thumbnail. */
constexpr int kCook = 256;
constexpr float kCard = 200;
constexpr float kGap = 16;
constexpr float kMargin = 40;
constexpr float kHeaderHeight = 104;
constexpr float kCaptionHeight = 46;
constexpr int kPerRow = 4;
/** The grid is as wide as its own cards, so a share IS a card — which is
 *  what lets the canvas be sized from the count rather than the other way
 *  round. */
constexpr float kGridWidth = kPerRow * kCard + (kPerRow - 1) * kGap;

constexpr material::Color kInk = hexColor(0xf0ece4);
constexpr material::Color kDim = hexColor(0xb4a894);

/** The archive's own look: warm ink on a cooled ground, and the card's
 *  two lines under its picture rather than around it. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious);
  look.captionWhere = kit::Caption::Where::Below;
  look.spacing.captionGap = 7;
  look.spacing.captionNoteGap = 7;
  look.spacing.cellGap = kGap;
  return look;
}

std::string archive() {
  return (std::filesystem::path(SIGIL_SUBSTANCE_SDK_DIR) / "assets" /
          "Autumn_Leaves.sbsar")
      .string();
}

/** One cooked channel: the usage the archive tagged it with, and the
 *  image itself wrapped so the layout can draw it like any other. */
struct Swatch {
  std::string usage;
  int width = 0;
  int height = 0;
  std::shared_ptr<const sigil::media::Image> asset;
};

Element card(const Swatch& swatch) {
  const std::string size =
      kit::formatted("%d × %d", swatch.width, swatch.height);
  return sketch::kit::caption(
             kCard, swatch.usage, size,
             image(swatch.asset)
                 .width(kCard)
                 .height(kCard)
                 .borderRadius({10})
                 .overflow(Overflow::Clip)
                 .foreground(
                     stroke(1.0f, Fill::color(hexColor(0xffffff, 0.16f)))))
      .width(kCard);
}

Element notice(Utf8 heading, Utf8 detail) {
  return box()
      .inset(kMargin)
      .borderRadius({16})
      .padding(28)
      .fill(Fill::color(hexColor(0x241c14, 0.9f)))
      .foreground(stroke(1.0f, Fill::color(hexColor(0xffb46b, 0.24f))))
      .column()
      .gap(10)
      .children({text(std::move(heading),
                      weave::textStyle({.size = 22, .color = kInk})),
                 text(std::move(detail),
                      weave::textStyle({.size = 13, .color = kDim}))});
}

}  // namespace

struct SubstanceSwatchesSketch {
  /** WHAT THIS MACHINE MUST HAVE. The feature cooks only where the
   *  SDK is, and the SDK's sample archives are a separate part of that
   *  install — an SDK without them renders nothing, which is a piece
   *  this machine cannot show rather than a piece that is broken. */
  static bool available(std::string* why) {
    if (!sbsar::available()) {
      if (why) *why = "this build has no Substance SDK";
      return false;
    }
    std::error_code ec;
    if (std::filesystem::is_regular_file(archive(), ec)) return true;
    if (why)
      *why = "the Substance SDK's sample archive is not installed (" +
             archive() + ")";
    return false;
  }

  /** A PIECE THIS MACHINE CANNOT SHOW: the plate is the reason, on a
   *  canvas sized for one notice rather than for cards that never came. */
  void refuse(sketch::SketchContext& ctx, Utf8 heading, Utf8 detail) {
    ctx.canvas(940, 320);
    ctx.composer.render(
        stack()
            .fill(Fill::color(hexColor(0x140f0a)))
            .children({notice(std::move(heading), std::move(detail))}));
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.background(sketch::kit::featureTheme().palette.ground);
    ctx.captureAt(0.5);

    sigil::io::Hub& hub = ctx.assets.hub();
    const sbsar::Description graph = sbsar::describe(hub, archive());
    if (graph.outputs.empty()) {
      refuse(ctx, u8"the archive did not load", archive());
      return;
    }
    // Every picture the graph declares, not only the ones the material
    // reads, so each channel gets its card.
    std::vector<sbsar::OutputRequest> every;
    for (const sbsar::Output& output : graph.outputs)
      if (output.image) every.push_back({output.usage});
    const material::Material leaves = material::substance(
        hub, archive(), {.resolution = kCook, .outputs = every});

    const sketch::kit::Provide look(sheetTheme());
    std::vector<Swatch> swatches;
    for (const sbsar::OutputRequest& output : every) {
      const sk_sp<SkImage> cooked =
          sbsar::output(leaves, output.usage).frameAt({}).image;
      if (!cooked) continue;
      swatches.push_back({output.usage, cooked->width(), cooked->height(),
                          (sigil::media::Image::of(cooked))});
    }
    if (swatches.empty()) {
      refuse(ctx, u8"the graph did not cook", archive());
      return;
    }

    // The canvas follows the archive: a piece whose content is a cooked
    // package cannot declare a size before it knows how many channels
    // came back.
    // Not arrange::moduleSize: this measures the CONTAINER back from a
    // fixed module and its gaps, which is that function run backwards.
    const int rows = ((int)swatches.size() + kPerRow - 1) / kPerRow;
    const float cardHeight = kCard + 7 + 18 + 7 + 16;
    ctx.canvas(kMargin * 2 + kGridWidth,
               kHeaderHeight + (float)rows * cardHeight +
                   (float)(rows - 1) * kGap + kCaptionHeight);

    // THE ENGINE'S VERSION GOES TO stderr, NOT ONTO THE PLATE. It is a
    // number this machine's SDK decides, so a plate carrying it would
    // change under an SDK upgrade that changed no pixel anyone authored,
    // and a byte-identity sweep would report that as a mover.
    std::fprintf(stderr, "[substance] engine %s · %s\n",
                 sbsar::engineVersion().c_str(), graph.graph.c_str());

    const std::string caption = kit::formatted(
        "%s · %zu inputs · %zu channels", graph.graph.c_str(),
        graph.inputs.size(), swatches.size());

    Element grid =
        box()
            .left(kMargin)
            .top(kHeaderHeight)
            .width(kGridWidth)
            .children({sketch::kit::panelGrid(
                {.cells = each(swatches, card), .columns = kPerRow})});

    // No page stands here, so the root states the theme's registers.
    ctx.composer.render(
        stack()
            .applyStyleSheet(sketch::kit::theme().styleSheet())
            .children({sketch::kit::backdrop(
                           {.ground = Fill::color(
                                sketch::kit::theme().palette.ground)}),
                       sketch::kit::titleCard(
                           {.title = {u8"A procedural archive, cooked"},
                            .subtitle = {caption}})
                           .left(kMargin)
                           .top(34),
                       std::move(grid)}));
  }
};

SIGIL_SKETCH(SubstanceSwatchesSketch, "Start & fixtures",
             "A .sbsar cooked through the Substance engine, one card per "
             "channel it declares")
