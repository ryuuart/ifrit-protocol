#pragma once

/** @file
 * @ingroup draw-pen
 *
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
#include <include/core/SkPathEffect.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <include/core/SkTypeface.h>
#include <include/core/SkVertices.h>
#include <sigilcore/compute/Noise.h>
#include <sigildraw/Color.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Noise.h>
#include <sigildraw/PenTypes.h>
#include <sigildraw/Retained.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/path/Stroke.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilweave/style/Type.h>

#include <concepts>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::weave {
class FontContext;
}

/** AN IMMEDIATE-MODE CANVAS WITH p5's VERBS. The pen carries p5's names,
 *  argument orders and defaults over an `SkCanvas`, so a sketch written
 *  for p5 pastes in and runs with `pen.` in front of each verb. Beside
 *  it stand the off-screen buffer a pen also draws into, the colour,
 *  maths and noise helpers a sketch reaches for, and the store a
 *  retained guest keeps its state in between frames.
 *
 *  This is the imperative way to draw here; a declarative scene
 *  description is the way beside it, and the two open onto each other.
 *  The natural-media tools over the pen are the nested `brush`
 *  namespace's. */
namespace sigil::draw {

class Pen;
class Graphics;

/** THE PEN: p5's surface, verbatim where C++ allows it — the same
 *  verbs, the same argument orders and counts, the same defaults. What
 *  is this library's own is an ADDED overload on the same verb, never a
 *  renamed one. Drawing happens between `begin` and `end`; the style
 *  survives from frame to frame as it does in p5, and the transform
 *  starts over at the canvas the frame was begun on. */
class Pen {
 public:
  Pen();
  ~Pen();
  Pen(const Pen&) = delete;
  Pen& operator=(const Pen&) = delete;

  /** @name The frame
   *  What whoever holds the canvas calls around a frame, and the
   *  canvas itself for a caller that draws past the pen.
   *  @{ */
  /** Starts a frame on @p canvas: the transform the canvas carries at
   *  this moment is what `resetMatrix()` returns to, and everything in
   *  @p frame is read into the variables below. */
  void begin(SkCanvas& canvas, const Frame& frame);
  /** Ends it, restoring the canvas to how `begin` found it. */
  void end();
  /** THE CANVAS ITSELF, carrying the pen's current transform; null
   *  between frames. It is the DOOR OUT of p5's vocabulary.
   *  @trap Leave it as it was found: an unbalanced `save` here is an
   *  unbalanced transform for every verb after it. */
  [[nodiscard]] SkCanvas* canvas() const { return m_canvas; }
  [[nodiscard]] weave::FontContext* fonts() const { return m_fonts; }
  /** How many device pixels one canvas unit covered when the frame
   *  began — what a hairline, a dash period or a bake resolution
   *  computed outside the pen has to be scaled by. */
  [[nodiscard]] float contentScale() const { return m_contentScale; }
  /** THE INK AND THE FONT THE PEN BEGINS IN, which a host hands over
   *  after `begin` each frame: they seed the style for what the PROGRAM
   *  has not set, and nothing else in the style is touched. A pen nobody
   *  calls this on keeps p5's own defaults.
   *  @silent the program has called the verb itself: that choice then
   *  holds from frame to frame and this stops reaching it. */
  void inherit(material::Color ink, const weave::Type& font);
  /** The pair the last `inherit` carried — black and
   *  `weave::initialType()` on a pen that was never told one. */
  [[nodiscard]] material::Color inheritedInk() const { return m_inheritedInk; }
  [[nodiscard]] const weave::Type& inheritedFont() const {
    return m_inheritedFont;
  }
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
  /** @} */

  /** @name p5's variables
   *  The ambient values p5 exposes as globals, read off the frame
   *  the runtime handed to `begin`.
   *  @{ */
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
  /** @} */

  /** @name The environment
   *  The clock, the rate the loop is asked to run at, and the keys
   *  held this frame.
   *  @{ */
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
  /** Every key code held this frame — what `keyIsDown` answers over. */
  [[nodiscard]] std::span<const int> keysDown() const { return m_keysDown; }
  /** @} */

  /** @name Colour
   *  The mode every component-taking verb is read in, the fill and
   *  stroke inks, and the ground a frame starts from.
   *  @{ */
  void colorMode(Constant mode);
  void colorMode(Constant mode, float max);
  void colorMode(Constant mode, float max1, float max2, float max3);
  void colorMode(Constant mode, float max1, float max2, float max3, float maxA);
  [[nodiscard]] material::Color color(float gray) const;
  [[nodiscard]] material::Color color(float gray, float alpha) const;
  [[nodiscard]] material::Color color(float v1, float v2, float v3) const;
  [[nodiscard]] material::Color color(float v1, float v2, float v3,
                                      float alpha) const;
  [[nodiscard]] material::Color color(std::string_view css) const;
  [[nodiscard]] material::Color color(material::Color c) const { return c; }
  [[nodiscard]] static material::Color lerpColor(material::Color a,
                                                 material::Color b,
                                                 float amount);

  void background(float gray);
  void background(float gray, float alpha);
  void background(float v1, float v2, float v3);
  void background(float v1, float v2, float v3, float alpha);
  void background(std::string_view css);
  void background(material::Color color);
  /** A material as the ground: a gradient, a shader, a recipe. */
  void background(const material::skia::Paint& paint);
  /** A recipe instance as the ground, as the fill and the stroke take
   *  one: the three ground verbs accept the same set. */
  void background(const material::Material& material);
  /** Every pixel to transparent. */
  void clear();

  void fill(float gray);
  void fill(float gray, float alpha);
  void fill(float v1, float v2, float v3);
  void fill(float v1, float v2, float v3, float alpha);
  void fill(std::string_view css);
  void fill(material::Color color);
  /** A material as the fill — the pen's own overload. A live material
   *  is resolved against the pen's clock on every draw; a static one
   *  once, here. */
  void fill(const material::skia::Paint& paint);
  /** THE SAME MATERIAL, FITTED TO WHAT IT PAINTS: `SHAPE` measures it
   *  against the bounds of each shape the pen draws, `CANVAS` — the
   *  default — against the frame. It is style, so `push` saves it.
   *  @trap Text, images and `background` are always the canvas, being no
   *  shape; a box with no width or no height falls back to it too. */
  void fill(const material::skia::Paint& paint, Constant fit);
  /** A recipe instance as the fill: a shader. */
  void fill(const material::Material& material);
  void noFill();

  void stroke(float gray);
  void stroke(float gray, float alpha);
  void stroke(float v1, float v2, float v3);
  void stroke(float v1, float v2, float v3, float alpha);
  void stroke(std::string_view css);
  void stroke(material::Color color);
  void stroke(const material::skia::Paint& paint);
  /** Fitted exactly as the fill is: `SHAPE` measures the material against
   *  each shape's bounds, `CANVAS` against the frame. */
  void stroke(const material::skia::Paint& paint, Constant fit);
  void stroke(const material::Material& material);
  void noStroke();
  void strokeWeight(float weight);
  void strokeCap(Constant cap);
  void strokeJoin(Constant join);
  void smooth();
  /** Jagged edges AND jagged pixels: antialiasing off on every shape,
   *  and `image` sampled nearest-neighbour with no mipmap. `smooth()`
   *  puts both back. */
  void noSmooth();
  /** THE PEN'S OWN STROKE VERB: a dashed stroke. @p intervals is the
   *  run of lengths it alternates along, on first, an odd run repeating
   *  itself; @p phase starts that run partway in. Both are the pen's own
   *  units, along the path, so a dashed shape under a `scale` dashes at
   *  the scaled length. It is style, so `push` saves it.
   *  @silent the verb is `point`, which is a disc and not a stroke, or
   *  the run has a negative length or no length at all. */
  void strokeDash(std::span<const float> intervals, float phase = 0);
  void strokeDash(std::initializer_list<float> intervals, float phase = 0) {
    strokeDash(std::span<const float>{intervals.begin(), intervals.size()},
               phase);
  }
  void noDash();
  /** @} */

  /** @name Blending
   *  How what is drawn combines with what is already there. It is
   *  style, so `push` saves it and `pop` puts it back.
   *  @{ */
  /** HOW WHAT IS DRAWN MEETS WHAT IS ALREADY THERE: `BLEND` over by
   *  alpha, where a pen starts; `ADD` clamped, which is what light does;
   *  `REPLACE`, `REMOVE`, and the rest of the separable functions. It
   *  reaches every verb that puts pixels down. */
  void blendMode(Constant mode);
  /** @} */

  /** @name Modes
   *  How a shape verb reads its arguments: which corner or centre a
   *  box is placed by, and what unit an angle is in.
   *  @{ */
  void rectMode(Constant mode);
  void ellipseMode(Constant mode);
  void imageMode(Constant mode);
  void angleMode(Constant mode);
  /** The mode every angle-taking verb reads: RADIANS until set. A
   *  library drawing beside the pen — a brush engine — reads it here so
   *  its angles and the pen's are in one unit. */
  [[nodiscard]] Constant angleMode() const { return m_style.angleMode; }
  /** @} */

  /** @name Shapes
   *  Every mark the pen puts down, in p5's spellings and in added
   *  overloads taking this repository's point and silhouette values.
   *  @{ */
  void point(float x, float y);
  void line(float x1, float y1, float x2, float y2);
  /** THE SAME MARKS TAKING POINTS. p5 has no such overload because p5 has
   *  no point value; this repository's points ARE values, and every verb
   *  beside these takes one — so a curve read off a contour or a chord
   *  found on a ring is drawn without being unpacked into four floats. */
  void line(SkPoint from, SkPoint to) { line(from.fX, from.fY, to.fX, to.fY); }
  void point(SkPoint at) { point(at.fX, at.fY); }
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
  void circle(SkPoint centre, float d) { circle(centre.fX, centre.fY, d); }
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
   *  WHEN IT IS ADDED, so a `fill` between two of them colours the shape
   *  corner by corner. A shape whose corners all carry one colour is
   *  drawn as a path exactly as before.
   *  @trap A shape whose corners differ is FILLED AS A TRIANGLE MESH, so
   *  its fill must be a solid colour; and only the triangle and quad
   *  kinds have a mesh. */
  void vertex(float x, float y);
  void curveVertex(float x, float y);
  void bezierVertex(float x2, float y2, float x3, float y3, float x4, float y4);
  void quadraticVertex(float cx, float cy, float x3, float y3);
  void beginContour();
  void endContour();
  void endShape(Constant mode = OPEN);

  /** THE PEN'S OWN SHAPE VERB: a silhouette — a geometry kit value, or
   *  any comparable value with `path(SkSize)` — fitted to the box the
   *  rect mode reads from the four numbers, filled and stroked as a
   *  rect is. The concept is the geometry kit's own, so a value written
   *  for `rounded` or `hatch` is a value this verb draws. */
  template <geometry::shapes::Silhouette S>
  void shape(const S& silhouette, float x, float y, float w, float h) {
    const SkRect box = rectBox(x, y, w, h);
    shape(silhouette.path({box.width(), box.height()})
              .makeOffset(box.left(), box.top()));
  }
  /** A path as it stands, filled and stroked with the current style. */
  void shape(const SkPath& path);

  /** THE PEN'S OWN MESH VERB: an `SkVertices` built somewhere else,
   *  drawn HERE with the pen's fill, blend, clip and transform. The
   *  pen's FILL is what paints it, the mesh's own corner colours
   *  standing aside for a material and standing in for a plain colour.
   *  @silent the mesh is stroked or recorded into a clip: it has no
   *  outline, as a `line` and an `image` have none. */
  void vertices(const sk_sp<SkVertices>& mesh);
  /** @} */

  /** @name The clip
   *  A mask drawn by a callable, confining everything drawn after it
   *  until the matching `pop()`.
   *  @{ */
  /** p5's CLIP: @p shape draws the mask, and everything drawn after it
   *  is confined to what @p shape covered. IT LASTS UNTIL THE MATCHING
   *  `pop()`, and to the end of the frame when it was set outside any
   *  `push`; a clip inside a clip keeps only what falls in both.
   *  @trap NOTHING @p shape DRAWS LANDS ON THE CANVAS: its shape verbs
   *  are recorded into one path instead, and a verb with no outline —
   *  a `line`, an `image`, a `text`, a `background` — adds nothing. */
  template <class Shape>
  void clip(Shape&& shape, ClipOptions options = {}) {
    recordClip();
    shape();
    applyClip(options);
  }
  /** @} */

  /** @name Text
   *  The type a run of text is set in, where it sits, and what it
   *  measures.
   *  @{ */
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
  /** @} */

  /** @name Images
   *  A picture or an offscreen buffer put down, whole or in part.
   *  @{ */
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
  /** @} */

  /** @name The transform
   *  Where the pen draws and how large, and the stack that saves and
   *  restores it together with the style.
   *  @{ */
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
  /** @} */

  /** @name Random and noise
   *  Seeded once per pen, so a frame stepped from zero draws the same
   *  picture every time.
   *  @{ */
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
  /** @} */

  /** @name A retained guest
   *  Something another library keeps between frames, painted inside a
   *  box on this one and told apart by the call site that painted it.
   *  @{ */
  /** THE OTHER WAY THROUGH THE DOOR: something another library keeps
   *  between frames, painted inside @p box on this frame, with this pen
   *  lending it the canvas, the transform above the box and the clock.
   *  The guest is told apart by the call site and @p index, and it
   *  advances only on the frames it is painted. */
  template <Retainable G>
  void element(const G& guest, const SkRect& box, int index = 0,
               std::source_location where = std::source_location::current()) {
    if (!m_canvas) return;
    paintRetained(*this, guest, box, Slot::at(where, index));
  }
  /** @} */

  /** @name The paints
   *  The fill and the stroke as Skia paints, resolved for this frame,
   *  for a caller drawing through the canvas rather than through a
   *  verb.
   *  @{ */
  /** The fill as an SkPaint, resolved against the CANVAS for this
   *  frame, a material fitted to the shape having no shape here.
   *  @trap NULL UNDER `noFill()`, which means there is no fill to hand
   *  over rather than a colourless one — and the pen's blend, its
   *  antialiasing and its dash go with it. */
  [[nodiscard]] const SkPaint* fillPaint();
  /** The stroke as an SkPaint, resolved for this frame; null when
   *  `noStroke()` holds or the weight is zero, on the same rule. */
  [[nodiscard]] const SkPaint* strokePaint();
  /** @} */

 private:
  struct Style {
    bool doFill = true;
    bool doStroke = true;
    /** Whether `fill` or `stroke` was ever called — p5 fills text black
     *  until a fill is set, and strokes it only once a stroke is. */
    bool fillSet = false;
    bool strokeSet = false;
    /** Whether `inherit` seeded the fill. The glyphs take a seeded fill
     *  exactly as they take a chosen one, so text under an inherited ink
     *  is set in it; p5's own black stands only for a pen nobody told. */
    bool fillSeeded = false;
    /** Whether `textFont`, `textSize` or `textStyle` was ever called — an
     *  inherited font seeds the type only until one of them does. */
    bool typeSet = false;
    /** Whether the fill's and the stroke's coordinates are measured
     *  against each shape's bounds rather than against the canvas. */
    bool fillFitted = false;
    bool strokeFitted = false;
    material::skia::Paint fill = material::skia::Paint::solid({1, 1, 1, 1});
    material::skia::Paint stroke = material::skia::Paint::solid({0, 0, 0, 1});
    float strokeWeight = 1.0f;
    geometry::path::Cap cap = geometry::path::Cap::Round;
    geometry::path::Join join = geometry::path::Join::Miter;
    /** The dash, already built; null is a solid stroke. */
    sk_sp<SkPathEffect> dash;
    bool antiAlias = true;
    Constant blend = BLEND;
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

  /** Opens the recording a `clip` shape is gathered into, and closes it
   *  onto the canvas at the transform the recording began at. */
  void recordClip();
  void applyClip(ClipOptions options);
  /** While a clip is being recorded, @p path joins the mask instead of
   *  being drawn, carried into the space the recording began in; the
   *  answer is whether it was taken. */
  bool recordShape(const SkPath& path);

  [[nodiscard]] SkRect rectBox(float x, float y, float w, float h) const;
  [[nodiscard]] static SkRect boxIn(Constant mode, float x, float y, float w,
                                    float h);
  [[nodiscard]] float toDegrees(float angle) const;
  [[nodiscard]] float toRadians(float angle) const;
  void applyStyle();
  /** The blend the style stands at, onto a paint about to draw with it.
   *  Every paint the pen hands to the canvas goes through here, so a
   *  mode set once holds for the image and the glyph as well as the
   *  shape. */
  void blendInto(SkPaint& paint) const;
  void resolveFill();
  void resolveStroke();
  [[nodiscard]] material::skia::PaintFrame paintFrame() const;
  /** The fill and the stroke resolved against @p box when the material was
   *  set `SHAPE`-fitted, and against the canvas otherwise. A null or
   *  degenerate box is the canvas. */
  [[nodiscard]] const SkPaint* fillPaint(const SkRect* box);
  [[nodiscard]] const SkPaint* strokePaint(const SkRect* box);
  [[nodiscard]] sk_sp<SkShader> fittedShader(const material::skia::Paint& paint,
                                             const SkRect& box) const;
  void paintFilled(const SkPath& path);
  /** The mesh a shape whose corners carry different fills is drawn as:
   *  triangles in threes, each corner its own colour. */
  void paintVertices(std::span<const SkPoint> positions,
                     std::span<const SkColor> colors);
  void paintOval(const SkRect& oval);
  void paintRect(const SkRect& rect);
  void flushCurve();
  void emitKind(std::span<const SkPoint> v);
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
  material::Color m_inheritedInk{0, 0, 0, 1};
  weave::Type m_inheritedFont = weave::initialType();

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

  // The mask a clip is gathering, and the transform it is gathered in.
  SkPathBuilder m_clipBuilder;
  SkM44 m_clipBase;
  bool m_clipRecording = false;

  core::noise::Mix64Stream m_random;
  bool m_gaussianHeld = false;
  float m_gaussianNext = 0.0f;
  NoiseField m_noise;
  Retained m_retained;
};

/** ONE DRAWING ON A CANVAS SOMEBODY ELSE HOLDS: a pen begun over
 *  @p canvas at @p size, @p program run with it, and the frame ended.
 *  The pen lives for the call and no longer, and it has no clock, so the
 *  elapsed time reads zero and the frame count one. @p fonts is what
 *  text is shaped with.
 *  @trap Nothing is kept between bakes, which is what separates this
 *  from `Graphics`. */
void on(SkCanvas& canvas, SkSize size, const std::function<void(Pen&)>& program,
        weave::FontContext* fonts = nullptr);

}  // namespace sigil::draw
