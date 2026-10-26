/** @file
 * Every shape the pen draws: p5's primitives, the vertex shapes, the
 * silhouettes and meshes this library adds, and the dash the outlines
 * wear.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkClipOp.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkVertices.h>
#include <include/effects/SkDashPathEffect.h>
#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Color.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <string_view>

#include "PenInternal.h"

namespace sigil::draw {

using detail::normalizeArc;

// ---- shapes -----------------------------------------------------------------

void Pen::paintFilled(const SkPath& path) {
  if (recordShape(path)) return;
  const SkRect box = path.getBounds();
  if (const SkPaint* fill = fillPaint(&box)) m_canvas->drawPath(path, *fill);
  if (const SkPaint* stroke = strokePaint(&box))
    m_canvas->drawPath(path, *stroke);
}

void Pen::paintOval(const SkRect& oval) {
  // A path only for a mask being recorded: an oval and a rect are drawn
  // by Skia's own words, and a path built for nobody to read is a heap
  // allocation on every mark.
  if (m_clipRecording) {
    recordShape(SkPath::Oval(oval));
    return;
  }
  if (const SkPaint* fill = fillPaint(&oval)) m_canvas->drawOval(oval, *fill);
  if (const SkPaint* stroke = strokePaint(&oval))
    m_canvas->drawOval(oval, *stroke);
}

void Pen::paintRect(const SkRect& rect) {
  if (m_clipRecording) {
    recordShape(SkPath::Rect(rect));
    return;
  }
  if (const SkPaint* fill = fillPaint(&rect)) m_canvas->drawRect(rect, *fill);
  if (const SkPaint* stroke = strokePaint(&rect))
    m_canvas->drawRect(rect, *stroke);
}

void Pen::vertices(const sk_sp<SkVertices>& mesh) {
  // A mesh carries no outline, so like a line and an image it adds nothing
  // to a mask being recorded, and there is nothing on it to stroke.
  if (!m_canvas || !mesh || m_clipRecording) return;
  const SkRect box = mesh->bounds();
  const SkPaint* fill = fillPaint(&box);
  if (!fill) return;
  // kDst is how the mesh's own corner colours meet the paint's: keep the
  // paint's. Skia ignores it where the paint has no shader, which is
  // exactly where the corner colours are what a caller meant — the same
  // rule the per-corner form of `vertex` follows.
  m_canvas->drawVertices(mesh, SkBlendMode::kDst, *fill);
}

void Pen::shape(const SkPath& path) {
  if (!m_canvas) return;
  paintFilled(path);
}

void Pen::point(float x, float y) {
  if (!m_canvas) return;
  const SkPoint at = {x, y};
  paintPoints({&at, 1});
}

void Pen::paintPoints(std::span<const SkPoint> centres) {
  const float radius = m_style.strokeWeight / 2.0f;
  if (m_clipRecording) {
    for (const SkPoint& centre : centres)
      recordShape(SkPath::Circle(centre.x(), centre.y(), radius));
    return;
  }
  // p5 draws a point as a disc of the stroke weight in the stroke colour.
  // A disc is not a stroke, so a dash has nothing to break up here. The
  // paint is the stroke's as it stands, made a fill once for the run; a
  // stroke fitted to each shape's box is fitted to each disc's.
  const bool fittedToEachDisc = m_style.strokeFitted &&
                                !m_style.stroke.isSolid() &&
                                !m_style.stroke.isNone();
  SkPaint dot;
  bool resolved = false;
  for (const SkPoint& centre : centres) {
    if (fittedToEachDisc || !resolved) {
      const SkRect disc =
          SkRect::MakeLTRB(centre.x() - radius, centre.y() - radius,
                           centre.x() + radius, centre.y() + radius);
      const SkPaint* stroke = strokePaint(&disc);
      if (!stroke) return;
      dot = *stroke;
      dot.setStyle(SkPaint::kFill_Style);
      dot.setPathEffect(nullptr);
      resolved = true;
    }
    // One draw per disc whatever the run: Skia's own run of round points
    // is a stroked zero-length line on a device and a path on raster,
    // neither of which is the disc a single point draws.
    m_canvas->drawCircle(centre.x(), centre.y(), radius, dot);
  }
}

void Pen::paintSegments(std::span<const SkPoint> ends) {
  // A line has no inside, so it adds nothing to a mask being recorded.
  if (!m_canvas || m_clipRecording) return;
  const size_t count = ends.size() & ~size_t{1};
  if (count == 0) return;
  // ONE DRAW FOR THE RUN, which is the picture a draw per pair makes:
  // Skia draws a single line as a run of two points and strokes every
  // pair of a run on its own, so overlaps meet exactly as separate lines
  // do. Two things are per pair and keep the pair's own draw: a stroke
  // fitted to each segment's box, and a dash, which Skia lays out
  // differently for a lone segment.
  const bool perPair = (m_style.strokeFitted && !m_style.stroke.isSolid() &&
                        !m_style.stroke.isNone()) ||
                       m_style.dash != nullptr;
  if (perPair) {
    for (size_t i = 0; i < count; i += 2)
      line(ends[i].x(), ends[i].y(), ends[i + 1].x(), ends[i + 1].y());
    return;
  }
  if (const SkPaint* stroke = strokePaint())
    m_canvas->drawPoints(SkCanvas::kLines_PointMode,
                         SkSpan<const SkPoint>(ends.data(), count), *stroke);
}

void Pen::line(float x1, float y1, float x2, float y2) {
  // A line has no inside, so it adds nothing to a mask being recorded.
  if (!m_canvas || m_clipRecording) return;
  const SkRect span = SkRect::MakeLTRB(std::min(x1, x2), std::min(y1, y2),
                                       std::max(x1, x2), std::max(y1, y2));
  if (const SkPaint* stroke = strokePaint(&span))
    m_canvas->drawLine(x1, y1, x2, y2, *stroke);
}

void Pen::rect(float x, float y, float width, float height) {
  if (!m_canvas) return;
  paintRect(rectBox(x, y, width, height));
}

void Pen::rect(float x, float y, float width, float height, float radius) {
  rect(x, y, width, height, radius, radius, radius, radius);
}

void Pen::rect(float x, float y, float width, float height, float topLeft,
               float topRight, float bottomRight, float bottomLeft) {
  if (!m_canvas) return;
  const SkRect box = rectBox(x, y, width, height);
  const SkVector radii[4] = {{topLeft, topLeft},
                             {topRight, topRight},
                             {bottomRight, bottomRight},
                             {bottomLeft, bottomLeft}};
  SkRRect rounded;
  rounded.setRectRadii(box, radii);
  if (recordShape(SkPath::RRect(rounded))) return;
  if (const SkPaint* fill = fillPaint(&box))
    m_canvas->drawRRect(rounded, *fill);
  if (const SkPaint* stroke = strokePaint(&box))
    m_canvas->drawRRect(rounded, *stroke);
}

void Pen::square(float x, float y, float size) { rect(x, y, size, size); }
void Pen::square(float x, float y, float size, float radius) {
  rect(x, y, size, size, radius);
}
void Pen::square(float x, float y, float size, float topLeft, float topRight,
                 float bottomRight, float bottomLeft) {
  rect(x, y, size, size, topLeft, topRight, bottomRight, bottomLeft);
}

void Pen::ellipse(float x, float y, float width) {
  ellipse(x, y, width, width);
}

void Pen::ellipse(float x, float y, float width, float height) {
  if (!m_canvas) return;
  paintOval(
      boxIn(m_style.ellipseMode, x, y, std::fabs(width), std::fabs(height)));
}

void Pen::circle(float x, float y, float diameter) {
  ellipse(x, y, diameter, diameter);
}

void Pen::arc(float x, float y, float width, float height, float start,
              float stop, Constant mode) {
  if (!m_canvas) return;
  width = std::fabs(width);
  height = std::fabs(height);
  if (!(width > 0.0f) || !(height > 0.0f)) return;
  const SkRect oval = boxIn(m_style.ellipseMode, x, y, width, height);
  float from = toRadians(start);
  float to = toRadians(stop);
  bool samePoint = false;
  normalizeArc(from, to, oval.width(), oval.height(), samePoint);
  if (samePoint) {
    paintOval(oval);
    return;
  }
  const float startDegrees = degrees(from);
  const float sweepDegrees = degrees(to - from);
  if (m_clipRecording) {
    SkPathBuilder wedge;
    if (mode == PIE) wedge.moveTo(oval.centerX(), oval.centerY());
    wedge.addArc(oval, startDegrees, sweepDegrees);
    wedge.close();
    recordShape(wedge.detach());
    return;
  }
  if (const SkPaint* fill = fillPaint(&oval))
    m_canvas->drawArc(oval, startDegrees, sweepDegrees, mode != CHORD, *fill);
  if (const SkPaint* stroke = strokePaint(&oval)) {
    if (mode == CHORD) {
      SkPathBuilder chord;
      chord.addArc(oval, startDegrees, sweepDegrees);
      chord.close();
      m_canvas->drawPath(chord.detach(), *stroke);
    } else {
      m_canvas->drawArc(oval, startDegrees, sweepDegrees, mode == PIE, *stroke);
    }
  }
}

void Pen::triangle(float x1, float y1, float x2, float y2, float x3, float y3) {
  if (!m_canvas) return;
  SkPathBuilder path;
  path.moveTo(x1, y1).lineTo(x2, y2).lineTo(x3, y3).close();
  paintFilled(path.detach());
}

void Pen::quad(float x1, float y1, float x2, float y2, float x3, float y3,
               float x4, float y4) {
  if (!m_canvas) return;
  SkPathBuilder path;
  path.moveTo(x1, y1).lineTo(x2, y2).lineTo(x3, y3).lineTo(x4, y4).close();
  paintFilled(path.detach());
}

void Pen::bezier(float x1, float y1, float x2, float y2, float x3, float y3,
                 float x4, float y4) {
  if (!m_canvas) return;
  SkPathBuilder path;
  path.moveTo(x1, y1).cubicTo(x2, y2, x3, y3, x4, y4);
  paintFilled(path.detach());
}

void Pen::curve(float x1, float y1, float x2, float y2, float x3, float y3,
                float x4, float y4) {
  if (!m_canvas) return;
  // Catmull-Rom to its cubic: the tangents at the two inner points are
  // a sixth of the chord across their neighbours, scaled by the slack
  // the tightness leaves.
  const float s = 1.0f - m_style.curveTightness;
  SkPathBuilder path;
  path.moveTo(x2, y2).cubicTo(
      x2 + s * (x3 - x1) / 6.0f, y2 + s * (y3 - y1) / 6.0f,
      x3 + s * (x2 - x4) / 6.0f, y3 + s * (y2 - y4) / 6.0f, x3, y3);
  paintFilled(path.detach());
}

void Pen::curveTightness(float amount) { m_style.curveTightness = amount; }

void Pen::beginShape(Constant kind) {
  m_shapeKind = kind;
  m_path = SkPathBuilder();
  m_hasPoint = false;
  m_newContour = false;
  m_vertices.clear();
  m_vertexColors.clear();
  m_vertexColorsVary = false;
  m_vertexFillsSolid = true;
  m_curve.clear();
}

void Pen::flushCurve() { m_curve.clear(); }

void Pen::vertex(float x, float y) {
  flushCurve();
  if (m_shapeKind != POLYGON) {
    m_vertices.push_back({x, y});
    // Points and lines wear the stroke, so their corners carry no fill.
    if (m_shapeKind == POINTS || m_shapeKind == LINES) return;
    // The fill AT THIS MOMENT, so a fill() between two vertex() calls
    // colours the corners either side of it differently. A fill that is
    // not a solid carries no per-corner colour, and one such corner
    // sends the whole shape down the path route, where the shader is
    // what draws it.
    if (!m_style.fill.isSolid()) {
      m_vertexFillsSolid = false;
      m_vertexColors.push_back(SK_ColorTRANSPARENT);
      return;
    }
    const SkColor packed =
        material::skia::toSkColor(m_style.fill.solidColor()).toSkColor();
    if (!m_vertexColors.empty() && packed != m_vertexColors.front())
      m_vertexColorsVary = true;
    m_vertexColors.push_back(packed);
    return;
  }
  if (!m_hasPoint || m_newContour) {
    m_path.moveTo(x, y);
    m_hasPoint = true;
    m_newContour = false;
  } else {
    m_path.lineTo(x, y);
  }
}

void Pen::curveVertex(float x, float y) {
  m_curve.push_back({x, y});
  if (m_curve.size() < 4) return;
  // Every four consecutive points draw the segment between the middle
  // two; the run's first segment starts at its second point, which is
  // where p5 starts it.
  const size_t n = m_curve.size();
  const SkPoint p0 = m_curve[n - 4];
  const SkPoint p1 = m_curve[n - 3];
  const SkPoint p2 = m_curve[n - 2];
  const SkPoint p3 = m_curve[n - 1];
  if (n == 4) {
    if (!m_hasPoint || m_newContour) {
      m_path.moveTo(p1);
      m_hasPoint = true;
      m_newContour = false;
    } else {
      m_path.lineTo(p1);
    }
  }
  const float s = 1.0f - m_style.curveTightness;
  m_path.cubicTo(p1.x() + s * (p2.x() - p0.x()) / 6.0f,
                 p1.y() + s * (p2.y() - p0.y()) / 6.0f,
                 p2.x() + s * (p1.x() - p3.x()) / 6.0f,
                 p2.y() + s * (p1.y() - p3.y()) / 6.0f, p2.x(), p2.y());
}

void Pen::bezierVertex(float x2, float y2, float x3, float y3, float x4,
                       float y4) {
  flushCurve();
  if (!m_hasPoint) return;
  m_path.cubicTo(x2, y2, x3, y3, x4, y4);
}

void Pen::quadraticVertex(float cx, float cy, float x3, float y3) {
  flushCurve();
  if (!m_hasPoint) return;
  m_path.quadTo(cx, cy, x3, y3);
}

void Pen::beginContour() {
  flushCurve();
  m_newContour = true;
}

void Pen::endContour() {
  flushCurve();
  if (m_hasPoint) m_path.close();
  m_newContour = true;
}

void Pen::paintVertices(std::span<const SkPoint> positions,
                        std::span<const SkColor> colors) {
  if (!m_canvas || positions.empty()) return;
  const sk_sp<SkVertices> mesh =
      SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode,
                           (int)positions.size(), positions.data(),
                           /*texs=*/nullptr, colors.data());
  if (!mesh) return;
  SkPaint paint;
  paint.setAntiAlias(m_style.antiAlias);
  // The paint's blend is how the mesh meets the canvas; kDst is a
  // different question — how the corner colours meet the paint's own —
  // and keeping them is what a paint with no shader wants.
  blendInto(paint);
  m_canvas->drawVertices(mesh, SkBlendMode::kDst, paint);
}

void Pen::emitKind(std::span<const SkPoint> v) {
  const size_t n = v.size();
  SkPathBuilder path;
  // A MESH ONLY WHERE THE CORNERS DISAGREE. One fill across the shape is
  // a path, which is what strokes, what a shader fills and what every
  // shape drawn before this distinction was drawn as.
  // A mask has no colours, so a shape recorded into one is a path
  // whatever its corners say.
  const bool mesh = m_vertexColorsVary && m_vertexFillsSolid &&
                    m_vertexColors.size() == n && m_style.doFill &&
                    !m_clipRecording;
  std::vector<SkPoint> meshPositions;
  std::vector<SkColor> meshColors;
  auto closed = [&](std::initializer_list<size_t> ring) {
    bool first = true;
    for (size_t i : ring) {
      if (first) {
        path.moveTo(v[i]);
        first = false;
      } else {
        path.lineTo(v[i]);
      }
    }
    path.close();
    if (!mesh) return;
    // The ring fanned from its first corner: one triangle for three
    // corners, two for four.
    const std::vector<size_t> corners(ring);
    for (size_t k = 1; k + 1 < corners.size(); ++k)
      for (size_t i : {corners[0], corners[k], corners[k + 1]}) {
        meshPositions.push_back(v[i]);
        meshColors.push_back(m_vertexColors[i]);
      }
  };
  switch (m_shapeKind) {
    case POINTS:
      paintPoints(v);
      return;
    case LINES:
      paintSegments(v);
      return;
    case TRIANGLES:
      for (size_t i = 0; i + 2 < n; i += 3) closed({i, i + 1, i + 2});
      break;
    case TRIANGLE_STRIP:
      for (size_t i = 0; i + 2 < n; ++i) closed({i, i + 1, i + 2});
      break;
    case TRIANGLE_FAN:
      for (size_t i = 1; i + 1 < n; ++i) closed({0, i, i + 1});
      break;
    case QUADS:
      for (size_t i = 0; i + 3 < n; i += 4) closed({i, i + 1, i + 2, i + 3});
      break;
    case QUAD_STRIP:
      for (size_t i = 0; i + 3 < n; i += 2) closed({i, i + 1, i + 3, i + 2});
      break;
    default:
      return;
  }
  if (!mesh) {
    paintFilled(path.detach());
    return;
  }
  paintVertices(meshPositions, meshColors);
  // The outline still belongs to the shape, so a stroked mesh is
  // stroked ring by ring exactly as the path form is.
  if (const SkPaint* stroke = strokePaint())
    m_canvas->drawPath(path.detach(), *stroke);
}

void Pen::endShape(Constant mode) {
  if (!m_canvas) return;
  flushCurve();
  if (m_shapeKind != POLYGON) {
    emitKind(m_vertices);
    m_vertices.clear();
    m_vertexColors.clear();
    m_vertexColorsVary = false;
    m_vertexFillsSolid = true;
    return;
  }
  if (!m_hasPoint) return;
  if (mode == CLOSE) m_path.close();
  paintFilled(m_path.detach());
  m_hasPoint = false;
}

}  // namespace sigil::draw
