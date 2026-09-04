/** @file
 * The pen's verbs: the frame, the style, the shapes and the transform.
 * Text is in Text.cpp.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkBlender.h>
#include <include/core/SkClipOp.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkString.h>
#include <include/core/SkVertices.h>
#include <include/effects/SkDashPathEffect.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>
#include <sigilio/hub/TextLibrary.h>
#include <sigilmaterial/core/Material.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <string_view>

namespace sigil::draw {

namespace {

std::string shaderSource(std::string_view name) {
  static io::TextLibrary library("shader://draw/", SIGIL_DRAW_SHADER_DIR);
  return library.text("shader://draw/" + std::string(name)).value_or("");
}

/** The seed every pen starts on, so a sketch stepped from zero draws the
 *  same picture on every machine. `randomSeed` moves off it. */
constexpr uint64_t kDefaultSeed = 0x5EED5EED5EED5EEDull;

/** p5's SQUARE ends the stroke at the point and PROJECT carries it
 *  half a weight past, which are Skia's butt and square caps. */
SkPaint::Cap capOf(Constant cap) {
  switch (cap) {
    case SQUARE:
      return SkPaint::kButt_Cap;
    case PROJECT:
      return SkPaint::kSquare_Cap;
    default:
      return SkPaint::kRound_Cap;
  }
}

/** HOW AN IMAGE IS SAMPLED under the pen's smoothing. Smoothing off is
 *  not only about the edges of shapes: it is what a blown-up pixel
 *  source needs, one source texel per destination block with nothing
 *  blended across the boundary, so the picture is blocks and not a
 *  blur. Mipmaps go with it — a mipmap IS a blend of neighbours. */
SkSamplingOptions samplingFor(bool smooth) {
  return smooth
             ? SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear)
             : SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone);
}

/** SUBTRACT: the source's colour taken out of the canvas's. No blend
 *  mode does it, so it is a blender of two lines, built once.
 *
 *  THE CANVAS KEEPS ITS OWN ALPHA. Subtracting alpha as well would turn
 *  an opaque ground transparent instead of dark, which is the opposite
 *  of what taking light away means; and the channels floor at nothing,
 *  which keeps each of them under the alpha a premultiplied colour has
 *  to stay under. */
sk_sp<SkBlender> subtractBlender() {
  static const sk_sp<SkBlender> blender = [] {
    SkRuntimeEffect::Result made = SkRuntimeEffect::MakeForBlender(
        SkString(shaderSource("Subtract.sksl")));
    return made.effect ? made.effect->makeBlender(nullptr) : sk_sp<SkBlender>();
  }();
  return blender;
}

/** p5's blend words as Skia spells them. All but one are a blend mode,
 *  and setting a mode drops any blender the paint carried, so a paint
 *  that was SUBTRACT is plain again the moment another word is set. */
void setBlend(SkPaint& paint, Constant mode) {
  switch (mode) {
    case ADD:
      paint.setBlendMode(SkBlendMode::kPlus);
      return;
    case DARKEST:
      paint.setBlendMode(SkBlendMode::kDarken);
      return;
    case LIGHTEST:
      paint.setBlendMode(SkBlendMode::kLighten);
      return;
    case DIFFERENCE:
      paint.setBlendMode(SkBlendMode::kDifference);
      return;
    case EXCLUSION:
      paint.setBlendMode(SkBlendMode::kExclusion);
      return;
    case MULTIPLY:
      paint.setBlendMode(SkBlendMode::kMultiply);
      return;
    case SCREEN:
      paint.setBlendMode(SkBlendMode::kScreen);
      return;
    case REPLACE:
      paint.setBlendMode(SkBlendMode::kSrc);
      return;
    case REMOVE:
      paint.setBlendMode(SkBlendMode::kDstOut);
      return;
    case OVERLAY:
      paint.setBlendMode(SkBlendMode::kOverlay);
      return;
    case HARD_LIGHT:
      paint.setBlendMode(SkBlendMode::kHardLight);
      return;
    case SOFT_LIGHT:
      paint.setBlendMode(SkBlendMode::kSoftLight);
      return;
    case DODGE:
      paint.setBlendMode(SkBlendMode::kColorDodge);
      return;
    case BURN:
      paint.setBlendMode(SkBlendMode::kColorBurn);
      return;
    case SUBTRACT:
      paint.setBlender(subtractBlender());
      return;
    default:
      paint.setBlendMode(SkBlendMode::kSrcOver);
      return;
  }
}

SkPaint::Join joinOf(Constant join) {
  switch (join) {
    case BEVEL:
      return SkPaint::kBevel_Join;
    case ROUND:
      return SkPaint::kRound_Join;
    default:
      return SkPaint::kMiter_Join;
  }
}

/** p5's arc angles made drawable: both brought into [0, 2π), each
 *  corrected from the geometric angle a sketch means to the parametric
 *  one an ellipse is traced by, and the stop carried past the start so
 *  the sweep is clockwise. @p samePoint says the two meet, which draws
 *  the whole ellipse.
 *
 *  In double, and through atan2: the correction is tan(t) = (w/h) tan(a)
 *  taken in a's own quadrant, and a float tangent at a right angle lands
 *  on either side of its pole, which would flip a quarter arc into its
 *  three-quarter complement. */
void normalizeArc(float& startOut, float& stopOut, float w, float h,
                  bool& samePoint) {
  constexpr double kEpsilon = 0.00001;
  constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
  double start = startOut;
  double stop = stopOut;
  start = start - kTwoPi * std::floor(start / kTwoPi);
  stop = stop - kTwoPi * std::floor(stop / kTwoPi);
  const double separation =
      std::min(std::fabs(start - stop), kTwoPi - std::fabs(start - stop));
  samePoint = separation < kEpsilon;
  auto correct = [&](double angle) {
    double t =
        std::atan2((double)w * std::sin(angle), (double)h * std::cos(angle));
    if (t < 0.0) t += kTwoPi;
    return t;
  };
  start = correct(start);
  stop = correct(stop);
  if (start > stop) stop += kTwoPi;
  startOut = (float)start;
  stopOut = (float)stop;
}

}  // namespace

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

namespace {

/** One material onto one SkPaint: a solid is a colour, anything else a
 *  shader. Returns whether the shader has to be resolved again on every
 *  draw, which a live or box-dependent material does. */
bool resolve(const material::skia::Paint& material, SkPaint& paint,
             const material::skia::PaintFrame& frame) {
  if (material.isSolid()) {
    paint.setShader(nullptr);
    paint.setColor4f(material.solidColor(), nullptr);
    return false;
  }
  if (material.isNone()) {
    paint.setShader(nullptr);
    paint.setColor4f({0, 0, 0, 0}, nullptr);
    return false;
  }
  paint.setColor4f({0, 0, 0, 1}, nullptr);
  const bool live = material.isAnimated() || material.geometryDependent();
  paint.setShader(material.shaderFor(frame));
  return live;
}

}  // namespace

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
namespace {
bool fittable(const SkRect* box) {
  return box && box->width() > 0.0f && box->height() > 0.0f;
}
}  // namespace

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

// ---- the clip ---------------------------------------------------------------

void Pen::recordClip() {
  m_clipBuilder = SkPathBuilder();
  m_clipBase = m_canvas ? m_canvas->getLocalToDevice() : SkM44();
  m_clipRecording = true;
}

bool Pen::recordShape(const SkPath& path) {
  if (!m_clipRecording) return false;
  SkM44 inverse;
  // The shape is in the space it was DRAWN in, and the mask is applied
  // in the space the recording began in, so a translate or a rotate
  // inside the shape function carries the mask with it.
  if (m_canvas && m_clipBase.invert(&inverse))
    m_clipBuilder.addPath(
        path.makeTransform((inverse * m_canvas->getLocalToDevice()).asM33()));
  else
    m_clipBuilder.addPath(path);
  return true;
}

void Pen::applyClip(ClipOptions options) {
  m_clipRecording = false;
  const SkPath mask = m_clipBuilder.detach();
  if (!m_canvas) return;
  // No save of its own: the clip rides the canvas's stack, so `pop`
  // takes it off and the end of the frame takes it off, which is where
  // p5 ends a mask too.
  m_canvas->clipPath(
      mask, options.invert ? SkClipOp::kDifference : SkClipOp::kIntersect,
      m_style.antiAlias);
}

SkRect Pen::rectBox(float x, float y, float w, float h) const {
  return boxIn(m_style.rectMode, x, y, w, h);
}

float Pen::toDegrees(float angle) const {
  return m_style.angleMode == DEGREES ? angle : degrees(angle);
}

float Pen::toRadians(float angle) const {
  return m_style.angleMode == DEGREES ? radians(angle) : angle;
}

// ---- shapes -----------------------------------------------------------------

void Pen::paintFilled(const SkPath& path) {
  if (recordShape(path)) return;
  const SkRect box = path.getBounds();
  if (const SkPaint* fill = fillPaint(&box)) m_canvas->drawPath(path, *fill);
  if (const SkPaint* stroke = strokePaint(&box))
    m_canvas->drawPath(path, *stroke);
}

void Pen::paintOval(const SkRect& oval) {
  if (recordShape(SkPath::Oval(oval))) return;
  if (const SkPaint* fill = fillPaint(&oval)) m_canvas->drawOval(oval, *fill);
  if (const SkPaint* stroke = strokePaint(&oval))
    m_canvas->drawOval(oval, *stroke);
}

void Pen::paintRect(const SkRect& rect) {
  if (recordShape(SkPath::Rect(rect))) return;
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
  if (recordShape(SkPath::Circle(x, y, m_style.strokeWeight / 2.0f))) return;
  // p5 draws a point as a disc of the stroke weight in the stroke colour.
  const SkRect disc = SkRect::MakeLTRB(
      x - m_style.strokeWeight / 2.0f, y - m_style.strokeWeight / 2.0f,
      x + m_style.strokeWeight / 2.0f, y + m_style.strokeWeight / 2.0f);
  const SkPaint* stroke = strokePaint(&disc);
  if (!stroke) return;
  SkPaint dot = *stroke;
  dot.setStyle(SkPaint::kFill_Style);
  // A disc is not a stroke, so a dash has nothing to break up here.
  dot.setPathEffect(nullptr);
  m_canvas->drawCircle(x, y, m_style.strokeWeight / 2.0f, dot);
}

void Pen::line(float x1, float y1, float x2, float y2) {
  // A line has no inside, so it adds nothing to a mask being recorded.
  if (!m_canvas || m_clipRecording) return;
  const SkRect span = SkRect::MakeLTRB(std::min(x1, x2), std::min(y1, y2),
                                       std::max(x1, x2), std::max(y1, y2));
  if (const SkPaint* stroke = strokePaint(&span))
    m_canvas->drawLine(x1, y1, x2, y2, *stroke);
}

void Pen::rect(float x, float y, float w, float h) {
  if (!m_canvas) return;
  paintRect(rectBox(x, y, w, h));
}

void Pen::rect(float x, float y, float w, float h, float radius) {
  rect(x, y, w, h, radius, radius, radius, radius);
}

void Pen::rect(float x, float y, float w, float h, float tl, float tr, float br,
               float bl) {
  if (!m_canvas) return;
  const SkRect box = rectBox(x, y, w, h);
  const SkVector radii[4] = {{tl, tl}, {tr, tr}, {br, br}, {bl, bl}};
  SkRRect rounded;
  rounded.setRectRadii(box, radii);
  if (recordShape(SkPath::RRect(rounded))) return;
  if (const SkPaint* fill = fillPaint()) m_canvas->drawRRect(rounded, *fill);
  if (const SkPaint* stroke = strokePaint())
    m_canvas->drawRRect(rounded, *stroke);
}

void Pen::square(float x, float y, float s) { rect(x, y, s, s); }
void Pen::square(float x, float y, float s, float radius) {
  rect(x, y, s, s, radius);
}
void Pen::square(float x, float y, float s, float tl, float tr, float br,
                 float bl) {
  rect(x, y, s, s, tl, tr, br, bl);
}

void Pen::ellipse(float x, float y, float w) { ellipse(x, y, w, w); }

void Pen::ellipse(float x, float y, float w, float h) {
  if (!m_canvas) return;
  paintOval(boxIn(m_style.ellipseMode, x, y, std::fabs(w), std::fabs(h)));
}

void Pen::circle(float x, float y, float d) { ellipse(x, y, d, d); }

void Pen::arc(float x, float y, float w, float h, float start, float stop,
              Constant mode) {
  if (!m_canvas) return;
  w = std::fabs(w);
  h = std::fabs(h);
  if (!(w > 0.0f) || !(h > 0.0f)) return;
  const SkRect oval = boxIn(m_style.ellipseMode, x, y, w, h);
  float from = toRadians(start);
  float to = toRadians(stop);
  bool samePoint = false;
  normalizeArc(from, to, oval.width(), oval.height(), samePoint);
  if (samePoint) {
    paintOval(oval);
    return;
  }
  const float startDeg = degrees(from);
  const float sweepDeg = degrees(to - from);
  if (m_clipRecording) {
    SkPathBuilder wedge;
    if (mode == PIE) wedge.moveTo(oval.centerX(), oval.centerY());
    wedge.addArc(oval, startDeg, sweepDeg);
    wedge.close();
    recordShape(wedge.detach());
    return;
  }
  if (const SkPaint* fill = fillPaint())
    m_canvas->drawArc(oval, startDeg, sweepDeg, mode != CHORD, *fill);
  if (const SkPaint* stroke = strokePaint()) {
    if (mode == CHORD) {
      SkPathBuilder chord;
      chord.addArc(oval, startDeg, sweepDeg);
      chord.close();
      m_canvas->drawPath(chord.detach(), *stroke);
    } else {
      m_canvas->drawArc(oval, startDeg, sweepDeg, mode == PIE, *stroke);
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
    const SkColor packed = m_style.fill.solidColor().toSkColor();
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

void Pen::paintVertices(const std::vector<SkPoint>& positions,
                        const std::vector<SkColor>& colors) {
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

void Pen::emitKind(const std::vector<SkPoint>& v) {
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
      for (const SkPoint& p : v) point(p.x(), p.y());
      return;
    case LINES:
      for (size_t i = 0; i + 1 < n; i += 2)
        line(v[i].x(), v[i].y(), v[i + 1].x(), v[i + 1].y());
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

// ---- image ------------------------------------------------------------------

void Pen::image(const sk_sp<SkImage>& img, float x, float y) {
  if (!img) return;
  image(img, x, y, (float)img->width(), (float)img->height());
}

void Pen::image(const sk_sp<SkImage>& img, float x, float y, float w, float h) {
  if (!m_canvas || !img || m_clipRecording) return;
  const SkRect box = boxIn(m_style.imageMode, x, y, w, h);
  SkPaint paint;
  blendInto(paint);
  m_canvas->drawImageRect(img, box, samplingFor(m_style.antiAlias), &paint);
}

void Pen::image(const sk_sp<SkImage>& img, float dx, float dy, float dw,
                float dh, float sx, float sy, float sw, float sh) {
  if (!m_canvas || !img || m_clipRecording) return;
  const SkRect box = boxIn(m_style.imageMode, dx, dy, dw, dh);
  SkPaint paint;
  blendInto(paint);
  m_canvas->drawImageRect(img, SkRect::MakeXYWH(sx, sy, sw, sh), box,
                          samplingFor(m_style.antiAlias), &paint,
                          SkCanvas::kStrict_SrcRectConstraint);
}

// ---- transform --------------------------------------------------------------

void Pen::translate(float x, float y) {
  if (m_canvas) m_canvas->translate(x, y);
}

void Pen::rotate(float angle) {
  if (m_canvas) m_canvas->rotate(toDegrees(angle));
}

void Pen::scale(float s) { scale(s, s); }

void Pen::scale(float sx, float sy) {
  if (m_canvas) m_canvas->scale(sx, sy);
}

void Pen::shearX(float angle) {
  if (m_canvas) m_canvas->skew(std::tan(toRadians(angle)), 0.0f);
}

void Pen::shearY(float angle) {
  if (m_canvas) m_canvas->skew(0.0f, std::tan(toRadians(angle)));
}

void Pen::push() {
  m_stack.push_back(m_style);
  if (m_canvas) m_canvas->save();
}

void Pen::pop() {
  if (m_stack.size() <= m_stackFloor) return;
  m_style = m_stack.back();
  m_stack.pop_back();
  if (m_canvas) m_canvas->restore();
  applyStyle();
}

void Pen::resetMatrix() {
  if (m_canvas) m_canvas->setMatrix(m_base);
}

void Pen::applyMatrix(float a, float b, float c, float d, float e, float f) {
  if (!m_canvas) return;
  // p5's six numbers are the column-major 2D affine: x' = a x + c y + e.
  m_canvas->concat(SkMatrix::MakeAll(a, c, e, b, d, f, 0, 0, 1));
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
