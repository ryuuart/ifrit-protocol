#pragma once
// A path read back as its on-curve points in order and a census of its
// verbs, for the cases that assert the geometry a router or a brush built.

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Outline.h>

#include <vector>

namespace {

/** The path's line-verb skeleton: every on-curve point in order, with the
 *  verb census alongside — the geometry assertions below read this. */
struct PathDump {
  std::vector<SkPoint> pts;
  int moves = 0, lines = 0, curves = 0, closes = 0;
};

PathDump dumpPath(const SkPath& p) {
  PathDump d;
  SkPath::Iter it(p, false);
  SkPoint v[4];
  for (SkPath::Verb verb; (verb = it.next(v)) != SkPath::kDone_Verb;) {
    switch (verb) {
      case SkPath::kMove_Verb:
        ++d.moves;
        d.pts.push_back(v[0]);
        break;
      case SkPath::kLine_Verb:
        ++d.lines;
        d.pts.push_back(v[1]);
        break;
      case SkPath::kQuad_Verb:
      case SkPath::kConic_Verb:
        ++d.curves;
        d.pts.push_back(v[2]);
        break;
      case SkPath::kCubic_Verb:
        ++d.curves;
        d.pts.push_back(v[3]);
        break;
      case SkPath::kClose_Verb:
        ++d.closes;
        break;
      default:
        break;
    }
  }
  return d;
}

PathDump dumpPath(const sigil::geometry::path::Outline& outline) {
  return dumpPath(sigil::geometry::path::toSk(outline));
}

}  // namespace
