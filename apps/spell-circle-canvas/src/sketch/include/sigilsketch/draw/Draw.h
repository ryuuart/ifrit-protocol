#pragma once

/** @file
 * The immediate-mode sketch surface: p5's setup and draw over a pen.
 * Include this and SIGIL_SKETCH registers a sketch that draws with a
 * `draw::Pen` every frame onto a canvas that persists between frames.
 */

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <sigildraw/Draw.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/CanvasSpec.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>

#include <concepts>
#include <memory>
#include <string_view>

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch {

/** WHAT A DRAW SKETCH IS HANDED AT SETUP: the canvas it declares, what
 *  is behind it, the moment a still is taken, the files it reaches for
 *  — and the pen, for whatever a p5 setup would have set on the canvas:
 *  a style, a font, a first background, a drawing that is never redrawn.
 *  What the pen draws during setup lands on the first frame's canvas.
 *
 *  Handed once per setup and non-copyable: it points at the session's
 *  own spec, and a copy kept past setup would write into nothing. */
struct DrawContext {
  draw::Pen& pen;
  Assets& assets;
  weave::FontContext& fonts;
  CanvasSpec* spec = nullptr;  ///< host-owned; written via the calls below

  DrawContext(draw::Pen& penIn, Assets& assetsIn, weave::FontContext& fontsIn,
              CanvasSpec* specIn, bool deterministicIn)
      : pen(penIn),
        assets(assetsIn),
        fonts(fontsIn),
        spec(specIn),
        deterministic(deterministicIn) {}
  DrawContext(const DrawContext&) = delete;
  DrawContext& operator=(const DrawContext&) = delete;

  /** The host is taking a capture that will be DIFFED, so anything the
   *  sketch measured about its own execution must be pinned. */
  bool deterministic = false;

  /** A number the sketch measured about ITS OWN EXECUTION — a build
   *  time, a bake cost. Returns @p value normally and @p pinned when the
   *  host is capturing for a diff, since a plate that carries a number
   *  no two runs agree on is a plate that differs from itself. */
  [[nodiscard]] double measured(double value, double pinned = 0.0) const {
    return deterministic ? pinned : value;
  }

  /** p5's createCanvas: the canvas in its own pixels. The pen's width
   *  and height follow at once, so a setup that draws after declaring
   *  draws at the right size. */
  void canvas(float width, float height) {
    if (spec) spec->size = {width, height};
    pen.width = width;
    pen.height = height;
  }
  /** THE GROUND the canvas starts on — what the first frame finds
   *  before anything is drawn, and what stands behind the canvas where
   *  a host letterboxes it. Read in the pen's colour mode, as p5 reads
   *  a background. */
  void background(SkColor4f color) {
    if (spec) spec->background = color;
  }
  void background(float gray) { background(pen.color(gray)); }
  void background(float gray, float alpha) {
    background(pen.color(gray, alpha));
  }
  void background(float v1, float v2, float v3) {
    background(pen.color(v1, v2, v3));
  }
  void background(float v1, float v2, float v3, float alpha) {
    background(pen.color(v1, v2, v3, alpha));
  }
  void background(std::string_view css) { background(pen.color(css)); }
  /** The scene time a STILL of this sketch is taken at — the moment the
   *  piece is most itself. */
  void captureAt(double seconds) {
    if (spec) spec->captureSeconds = seconds;
  }
  /** p5's loadImage: the image at "res://<name>" — the sketch's assets
   *  directory — as something `pen.image` draws. A file not there yet
   *  yields the placeholder, and the sketch is set up again the moment
   *  it appears. Null only for an asset with no frames. */
  [[nodiscard]] sk_sp<SkImage> loadImage(std::string_view name);
};

/** A SKETCH THAT DRAWS WITH A PEN, p5's way: `setup` once, `draw` every
 *  frame, onto a canvas that KEEPS what earlier frames drew — a
 *  `background` each frame clears it, a translucent one leaves a trail,
 *  none at all accumulates.
 *
 *  The clock is the runtime's: `millis`, `deltaTime` and `frameCount`
 *  on the pen are stepped when a host steps and read off the wall when
 *  a window runs, and `random` starts from the same seed in every
 *  session, so a plate stepped from zero is the same picture every
 *  time. Keep state in members; every reload constructs a fresh
 *  instance and the piece starts over. */
class DrawSketch {
 public:
  virtual ~DrawSketch() = default;
  /** Once per (re)load, and again when an asset file changes. */
  virtual void setup(DrawContext& ctx) = 0;
  /** Every frame, while the loop runs. */
  virtual void draw(draw::Pen& pen) = 0;

  /** p5's pointer and key events, called between frames with the pen
   *  ready to draw; the pen's variables already say where the pointer
   *  is and which key it was. */
  virtual void mousePressed(draw::Pen& pen) { (void)pen; }
  virtual void mouseReleased(draw::Pen& pen) { (void)pen; }
  virtual void mouseMoved(draw::Pen& pen) { (void)pen; }
  virtual void mouseDragged(draw::Pen& pen) { (void)pen; }
  virtual void keyPressed(draw::Pen& pen) { (void)pen; }
  virtual void keyReleased(draw::Pen& pen) { (void)pen; }
};

/** THE IMMEDIATE-MODE KIND: a pen over a surface the session keeps,
 *  stepped by a clock the session owns. */
class DrawKind final : public KindOps {
 public:
  using Factory = DrawSketch* (*)();
  explicit DrawKind(Factory factory) : m_factory(factory) {}
  /** What identifies a kind is the body it opens; see the 2D kind. */
  bool operator==(const DrawKind& other) const {
    return m_factory == other.m_factory;
  }

  [[nodiscard]] std::string_view runtime() const override { return "draw"; }

  [[nodiscard]] std::unique_ptr<Session> open(
      weave::FontContext& fonts, Assets& assets,
      bool deterministic) const override;

 private:
  Factory m_factory;
};

/** The factory SIGIL_SKETCH takes the ADDRESS of; see the 2D one for why
 *  it is a named template rather than a lambda. */
template <class SketchType>
[[nodiscard]] DrawSketch* makeDrawSketch() {
  return new SketchType();
}

/** The kind a draw sketch draws through. */
template <class SketchType>
  requires std::derived_from<SketchType, DrawSketch>
[[nodiscard]] Kind kindOf() {
  return DrawKind{&makeDrawSketch<SketchType>};
}

}  // namespace sigil::sketch
