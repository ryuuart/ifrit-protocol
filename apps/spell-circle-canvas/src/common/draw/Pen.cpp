/** @file
 * The pen itself: what a frame begins and ends, the style it carries and
 * resolves into paints, p5's colour model, the modes a verb reads, and
 * the seeded random stream. What it DRAWS is beside this — PenShapes.cpp,
 * PenClip.cpp, PenImage.cpp and PenTransform.cpp.
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

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <string_view>

#include "PenInternal.h"

namespace sigil::draw {

using detail::capOf;
using detail::fittable;
using detail::joinOf;
using detail::kDefaultSeed;
using detail::resolve;
using detail::setBlend;

Pen::Pen() : m_random(kDefaultSeed) { applyStyle(); }

Pen::~Pen() = default;

// ---- the frame --------------------------------------------------------------

void Pen::begin(SkCanvas& canvas, const Frame& frame) {
  if (m_canvas) end();
  m_canvas = &canvas;
  m_fonts = frame.fonts;
  m_saveCount = canvas.save();
  m_base = canvas.getLocalToDevice();
  const float scale = canvas.getTotalMatrix().getMaxScale();
  m_contentScale = std::isfinite(scale) && scale > 0.0f ? scale : 1.0f;
  width = frame.width;
  height = frame.height;
  frameCount = frame.frameCount;
  deltaTime = frame.deltaSeconds * 1000.0;
  m_seconds = frame.seconds;
  pmouseX = m_hadFrame ? mouseX : frame.mouseX;
  pmouseY = m_hadFrame ? mouseY : frame.mouseY;
  mouseX = frame.mouseX;
  mouseY = frame.mouseY;
  mouseIsPressed = frame.mouseIsPressed;
  keyIsPressed = frame.keyIsPressed;
  key.assign(frame.key);
  keyCode = frame.keyCode;
  m_keysDown.assign(frame.keysDown.begin(), frame.keysDown.end());
  m_hadFrame = true;
  m_stackFloor = m_stack.size();
  applyStyle();
}

void Pen::end() {
  if (!m_canvas) return;
  // A push left open at the end of a frame is closed here, back to the
  // style that stood when it opened, so one unbalanced frame cannot leak
  // a style into every frame after it.
  if (m_stack.size() > m_stackFloor) {
    m_style = m_stack[m_stackFloor];
    m_stack.resize(m_stackFloor);
    applyStyle();
  }
  m_canvas->restoreToCount(m_saveCount);
  m_canvas = nullptr;
}

bool Pen::takeRedraw() {
  const bool asked = m_redraw;
  m_redraw = false;
  return asked;
}

double Pen::frameRate() const {
  return deltaTime > 0.0 ? 1000.0 / deltaTime : 0.0;
}

bool Pen::keyIsDown(int code) const {
  return std::find(m_keysDown.begin(), m_keysDown.end(), code) !=
         m_keysDown.end();
}

// ---- the style --------------------------------------------------------------

void Pen::applyStyle() {
  m_fillPaint.setAntiAlias(m_style.antiAlias);
  m_fillPaint.setStyle(SkPaint::kFill_Style);
  m_strokePaint.setAntiAlias(m_style.antiAlias);
  m_strokePaint.setStyle(SkPaint::kStroke_Style);
  m_strokePaint.setStrokeWidth(m_style.strokeWeight);
  m_strokePaint.setStrokeCap(m_style.cap);
  m_strokePaint.setStrokeJoin(m_style.join);
  m_strokePaint.setPathEffect(m_style.dash);
  blendInto(m_fillPaint);
  blendInto(m_strokePaint);
  resolveFill();
  resolveStroke();
}

void Pen::blendInto(SkPaint& paint) const { setBlend(paint, m_style.blend); }

void Pen::blendMode(Constant mode) {
  m_style.blend = mode;
  blendInto(m_fillPaint);
  blendInto(m_strokePaint);
}

material::skia::PaintFrame Pen::paintFrame() const {
  material::skia::PaintFrame frame;
  frame.size = {width, height};
  frame.seconds = m_seconds;
  frame.contentScale = m_contentScale;
  return frame;
}

void Pen::resolveFill() {
  m_fillLive = resolve(m_style.fill, m_fillPaint, paintFrame());
}

void Pen::resolveStroke() {
  m_strokeLive = resolve(m_style.stroke, m_strokePaint, paintFrame());
}

sk_sp<SkShader> Pen::fittedShader(const material::skia::Paint& paint,
                                  const SkRect& box) const {
  // The material is asked for a shader as though the shape were the whole
  // canvas — its unit square IS this box — and the answer is then moved to
  // where the box actually sits, since the pen paints in canvas
  // coordinates and a compose node paints at its own origin.
  material::skia::PaintFrame frame = paintFrame();
  frame.size = {box.width(), box.height()};
  frame.rootSize = {width, height};
  frame.toRoot = SkMatrix::Translate(box.left(), box.top());
  sk_sp<SkShader> shader = paint.shaderFor(frame);
  if (!shader) return nullptr;
  return shader->makeWithLocalMatrix(
      SkMatrix::Translate(box.left(), box.top()));
}

/** Whether @p box is a unit square a material can be measured against: a
 *  horizontal line and a zero-radius circle are not, and asking a material
 *  to divide by their extent is how a fitted fill turns into nothing. */

const SkPaint* Pen::fillPaint(const SkRect* box) {
  if (!m_style.doFill) return nullptr;
  if (m_style.fillFitted && fittable(box) && !m_style.fill.isSolid() &&
      !m_style.fill.isNone()) {
    m_fillPaint.setShader(fittedShader(m_style.fill, *box));
    return &m_fillPaint;
  }
  if (m_fillLive) m_fillPaint.setShader(m_style.fill.shaderFor(paintFrame()));
  return &m_fillPaint;
}

const SkPaint* Pen::strokePaint(const SkRect* box) {
  if (!m_style.doStroke || !(m_style.strokeWeight > 0.0f)) return nullptr;
  if (m_style.strokeFitted && fittable(box) && !m_style.stroke.isSolid() &&
      !m_style.stroke.isNone()) {
    m_strokePaint.setShader(fittedShader(m_style.stroke, *box));
    return &m_strokePaint;
  }
  if (m_strokeLive)
    m_strokePaint.setShader(m_style.stroke.shaderFor(paintFrame()));
  return &m_strokePaint;
}

const SkPaint* Pen::fillPaint() { return fillPaint(nullptr); }
const SkPaint* Pen::strokePaint() { return strokePaint(nullptr); }

// ---- colour -----------------------------------------------------------------

void Pen::colorMode(Constant mode) {
  m_style.colorMode = ColorMode::standard(mode);
}
void Pen::colorMode(Constant mode, float max) {
  m_style.colorMode = {mode, max, max, max, max};
}
void Pen::colorMode(Constant mode, float max1, float max2, float max3) {
  m_style.colorMode = {mode, max1, max2, max3, m_style.colorMode.maxA};
}
void Pen::colorMode(Constant mode, float max1, float max2, float max3,
                    float maxA) {
  m_style.colorMode = {mode, max1, max2, max3, maxA};
}

SkColor4f Pen::color(float gray) const {
  return colorFrom(m_style.colorMode, gray);
}
SkColor4f Pen::color(float gray, float alpha) const {
  return colorFrom(m_style.colorMode, gray, alpha);
}
SkColor4f Pen::color(float v1, float v2, float v3) const {
  return colorFrom(m_style.colorMode, v1, v2, v3);
}
SkColor4f Pen::color(float v1, float v2, float v3, float alpha) const {
  return colorFrom(m_style.colorMode, v1, v2, v3, alpha);
}
SkColor4f Pen::color(std::string_view css) const { return parseColor(css); }

SkColor4f Pen::lerpColor(SkColor4f a, SkColor4f b, float amount) {
  const float t = std::clamp(amount, 0.0f, 1.0f);
  return {lerp(a.fR, b.fR, t), lerp(a.fG, b.fG, t), lerp(a.fB, b.fB, t),
          lerp(a.fA, b.fA, t)};
}

void Pen::background(float gray) { background(color(gray)); }
void Pen::background(float gray, float alpha) {
  background(color(gray, alpha));
}
void Pen::background(float v1, float v2, float v3) {
  background(color(v1, v2, v3));
}
void Pen::background(float v1, float v2, float v3, float alpha) {
  background(color(v1, v2, v3, alpha));
}
void Pen::background(std::string_view css) { background(parseColor(css)); }
void Pen::background(SkColor4f color) {
  background(material::skia::Paint::solid(color));
}

void Pen::background(const material::skia::Paint& paint) {
  if (!m_canvas || paint.isNone() || m_clipRecording) return;
  SkPaint ground;
  resolve(paint, ground, paintFrame());
  blendInto(ground);
  // The whole canvas under the transform the frame began on: a
  // background is not a rect in the sketch's current space, and it is
  // not a clear either, since it may carry alpha and must blend.
  SkAutoCanvasRestore restore(m_canvas, true);
  m_canvas->setMatrix(m_base);
  m_canvas->drawRect(SkRect::MakeWH(width, height), ground);
}

void Pen::clear() {
  if (!m_canvas || m_clipRecording) return;
  SkPaint erase;
  erase.setBlendMode(SkBlendMode::kClear);
  SkAutoCanvasRestore restore(m_canvas, true);
  m_canvas->setMatrix(m_base);
  m_canvas->drawRect(SkRect::MakeWH(width, height), erase);
}

void Pen::fill(float gray) { fill(color(gray)); }
void Pen::fill(float gray, float alpha) { fill(color(gray, alpha)); }
void Pen::fill(float v1, float v2, float v3) { fill(color(v1, v2, v3)); }
void Pen::fill(float v1, float v2, float v3, float alpha) {
  fill(color(v1, v2, v3, alpha));
}
void Pen::fill(std::string_view css) { fill(parseColor(css)); }
void Pen::fill(SkColor4f color) { fill(material::skia::Paint::solid(color)); }
void Pen::fill(const material::skia::Paint& paint) {
  m_style.fill = paint;
  // The fit belongs to the material it was set with, so a fill set without
  // a word is measured against the canvas whatever the fill before it said.
  m_style.fillFitted = false;
  m_style.doFill = true;
  m_style.fillSet = true;
  resolveFill();
}
void Pen::fill(const material::skia::Paint& paint, Constant fit) {
  fill(paint);
  m_style.fillFitted = fit == SHAPE;
}
void Pen::fill(const material::Material& material) {
  fill(material::skia::Paint::recipe(material));
}
void Pen::noFill() { m_style.doFill = false; }

void Pen::stroke(float gray) { stroke(color(gray)); }
void Pen::stroke(float gray, float alpha) { stroke(color(gray, alpha)); }
void Pen::stroke(float v1, float v2, float v3) { stroke(color(v1, v2, v3)); }
void Pen::stroke(float v1, float v2, float v3, float alpha) {
  stroke(color(v1, v2, v3, alpha));
}
void Pen::stroke(std::string_view css) { stroke(parseColor(css)); }
void Pen::stroke(SkColor4f color) {
  stroke(material::skia::Paint::solid(color));
}
void Pen::stroke(const material::skia::Paint& paint) {
  m_style.stroke = paint;
  m_style.strokeFitted = false;
  m_style.doStroke = true;
  m_style.strokeSet = true;
  resolveStroke();
}
void Pen::stroke(const material::skia::Paint& paint, Constant fit) {
  stroke(paint);
  m_style.strokeFitted = fit == SHAPE;
}
void Pen::stroke(const material::Material& material) {
  stroke(material::skia::Paint::recipe(material));
}
void Pen::noStroke() { m_style.doStroke = false; }

void Pen::strokeWeight(float weight) {
  m_style.strokeWeight = std::max(0.0f, weight);
  m_strokePaint.setStrokeWidth(m_style.strokeWeight);
}
void Pen::strokeCap(Constant cap) {
  m_style.cap = capOf(cap);
  m_strokePaint.setStrokeCap(m_style.cap);
}
void Pen::strokeJoin(Constant join) {
  m_style.join = joinOf(join);
  m_strokePaint.setStrokeJoin(m_style.join);
}
void Pen::strokeDash(std::initializer_list<float> intervals, float phase) {
  std::vector<SkScalar> run(intervals.begin(), intervals.end());
  // An odd run repeats itself, so {6} is six drawn and six skipped —
  // which is what a line dash of an odd length means everywhere else.
  // Reserved first, since each entry is copied from the same vector.
  if (run.size() % 2 == 1) {
    const size_t stated = run.size();
    run.reserve(stated * 2);
    for (size_t i = 0; i < stated; ++i) run.push_back(run[i]);
  }
  float total = 0.0f;
  for (SkScalar length : run) {
    if (!(length >= 0.0f)) return noDash();
    total += length;
  }
  if (run.empty() || !(total > 0.0f)) return noDash();
  m_style.dash = SkDashPathEffect::Make(
      SkSpan<const SkScalar>(run.data(), run.size()), phase);
  m_strokePaint.setPathEffect(m_style.dash);
}

void Pen::noDash() {
  m_style.dash = nullptr;
  m_strokePaint.setPathEffect(nullptr);
}

void Pen::smooth() {
  m_style.antiAlias = true;
  m_fillPaint.setAntiAlias(true);
  m_strokePaint.setAntiAlias(true);
}
void Pen::noSmooth() {
  m_style.antiAlias = false;
  m_fillPaint.setAntiAlias(false);
  m_strokePaint.setAntiAlias(false);
}

// ---- modes ------------------------------------------------------------------

void Pen::rectMode(Constant mode) { m_style.rectMode = mode; }
void Pen::ellipseMode(Constant mode) { m_style.ellipseMode = mode; }
void Pen::imageMode(Constant mode) { m_style.imageMode = mode; }
void Pen::angleMode(Constant mode) { m_style.angleMode = mode; }

SkRect Pen::boxIn(Constant mode, float x, float y, float w, float h) {
  switch (mode) {
    case CORNERS:
      return SkRect::MakeLTRB(x, y, w, h).makeSorted();
    case CENTER:
      return SkRect::MakeXYWH(x - w / 2.0f, y - h / 2.0f, w, h).makeSorted();
    case RADIUS:
      return SkRect::MakeLTRB(x - w, y - h, x + w, y + h).makeSorted();
    default:
      return SkRect::MakeXYWH(x, y, w, h).makeSorted();
  }
}

// ---- random -----------------------------------------------------------------

float Pen::random() { return m_random.unit(); }
float Pen::random(float max) { return m_random.unit() * max; }
float Pen::random(float min, float max) { return m_random.range(min, max); }

void Pen::randomSeed(uint64_t seed) {
  m_random = core::noise::Mix64Stream(seed);
  m_gaussianHeld = false;
}

float Pen::randomGaussian(float mean, float sd) {
  // Box-Muller, both values kept: the second draw costs nothing.
  if (m_gaussianHeld) {
    m_gaussianHeld = false;
    return mean + m_gaussianNext * sd;
  }
  float u1 = m_random.unit();
  while (u1 <= 1.0e-7f) u1 = m_random.unit();
  const float u2 = m_random.unit();
  const float r = std::sqrt(-2.0f * std::log(u1));
  m_gaussianNext = r * std::sin(TWO_PI * u2);
  m_gaussianHeld = true;
  return mean + r * std::cos(TWO_PI * u2) * sd;
}

}  // namespace sigil::draw
