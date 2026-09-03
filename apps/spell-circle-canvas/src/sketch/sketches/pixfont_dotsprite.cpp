/** @file
 * pixfont_dotsprite — the aliased bake, and the stamp a sink repeats.
 *
 * Two small kit files, both of which exist because a leaf cannot do the
 * job. `kit/PixelType.h` shapes a run with antialiasing off, thresholds
 * the raster to a 1-bit A8 mask, and presents that mask at an INTEGER
 * scale with nearest sampling — pixel type whose every edge lands on the
 * grid. `kit::bakeRun` gives one run as a `Mask`; `kit::bakeFont` bakes
 * all 96 printable ASCII cells as a `PixFont` — each cropped to its ink
 * and carrying where that ink sits inside the shared line box, so a run
 * of figures, x-heights and descenders stands on ONE baseline and
 * `lineHeight` is how deep the cells reach below the top of that box.
 * `kit::blit` walks a pen over them inside a `custom()` leaf, which is
 * the only way a LIVE number is drawn at all: `text()` takes a string,
 * not an animatable, so a rolling readout cannot be a text node.
 *
 * The traps are the sheet. The SIZE is the control and the threshold is
 * inert — under aliased shaping Skia lights a pixel iff its centre is
 * inside the outline, so the coverage is already binary. Digits want one
 * tabular advance or a readout shivers as a `1` narrows the string. And
 * a bitmap face at a fractional scale is a blurry bitmap face.
 *
 * `kit::dotSprite` is the other half: a WHITE round dot on transparency,
 * baked once, stamped thousands of times, white because a sink tints per
 * point and a coloured stamp would multiply into every tint.
 *
 * EDIT THESE FIRST
 *   kBakeSizes — the three font sizes the first cell sweeps.
 *   kOn — the colour a mask is blitted in.
 *   kScale — the integer present scale.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/PixelType.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSamplingOptions.h>

#include <cstdio>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 424};
constexpr float kCell = 200;
constexpr float kPicture = 200;

constexpr float kBakeSizes[3] = {9, 12, 16};  // the sweep in the first cell
constexpr float kScale = 3;                   // integer, always
constexpr SkColor4f kOn{0.62f, 0.98f, 0.72f, 1};  // what a mask is tinted

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.055f, 0.065f, 0.06f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

/** The face the bake reads: aliased, so the coverage handed back is
 *  already 0 or 1 and the threshold has nothing left to decide. The
 *  96-cell font is baked from a PROPORTIONAL face on purpose — a `1` is
 *  narrower than a `0` there, which is the whole of trap 3. */
weave::TextStyle bakeFace(float size, bool proportional = false) {
  static const sk_sp<SkTypeface> code = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  static const sk_sp<SkTypeface> text = weave::ports::pickTypeface(
      {"Helvetica Neue", "Helvetica", "Arial", "sans-serif"});
  return weave::textStyle({.face = proportional ? text : code,
                           .size = size,
                           .color = {1, 1, 1, 1},
                           .aliased = true});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

Element plate(Element body) {
  return box()
      .width(Dim(kCell))
      .height(Dim(kPicture))
      .clip()
      .fill(Fill::color(kCellGround))
      .child(std::move(body).absolute().inset(14));
}

Element cell(const char* call, const char* note, Element body) {
  return kit::cell(voice(), toU8(call), toU8(note), plate(std::move(body)));
}

std::string line(const char* format, auto... args) {
  char buffer[96];
  std::snprintf(buffer, sizeof buffer, format, args...);
  return buffer;
}

}  // namespace

struct PixFontDotSprite final : sketch::Sketch {
  kit::PixFont font;
  kit::Mask sweep[3];
  sk_sp<SkImage> dot;

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    // The readout is live, so the sheet is complete only once the number
    // has run far enough to have moved every column of it.
    ctx.captureAt(1.28);

    for (int i = 0; i < 3; ++i)
      sweep[i] = kit::bakeRun(u8"3.eg", *ctx.fonts, bakeFace(kBakeSizes[i]));
    font = kit::bakeFont(*ctx.fonts, bakeFace(kBakeSizes[2], true));
    dot = kit::dotSprite(32);

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("PIXEL TYPE AND THE STAMP \xc2\xb7 kit::bakeRun, "
                           "kit::bakeFont / kit::blit, kit::dotSprite"),
             .subtitle =
                 toU8("dials \xc2\xb7 the bake size (9, 12, 16 px) \xc2\xb7 "
                      "the present scale (3\xc3\x97, integer) \xc2\xb7 the "
                      "on colour \xc2\xb7 the blit's track (1 and 5 px)"),
             .footer =
                 toU8("the threshold is INERT under aliased shaping: Skia "
                      "lights a pixel iff its centre is inside the outline, "
                      "so the coverage is already binary and what decides "
                      "legibility is whether the x-height rounds up or down"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells({.cells = {sizeSweep(), presented(), shadowed(),
                                  readout(), stamp()},
                        .gap = 12}))
            .absolute()
            .inset(0));
  }

  /** Trap 2: the size is the control. One run baked at three sizes and
   *  presented at one scale, so the only difference on screen is which
   *  pixel centres the outline happened to contain. */
  Element sizeSweep() {
    Element column = box().column().gap(12);
    for (int i = 0; i < 3; ++i)
      column.child(box().row().gap(10).alignItems(Align::Center)
                       .child(text(toU8(line("%2.0f", kBakeSizes[i])),
                                   mono(9, kAsh)))
                       .child(kit::masked(sweep[i],
                                          {.colour = kOn, .scale = 2})));
    return cell("bakeRun(\"3.eg\", fonts, aliased(size))",
                "one run, three bake sizes, one present scale \xc2\xb7 at the "
                "smallest the counters hold no pixel centre and close",
                std::move(column));
  }

  /** Trap 4: an integer scale with nearest sampling, beside the 1\xc3\x97
   *  bake it came from. */
  Element presented() {
    return cell("kit::masked(mask, {.scale = 3})",
                "the same 1-bit mask at 1\xc3\x97 and at 3\xc3\x97 \xc2\xb7 "
                "nearest sampling, so a bake stays a bake",
                box()
                    .column()
                    .gap(18)
                    .alignItems(Align::Start)
                    .child(kit::masked(sweep[2], {.colour = kOn}))
                    .child(kit::masked(sweep[2],
                                       {.colour = kOn, .scale = kScale})));
  }

  /** The shadow pass: a second blit underneath at an offset in
   *  DESTINATION px, with the colour's RGB multiplied down. */
  Element shadowed() {
    return cell("Present{.shadowOffset = {3, 3}}",
                "one extra pass under the mask, offset in destination px "
                "and multiplied by a quarter \xc2\xb7 one bake, two draws",
                box()
                    .column()
                    .gap(20)
                    .child(kit::masked(sweep[2],
                                       {.colour = kOn, .scale = kScale}))
                    .child(kit::masked(sweep[2], {.colour = kOn,
                                                  .scale = kScale,
                                                  .shadowOffset = {3, 3},
                                                  .shadowMul = 0.25f})));
  }

  /** Trap 3, and the whole reason the 96-cell bake exists: a number that
   *  changes every frame, drawn by a pen walk inside one custom() leaf
   *  with nothing re-described. */
  Element readout() {
    const kit::PixFont* f = &font;
    return cell("kit::blit(canvas, font, at, run, colour, Blit)",
                "a LIVE readout, no text node at all \xc2\xb7 figures, an "
                "x-height and a descender on ONE baseline, the same run at "
                "track 1 and track 5",
                custom("pixfont.readout",
                       [f](SkCanvas& canvas, const PaintContext& pc) {
                         const double t = pc.elapsedSeconds;
                         // A cell is cropped to its ink and carries where
                         // that ink sits inside the shared line box, so a
                         // run mixing figures, an x-height and a descender
                         // stands on one baseline.
                         const std::string run =
                             line("%04.0fpx", 1100.0 + t * 111.0);
                         canvas.save();
                         canvas.scale(2, 2);
                         kit::blit(canvas, *f, {0, 0}, run, kOn,
                                   {.track = 1, .tabularDigits = true,
                                    .snap = 1});
                         kit::blit(canvas, *f, {0, (float)f->lineHeight + 8},
                                   run, kOn,
                                   {.track = 5, .tabularDigits = true,
                                    .snap = 1});
                         canvas.restore();
                       })
                    .absolute()
                    .inset(0)
                    .cache(Cache::None));
  }

  /** The stamp: white on transparency, so the tint is the caller's. */
  Element stamp() {
    sk_sp<SkImage> image = dot;
    return cell("kit::dotSprite(32)",
                "a white antialiased disc with a transparent ring around it "
                "\xc2\xb7 baked once, tinted per point, never a square edge",
                custom("pixfont.dot",
                       [image](SkCanvas& canvas, const PaintContext& pc) {
                         static constexpr SkColor4f kTints[3] = {
                             {1, 1, 1, 1}, kOn, {1.0f, 0.55f, 0.35f, 1}};
                         const float side = pc.size.width() / 3.4f;
                         // A white stamp is TINTED by modulating it —
                         // setting a paint colour does nothing to a colour
                         // image, and this is the step a point sink takes
                         // per point.
                         SkPaint paint;
                         for (int i = 0; i < 3; ++i) {
                           paint.setColorFilter(SkColorFilters::Blend(
                               kTints[i], nullptr, SkBlendMode::kModulate));
                           canvas.drawImageRect(
                               image,
                               SkRect::MakeXYWH(i * (side + 8), 10, side,
                                                side),
                               SkSamplingOptions(SkFilterMode::kLinear),
                               &paint);
                         }
                         // …and the same stamp small enough that the
                         // transparent margin is the only reason its edge
                         // is not a square.
                         paint.setColorFilter(SkColorFilters::Blend(
                             kOn, nullptr, SkBlendMode::kModulate));
                         for (int i = 0; i < 9; ++i)
                           canvas.drawImageRect(
                               image,
                               SkRect::MakeXYWH(i * 16.0f, side + 26, 14, 14),
                               SkSamplingOptions(SkFilterMode::kLinear),
                               &paint);
                       })
                    .absolute()
                    .inset(0));
  }
};

SIGIL_SKETCH(PixFontDotSprite, "Kit \xc2\xb7 API",
             "the aliased bake at three sizes and one integer scale, a live "
             "number blitted from the 96-cell font, and the white dot a "
             "point sink stamps")
