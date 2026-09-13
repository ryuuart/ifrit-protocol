#include <include/core/SkRect.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Paint.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilsketch/kit/Chart.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>

namespace sigil::sketch::kit::detail {

using compose::Align;
using compose::Element;
using compose::LayoutInput;

namespace {

/** Degrees as radians, for Skia's canvas angles. */
float radians(double degrees) {
  return (float)(degrees * std::numbers::pi / 180.0);
}

/** WHERE A CHILD OF @p extent SITS so that @p align holds at @p at: its
 *  near edge there, its middle, or its far edge. Everything that is not
 *  Center or End reads as Start, because a placed mark has no run of
 *  siblings to share a baseline with and nothing to stretch against. */
float edge(Align align, float at, float extent) {
  if (align == Align::Center) return at - extent * 0.5f;
  if (align == Align::End) return at - extent;
  return at;
}

/** THE OFFSET APPLIED TO A MAPPED POINT: the two numbers as box axes on a
 *  Cartesian frame, and as the tangent and the radius at that angle on a
 *  polar one — which is what carries a word off a rim. */
SkPoint nudged(const Plot& frame, SkPoint point, const Datum& datum,
               SkVector offset) {
  if (!frame.polar) return {point.fX + offset.fX, point.fY + offset.fY};
  const float theta = radians(frame.angle(datum.x));
  const float cosine = std::cos(theta);
  const float sine = std::sin(theta);
  return {point.fX - sine * offset.fX + cosine * offset.fY,
          point.fY + cosine * offset.fX + sine * offset.fY};
}

/** THE ANGULAR OR HORIZONTAL BAND ONE ENTRY OWNS on @p ranged: the width
 *  the transform gives it, the spacing between neighbours where it gives
 *  no width, and one unit where it gives neither — a band layer over a
 *  continuous scale has no band to draw, and a hairline says so. */
double bandOf(const data::Scale& ranged) {
  const double width = ranged.bandwidth();
  if (width != 0.0) return width;
  const double step = ranged.stepWidth();
  return step != 0.0 ? step : 1.0;
}

/** ONE CHILD PER DATUM, each anchored on the point the frame maps it to —
 *  the scheme behind every layer that places elements rather than
 *  recording a path. The frame's ranges come from the container's own
 *  resolved size, so nothing above this computed a pixel. */
struct Anchored {
  Plot frame;
  std::vector<Datum> data;
  Anchor anchor;

  std::vector<SkRect> place(const LayoutInput& in) const {
    std::vector<SkRect> rects(in.childSizes.size());
    for (std::size_t i = 0; i < rects.size() && i < data.size(); ++i) {
      const SkPoint point =
          nudged(frame, frame.at(data[i].x, data[i].y, in.container), data[i],
                 anchor.offset);
      const SkSize size = in.childSizes[i];
      rects[i] = SkRect::MakeXYWH(edge(anchor.across, point.fX, size.width()),
                                  edge(anchor.down, point.fY, size.height()),
                                  size.width(), size.height());
    }
    return rects;
  }
};

/** THE BAND EACH DATUM OWNS, DRAWN OUT TO ITS VALUE: on a Cartesian frame
 *  the box from the base to the value across the x scale's own band; on a
 *  polar one the square the wedge of that radius is inscribed in, which is
 *  what makes the wedge's own shape a describe-time value and leaves only
 *  its extent to layout. */
struct Spanned {
  Plot frame;
  std::vector<Datum> data;
  double base = 0.0;

  std::vector<SkRect> place(const LayoutInput& in) const {
    std::vector<SkRect> rects(in.childSizes.size());
    if (frame.polar) {
      const SkPoint hub = frame.centre(in.container);
      const float outer = frame.radius(in.container);
      for (std::size_t i = 0; i < rects.size() && i < data.size(); ++i) {
        const float r = (float)frame.radiusFraction(data[i].y) * outer;
        rects[i] =
            SkRect::MakeLTRB(hub.fX - r, hub.fY - r, hub.fX + r, hub.fY + r);
      }
      return rects;
    }
    const data::Scale across = frame.scale(Axis::X, in.container);
    const data::Scale up = frame.scale(Axis::Y, in.container);
    const float width = (float)bandOf(across);
    const float from = (float)up.apply(base);
    for (std::size_t i = 0; i < rects.size() && i < data.size(); ++i) {
      const float left = (float)across.apply(data[i].x);
      const float to = (float)up.apply(data[i].y);
      rects[i] =
          SkRect::MakeLTRB(std::min(left, left + width), std::min(from, to),
                           std::max(left, left + width), std::max(from, to));
    }
    return rects;
  }
};

/** The container's own name: the plot's key, the class it dresses, and
 *  where in the run the layer stood. */
std::string named(std::string_view key, std::string_view styleClass,
                  std::size_t index) {
  return std::string(key) + "-" + std::string(styleClass) +
         std::to_string(index);
}

}  // namespace

Layer anchored(std::vector<Datum> data, std::vector<Element> children,
               const Anchor& anchor, std::string_view styleClass) {
  return [data = std::move(data), children = std::move(children), anchor,
          cls = std::string(styleClass)](
             const Plot& frame, std::string_view key, std::size_t index) {
    Element field = compose::layout(Anchored{frame, data, anchor})
                        .styleClass(cls)
                        .absolute()
                        .inset(0);
    if (!key.empty()) field.key(named(key, cls, index));
    if (!children.empty()) field.children({children});
    return field;
  };
}

Layer banded(std::vector<Datum> data, double base, float corners,
             compose::kit::Part<std::size_t, double> part,
             std::string_view styleClass) {
  return [data = std::move(data), base, corners, part,
          cls = std::string(styleClass)](
             const Plot& frame, std::string_view key, std::size_t index) {
    const std::string stem = named(key, cls, index);
    // A polar band's own shape needs no box: its two angles come from the
    // angle scale, whose range is the stated sweep, and its two radii are
    // fractions of whatever the outer radius turns out to be.
    const data::Scale angles = frame.scale(Axis::X, SkSize::MakeEmpty());
    const double sweep = frame.polar ? bandOf(angles) : 0.0;
    std::vector<Element> children;
    children.reserve(data.size());
    for (std::size_t i = 0; i < data.size(); ++i) {
      Element one = part ? part(i, data[i].y)
                         : compose::box().fill(compose::Fill::currentInk());
      if (corners > 0) one.corners(compose::Corners{corners});
      if (frame.polar) {
        const double reach = frame.radiusFraction(data[i].y);
        const float inner =
            reach != 0.0 ? (float)std::clamp(frame.radiusFraction(base) / reach,
                                             0.0, 0.999)
                         : 0.0f;
        one.shape(geometry::shapes::sector((float)angles.apply(data[i].x),
                                           (float)sweep, inner));
      }
      children.push_back(std::move(one.key(stem + "-" + std::to_string(i))));
    }
    Element field = compose::layout(Spanned{frame, data, base})
                        .styleClass(cls)
                        .absolute()
                        .inset(0)
                        .key(stem);
    if (!children.empty()) field.children({std::move(children)});
    return field;
  };
}

}  // namespace sigil::sketch::kit::detail

namespace sigil::sketch::kit {

Layer label(compose::Utf8 words, double x, double y, const Label& how) {
  return [words = std::move(words), x, y, how](
             const Plot& frame, std::string_view key, std::size_t index) {
    const Layer one =
        detail::anchored({Datum{x, y}}, {compose::text(words)}, how.anchor,
                         detail::classOf(how.styleClass, "label"));
    return one(frame, key, index);
  };
}

}  // namespace sigil::sketch::kit
