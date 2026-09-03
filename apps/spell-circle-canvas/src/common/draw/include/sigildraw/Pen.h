#pragma once

/** @file
 * The pen: p5's verbs over an SkCanvas, with p5's names, argument orders
 * and defaults, held as ONE VALUE — the style, the transform, the seeded
 * streams and what it keeps between frames — so two pens draw side by
 * side and nothing is global.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkM44.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <include/core/SkTypeface.h>
#include <sigilcore/compute/Noise.h>
#include <sigildraw/Color.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Noise.h>
#include <sigildraw/Retained.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilweave/style/Type.h>

#include <concepts>
#include <cstdint>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::weave {
class FontContext;
}

namespace sigil::draw {

class Pen;
class Graphics;

/** A GUEST: something another library keeps between frames and paints
 *  inside a box the pen names — a retained element tree, a shaped page.
 *  The guest's own library says how, by declaring
 *  `paintRetained(Pen&, const Guest&, const SkRect&, Slot)` in the
 *  guest's namespace, where argument lookup finds it; this library
 *  names no guest. */
template <class G>
concept Retainable =
    requires(Pen& pen, const G& guest, const SkRect& box, Slot slot) {
      paintRetained(pen, guest, box, slot);
    };

/** A SILHOUETTE: any value that answers a path over a size — the
 *  geometry kit's generators, or one of your own. */
template <class S>
concept Silhouette = requires(const S& s, SkSize size) {
  { s.path(size) } -> std::convertible_to<SkPath>;
};

/** WHAT A FRAME SUPPLIES THE PEN, none of it the sketch's to set: the
 *  canvas in its own pixels, the clock of whoever is stepping, the frame
 *  count, the fonts text is shaped with, and the pointer and keys a host
 *  fed. The clock is the caller's: a pen never reads the wall. */
struct Frame {
  float width = 0;
  float height = 0;
  /** Seconds since the sketch began, on the caller's clock. */
  double seconds = 0;
  /** The step this frame took, in seconds. */
  double deltaSeconds = 0;
  /** p5's `frameCount`: 1 on the first draw. */
  int frameCount = 0;
  weave::FontContext* fonts = nullptr;
  float mouseX = 0;
  float mouseY = 0;
  bool mouseIsPressed = false;
  bool keyIsPressed = false;
  /** The key most recently pressed, spelled as a keyboard spells it:
   *  "a", "ArrowLeft", "Enter". */
  std::string_view key;
  int keyCode = 0;
  /** Every key code held down this frame. */
  std::span<const int> keysDown;
};

/** THE PEN.
 *
 *  p5's surface, verbatim where C++ allows it: the same verbs, the same
 *  argument orders and counts, the same defaults — a white fill, a black
 *  one-pixel stroke, `rectMode(CORNER)`, `ellipseMode(CENTER)`,
 *  `angleMode(RADIANS)`, `colorMode(RGB)` over 255. A sketch pasted from
 *  p5 differs by `pen.` in front of each verb and nothing else it needs
 *  to think about; what is this library's own is an ADDED overload on
 *  the same verb, never a renamed one — a material as a fill, a
 *  silhouette as a shape, a `weave::Type` as the font, a retained guest.
 *
 *  Drawing happens between `begin` and `end`, which whoever holds the
 *  canvas calls around a frame. The style survives from frame to frame
 *  as it does in p5 — a `noStroke()` in setup holds — and the transform
 *  starts over at the canvas the frame was begun on. */
class Pen {
 public:
  Pen();
  ~Pen();
  Pen(const Pen&) = delete;
  Pen& operator=(const Pen&) = delete;

  // ---- the frame, for whoever holds the canvas ---------------------------
  /** Starts a frame on @p canvas: the transform the canvas carries at
   *  this moment is what `resetMatrix()` returns to, and everything in
   *  @p frame is read into the variables below. */
  void begin(SkCanvas& canvas, const Frame& frame);
  /** Ends it, restoring the canvas to how `begin` found it. */
  void end();
  /** THE CANVAS ITSELF, carrying the pen's current transform — every
   *  `translate`, `rotate`, `scale` and open `push` this frame is
   *  already on it, so a rect drawn here lands where `pen.rect` would
   *  put it. Null between frames.
   *
   *  It is the DOOR OUT of p5's vocabulary: another library's drawing
   *  takes an `SkCanvas&` and this is the one to hand it, alongside
   *  `fillPaint()` and `strokePaint()` for the style the pen stands at
   *  and `contentScale()` for the device pixels one canvas unit covers.
   *  Whatever is drawn through it lands in the same place in the same
   *  order as the pen's own verbs, since there is only one canvas.
   *
   *  Leave it as it was found: the pen's transform and clip carry into
   *  the rest of the frame, so an unbalanced `save` here is an
   *  unbalanced transform for every verb after it. */
  [[nodiscard]] SkCanvas* canvas() const { return m_canvas; }
  [[nodiscard]] weave::FontContext* fonts() const { return m_fonts; }
  /** How many device pixels one canvas unit covered when the frame
   *  began — one on a plate at the declared size, two on a doubled
   *  screen. What a hairline, a dash period or a bake resolution
   *  computed outside the pen has to be scaled by. */
  [[nodiscard]] float contentScale() const { return m_contentScale; }
  /** What this pen keeps between frames for its guests. */
  [[nodiscard]] Retained& retained() { return m_retained; }
  [[nodiscard]] const Retained& retained() const { return m_retained; }
  /** Whether the loop is running (`noLoop` stops it, `loop` restarts
   *  it), and whether one `redraw()` was asked for since the last frame
   *  — the runtime reads both and clears the second. */
  [[nodiscard]] bool isLooping() const { return m_looping; }
  [[nodiscard]] bool takeRedraw();
  /** The rate `frameRate(fps)` asked for, or zero for whatever the
   *  runtime steps at. */
  [[nodiscard]] double targetFrameRate() const { return m_targetFrameRate; }

  // ---- p5's variables ------------------------------------------------------
  float width = 0;
  float height = 0;
  int frameCount = 0;
  /** Milliseconds the last frame took, as p5 spells it. */
  double deltaTime = 0;
  float mouseX = 0;
  float mouseY = 0;
  float pmouseX = 0;
  float pmouseY = 0;
  bool mouseIsPressed = false;
  bool keyIsPressed = false;
  std::string key;
  int keyCode = 0;

  // ---- environment ---------------------------------------------------------
  /** Milliseconds since the sketch began, on the clock of whoever
   *  steps it — stepped, on a plate; the wall, in a window. */
  [[nodiscard]] double millis() const { return m_seconds * 1000.0; }
  /** Frames per second the runtime is stepping at: one over the last
   *  step. */
  [[nodiscard]] double frameRate() const;
  /** Asks the runtime to run `draw` at most @p fps times a second. The
   *  runtime owns the clock, so this is a request it honours by skipping
   *  draws, and zero asks for every frame. */
  void frameRate(double fps) { m_targetFrameRate = fps < 0 ? 0 : fps; }
  void noLoop() { m_looping = false; }
  void loop() { m_looping = true; }
  void redraw() { m_redraw = true; }
  [[nodiscard]] bool keyIsDown(int code) const;

  // ---- colour --------------------------------------------------------------
  void colorMode(Constant mode);
  void colorMode(Constant mode, float max);
  void colorMode(Constant mode, float max1, float max2, float max3);
  void colorMode(Constant mode, float max1, float max2, float max3, float maxA);
  [[nodiscard]] SkColor4f color(float gray) const;
  [[nodiscard]] SkColor4f color(float gray, float alpha) const;
  [[nodiscard]] SkColor4f color(float v1, float v2, float v3) const;
  [[nodiscard]] SkColor4f color(float v1, float v2, float v3,
                                float alpha) const;
  [[nodiscard]] SkColor4f color(std::string_view css) const;
  [[nodiscard]] SkColor4f color(SkColor4f c) const { return c; }
  [[nodiscard]] static SkColor4f lerpColor(SkColor4f a, SkColor4f b,
                                           float amount);

  void background(float gray);
  void background(float gray, float alpha);
  void background(float v1, float v2, float v3);
  void background(float v1, float v2, float v3, float alpha);
  void background(std::string_view css);
  void background(SkColor4f color);
  /** A material as the ground: a gradient, a shader, a recipe. */
  void background(const material::skia::Paint& paint);
  /** Every pixel to transparent. */
  void clear();

  void fill(float gray);
  void fill(float gray, float alpha);
  void fill(float v1, float v2, float v3);
  void fill(float v1, float v2, float v3, float alpha);
  void fill(std::string_view css);
  void fill(SkColor4f color);
  /** A material as the fill — the pen's own overload. A live material
   *  is resolved against the pen's clock on every draw; a static one
   *  once, here. */
  void fill(const material::skia::Paint& paint);
  /** A recipe instance as the fill: a shader. */
  void fill(const material::Material& material);
  void noFill();

  void stroke(float gray);
  void stroke(float gray, float alpha);
  void stroke(float v1, float v2, float v3);
  void stroke(float v1, float v2, float v3, float alpha);
  void stroke(std::string_view css);
  void stroke(SkColor4f color);
  void stroke(const material::skia::Paint& paint);
  void stroke(const material::Material& material);
  void noStroke();
  void strokeWeight(float weight);
  void strokeCap(Constant cap);
  void strokeJoin(Constant join);
  void smooth();
  /** Jagged edges AND jagged pixels: antialiasing off on every shape,
   *  and `image` sampled nearest-neighbour with no mipmap, so a small
   *  source blown up is blocks rather than a blur. `smooth()` puts both
   *  back. */
  void noSmooth();

  // ---- modes ---------------------------------------------------------------
  void rectMode(Constant mode);
  void ellipseMode(Constant mode);
  void imageMode(Constant mode);
  void angleMode(Constant mode);

  // ---- shapes --------------------------------------------------------------
  void point(float x, float y);
  void line(float x1, float y1, float x2, float y2);
  void rect(float x, float y, float w, float h);
  void rect(float x, float y, float w, float h, float radius);
  void rect(float x, float y, float w, float h, float tl, float tr, float br,
            float bl);
  void square(float x, float y, float s);
  void square(float x, float y, float s, float radius);
  void square(float x, float y, float s, float tl, float tr, float br,
              float bl);
  void ellipse(float x, float y, float w);
  void ellipse(float x, float y, float w, float h);
  void circle(float x, float y, float d);
  /** p5's arc: from @p start to @p stop clockwise, in the current angle
   *  mode; the fill is the pie unless the mode is CHORD, and the stroke
   *  is the arc alone under OPEN, closed by its chord under CHORD, and
   *  closed through the centre under PIE. */
  void arc(float x, float y, float w, float h, float start, float stop,
           Constant mode = OPEN);
  void triangle(float x1, float y1, float x2, float y2, float x3, float y3);
  void quad(float x1, float y1, float x2, float y2, float x3, float y3,
            float x4, float y4);
  void bezier(float x1, float y1, float x2, float y2, float x3, float y3,
              float x4, float y4);
  /** A Catmull-Rom segment from the second point to the third, the
   *  first and fourth steering it. */
  void curve(float x1, float y1, float x2, float y2, float x3, float y3,
             float x4, float y4);
  void curveTightness(float amount);

  void beginShape(Constant kind = POLYGON);
  /** A corner of the shape being built, WEARING THE FILL THAT STANDS
   *  WHEN IT IS ADDED. Calling `fill` between two `vertex` calls
   *  therefore colours the shape corner by corner, and the colour is
   *  interpolated across each triangle of the mesh the kind describes —
   *  which is how a ramp along a streak, a lit facet or a heat gradient
   *  is drawn without one shape per band.
   *
   *  It costs nothing where nothing changes: a shape whose corners all
   *  carry one colour is drawn as a path, filled and stroked exactly as
   *  before. A shape whose corners differ is FILLED AS A TRIANGLE MESH,
   *  so the fill must be a solid colour — a gradient or an effect
   *  cannot also be interpolated per corner — while the stroke, if
   *  there is one, still follows the shape's outline.
   *
   *  Only the triangle and quad kinds have a mesh; `POLYGON` is one
   *  path with one fill, as p5 has it. */
  void vertex(float x, float y);
  void curveVertex(float x, float y);
  void bezierVertex(float x2, float y2, float x3, float y3, float x4, float y4);
  void quadraticVertex(float cx, float cy, float x3, float y3);
  void beginContour();
  void endContour();
  void endShape(Constant mode = OPEN);

  /** THE PEN'S OWN SHAPE VERB. A silhouette — a geometry kit value, or
   *  anything with `path(SkSize)` — fitted to the box the rect mode
   *  reads from the four numbers, filled and stroked as a rect is. */
  template <Silhouette S>
  void shape(const S& silhouette, float x, float y, float w, float h) {
    const SkRect box = rectBox(x, y, w, h);
    shape(silhouette.path({box.width(), box.height()})
              .makeOffset(box.left(), box.top()));
  }
  /** A path as it stands, filled and stroked with the current style. */
  void shape(const SkPath& path);

  // ---- text ----------------------------------------------------------------
  void textSize(float size);
  /** A family by name, matched through the font context's manager; a
   *  family it cannot find falls back to the context's default face. */
  void textFont(std::string_view family);
  void textFont(std::string_view family, float size);
  void textFont(sk_sp<SkTypeface> face);
  /** The pen's own overload: a whole type — face, size, tracking,
   *  condensation, variable axes — as this library's `Type` spells it.
   *  Its colour is ignored; the fill colours text, as in p5. */
  void textFont(const weave::Type& type);
  void textAlign(Constant horizontal);
  void textAlign(Constant horizontal, Constant vertical);
  void textLeading(float leading);
  [[nodiscard]] float textLeading() const;
  void textStyle(Constant style);
  /** @p str at (@p x, @p y): the start of the baseline under the default
   *  alignment; `\n` breaks a line. Shaped and laid out by this
   *  repository's text engine, so what lands is real typography — kerned,
   *  with fallback fonts where the face lacks a glyph. */
  void text(std::string_view str, float x, float y);
  /** @p str wrapped inside the box the rect mode reads from the four
   *  numbers, aligned inside it. */
  void text(std::string_view str, float x, float y, float w, float h);
  void text(double value, float x, float y);
  [[nodiscard]] float textWidth(std::string_view str);
  [[nodiscard]] float textAscent();
  [[nodiscard]] float textDescent();

  // ---- image ---------------------------------------------------------------
  void image(const sk_sp<SkImage>& img, float x, float y);
  void image(const sk_sp<SkImage>& img, float x, float y, float w, float h);
  /** The part of @p img at (@p sx, @p sy, @p sw, @p sh) drawn into the
   *  box (@p dx, @p dy, @p dw, @p dh). */
  void image(const sk_sp<SkImage>& img, float dx, float dy, float dw, float dh,
             float sx, float sy, float sw, float sh);
  /** An offscreen buffer put down, as p5 puts a Graphics down: placed
   *  and sized by the buffer's CANVAS size, not by the pixel count it
   *  was formed at. Its words are in `<sigildraw/Graphics.h>`. */
  void image(const Graphics& buffer, float x, float y);
  void image(const Graphics& buffer, float x, float y, float w, float h);

  // ---- transform -----------------------------------------------------------
  void translate(float x, float y);
  /** In the current angle mode. */
  void rotate(float angle);
  void scale(float s);
  void scale(float sx, float sy);
  void shearX(float angle);
  void shearY(float angle);
  /** Saves the style and the transform; `pop` restores both. */
  void push();
  void pop();
  /** Back to the transform the frame began on. */
  void resetMatrix();
  void applyMatrix(float a, float b, float c, float d, float e, float f);

  // ---- random and noise ----------------------------------------------------
  /** [0, 1), [0, max) or [min, max), from a stream seeded once per pen —
   *  so a plate stepped from zero draws the same picture every time.
   *  `randomSeed` restarts the stream. */
  [[nodiscard]] float random();
  [[nodiscard]] float random(float max);
  [[nodiscard]] float random(float min, float max);
  [[nodiscard]] float randomGaussian(float mean = 0.0f, float sd = 1.0f);
  void randomSeed(uint64_t seed);
  /** p5's noise: see NoiseField. */
  [[nodiscard]] float noise(float x, float y = 0.0f, float z = 0.0f) const {
    return m_noise.at(x, y, z);
  }
  void noiseSeed(uint32_t seed) { m_noise.seed(seed); }
  void noiseDetail(int lod, float falloff) { m_noise.detail(lod, falloff); }

  // ---- a retained guest ----------------------------------------------------
  /** THE OTHER WAY THROUGH THE DOOR. Something another library keeps
   *  between frames, painted inside @p box on this frame: laid out,
   *  reconciled and cached by its own library, with this pen lending it
   *  the canvas, the transform above the box and the clock. The guest is
   *  told apart by the call site, so a loop that paints several passes
   *  @p index; the pen's clock is what the guest's clock is stepped by,
   *  so a guest advances on the frames it is painted and stands still on
   *  the frames it is not. */
  template <Retainable G>
  void element(const G& guest, const SkRect& box, int index = 0,
               std::source_location where = std::source_location::current()) {
    if (!m_canvas) return;
    paintRetained(*this, guest, box, Slot::at(where, index));
  }

  // ---- the paints, for a guest that draws with them -----------------------
  /** The fill as an SkPaint, resolved for this frame; unset when
   *  `noFill()` holds. */
  [[nodiscard]] const SkPaint* fillPaint();
  /** The stroke as an SkPaint, resolved for this frame; unset when
   *  `noStroke()` holds or the weight is zero. */
  [[nodiscard]] const SkPaint* strokePaint();

 private:
  struct Style {
    bool doFill = true;
    bool doStroke = true;
    /** Whether `fill` or `stroke` was ever called — p5 fills text black
     *  until a fill is set, and strokes it only once a stroke is. */
    bool fillSet = false;
    bool strokeSet = false;
    material::skia::Paint fill = material::skia::Paint::solid({1, 1, 1, 1});
    material::skia::Paint stroke = material::skia::Paint::solid({0, 0, 0, 1});
    float strokeWeight = 1.0f;
    SkPaint::Cap cap = SkPaint::kRound_Cap;
    SkPaint::Join join = SkPaint::kMiter_Join;
    bool antiAlias = true;
    Constant rectMode = CORNER;
    Constant ellipseMode = CENTER;
    Constant imageMode = CORNER;
    Constant angleMode = RADIANS;
    ColorMode colorMode;
    float curveTightness = 0.0f;
    weave::Type type{.size = 12.0f};
    std::string family;
    Constant textStyle = NORMAL;
    /** The face the family and text style matched to, kept until either
     *  changes. */
    sk_sp<SkTypeface> matched;
    Constant textAlignX = LEFT;
    Constant textAlignY = BASELINE;
    float leading = 15.0f;
  };

  [[nodiscard]] SkRect rectBox(float x, float y, float w, float h) const;
  [[nodiscard]] static SkRect boxIn(Constant mode, float x, float y, float w,
                                    float h);
  [[nodiscard]] float toDegrees(float angle) const;
  [[nodiscard]] float toRadians(float angle) const;
  void applyStyle();
  void resolveFill();
  void resolveStroke();
  [[nodiscard]] material::skia::PaintFrame paintFrame() const;
  void paintFilled(const SkPath& path);
  /** The mesh a shape whose corners carry different fills is drawn as:
   *  triangles in threes, each corner its own colour. */
  void paintVertices(const std::vector<SkPoint>& positions,
                     const std::vector<SkColor>& colors);
  void paintOval(const SkRect& oval);
  void paintRect(const SkRect& rect);
  void flushCurve();
  void emitKind(const std::vector<SkPoint>& v);
  [[nodiscard]] sk_sp<SkTypeface> face();
  [[nodiscard]] weave::TextStyle textStyleNow();
  void textLine(std::string_view line, float x, float baseline);

  SkCanvas* m_canvas = nullptr;
  weave::FontContext* m_fonts = nullptr;
  int m_saveCount = 0;
  SkM44 m_base;
  float m_contentScale = 1.0f;
  double m_seconds = 0.0;
  bool m_hadFrame = false;
  bool m_looping = true;
  bool m_redraw = false;
  double m_targetFrameRate = 0.0;
  std::vector<int> m_keysDown;

  Style m_style;
  std::vector<Style> m_stack;
  size_t m_stackFloor = 0;
  SkPaint m_fillPaint;
  SkPaint m_strokePaint;
  bool m_fillLive = false;
  bool m_strokeLive = false;

  // The shape being built between beginShape and endShape.
  Constant m_shapeKind = POLYGON;
  SkPathBuilder m_path;
  bool m_hasPoint = false;
  bool m_newContour = false;
  std::vector<SkPoint> m_vertices;
  // The fill each vertex was added under, and whether any two of them
  // differ — which is the only thing that decides between a path and a
  // mesh, so a shape drawn under one fill costs no comparison per draw.
  std::vector<SkColor> m_vertexColors;
  bool m_vertexColorsVary = false;
  bool m_vertexFillsSolid = true;
  std::vector<SkPoint> m_curve;

  core::noise::Mix64Stream m_random;
  bool m_gaussianHeld = false;
  float m_gaussianNext = 0.0f;
  NoiseField m_noise;
  Retained m_retained;
};

}  // namespace sigil::draw
