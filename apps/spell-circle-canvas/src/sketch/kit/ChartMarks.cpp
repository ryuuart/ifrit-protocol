#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
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

/** THE BOUNDS OF A SECTOR of outer radius 1 about its own hub at the
 *  origin: the two arcs' ends, the inner corner, and ±1 on whichever axes
 *  the sweep crosses. Exact rather than measured off a path, because it is
 *  read at describe time to shape the wedge and at layout to place it, and
 *  the two must agree. */
SkRect sectorBounds(double startDeg, double sweepDeg, double innerRatio) {
  const double from = sweepDeg >= 0 ? startDeg : startDeg + sweepDeg;
  const double to = from + std::abs(sweepDeg);
  const auto at = [](double deg, double r) {
    return SkPoint{(float)(r * std::cos(radians(deg))),
                   (float)(r * std::sin(radians(deg)))};
  };
  const SkPoint corners[4] = {at(from, 1.0), at(to, 1.0), at(from, innerRatio),
                              at(to, innerRatio)};
  SkRect bounds = SkRect::MakeEmpty();
  bounds.setBounds(corners);
  // A sweep that crosses a cardinal direction reaches the rim on that axis
  // whatever its ends do, and one that does not is bounded by its ends.
  for (int quarter = -4; quarter <= 8; ++quarter) {
    const double cardinal = quarter * 90.0;
    if (cardinal < from || cardinal > to) continue;
    bounds.join(
        SkRect::MakeXYWH(at(cardinal, 1.0).fX, at(cardinal, 1.0).fY, 0, 0));
  }
  return bounds;
}

/** THE WEDGE A DATUM OWNS, DRAWN IN ITS OWN BOUNDS — the sector re-based
 *  so that the box it is laid out in is the box it fills, with the hub
 *  wherever outside that box it falls.
 *
 *  A wedge inscribed in the whole disc is a bake nine parts transparent,
 *  and a wheel of seventy-two of them pays that per wedge per entrance
 *  frame. It is a comparable value and not a callable, because a shaped
 *  node prunes only while the reconciler can prove the shape is the same
 *  one. */
struct Wedge {
  geometry::shapes::Sector sector;
  /** The sector's own bounds in units of the outer radius, about the hub —
   *  which is what says where the hub stands in the box below. */
  SkRect unit{0, 0, 0, 0};
  bool operator==(const Wedge&) const = default;

  SkPath path(SkSize box) const {
    const float span = std::max(unit.width(), unit.height());
    if (!(span > 0)) return SkPath();
    const float outer =
        unit.width() >= unit.height()
            ? (unit.width() > 0 ? box.width() / unit.width() : 0.0f)
            : (unit.height() > 0 ? box.height() / unit.height() : 0.0f);
    // The generator strikes the sector from the middle of the box it is
    // given; this box's own middle is not the hub.
    return sector.path(SkSize{2 * outer, 2 * outer})
        .makeTransform(SkMatrix::Translate(-unit.fLeft * outer - outer,
                                           -unit.fTop * outer - outer));
  }
};

/** THE BAND EACH DATUM OWNS, DRAWN OUT TO ITS VALUE: on a Cartesian frame
 *  the box from the base to the value across the band scale's own band; on
 *  a polar one the bounds of the wedge itself, struck from the hub. */
struct Spanned {
  Plot frame;
  std::vector<Datum> data;
  std::vector<SkRect> units;  ///< one per polar datum, in units of the radius
  double base = 0.0;
  Axis along = Axis::X;

  std::vector<SkRect> place(const LayoutInput& in) const {
    std::vector<SkRect> rects(in.childSizes.size());
    if (frame.polar) {
      const SkPoint hub = frame.centre(in.container);
      const float outer = frame.radius(in.container);
      for (std::size_t i = 0; i < rects.size() && i < data.size(); ++i) {
        const float r = (float)frame.radiusFraction(data[i].y) * outer;
        const SkRect unit = i < units.size() ? units[i] : SkRect::MakeEmpty();
        rects[i] = SkRect::MakeLTRB(
            hub.fX + unit.fLeft * r, hub.fY + unit.fTop * r,
            hub.fX + unit.fRight * r, hub.fY + unit.fBottom * r);
      }
      return rects;
    }
    // The scale the bands run along hands out the band; the other one
    // carries the value, and the base is a value on THAT one.
    const data::Scale banding = frame.scale(along, in.container);
    const data::Scale valued =
        frame.scale(along == Axis::X ? Axis::Y : Axis::X, in.container);
    const float width = (float)bandOf(banding);
    const float from = (float)valued.apply(base);
    for (std::size_t i = 0; i < rects.size() && i < data.size(); ++i) {
      const float start =
          (float)banding.apply(along == Axis::X ? data[i].x : data[i].y);
      const float to =
          (float)valued.apply(along == Axis::X ? data[i].y : data[i].x);
      rects[i] = along == Axis::X
                     ? SkRect::MakeLTRB(
                           std::min(start, start + width), std::min(from, to),
                           std::max(start, start + width), std::max(from, to))
                     : SkRect::MakeLTRB(
                           std::min(from, to), std::min(start, start + width),
                           std::max(from, to), std::max(start, start + width));
    }
    return rects;
  }
};

/** THE LINE ACROSS A BOX, corner to corner — the shape a segment placed
 *  at the bounds of its own two ends is drawn as. Which diagonal it is
 *  falls out of the two points' order, which is why the scheme below
 *  hands it over rather than leaving the caller to work it out. */
struct Chord {
  bool rising = false;
  bool operator==(const Chord&) const = default;

  SkPath path(SkSize box) const {
    SkPathBuilder line;
    line.moveTo(0, rising ? box.height() : 0);
    line.lineTo(box.width(), rising ? 0 : box.height());
    return line.detach();
  }
};

/** ONE CHILD PER PAIR OF ENDS, each at the bounds of its own segment. A
 *  run of chords costs its own pixels rather than a full field's each,
 *  which is what a plate of thirty-six of them is paying for. */
struct Between {
  Plot frame;
  std::vector<Datum> ends;  ///< two per child, in order

  std::vector<SkRect> place(const LayoutInput& in) const {
    std::vector<SkRect> rects(in.childSizes.size());
    for (std::size_t i = 0; i < rects.size() && 2 * i + 1 < ends.size(); ++i) {
      const SkPoint from = frame.at(ends[2 * i].x, ends[2 * i].y, in.container);
      const SkPoint to =
          frame.at(ends[2 * i + 1].x, ends[2 * i + 1].y, in.container);
      // A segment along an axis still needs a box to be stroked in, so
      // neither side is allowed to close to nothing.
      rects[i] = SkRect::MakeLTRB(
          std::min(from.fX, to.fX), std::min(from.fY, to.fY),
          std::max(from.fX, to.fX) + 0.01f, std::max(from.fY, to.fY) + 0.01f);
    }
    return rects;
  }
};

/** The container's own name: the plot's key, the word for the part it
 *  is, and where in the run the layer stood. */
std::string named(std::string_view key, std::string_view word,
                  std::size_t index) {
  return std::string(key) + "-" + std::string(word) + std::to_string(index);
}

}  // namespace

Layer anchored(std::vector<Datum> data, std::vector<Element> children,
               const Anchor& anchor, std::string_view word,
               std::string_view styleClass) {
  return [data = std::move(data), children = std::move(children), anchor,
          word = std::string(word), cls = std::string(styleClass)](
             const Plot& frame, std::string_view key, std::size_t index) {
    Element field = compose::layout(Anchored{frame, data, anchor})
                        .styleClass(cls)
                        .absolute()
                        .inset(0);
    if (!key.empty()) field.key(named(key, word, index));
    if (!children.empty()) field.children({children});
    return field;
  };
}

Layer between(std::vector<Datum> ends, std::vector<Element> children,
              std::string_view word, std::string_view styleClass) {
  return [ends = std::move(ends), children = std::move(children),
          word = std::string(word), cls = std::string(styleClass)](
             const Plot& frame, std::string_view key, std::size_t index) {
    const std::string stem = named(key, word, index);
    std::vector<Element> shaped;
    shaped.reserve(children.size());
    for (std::size_t i = 0; i < children.size(); ++i) {
      Element one = children[i];
      // Which diagonal of its own box the line is: the two ends' order
      // decides it, and the frame's y runs UP, so a segment whose second
      // end is higher in the domain rises across its box.
      const bool rising =
          2 * i + 1 < ends.size() &&
          ((ends[2 * i].x <= ends[2 * i + 1].x) !=
           (frame.at(ends[2 * i].x, ends[2 * i].y, SkSize{100, 100}).fY <=
            frame.at(ends[2 * i + 1].x, ends[2 * i + 1].y, SkSize{100, 100})
                .fY));
      shaped.push_back(std::move(
          one.shape(Chord{rising}).key(stem + "-" + std::to_string(i))));
    }
    Element field = compose::layout(Between{frame, ends})
                        .styleClass(cls)
                        .absolute()
                        .inset(0)
                        .key(stem);
    if (!shaped.empty()) field.children({std::move(shaped)});
    return field;
  };
}

Layer banded(std::vector<Datum> data, double base, float corners, Axis along,
             compose::kit::Part<std::size_t, double> part,
             std::string_view word, std::string_view styleClass) {
  return [data = std::move(data), base, corners, along, part,
          word = std::string(word), cls = std::string(styleClass)](
             const Plot& frame, std::string_view key, std::size_t index) {
    const std::string stem = named(key, word, index);
    // A polar band's own shape needs no box: its two angles come from the
    // angle scale, whose range is the stated sweep, and its two radii are
    // fractions of whatever the outer radius turns out to be. Its BOUNDS
    // are the same value, which is what lets the scheme below place the
    // wedge in the room it actually occupies.
    const data::Scale angles = frame.scale(Axis::X, SkSize::MakeEmpty());
    const double sweep = frame.polar ? bandOf(angles) : 0.0;
    std::vector<Element> children;
    std::vector<SkRect> units;
    children.reserve(data.size());
    units.reserve(frame.polar ? data.size() : 0);
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
        const double start = angles.apply(data[i].x);
        const Wedge wedge{
            geometry::shapes::sector((float)start, (float)sweep, inner),
            sectorBounds(start, sweep, inner)};
        units.push_back(wedge.unit);
        // THE PIVOT STAYS AT THE HUB, wherever in or out of its own box
        // that falls, so a wedge that grows in grows out of the centre of
        // the wheel and not out of the middle of itself.
        const SkRect unit = wedge.unit;
        one.shape(wedge).transformOrigin(
            compose::pct(100.0f * (unit.width() > 0 ? -unit.fLeft / unit.width()
                                                    : 0.0f)),
            compose::pct(100.0f * (unit.height() > 0
                                       ? -unit.fTop / unit.height()
                                       : 0.0f)));
      }
      children.push_back(std::move(one.key(stem + "-" + std::to_string(i))));
    }
    Element field =
        compose::layout(Spanned{frame, data, std::move(units), base, along})
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
                         "label", detail::classOf(how.styleClass, "plotLabel"));
    return one(frame, key, index);
  };
}

}  // namespace sigil::sketch::kit
