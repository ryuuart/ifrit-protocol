/** @file
 * Curved lettering at label and display sizes, with snapped and continuous
 * tangents under the same path and type. A magnified two-colour overlay
 * reveals their difference without changing the underlying glyphs.
 */

// TAGS: Typography/Lettering

#include <include/core/SkBitmap.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Curves.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 850};
constexpr float kCell = 328;
constexpr float kPicture = 190;

constexpr float kLabelSize = 15;    // where the ladder is invisible
constexpr float kDisplaySize = 74;  // display size, still on the ladder
constexpr float kDetailSize =
    260;                        // the raster's type size before magnification
constexpr float kTurns = 3.2f;  // the spiral's turns

constexpr SkColor4f kSnapped{0.95f, 0.44f, 0.32f, 1};
constexpr SkColor4f kExact{0.40f, 0.76f, 0.98f, 1};
constexpr SkColor4f kShared{0.76f, 0.76f, 0.76f, 1};
constexpr SkISize kRaster = {328, 420};
constexpr SkISize kCrop = {41, 20};
constexpr float kMagnification = 8;

/** THE INSCRIPTION'S VOICE, as a class beside the page's registers: the
 *  old-style serif, untracked — stated, because the page's running
 *  register is tracked and an inscription is not. A run states its size
 *  and colour over it. */
weave::StyleSheet voices() {
  weave::StyleSheet classes = sketch::kit::theme().styleSheet();
  classes.set("inscription",
              {.face = weave::ports::face(
                   {"Iowan Old Style", "Georgia", "Times New Roman", "serif"}),
               .track = 0.0f});
  return classes;
}

/** A run on the tight spiral. The baseline resolves against the TEXT
 *  node's own box, so the leaf carries the plate's dimensions. */
Element run(const char* word, float size, SkColor4f colour, bool exact,
            float inset = 16) {
  return text(word)
      .styleClass("inscription")
      .font({.size = size, .color = colour})
      .inset(inset)
      .onPath({.path = shapes::spiral(kTurns),
               .at = 0.42f,
               .align = TextPath::Align::Center,
               .exactTangent = exact});
}

/** A run on the inscribed oval — the large-size cells. `offset` rides
 *  the type inside the baseline so a big face stays on the plate. */
Element arcRun(const char* word, float size, SkColor4f colour, bool exact,
               float at = 0.75f, float offset = -65, float inset = 14) {
  return text(word)
      .styleClass("inscription")
      .font({.size = size, .color = colour})
      .inset(inset)
      .onPath({.path = shapes::circle(),
               .at = at,
               .align = TextPath::Align::Center,
               .offset = offset,
               .exactTangent = exact});
}

SkBitmap coverage(weave::FontContext& fonts, bool exact) {
  const auto raster =
      texture(
          box().styleSheet(voices()).children(
              {arcRun("R", kDetailSize, SkColors::kWhite, exact, 0.78f, -140, 4)
                   .translateY(110)}),
          kRaster, fonts)
          .image();
  SkBitmap mask;
  mask.allocN32Pixels(kRaster.width(), kRaster.height());
  if (!raster || !raster->readPixels(mask.pixmap(), 0, 0))
    throw std::runtime_error("Cannot read the tangent comparison's coverage");
  return mask;
}

/** Coverage is shared once, with only the excess assigned to either colour.
 *  This symmetric comparison has no upper layer that can hide the other.
 *  The crop follows the largest coverage difference so font fallback can
 *  move the edge. Both views sample this one raster: enlarging the text tree
 *  itself would change rasterisation and would not magnify the same pixels. */
Element tangentDetail(weave::FontContext& fonts) {
  const SkBitmap snapped = coverage(fonts, false);
  const SkBitmap exact = coverage(fonts, true);
  SkBitmap overlay;
  overlay.allocN32Pixels(kRaster.width(), kRaster.height());
  SkIRect ink = SkIRect::MakeEmpty();
  SkIPoint peak = {kRaster.width() / 2, kRaster.height() / 2};
  int largestDifference = 0;
  for (int y = 0; y < kRaster.height(); ++y) {
    for (int x = 0; x < kRaster.width(); ++x) {
      const int warm = SkColorGetA(snapped.getColor(x, y));
      const int cool = SkColorGetA(exact.getColor(x, y));
      const int shared = std::min(warm, cool);
      const int alpha = std::max(warm, cool);
      if (alpha) ink.join(SkIRect::MakeXYWH(x, y, 1, 1));
      const int difference = std::abs(warm - cool);
      if (difference > largestDifference) {
        largestDifference = difference;
        peak = {x, y};
      }
      const auto channel = [&](float neutral, float a, float b) {
        return alpha ? static_cast<unsigned>(
                           std::lround(255 *
                                       (shared * neutral + (warm - shared) * a +
                                        (cool - shared) * b) /
                                       alpha))
                     : 0u;
      };
      *overlay.getAddr32(x, y) =
          SkPreMultiplyARGB(alpha, channel(kShared.fR, kSnapped.fR, kExact.fR),
                            channel(kShared.fG, kSnapped.fG, kExact.fG),
                            channel(kShared.fB, kSnapped.fB, kExact.fB));
    }
  }
  overlay.setImmutable();
  const auto pixels = overlay.asImage();
  const SkRect crop =
      SkRect::MakeXYWH(std::clamp(peak.x() - kCrop.width() / 2, 0,
                                  kRaster.width() - kCrop.width()),
                       std::clamp(peak.y() - kCrop.height() / 2, 0,
                                  kRaster.height() - kCrop.height()),
                       kCrop.width(), kCrop.height());
  const SkColor4f marker = sketch::kit::theme().palette.ash;
  const SkRect overview =
      SkRect::Make(ink.isEmpty() ? SkIRect::MakeSize(kRaster) : ink);
  const float scale =
      std::min((kCell - 40) / overview.width(), 186 / overview.height());
  const SkPoint origin = {(kCell - overview.width() * scale) / 2,
                          (210 - overview.height() * scale) / 2};
  return box().column().gap(10).children(
      {box().width(kCell).height(210).children(
           {image(pixels, Fit::Stretch)
                .region(overview)
                .width(overview.width() * scale)
                .height(overview.height() * scale)
                .at(origin),
            box()
                .rect(SkRect::MakeXYWH(
                    origin.x() + (crop.x() - overview.x()) * scale,
                    origin.y() + (crop.y() - overview.y()) * scale,
                    crop.width() * scale, crop.height() * scale))
                .foreground(decorations::border(1, Fill::color(marker)))}),
       document::label("EDGE DETAIL \u00b7 8\u00d7 RASTER").padding(10, 0),
       image(pixels, Fit::Stretch)
           .region(crop)
           .sampling(SkSamplingOptions(SkFilterMode::kNearest))
           .width(kCrop.width() * kMagnification)
           .height(kCrop.height() * kMagnification)});
}

/** The plate every specimen on this sheet stands on, and the
 *  measure its caption is set to. */
const sketch::kit::Cell kSpecimen{
    .plate = {.width = kCell, .height = kPicture}};

}  // namespace

struct ExactTangent {
  void setup(sketch::SketchContext& ctx) {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    const SkColor4f figure = sketch::kit::theme().palette.figure;

    ctx.composer.render(
        sketch::kit::page(
            {.title = "How exact is a tangent?",
             .subtitle = "Matched type at two sizes, with a magnified overlay "
                         "showing where the two settings disagree.",
             .footer = "Exact tangents are useful for large static lettering; "
                       "the default bounds the number of rotation strikes."},
            box()
                .row()
                .alignItems(Align::Start)
                .gap(18)
                .children(
                    {box().column().gap(22).children(
                         {sketch::kit::sectionHeader(
                              {.label = "LABEL SIZE",
                               .note = "One spiral, the same text"}),
                          sketch::kit::comparison(
                              {.cases =
                                   {{.title = "SNAPPED",
                                     .control =
                                         "15 px \u00b7 exactTangent = false",
                                     .figure = sketch::kit::cell(
                                         kSpecimen, "", "",
                                         run("a tight spiral carries its whole "
                                             "run",
                                             kLabelSize, figure, false)),
                                     .note = "The default rotation ladder at "
                                             "label size."},
                                    {.title = "EXACT",
                                     .control =
                                         "15 px \u00b7 exactTangent = true",
                                     .figure = sketch::kit::cell(
                                         kSpecimen, "", "",
                                         run("a tight spiral carries its whole "
                                             "run",
                                             kLabelSize, figure, true)),
                                     .note = "The same line with continuous "
                                             "tangents."}},
                               .measure = 674,
                               .gap = 18}),
                          sketch::kit::sectionHeader(
                              {.label = "DISPLAY SIZE",
                               .note = "One oval, the same letters"}),
                          sketch::kit::comparison(
                              {.cases =
                                   {{.title = "SNAPPED",
                                     .control =
                                         "74 px \u00b7 exactTangent = false",
                                     .figure = sketch::kit::cell(
                                         kSpecimen, "", "",
                                         arcRun("Ravello", kDisplaySize, figure,
                                                false)),
                                     .note = "The default at display size."},
                                    {.title = "EXACT",
                                     .control =
                                         "74 px \u00b7 exactTangent = true",
                                     .figure = sketch::kit::cell(
                                         kSpecimen, "", "",
                                         arcRun("Ravello", kDisplaySize, figure,
                                                true)),
                                     .note = "The same letters on exact "
                                             "tangents."}},
                               .measure = 674,
                               .gap = 18})}),
                     box().column().gap(22).children(
                         {sketch::kit::sectionHeader(
                              {.label = "MAGNIFIED DIFFERENCE",
                               .note =
                                   "One raster, enlarged without reshaping"}),
                          sketch::kit::comparison(
                              {.cases =
                                   {{.title = "SUPERIMPOSED",
                                     .control = "260 px \u00b7 shared coverage "
                                                "in grey",
                                     .figure = sketch::kit::cell(
                                         sketch::kit::Cell{
                                             .plate = {.width = kCell,
                                                       .height = 420}},
                                         "", "", tangentDetail(*ctx.fonts)),
                                     .note =
                                         "Warm: extra snapped coverage. Cool: "
                                         "extra exact coverage. Grey: "
                                         "shared."}},
                               .measure = 328,
                               .gap = 18}),
                          document::caption(
                              "The rotation ladder is bounded between 64 and "
                              "2048 steps. Its spacing follows rendered type "
                              "size; exact tangents remove that quantisation.")
                              .width(328)})}))
            .styleSheet(voices()));
  }
};

SIGIL_SKETCH(ExactTangent, "Kit · API",
             "curved lettering with the rotation ladder on and lifted, at "
             "label size where it cannot be seen and at display size "
             "magnified until it can")
