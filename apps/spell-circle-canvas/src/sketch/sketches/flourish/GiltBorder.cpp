// The gilt border's drawing: the muted parchment ground, the acanthus leaf
// with its veins and the bead chain, painted with Skia's canvas.
#include "GiltBorder.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/effects/SkPerlinNoiseShader.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/path/Skia.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/paint/Bases.h>

namespace flourish {

Fill flourishParchment(const FlourishStyle& s, float freq) {
  sk_sp<SkShader> noise = SkShaders::MakeFractalNoise(freq, freq, 3, 7.0f);
  if (!noise) return Fill::color(s.parchment);
  sk_sp<SkShader> muted = SkShaders::Blend(
      SkBlendMode::kLuminosity,
      SkShaders::Color(SkColorSetARGB(255, 128, 128, 128)), std::move(noise));
  return Fill{material::skia::base(material::skia::paint(SkShaders::Blend(
      SkBlendMode::kSoftLight,
      SkShaders::Color(material::skia::toSkColor(s.parchment).toSkColor()),
      std::move(muted))))};
}

Shape leafOutline() {
  return [](glm::vec2 size) {
    const float w = size.x, h = size.y;
    SkPathBuilder b;
    b.moveTo(w * 0.06f, h * 0.5f);
    b.quadTo(w * 0.42f, h * 0.04f, w * 0.96f, h * 0.5f);
    b.quadTo(w * 0.42f, h * 0.96f, w * 0.06f, h * 0.5f);
    b.close();
    return geometry::path::fromSk(b.detach());
  };
}

Element acanthusLeaf(const FlourishStyle& s, float w,
                            float h) {
  ContourWalk veins;  // recursion level 2: the stamp walks its own contour
  veins.spacing = 4.0f;
  const material::Color bead = s.goldBright;
  veins.draw = [bead](sigil::draw::Pen& pen) {
    SkCanvas& c = *pen.canvas();
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f(material::skia::toSkColor(bead), nullptr);
    c.drawCircle(0, 0, 0.7f, p);
  };
  const material::Color rib = s.goldBright;
  Decoration midrib{PaintProgram([rib](sigil::draw::Pen& pen, const PaintContext& ctx) {
    SkCanvas& c = *pen.canvas();
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(1.1f);
    p.setColor4f(material::skia::toSkColor(rib), nullptr);
    c.drawLine(ctx.size.x * 0.1f, ctx.size.y * 0.5f,
               ctx.size.x * 0.92f, ctx.size.y * 0.5f, p);
  })};
  return box()
      .width(w)
      .height(h)
      .shape(leafOutline())
      .fill(material::linearGradient(
          {0, 0}, {w, h}, {s.leaf, s.bronze},
          {.units = material::GradientUnits::Pixels}))
      .foreground(sigil::compose::stroke(1.1f, Fill::color(s.goldBright)))
      .foreground(midrib)
      .foreground(veins);
}

PathFormat beadChain(material::Color color, float advance,
                            float r) {
  SkPathBuilder bead;
  bead.moveTo(0, -r);
  bead.lineTo(r, 0);
  bead.lineTo(0, r);
  bead.lineTo(-r, 0);
  bead.close();
  PathFormat f;
  f.width = 1.0f;
  f.strokeFill = Fill::color(color);
  f.stampPath = geometry::path::fromSk(bead.detach());
  f.stampAdvance = advance;
  return f;
}

}  // namespace flourish
