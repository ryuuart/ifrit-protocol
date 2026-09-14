#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/kit/Chart.h>
#include <sigilsketch/kit/Theme.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <tuple>
#include <utility>

namespace sigil::sketch::kit {

using compose::Element;
using compose::PaintContext;

namespace {

/** WHERE A MARK STANDS on @p ranged — the middle of the band a transform
 *  with bands gives @p value, the value's own position where there is no
 *  band. One rule for the tick, the dot and the word, so the three land
 *  together; the band's own extent is read off the scale itself. */
double markOn(const data::Scale& ranged, double value) {
  const double width = ranged.bandwidth();
  return ranged.apply(value) + width * 0.5;
}

/** Degrees as radians, for Skia's canvas angles. */
float radians(double degrees) {
  return (float)(degrees * std::numbers::pi / 180.0);
}

/** The recording's own name: the plot's key, the part, and where in the
 *  run the layer stood — unique among the plots on one sheet, which is
 *  what a keyed recording needs to prune on. */
std::string named(std::string_view key, const char* part, std::size_t index) {
  return std::string(key) + "-" + part + std::to_string(index);
}

/** A recording of one drawn part, filling the plot's box and set in
 *  @p styleClass so the ink in force is the class's colour.
 *
 *  The name is BOTH the program's identity — which is what lets an
 *  incomparable callable prune — and the node's own key, so a sketch can
 *  read a drawn layer's box back and hang an annotation off it. */
Element recorded(const std::string& name, std::string_view styleClass,
                 compose::PaintProgram program) {
  return compose::custom(name, std::move(program))
      .key(name)
      .styleClass(std::string(styleClass))
      .absolute()
      .inset(0);
}

/** A pen in the ink in force, stroking at @p width. */
SkPaint strokePen(const PaintContext& context, float width) {
  SkPaint pen;
  pen.setAntiAlias(true);
  pen.setColor4f(context.ink);
  pen.setStyle(SkPaint::kStroke_Style);
  pen.setStrokeWidth(std::max(width, 0.0f));
  return pen;
}

/** The oval a polar arc of radius @p r about @p centre is drawn on. */
SkRect ring(SkPoint centre, float r) {
  return SkRect::MakeLTRB(centre.fX - r, centre.fY - r, centre.fX + r,
                          centre.fY + r);
}

/** The domain value an axis of @p how stands at on the OTHER scale: its
 *  low end, or a polar x axis's high end, which is the rim. */
double standsAt(const Ruler& how, const Plot& frame) {
  if (how.at) return *how.at;
  const data::Interval other =
      how.of == Axis::X ? frame.y.domain : frame.x.domain;
  const bool rim = frame.polar && how.of == Axis::X;
  return rim ? other.high : other.low;
}

/** The values an axis of @p how ticks. */
std::vector<double> tickValues(const Ruler& how, const Plot& frame) {
  if (!how.ticks.empty()) return how.ticks;
  return (how.of == Axis::X ? frame.x : frame.y).ticks(std::max(how.count, 1));
}

}  // namespace

// ---------------------------------------------------------------------------
// The frame

data::Scale Plot::scale(Axis which, SkSize size) const {
  data::Scale ranged = which == Axis::X ? x : y;
  if (polar) {
    // x is the angle over the stated sweep, which is the same at every
    // size; y is the radius, from the inner fraction out to the rim.
    const float outer = radius(size);
    ranged.range = which == Axis::X
                       ? polar->sweep
                       : data::Interval{polar->inner * outer, outer};
    return ranged;
  }
  // y runs UP: its low end is the box's bottom edge.
  ranged.range = which == Axis::X
                     ? data::Interval{pad, std::max(size.width() - pad, pad)}
                     : data::Interval{std::max(size.height() - pad, pad), pad};
  return ranged;
}

SkPoint Plot::centre(SkSize size) const {
  return {size.width() * 0.5f, size.height() * 0.5f};
}

float Plot::radius(SkSize size) const {
  return std::max(0.0f, std::min(size.width(), size.height()) * 0.5f - pad);
}

double Plot::angle(double xValue) const {
  if (!polar) return 0.0;
  return markOn(scale(Axis::X, SkSize::MakeEmpty()), xValue);
}

double Plot::radiusFraction(double yValue) const {
  if (!polar) return 0.0;
  data::Scale fraction = y;
  fraction.range = {polar->inner, 1.0};
  return markOn(fraction, yValue);
}

SkPoint Plot::at(double xValue, double yValue, SkSize size) const {
  if (polar) {
    const SkPoint hub = centre(size);
    const float r = (float)radiusFraction(yValue) * radius(size);
    const float theta = radians(angle(xValue));
    return {hub.fX + r * std::cos(theta), hub.fY + r * std::sin(theta)};
  }
  return {(float)markOn(scale(Axis::X, size), xValue),
          (float)markOn(scale(Axis::Y, size), yValue)};
}

// ---------------------------------------------------------------------------
// The plot

Element plot(std::string_view key, const Plot& frame,
             std::vector<Layer> layers) {
  Element field = compose::box();
  if (!key.empty()) field.key(key);
  std::vector<Element> drawn;
  drawn.reserve(layers.size());
  for (std::size_t i = 0; i < layers.size(); ++i)
    if (layers[i]) drawn.push_back(layers[i](frame, key, i));
  if (!drawn.empty()) field.children({std::move(drawn)});
  return field;
}

// ---------------------------------------------------------------------------
// The axis

Layer axis(const Ruler& how) {
  return [how](const Plot& frame, std::string_view key, std::size_t index) {
    const double other = standsAt(how, frame);
    const std::vector<double> ticks = tickValues(how, frame);
    const float reach = how.reach.value_or(theme().spacing.tickReach);
    const bool polar = frame.polar.has_value();
    std::vector<Element> parts;

    if (how.line)
      parts.push_back(recorded(
          named(key, "axis", index), "plotAxis",
          [how, frame, other, polar](SkCanvas& canvas, const PaintContext& pc) {
            SkPaint pen = strokePen(pc, how.width);
            if (polar && how.of == Axis::X) {
              // The rim: the whole sweep at the radius `other` stands at.
              const float r =
                  (float)frame.radiusFraction(other) * frame.radius(pc.size);
              const data::Interval sweep = frame.polar->sweep;
              canvas.drawArc(ring(frame.centre(pc.size), r), (float)sweep.low,
                             (float)sweep.extent(), false, pen);
              return;
            }
            if (polar) {
              // One spoke to number the rings along.
              const SkPoint hub = frame.centre(pc.size);
              const float outer = frame.radius(pc.size);
              const float theta =
                  radians(how.at ? frame.angle(other) : frame.polar->sweep.low);
              const float inner = frame.polar->inner * outer;
              canvas.drawLine({hub.fX + inner * std::cos(theta),
                               hub.fY + inner * std::sin(theta)},
                              {hub.fX + outer * std::cos(theta),
                               hub.fY + outer * std::sin(theta)},
                              pen);
              return;
            }
            // The line spans the whole field: its extent is the box's, and
            // only its position comes from the other scale's mapping, so a
            // category axis is as long as the field whatever its domain is.
            const data::Scale across =
                frame.scale(how.of == Axis::X ? Axis::X : Axis::Y, pc.size);
            const data::Scale along =
                frame.scale(how.of == Axis::X ? Axis::Y : Axis::X, pc.size);
            const float fixed = (float)along.apply(other);
            const float from = (float)across.range.low;
            const float to = (float)across.range.high;
            if (how.of == Axis::X)
              canvas.drawLine({from, fixed}, {to, fixed}, pen);
            else
              canvas.drawLine({fixed, from}, {fixed, to}, pen);
          }));

    if (reach > 0 && !ticks.empty())
      parts.push_back(recorded(
          named(key, "tick", index), "plotTick",
          [how, frame, other, ticks, reach, polar](SkCanvas& canvas,
                                                   const PaintContext& pc) {
            SkPaint pen = strokePen(pc, how.width);
            if (polar) {
              const SkPoint hub = frame.centre(pc.size);
              const float outer = frame.radius(pc.size);
              const float inner = frame.polar->inner * outer;
              for (double tick : ticks) {
                if (how.of == Axis::Y) {
                  // A ring at the radius this tick stands at.
                  const float r = (float)frame.radiusFraction(tick) * outer;
                  const data::Interval sweep = frame.polar->sweep;
                  canvas.drawArc(ring(hub, r), (float)sweep.low,
                                 (float)sweep.extent(), false, pen);
                  continue;
                }
                // A spoke: the whole radius, which is what divides a wheel.
                const float theta = radians(frame.angle(tick));
                canvas.drawLine({hub.fX + inner * std::cos(theta),
                                 hub.fY + inner * std::sin(theta)},
                                {hub.fX + outer * std::cos(theta),
                                 hub.fY + outer * std::sin(theta)},
                                pen);
              }
              return;
            }
            // A tick reaches OUTWARD from the field: down off an x axis,
            // left off a y one.
            for (double tick : ticks) {
              const SkPoint point = how.of == Axis::X
                                        ? frame.at(tick, other, pc.size)
                                        : frame.at(other, tick, pc.size);
              if (how.of == Axis::X)
                canvas.drawLine(point, {point.fX, point.fY + reach}, pen);
              else
                canvas.drawLine(point, {point.fX - reach, point.fY}, pen);
            }
          }));

    if (how.numbers && !ticks.empty()) {
      std::vector<Datum> data;
      std::vector<Element> words;
      data.reserve(ticks.size());
      words.reserve(ticks.size());
      for (double tick : ticks) {
        data.push_back(how.of == Axis::X ? Datum{tick, other}
                                         : Datum{other, tick});
        words.push_back(how.tickLine
                            ? how.tickLine(tick, how)
                            : tickLabel(compose::kit::formatted("%.3g", tick)));
      }
      // Where a number stands off its tick: below an x axis, before a y
      // one, and outward past a polar rim — which on a polar frame is what
      // the anchor's outward offset means.
      Anchor anchor;
      if (how.of == Axis::X && !polar) {
        anchor = {compose::Align::Center,
                  compose::Align::Start,
                  {0, reach + how.gap}};
      } else if (how.of == Axis::X) {
        anchor = {compose::Align::Center,
                  compose::Align::Center,
                  {0, reach + how.gap}};
      } else if (!polar) {
        anchor = {compose::Align::End,
                  compose::Align::Center,
                  {-(reach + how.gap), 0}};
      } else {
        anchor = {compose::Align::Start, compose::Align::Center, {how.gap, 0}};
      }
      const Layer numbers = detail::anchored(std::move(data), std::move(words),
                                             anchor, "tick", "plotTick");
      parts.push_back(numbers(frame, named(key, "number", index), 0));
    }

    Element stack = compose::box().absolute().inset(0);
    if (!parts.empty()) stack.children({std::move(parts)});
    return stack;
  };
}

// ---------------------------------------------------------------------------
// The rules

Layer rules(const Rules& how) {
  return [how](const Plot& frame, std::string_view key, std::size_t index) {
    // One path holding every hairline of the ladder, stroked once with the
    // caller's pen — so a ruled ladder dashes exactly as a curve does.
    const std::string name = named(key, "rule", index);
    return compose::box()
        .key(name)
        .styleClass(std::string(detail::classOf(how.styleClass, "plotRule")))
        .absolute()
        .inset(0)
        .shape(compose::keyedShape(
            std::tuple{name, frame, how.x, how.y},
            [how, frame](SkSize size) {
              SkPathBuilder path;
              if (frame.polar) {
                const SkPoint hub = frame.centre(size);
                const float outer = frame.radius(size);
                const float inner = frame.polar->inner * outer;
                const data::Interval sweep = frame.polar->sweep;
                for (double value : how.y)
                  path.addArc(
                      ring(hub, (float)frame.radiusFraction(value) * outer),
                      (float)sweep.low, (float)sweep.extent());
                for (double value : how.x) {
                  const float theta = radians(frame.angle(value));
                  path.moveTo(hub.fX + inner * std::cos(theta),
                              hub.fY + inner * std::sin(theta));
                  path.lineTo(hub.fX + outer * std::cos(theta),
                              hub.fY + outer * std::sin(theta));
                }
                return path.detach();
              }
              const data::Scale across = frame.scale(Axis::X, size);
              const data::Scale up = frame.scale(Axis::Y, size);
              for (double value : how.x) {
                const float at = (float)markOn(across, value);
                path.moveTo(at, (float)up.range.low);
                path.lineTo(at, (float)up.range.high);
              }
              for (double value : how.y) {
                const float at = (float)markOn(up, value);
                path.moveTo((float)across.range.low, at);
                path.lineTo((float)across.range.high, at);
              }
              return path.detach();
            }))
        .stroke(how.pen);
  };
}

// ---------------------------------------------------------------------------
// The curve and the band under it

namespace {

/** The curve @p f walked across @p frame's x domain into a path in the
 *  box's own pixels. */
SkPathBuilder walked(const Plot& frame,
                     const sigil::core::Callable<double(double)>& f,
                     int samples, SkSize size) {
  SkPathBuilder path;
  const int steps = std::max(samples, 1);
  const data::Interval domain = frame.x.domain;
  for (int i = 0; i <= steps; ++i) {
    const double value =
        domain.low + domain.extent() * ((double)i / (double)steps);
    const SkPoint point = frame.at(value, f(value), size);
    i == 0 ? path.moveTo(point) : path.lineTo(point);
  }
  return path;
}

}  // namespace

Layer trace(sigil::core::Callable<double(double)> f, const Trace& how) {
  return [f = std::move(f), how](const Plot& frame, std::string_view key,
                                 std::size_t index) {
    // THE CURVE IS A SHAPE AND NOT A RECORDING, because a span gate runs
    // along a node's shape and a curve that draws itself on is what every
    // plate with a reveal on it wants. The key is the caller's statement
    // that this is the same drawing, exactly as a recording's is: the
    // callable compares to nothing, so the plot's name, the frame and the
    // sampling stand for it.
    const std::string name = named(key, "trace", index);
    Element curve =
        compose::box()
            .key(name)
            .styleClass(
                std::string(detail::classOf(how.styleClass, "plotTrace")))
            .absolute()
            .inset(0)
            .shape(compose::keyedShape(
                std::tuple{name, frame, how.samples},
                [f, frame, samples = how.samples](SkSize box) {
                  return f ? walked(frame, f, samples, box).detach() : SkPath();
                }));
    if (how.along)
      curve.stroke(*how.along, how.pen);
    else
      curve.stroke(how.pen);
    return curve;
  };
}

Layer area(sigil::core::Callable<double(double)> f, const Area& how) {
  return [f = std::move(f), how](const Plot& frame, std::string_view key,
                                 std::size_t index) {
    return recorded(
        named(key, "area", index), detail::classOf(how.styleClass, "plotArea"),
        [f, how, frame](SkCanvas& canvas, const PaintContext& pc) {
          if (!f) return;
          SkPaint pen;
          pen.setAntiAlias(true);
          pen.setColor4f(pc.ink);
          SkPathBuilder path = walked(frame, f, how.samples, pc.size);
          // Closed back along the base, so the band between the
          // curve and that value is what is filled.
          const int steps = std::max(how.samples, 1);
          const data::Interval domain = frame.x.domain;
          for (int i = steps; i >= 0; --i)
            path.lineTo(frame.at(
                domain.low + domain.extent() * ((double)i / (double)steps),
                how.base, pc.size));
          path.close();
          canvas.drawPath(path.detach(), pen);
        });
  };
}

}  // namespace sigil::sketch::kit
