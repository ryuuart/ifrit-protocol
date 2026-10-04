// The manuscript border's drawing, painted with Skia's canvas: each piece
// sets its own paint and path, so a change to one leaves the others as
// they are.
#include "Ornament.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkDashPathEffect.h>
#include <include/effects/SkPerlinNoiseShader.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmedia/advanced/Skia.h>

#include <cmath>
#include <vector>

namespace nine_slice {

namespace {

/** Small gilded diamond, the reference's corner stud. */
void drawDiamond(SkCanvas& c, SkPoint at, float r, material::Color color) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(material::skia::toSkColor(color), nullptr);
  SkPathBuilder d;
  d.moveTo(at.x(), at.y() - r);
  d.lineTo(at.x() + r, at.y());
  d.lineTo(at.x(), at.y() + r);
  d.lineTo(at.x() - r, at.y());
  d.close();
  c.drawPath(d.detach(), p);
}

}  // namespace

/** Draws a carved dialog frame onto an intermediate canvas and hands
 *  back the texture: rounded wood band, gilded trim, corner bosses,
 *  edge studs, translucent parchment center. The nine-slice source —
 *  generate once per palette, stretch everywhere. */
std::shared_ptr<const sigil::media::Image> makeCarvedFrame(const Palette& pal,
                                                           int size) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size, size));
  SkCanvas& c = *surface->getCanvas();
  c.clear(SK_ColorTRANSPARENT);
  const float s = (float)size;

  SkPaint p;
  p.setAntiAlias(true);

  // Parchment center (stretches under content).
  material::Color ground = pal.parchment;
  ground.a = 0.96f;
  p.setColor4f(material::skia::toSkColor(ground), nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(5, 5, s - 5, s - 5), s * 0.14f, s * 0.14f,
                  p);

  // Wood band.
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(s * 0.10f);
  p.setColor4f(material::skia::toSkColor(pal.stem), nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.07f, s * 0.07f, s * 0.93f, s * 0.93f),
                  s * 0.16f, s * 0.16f, p);

  // Gilded trims inside and outside the band.
  p.setStrokeWidth(1.6f);
  p.setColor4f(material::skia::toSkColor(pal.gold), nullptr);
  c.drawRoundRect(
      SkRect::MakeLTRB(s * 0.135f, s * 0.135f, s * 0.865f, s * 0.865f),
      s * 0.10f, s * 0.10f, p);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.02f, s * 0.02f, s * 0.98f, s * 0.98f),
                  s * 0.20f, s * 0.20f, p);

  // Corner bosses: darker disc + gilded diamond stud.
  p.setStyle(SkPaint::kFill_Style);
  const float bossAt[4][2] = {{s * 0.13f, s * 0.13f},
                              {s * 0.87f, s * 0.13f},
                              {s * 0.87f, s * 0.87f},
                              {s * 0.13f, s * 0.87f}};
  for (auto& at : bossAt) {
    material::Color dark = pal.stem;
    dark.r *= 0.6f;
    dark.g *= 0.6f;
    dark.b *= 0.6f;
    p.setColor4f(material::skia::toSkColor(dark), nullptr);
    c.drawCircle(at[0], at[1], s * 0.085f, p);
    drawDiamond(c, {at[0], at[1]}, s * 0.042f, pal.gold);
  }

  // Edge studs at the halves (in the stretchable bands).
  p.setColor4f(material::skia::toSkColor(pal.gold), nullptr);
  c.drawCircle(s * 0.5f, s * 0.075f, 2.6f, p);
  c.drawCircle(s * 0.5f, s * 0.925f, 2.6f, p);
  c.drawCircle(s * 0.075f, s * 0.5f, 2.6f, p);
  c.drawCircle(s * 0.925f, s * 0.5f, 2.6f, p);

  return sigil::media::Image::of(surface->makeImageSnapshot());
}

}  // namespace nine_slice
