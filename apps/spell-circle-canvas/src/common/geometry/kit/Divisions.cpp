/** @file
 * The division ladders as settings of the radial arrangement: ticks,
 * arc segments and chords.
 */

#include "sigilgeometry/kit/Divisions.h"

#include <algorithm>
#include <vector>

namespace sigil::geometry::shapes {

namespace {
path::Mark markOf(const Span& span, float width) {
  return width > 0.0f ? path::Mark::bar(width, span.inner, span.outer)
                      : path::Mark::line(span.inner, span.outer);
}
}  // namespace

path::RadialOptions radialOf(const Ticks& t, path::PolarFrame conventions) {
  path::RadialOptions options;
  options.fromDegrees = t.from;
  options.sweepDegrees = t.sweep;
  options.closed = t.closed;
  options.connect = path::Connect::None;
  options.uniform = true;
  options.frame = conventions;
  if (t.longEvery > 0) {
    options.marks.assign((size_t)t.longEvery, markOf(t.mark, t.markPx));
    options.marks.front() = markOf(t.longMark, t.markPx);
  } else {
    options.marks = {markOf(t.mark, t.markPx)};
  }
  if (t.classify) {
    options.each = [classify = t.classify](int index, path::Mark mark) {
      const Span span = classify(index, Span{mark.inner, mark.outer});
      mark.inner = span.inner;
      mark.outer = span.outer;
      return mark;
    };
  }
  return options;
}

path::RadialOptions radialOf(const Arcs& a, path::PolarFrame conventions) {
  path::RadialOptions options;
  options.fromDegrees = a.from;
  options.sweepDegrees = a.sweep;
  options.closed = a.closed;
  options.connect = path::Connect::None;
  options.marks = {path::Mark::segment(a.spanDeg, a.mark.inner, a.mark.outer)};
  options.uniform = true;
  options.frame = conventions;
  return options;
}

path::RadialOptions radialOf(const Chords& c, path::PolarFrame conventions) {
  path::RadialOptions options;
  options.radii = {c.radius};
  options.fromDegrees = c.from;
  options.skip = std::max(1, c.step);
  options.connect = c.closed ? path::Connect::Loop : path::Connect::Each;
  options.inset = c.inset;
  options.uniform = true;
  options.frame = conventions;
  return options;
}

path::Outline ticks(const path::PolarFrame& frame, const Ticks& t) {
  return path::radialOutline(t.divisions, radialOf(t, frame), frame);
}

path::Outline arcs(const path::PolarFrame& frame, const Arcs& a) {
  return path::radialOutline(a.divisions, radialOf(a, frame), frame);
}

path::Outline chords(const path::PolarFrame& frame, const Chords& c) {
  return path::radialOutline(std::max(2, c.sides), radialOf(c, frame), frame);
}

}  // namespace sigil::geometry::shapes
