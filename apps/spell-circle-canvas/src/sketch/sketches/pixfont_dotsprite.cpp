/** @file
 * A binary glyph bake, its integer presentation and its reuse.
 * The first comparison isolates bake size, nearest-neighbour enlargement
 * and an offset shadow pass. The second shows a changing number blitted from
 * a baked font and one white dot tinted for several stamps. The aliased
 * source already has binary coverage; font size determines its counters.
 */

// TAGS: Typography/Lettering, Media/Images

#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/PixelType.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <string>
#include <utility>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 840};
constexpr float kCell = 324;
constexpr float kPicture = 190;

constexpr float kBakeSizes[3] = {9, 12, 16};      // the sweep in the first cell
constexpr float kScale = 3;                       // integer, always
constexpr material::Color kOn{0.62f, 0.98f, 0.72f, 1};  // what a mask is tinted

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.cellGround = {0.055f, 0.065f, 0.06f, 1};
  return look;
}

/** The face the bake reads: aliased, so the coverage handed back is
 *  already 0 or 1 and the threshold has nothing left to decide. The
 *  96-cell font is baked from a PROPORTIONAL face on purpose — a `1` is
 *  narrower than a `0` there, which is the whole of trap 3. */
weave::TextStyle bakeFace(float size, bool proportional = false) {
  const sk_sp<SkTypeface> code =
      weave::ports::face({"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  const sk_sp<SkTypeface> text = weave::ports::face(
      {"Helvetica Neue", "Helvetica", "Arial", "sans-serif"});
  return weave::textStyle(
      {.face = proportional ? text : code,
       .size = size,
       .color = material::skia::toSkColor(material::Color{1, 1, 1, 1}),
       .aliased = true});
}

sketch::kit::ComparisonCase example(const char* title, const char* control,
                                    const char* note, Element body,
                                    float width = kCell) {
  return {.title = title,
          .control = control,
          .figure = sketch::kit::well({.width = width, .height = kPicture})
                        .children({std::move(body).absolute().inset(20)}),
          .note = note};
}

}  // namespace

struct PixFontDotSprite {
  kit::PixFont font;
  kit::Mask sweep[3];
  sk_sp<SkImage> dot;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas});
    // The readout is live, so the sheet is complete only once the number
    // has run far enough to have moved every column of it.
    ctx.captureAt(1.28);

    for (int i = 0; i < 3; ++i)
      sweep[i] = kit::bakeRun(u8"3.eg", *ctx.fonts, bakeFace(kBakeSizes[i]));
    font = kit::bakeFont(*ctx.fonts, bakeFace(kBakeSizes[2], true));
    dot = kit::dotSprite(32);

    ctx.composer.render(sketch::kit::page(
        {.title = "Bake once. Place on the pixel grid.",
         .subtitle = "A binary glyph mask, an integer enlargement and two ways "
                     "to reuse a baked image.",
         .footer =
             "Aliased shaping already produces binary coverage. Font size "
             "chooses the pixels; threshold cannot recover a closed counter."},
        box().column().gap(26).children(
            {sketch::kit::sectionHeader({.label = "01  FROM OUTLINE TO MASK"}),
             sketch::kit::comparison(
                 {.cases = {sizeSweep(), presented(), shadowed()},
                  .measure = 1020,
                  .gap = 24}),
             sketch::kit::sectionHeader({.label = "02  REUSE THE BAKE"}),
             sketch::kit::comparison({.cases = {readout(), stamp()},
                                      .measure = 1020,
                                      .gap = 28})})));
  }

  /** Trap 2: the size is the control. One run baked at three sizes and
   *  presented at one scale, so the only difference on screen is which
   *  pixel centres the outline happened to contain. */
  sketch::kit::ComparisonCase sizeSweep() {
    // One row per bake: the size it was baked at, and the run.
    const auto row = [this](float size, std::size_t i) {
      return box()
          .row()
          .gap(10)
          .alignItems(Align::Center)
          .children({text(kit::formatted("%2.0f", size))
                         .font({.face = sketch::kit::theme().type.mono,
                                .size = 12,
                                .track = 0})
                         .ink(sketch::kit::theme().palette.ash),
                     kit::masked(sweep[i], {.colour = kOn, .scale = 2})});
    };
    return example("CHOOSE THE BAKE SIZE", "9 / 12 / 16 px · shown at 2×",
                   "one run, three bake sizes, one present scale · at the "
                   "smallest the counters hold no pixel centre and close",
                   box().column().gap(12).children({each(kBakeSizes, row)}));
  }

  /** Trap 4: an integer scale with nearest sampling, beside the 1×
   *  bake it came from. */
  sketch::kit::ComparisonCase presented() {
    return example("KEEP THE PIXELS SHARP", "masked · scale 1 and 3",
                   "the same 1-bit mask at 1× and at 3× · "
                   "nearest sampling, so a bake stays a bake",
                   box()
                       .column()
                       .gap(18)
                       .alignItems(Align::Start)
                       .children({kit::masked(sweep[2], {.colour = kOn}),
                                  kit::masked(sweep[2], {.colour = kOn,
                                                         .scale = kScale})}));
  }

  /** The shadow pass: a second blit underneath at an offset in
   *  DESTINATION px, with the colour's RGB multiplied down. */
  sketch::kit::ComparisonCase shadowed() {
    return example("ADD A SECOND PASS", "shadowOffset = {3, 3}",
                   "one extra pass under the mask, offset in destination px "
                   "and multiplied by a quarter · one bake, two draws",
                   box().column().gap(20).children(
                       {kit::masked(sweep[2], {.colour = kOn, .scale = kScale}),
                        kit::masked(sweep[2], {.colour = kOn,
                                               .scale = kScale,
                                               .shadowOffset = {3, 3},
                                               .shadowMultiplier = 0.25f})}));
  }

  /** Trap 3, and the whole reason the 96-cell bake exists: a number that
   *  changes every frame, drawn by a pen walk inside one custom() leaf
   *  with nothing re-described. */
  sketch::kit::ComparisonCase readout() {
    const kit::PixFont* f = &font;
    return example(
        "A LIVE NUMBER", "bakeFont → blit · track 1 and 5",
        "Digits, x-height and a descender share one baseline.\n"
        "The live readout uses baked glyphs at two tracking widths.",
        custom("pixfont.readout",
               [f](SkCanvas& canvas, const PaintContext& pc) {
                 const double t = pc.elapsedSeconds;
                 // A cell is cropped to its ink and carries where
                 // that ink sits inside the shared line box, so a
                 // run mixing figures, an x-height and a descender
                 // stands on one baseline.
                 const std::string run =
                     kit::formatted("%04.0fpx", 1100.0 + t * 111.0);
                 canvas.save();
                 canvas.scale(2, 2);
                 kit::blit(canvas, *f, {0, 0}, run, kOn,
                           {.track = 1, .tabularDigits = true, .snap = 1});
                 kit::blit(canvas, *f, {0, (float)f->lineHeight + 8}, run, kOn,
                           {.track = 5, .tabularDigits = true, .snap = 1});
                 canvas.restore();
               })
            .cover()
            .cache(Cache::None),
        496);
  }

  /** The stamp: white on transparency, so the tint is the caller's. */
  sketch::kit::ComparisonCase stamp() {
    sk_sp<SkImage> image = dot;
    return example(
        "ONE WHITE STAMP, MANY TINTS", "dotSprite(32)",
        "One antialiased white disc, baked once and tinted per point.\n"
        "The transparent ring prevents square edges.",
        custom("pixfont.dot",
               [image](SkCanvas& canvas, const PaintContext& pc) {
                 static constexpr material::Color kTints[3] = {
                     {1, 1, 1, 1}, kOn, {1.0f, 0.55f, 0.35f, 1}};
                 const float side = std::min(90.0f, pc.size.width() / 3.4f);
                 // A white stamp is TINTED by modulating it —
                 // setting a paint colour does nothing to a colour
                 // image, and this is the step a point sink takes
                 // per point.
                 SkPaint paint;
                 for (int i = 0; i < 3; ++i) {
                   paint.setColorFilter(SkColorFilters::Blend(
                       material::skia::toSkColor(kTints[i]), nullptr,
                       SkBlendMode::kModulate));
                   canvas.drawImageRect(
                       image, SkRect::MakeXYWH(i * (side + 8), 10, side, side),
                       SkSamplingOptions(SkFilterMode::kLinear), &paint);
                 }
                 // …and the same stamp small enough that the
                 // transparent margin is the only reason its edge
                 // is not a square.
                 paint.setColorFilter(
                     SkColorFilters::Blend(material::skia::toSkColor(kOn),
                                           nullptr, SkBlendMode::kModulate));
                 for (int i = 0; i < 9; ++i)
                   canvas.drawImageRect(
                       image, SkRect::MakeXYWH(i * 38.0f, side + 26, 14, 14),
                       SkSamplingOptions(SkFilterMode::kLinear), &paint);
               })
            .cover(),
        496);
  }
};

SIGIL_SKETCH(PixFontDotSprite, "Kit · API",
             "the aliased bake at three sizes and one integer scale, a live "
             "number blitted from the 96-cell font, and the white dot a "
             "point sink stamps")
