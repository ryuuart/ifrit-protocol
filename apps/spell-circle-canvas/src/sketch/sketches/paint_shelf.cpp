/** @file
 * paint_shelf — the paint leaves a plain gradient cannot spell, and the
 * two dials that decide what a paint's coordinates MEAN.
 *
 * `conical` is the offset-focus radial, and it is what a moved `radial`
 * is not: moving a radial's centre couples the falloff to the
 * displacement, so the whole ramp slides including its outer edge. Here
 * the outer circle stays put and only the hot spot moves, which is what
 * a highlight displaced off a sphere actually does.
 *
 * `sweep` runs from a start angle around the centre, and its angles
 * CLAMP rather than wrap: a window that leaves [0, 360) paints the part
 * outside it in the nearest stop's flat colour, because no canvas angle
 * ever reaches past 360.
 *
 * `buffer` is content that changes without re-describing: a
 * caller-owned raster the paint samples, published with `commit()`. The
 * recipe compares by (source, revision), so an identical re-describe
 * between commits prunes and nothing repaints — which is the whole
 * point, since the alternative gives up the node's picture caching and
 * its decorations.
 *
 * `worldSpace` moves the coordinates a paint is evaluated in from the
 * node's own box to the ROOT's. The bottom-right pair is one paint on
 * two nodes: unflagged each node gets its own copy of the ramp, flagged
 * both read one field that runs across the page.
 *
 * EDIT THESE FIRST
 *   kFocus  — how far the conical's hot spot is displaced, px.
 *   kWindow — the sweep's start and end angles.
 */

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <memory>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace paint = sigil::material::skia;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 646};
constexpr float kCell = 252;
constexpr float kPicture = 190;

constexpr float kFocus = 44;       // the conical's hot spot displacement, px
constexpr float kWindowFrom = 45;  // the sweep window that does not fill a turn
constexpr float kWindowTo = 315;

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.09f, 0.095f, 0.11f, 1};
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

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

SkPoint middle() { return {kCell * 0.5f, kPicture * 0.5f}; }

/** The one ramp every radial cell runs, so what differs between them is
 *  the geometry of the falloff and never the colours. */
std::vector<paint::Stop> ember() {
  return {{0.0f, {1.00f, 0.96f, 0.82f, 1}},
          {0.35f, {0.98f, 0.62f, 0.24f, 1}},
          {1.0f, {0.12f, 0.10f, 0.16f, 1}}};
}

/** A wheel of hues for the two sweeps, ending where it began so the seam
 *  at the start angle is the only edge in it. */
std::vector<paint::Stop> wheel() {
  return {{0.00f, {0.94f, 0.34f, 0.32f, 1}},
          {0.25f, {0.94f, 0.82f, 0.32f, 1}},
          {0.50f, {0.36f, 0.86f, 0.56f, 1}},
          {0.75f, {0.40f, 0.60f, 0.96f, 1}},
          {1.00f, {0.94f, 0.34f, 0.32f, 1}}};
}

Element cell(const char* call, const char* note, Element body) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   kit::well({.width = kCell,
                              .height = kPicture,
                              .ground = Fill::color(kCellGround)},
                             std::move(body)));
}

/** One paint across the whole cell. */
Element swatch(const char* call, const char* note, paint::Paint fill) {
  return cell(call, note,
              box().child(box().absolute().inset(0).fill(std::move(fill))));
}

}  // namespace

struct PaintShelf final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    // The caller-owned raster: drawn once here and published. A running
    // sketch would draw into it and commit() again; the node's picture
    // caching survives either way.
    auto pixels = std::make_shared<paint::PixelBuffer>(120, 90);
    {
      SkCanvas& into = pixels->canvas();
      into.clear(SkColor4f{0.09f, 0.12f, 0.18f, 1}.toSkColor());
      SkPaint mark;
      mark.setAntiAlias(true);
      for (int i = 0; i < 9; ++i) {
        mark.setColor4f({0.30f + 0.07f * (float)i, 0.86f - 0.05f * (float)i,
                         0.92f - 0.03f * (float)i, 1});
        into.drawCircle(14.0f + 12.0f * (float)i,
                        22.0f + 46.0f * (i % 2 == 0 ? 0.0f : 1.0f),
                        6.0f + (float)i, mark);
      }
      pixels->commit();
    }

    // The one paint the last two cells share: a diagonal unit ramp, so
    // "the node's own box" and "the root's box" are two visibly
    // different readings of the same description.
    const auto field = [](bool world) {
      paint::Paint p =
          paint::Paint::linearUnit({0, 0}, {1, 1},
                                   {{0.0f, {0.16f, 0.20f, 0.34f, 1}},
                                    {0.5f, {0.44f, 0.78f, 0.86f, 1}},
                                    {1.0f, {0.96f, 0.72f, 0.34f, 1}}});
      return p.worldSpace(world);
    };
    const auto pair = [&](bool world) {
      return box()
          .row()
          .padding(18, 34)
          .gap(16)
          .child(box().grow(1).alignSelf(Align::Stretch).fill(field(world)))
          .child(box().grow(1).alignSelf(Align::Stretch).fill(field(world)));
    };

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("PAINT SHELF \xc2\xb7 skia::Paint conical, sweep, "
                           "buffer, worldSpace"),
             .subtitle = toU8("dials \xc2\xb7 the focal offset (44 px) "
                              "\xc2\xb7 the sweep window (45\xc2\xb0 to "
                              "315\xc2\xb0) \xc2\xb7 worldSpace on or off"),
             .footer = toU8("a paint sits in one of three volatility tiers "
                            "\xe2\x80\x94 static, geometry, live "
                            "\xe2\x80\x94 and every leaf here but the buffer "
                            "is static or geometry, so a node painted with "
                            "one still caches and prunes"),
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
                     {kit::cells(
                          {.cells =
                               {swatch("Paint::radial(centre, 92, ember)",
                                       "the baseline \xc2\xb7 one circle, so "
                                       "moving its centre would slide the "
                                       "outer edge with the hot spot",
                                       paint::Paint::radial(middle(), 92,
                                                            ember())),
                                swatch("conical(focus, 0, centre, 92, ember)",
                                       "the ramp runs from a circle of "
                                       "radius 0 at the focus to the circle "
                                       "at the centre \xc2\xb7 the outer "
                                       "edge stays put",
                                       paint::Paint::conical(
                                           {middle().fX - kFocus,
                                            middle().fY - kFocus * 0.6f},
                                           0, middle(), 92, ember())),
                                swatch("\xe2\x80\xa6"
                                       "with the focus moved "
                                       "across",
                                       "the one dial \xc2\xb7 the "
                                       "highlight crosses the face while "
                                       "the outer circle does not move at "
                                       "all",
                                       paint::Paint::conical(
                                           {middle().fX + 1.3f * kFocus,
                                            middle().fY + 0.8f * kFocus},
                                           0, middle(), 92, ember())),
                                swatch("Paint::sweep(centre, wheel)",
                                       "an angular ramp from 0\xc2\xb0 round "
                                       "the centre \xc2\xb7 the stops end "
                                       "where they began, so the only edge "
                                       "is the start",
                                       paint::Paint::sweep(middle(), wheel()))},
                           .gap = 14}),
                      kit::cells(
                          {.cells =
                               {swatch("sweep(centre, wheel, 45, 315)",
                                       "angles CLAMP, they do not wrap "
                                       "\xc2\xb7 outside the window the "
                                       "nearest stop's flat colour, which is "
                                       "the wedge at the top",
                                       paint::Paint::sweep(middle(), wheel(),
                                                           kWindowFrom,
                                                           kWindowTo)),
                                swatch("Paint::buffer(pixels)",
                                       "a caller-owned raster, published "
                                       "with commit() \xc2\xb7 the recipe "
                                       "compares by (source, revision), so "
                                       "an unchanged describe prunes",
                                       paint::Paint::buffer(
                                           pixels, SkTileMode::kRepeat,
                                           SkTileMode::kRepeat)),
                                cell("linearUnit(\xe2\x80\xa6"
                                     ").worldSpace(false)",
                                     "two nodes, one description \xc2\xb7 "
                                     "each reads uResolution as its OWN box, "
                                     "so each carries a whole copy of the "
                                     "ramp",
                                     pair(false)),
                                cell("linearUnit(\xe2\x80\xa6"
                                     ").worldSpace(true)",
                                     "the same two nodes anchored to the "
                                     "root \xc2\xb7 one field across the "
                                     "whole page, so two small boxes near "
                                     "its far corner both land in one part "
                                     "of it",
                                     pair(true))},
                           .gap = 14})},
                 .column = true,
                 .gap = 18}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(PaintShelf, "Specimen",
             "the offset-focus radial beside the plain one, the sweep and "
             "its clamped window, a caller-owned raster, and one ramp read "
             "in the node's box and then in the root's")
