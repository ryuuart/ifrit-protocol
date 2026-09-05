#pragma once

/** @file
 * SigilCompose KIT — instruments for looking at arithmetic while you
 * author it: `trackMeter`, a cascade's schedule drawn; `restGhost`, the
 * same text at rest under the moving copy; and `curvePlot`, a function
 * of one variable drawn over its own domain. A cascade is an invisible
 * remap, a deviation has nothing on screen to be measured against, and a
 * curve is a number nobody can see, so all three draw what is otherwise
 * only inferable.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/typography/Track.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

// Everything here is an instrument, on one set of terms: for a sketch
// under the eye of whoever is tuning it, for a test that has to see the
// schedule, and NOT for the paint loop of something that ships. All three
// are describe-time — they read a resolved layout and hand back ordinary
// elements — so none costs anything on a frame that does not build one.

/** WHERE A METER'S CELLS STAND relative to the beats they report on.
 *
 *  A cell over its beat is the reading a cascade wants while it is being
 *  tuned: the fraction is on the letter it belongs to and nothing else has
 *  to be looked at. It is the wrong reading when the letters themselves
 *  are what is being watched — under a pass that paints the type, a cell
 *  laid over it hides the thing the meter is reporting on — so a meter can
 *  also stand its cells UNDER the beats as a rule of its own thickness.
 *
 *  `trim` shortens every cell by that many pixels, which is what keeps a
 *  run of finished beats reading as a run of beats rather than as one
 *  filled bar. */
struct MeterPlacement {
  enum class Where { Over, Under };
  Where where = Where::Over;
  float thickness = 3.0f;  // Under only; Over takes the beat's own height
  float gap = 6.0f;        // Under only: below the beat's bottom edge
  float trim = 0.0f;       // taken off every cell's width
};

/** THE SCHEDULE, DRAWN: one cell per beat of track @p trackIndex on the
 *  keyed text node, at that beat's own laid-out rect, filled left to right
 *  by that beat's local progress.
 *
 *  Every cascade is otherwise invisible — it numbers units, spreads them
 *  and tells nobody — so tuning one means watching letters and guessing.
 *  This is `Composer::beatsOf` drawn without an intermediate: a beat that
 *  has not opened shows bed alone, one running shows its own fraction, one
 *  finished is full, and the pitch between cells is the cascade's real
 *  pitch, uneven where a cue table made it uneven.
 *
 *  THE RECTS ARE IN THE COMPOSER'S SPACE, because that is the space
 *  `beatsOf` answers in. Put the result over the whole composition —
 *  `root.child(kit::trackMeter(...).absolute().inset(0))` — and the cells
 *  land on the type wherever it is. It is read at DESCRIBE time from the
 *  layout the last draw left standing, so a moving cascade wants a
 *  re-describe per frame to move with it; that is the cost, and it is why
 *  this is an instrument and not a component.
 *
 *  An unknown key, a node that is not text and a track index past the
 *  node's list all give an EMPTY overlay, silently, exactly as `beatsOf`
 *  does. */
[[nodiscard]] inline Element trackMeter(const Composer& composer,
                                        std::string_view key, size_t trackIndex,
                                        SkColor4f fill,
                                        SkColor4f bed = {1, 1, 1, 0.10f},
                                        MeterPlacement placement = {}) {
  Element overlay = positioned();
  const bool under = placement.where == MeterPlacement::Where::Under;
  const std::vector<Beat> beats = composer.beatsOf(key, trackIndex);
  for (size_t i = 0; i < beats.size(); ++i) {
    const SkRect& rect = beats[i].rect;
    const float width = std::max(0.0f, rect.width() - placement.trim);
    const float height = under ? placement.thickness : rect.height();
    const float top = under ? rect.bottom() + placement.gap : rect.top();
    const std::string cell = "beat" + std::to_string(i);
    overlay.child(
        box()
            .key(cell)
            .left(rect.left())
            .top(top)
            .width(width)
            .height(height)
            .fill(Fill::color(bed))
            .child(box()
                       .key(cell + "-t")
                       .left(0)
                       .top(0)
                       .width(width * std::clamp(beats[i].localT, 0.0f, 1.0f))
                       .height(height)
                       .fill(Fill::color(fill))));
  }
  return overlay;
}

/** THE SAME TEXT AT REST, UNDER THE MOVING COPY — what a deformation is
 *  measured against.
 *
 *  A track's deviation is per glyph and lives only in the draw, so the
 *  undeformed letter is nowhere on screen to compare with: a squash reads
 *  as a squash only beside the shape it squashed. This returns a box
 *  holding two copies of @p moving — `Element::atRest`'s copy set in
 *  @p colour and pinned at the box's origin, and @p moving itself in the
 *  flow, which is what sizes the box. Drop it in where the text was:
 *
 *      box().child(kit::restGhost(
 *          text(u8"RUBBERBAND", set).key("word").fx({…}), rest))
 *
 *  What the rest copy is — the same content, style, width and layout with
 *  no tracks, no span restyles and none of the moving copy's children,
 *  keyed `-rest` after the original — is `Element::atRest`'s statement;
 *  this adds the one ink, over whatever the style paints, and drops any
 *  glyph stroke so the ghost reads as one flat colour. Anything but text
 *  comes back as a plain copy beside the original, with the warning that
 *  verb gives. */
[[nodiscard]] inline Element restGhost(Element moving, SkColor4f colour) {
  Element ghost = moving.atRest();
  ghost.textFill(material::skia::Paint::solid(colour))
      .textStroke(0.0f, Fill{})
      // Pinned at the origin so the two copies share one origin, and
      // absolute so the MOVING copy is what sizes the box around them.
      .left(0.0f)
      .top(0.0f);
  return box().child(std::move(ghost)).child(std::move(moving));
}

/** ONE CURVE ON A PLOT: a function of the domain, and how it is drawn.
 *
 *  The function is a callable rather than a table because what is being
 *  looked at is usually an expression — an ease, a decay, a spring walked
 *  from its own state — and pinning it to samples would be pinning the
 *  answer instead of asking the question. */
struct Trace {
  std::function<float(float t)> f;
  SkColor4f colour = {1, 1, 1, 1};
  float width = 1.6f;
};

/** THE PLOT'S FRAME: what the box's two axes mean, what is ruled on it,
 *  and where a sample is marked.
 *
 *  The domain runs left to right and the range runs UP — y at `fromY` is
 *  the bottom of the box — because that is the direction a curve is read
 *  in, and the whole reason the mapping is a value here is so that a
 *  caller's own label, cursor or annotation goes through the same one
 *  `at()` draws with and lands ON the curve rather than near it.
 *
 *  `rulesT` and `rulesY` are stated in the DOMAIN's own units, not in
 *  fractions: a rule at one time constant, at 1.0, at the clock's own
 *  step rate is what a curve is read against, and converting those to
 *  fractions at the call site is the arithmetic this exists to hold.
 *  `marks` are domain values dotted on every trace — the published
 *  samples a reference quotes, which is what turns a drawn curve into a
 *  check of one. */
struct Plot {
  float fromT = 0.0f, toT = 1.0f;
  float fromY = 0.0f, toY = 1.0f;
  /** px of margin inside the box, so a curve at the extremes of its
   *  range is not half a stroke outside the drawing. */
  float pad = 0.0f;
  /** How finely each trace is walked. A staircase reads as a staircase
   *  only when the sampling is finer than its steps. */
  int samples = 240;
  std::vector<float> rulesT, rulesY;
  SkColor4f rule = {1, 1, 1, 0.10f};
  float ruleWidth = 1.0f;
  std::vector<float> marks;
  float markRadius = 2.5f;

  /** WHERE (t, y) LANDS in a box of @p size — the mapping every trace,
   *  rule and mark is drawn through. A degenerate axis maps to the
   *  middle of that axis rather than to infinity. */
  [[nodiscard]] SkPoint at(float t, float y, SkSize size) const {
    const float w = std::max(0.0f, size.width() - 2 * pad);
    const float h = std::max(0.0f, size.height() - 2 * pad);
    const float u = toT != fromT ? (t - fromT) / (toT - fromT) : 0.5f;
    const float v = toY != fromY ? (y - fromY) / (toY - fromY) : 0.5f;
    return {pad + w * u, pad + h * (1.0f - v)};
  }
};

/** THE CURVE, DRAWN: every trace walked across @p plot's domain, over the
 *  rules it names, with a dot on each trace at every marked sample.
 *
 *  A number over time is otherwise only inferable from what it moves, so
 *  tuning one means watching the thing and guessing — and a curve
 *  quoted in a reference cannot be checked against one at all. This is
 *  the drawing that lets both happen, and it is ONE recording rather than
 *  a node per sample.
 *
 *  It is a keyed leaf, so it prunes on the KEY: the traces are callables
 *  and compare to nothing, and the key is the caller's statement that
 *  this is the same drawing. Give two plots two keys.
 *
 *  Labels are the caller's, and deliberately: where a tick's number sits,
 *  and whether the plot leaves a gutter for it, is a look rather than
 *  arithmetic. `Plot::at` is the whole of what a label needs — put a
 *  `text()` at the point it answers and the label is on the curve. */
[[nodiscard]] inline Element curvePlot(std::string_view key,
                                       std::vector<Trace> traces,
                                       Plot plot = {}) {
  return custom(key,
                [traces = std::move(traces), plot](SkCanvas& canvas,
                                                   const PaintContext& pc) {
                  SkPaint paint;
                  paint.setAntiAlias(true);
                  paint.setColor4f(plot.rule);
                  const float half = std::max(0.0f, plot.ruleWidth) * 0.5f;
                  for (float t : plot.rulesT) {
                    const SkPoint a = plot.at(t, plot.fromY, pc.size);
                    const SkPoint b = plot.at(t, plot.toY, pc.size);
                    canvas.drawRect({a.fX - half, b.fY, a.fX + half, a.fY},
                                    paint);
                  }
                  for (float y : plot.rulesY) {
                    const SkPoint a = plot.at(plot.fromT, y, pc.size);
                    const SkPoint b = plot.at(plot.toT, y, pc.size);
                    canvas.drawRect({a.fX, a.fY - half, b.fX, a.fY + half},
                                    paint);
                  }
                  const int steps = std::max(1, plot.samples);
                  for (const Trace& trace : traces) {
                    if (!trace.f) continue;
                    SkPathBuilder path;
                    for (int i = 0; i <= steps; ++i) {
                      const float t =
                          plot.fromT + (plot.toT - plot.fromT) *
                                           ((float)i / (float)steps);
                      const SkPoint p = plot.at(t, trace.f(t), pc.size);
                      i == 0 ? path.moveTo(p) : path.lineTo(p);
                    }
                    paint.setColor4f(trace.colour);
                    paint.setStyle(SkPaint::kStroke_Style);
                    paint.setStrokeWidth(trace.width);
                    canvas.drawPath(path.detach(), paint);
                    paint.setStyle(SkPaint::kFill_Style);
                    if (plot.markRadius > 0)
                      for (float t : plot.marks)
                        canvas.drawCircle(plot.at(t, trace.f(t), pc.size),
                                          plot.markRadius, paint);
                  }
                })
      .absolute()
      .inset(0);
}

}  // namespace sigil::compose::kit
