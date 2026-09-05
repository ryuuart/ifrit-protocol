/** @file
 * The immediate-mode session: a clock, a pen and a surface that keeps
 * what earlier frames drew, with one sketch body drawing into it.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilmeasure/time/Laps.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilsketch/draw/Draw.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace sigil::sketch {

sk_sp<SkImage> DrawContext::loadImage(std::string_view name) {
  const std::shared_ptr<const image::ImageAsset> asset = assets.image(name);
  if (!asset || asset->frames().empty()) return nullptr;
  return asset->frames().front().image;
}

namespace {

/** HOW MANY CANVAS PIXELS ONE DECLARED UNIT COVERS on the canvas a host
 *  hands over: nothing on a plate's, the fit and the screen's own scale
 *  on a live window's. The surface is formed at that many pixels so a
 *  window on a scaled screen is not a magnified picture of a smaller
 *  one, and on a plate's canvas it is the declared size exactly. */
float pixelScale(const SkCanvas& canvas) {
  const float scale = canvas.getTotalMatrix().getMaxScale();
  return std::isfinite(scale) && scale > 0.0f ? scale : 1.0f;
}

/** ONE DRAW SKETCH, RUNNING.
 *
 *  The canvas the sketch draws on is a surface this session keeps, not
 *  the host's: p5's canvas persists between frames, so a trail, a slow
 *  accumulation and a `noLoop` picture all depend on the last frame
 *  still standing when the next begins. Each frame the sketch draws
 *  into the surface and the surface is put back on the host's canvas. */
class DrawSession final : public Session {
 public:
  DrawSession(DrawSketch* sketch, weave::FontContext& fonts, Assets& assets,
              bool deterministic)
      : m_fonts(fonts),
        m_assets(assets),
        m_sketch(sketch),
        m_deterministic(deterministic) {
    runSetup();
  }

  ~DrawSession() override {
    // What the pen kept for its guests and what the ticker steps both
    // point into sketch-owned state, so both go before their author.
    m_pen.retained().clear();
    m_ticker.reset();
    m_sketch.reset();
  }

  [[nodiscard]] const CanvasSpec& canvas() const override { return m_spec; }

  void frame(SkCanvas& canvas, double dt) override {
    m_laps.reset();
    // A STATED step and a wall-clock one are the same clock, exactly as
    // the 2D session keeps them: the raw reading is rebased when a run
    // comes back to wall time, or the first live frame after a sweep
    // would inject one whole maxDelta.
    double step = 0.0;
    if (dt >= 0.0) {
      m_stepping = true;
      step = m_clock.advance(dt);
    } else {
      if (m_stepping) {
        const bool wasPaused = m_clock.paused();
        m_clock.setPaused(true);
        m_clock.tick();
        m_clock.setPaused(wasPaused);
        m_stepping = false;
      }
      step = m_clock.tick();
    }
    m_sinceDraw += step;
    // Stepped whether or not this frame draws: a frame skipped by
    // `frameRate(fps)` or by `noLoop` is still a frame of elapsed time,
    // and a fixed-step simulation counts from elapsed time.
    m_ticker->tick(step);
    ensureSurface(canvas);
    if (shouldDraw()) {
      ++m_frameCount;
      SkCanvas& target = *m_surface->getCanvas();
      SkAutoCanvasRestore restore(&target, true);
      target.scale(m_scale, m_scale);
      m_pen.begin(target,
                  frameNow(m_frameCount, m_clock.elapsed(), m_sinceDraw));
      dispatchEvents();
      DrawContext ctx{m_pen,   *m_ticker, m_assets,
                      m_fonts, &m_spec,   m_deterministic};
      m_sketch->draw(ctx);
      m_pen.end();
      m_sinceDraw = 0.0;
    }
    m_timing.updateMs = m_laps.mark("draw");
    paint(canvas);
    m_timing.drawMs = m_laps.mark("blit");
    m_timing.totalMs = m_laps.totalMs();
    m_lanes = {Lane{"draw", m_timing.updateMs}, Lane{"blit", m_timing.drawMs}};
  }

  void repaint(SkCanvas& canvas) override { paint(canvas); }

  /** The plate IS the frame just finished. The surface holds every
   *  frame's residue, so there is nothing to draw again larger here: a
   *  bigger canvas at this moment would magnify what is already drawn.
   *  A sketch that wants a plate finer than its canvas declares an
   *  oversample instead, which forms the KEPT surface at that many
   *  pixels from the first frame — the sharpening has to happen while
   *  the piece is being drawn, not while it is being photographed. */
  void still(SkCanvas& canvas) override { paint(canvas); }

  void redeclare() override {
    runSetup();
    // A fresh setup starts the canvas over, so the next frame forms it
    // again and lands the new setup's drawing on it.
    m_surface.reset();
  }

  [[nodiscard]] Timing timing() const override { return m_timing; }

  [[nodiscard]] std::span<const Lane> lanes() const override { return m_lanes; }

  [[nodiscard]] std::string counters() const override {
    char line[160];
    std::snprintf(line, sizeof line,
                  "frame %d   surface %dx%d   retained %zu   %s", m_frameCount,
                  m_extent.width(), m_extent.height(), m_pen.retained().size(),
                  m_pen.isLooping() ? "looping" : "noLoop");
    return line;
  }

  /** THE POINTER, in the sketch's own canvas units. Read into the pen at
   *  the next frame; an edge on the button is an event that frame. */
  void pointer(float x, float y, bool pressed) override {
    m_input.mouseX = x;
    m_input.mouseY = y;
    if (pressed != m_input.mouseIsPressed) {
      m_input.mouseIsPressed = pressed;
      (pressed ? m_events.pressed : m_events.released) = true;
    }
  }

  void key(std::string_view name, int code, bool pressed) override {
    auto held =
        std::find(m_input.keysDown.begin(), m_input.keysDown.end(), code);
    if (pressed) {
      m_input.key.assign(name);
      m_input.keyCode = code;
      if (held == m_input.keysDown.end()) m_input.keysDown.push_back(code);
      m_events.keyPressed = true;
    } else {
      if (held != m_input.keysDown.end()) m_input.keysDown.erase(held);
      m_input.keyCode = code;
      m_input.key.assign(name);
      m_events.keyReleased = true;
    }
    m_input.keyIsPressed = !m_input.keysDown.empty();
  }

 private:
  struct Input {
    float mouseX = 0;
    float mouseY = 0;
    bool mouseIsPressed = false;
    bool keyIsPressed = false;
    std::string key;
    int keyCode = 0;
    std::vector<int> keysDown;
  };
  struct Events {
    bool pressed = false;
    bool released = false;
    bool keyPressed = false;
    bool keyReleased = false;
  };

  [[nodiscard]] draw::Frame frameNow(int count, double seconds,
                                     double delta) const {
    draw::Frame frame;
    frame.width = m_spec.size.width();
    frame.height = m_spec.size.height();
    frame.seconds = seconds;
    frame.deltaSeconds = delta;
    frame.frameCount = count;
    frame.fonts = &m_fonts;
    frame.mouseX = m_input.mouseX;
    frame.mouseY = m_input.mouseY;
    frame.mouseIsPressed = m_input.mouseIsPressed;
    frame.keyIsPressed = m_input.keyIsPressed;
    frame.key = m_input.key;
    frame.keyCode = m_input.keyCode;
    frame.keysDown = m_input.keysDown;
    return frame;
  }

  /** SETUP, with the pen recording: what a p5 setup draws lands on the
   *  canvas, and here the canvas does not exist until a host hands one
   *  over, so the setup's drawing is kept as a picture and replayed onto
   *  the surface when it is formed. */
  void runSetup() {
    // A FRESH TICKER, because setup is where steppables are registered:
    // a second setup over the first's registrations would step one
    // simulation twice per frame.
    m_ticker = std::make_unique<motion::Ticker>();
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(SkRect::MakeWH(16384, 16384));
    m_pen.begin(*canvas, frameNow(0, 0.0, 0.0));
    m_pen.width = m_spec.size.width();
    m_pen.height = m_spec.size.height();
    DrawContext ctx{m_pen,   *m_ticker, m_assets,
                    m_fonts, &m_spec,   m_deterministic};
    m_sketch->setup(ctx);
    m_pen.end();
    m_setupPicture = recorder.finishRecordingAsPicture();
  }

  /** Whether this frame runs `draw`: the loop is running or one redraw
   *  was asked for, and a requested frame rate has room for another. */
  bool shouldDraw() {
    const bool asked = m_pen.takeRedraw();
    if (!m_pen.isLooping() && !asked) return false;
    if (asked) return true;
    const double target = m_pen.targetFrameRate();
    if (!(target > 0.0)) return true;
    const double period = 1.0 / target;
    // The time since the last draw has to cover one period; the slack
    // past it carries into the next, bounded so a stall cannot bank a
    // burst of draws.
    if (m_sinceDraw + 1e-9 < period - m_slack) return false;
    m_slack = std::clamp(m_sinceDraw + m_slack - period, 0.0, period);
    return true;
  }

  void dispatchEvents() {
    const Events events = m_events;
    m_events = Events{};
    if (events.pressed) m_sketch->mousePressed(m_pen);
    if (events.released) m_sketch->mouseReleased(m_pen);
    if (m_pen.mouseX != m_pen.pmouseX || m_pen.mouseY != m_pen.pmouseY) {
      if (m_pen.mouseIsPressed)
        m_sketch->mouseDragged(m_pen);
      else
        m_sketch->mouseMoved(m_pen);
    }
    if (events.keyPressed) m_sketch->keyPressed(m_pen);
    if (events.keyReleased) m_sketch->keyReleased(m_pen);
  }

  /** The declared canvas in the pixels @p canvas has for it, or in the
   *  declared oversample where that is more — the number is a FLOOR, so
   *  a sketch that reconstructs pixels draws at its own grid on a plate
   *  and on a screen coarser than it alike, and a finer screen still
   *  gets its own pixels. */
  [[nodiscard]] SkISize extentOn(const SkCanvas& canvas) const {
    const float scale =
        std::max(pixelScale(canvas), (float)std::max(1, m_spec.oversample));
    return {std::max(1, (int)std::lround(m_spec.size.width() * scale)),
            std::max(1, (int)std::lround(m_spec.size.height() * scale))};
  }

  /** The surface the sketch draws on, formed at the pixels the host's
   *  canvas has for the declared size. It is made through the host's
   *  canvas so it lives where the host draws — on the device when the
   *  host is on one — and falls back to raster. The first surface starts
   *  on the ground and the setup's drawing. A replacement surface scales
   *  the pixels already kept into its new extent, because changing the
   *  presentation scale must not erase an accumulated canvas. */
  void ensureSurface(SkCanvas& canvas) {
    const SkISize extent = extentOn(canvas);
    if (m_surface && m_extent == extent) return;
    const SkImageInfo info = SkImageInfo::MakeN32Premul(extent);
    sk_sp<SkSurface> surface = canvas.makeSurface(info);
    if (!surface) surface = SkSurfaces::Raster(info);
    SkCanvas& target = *surface->getCanvas();
    target.clear(m_spec.background);
    if (m_surface) {
      SkAutoCanvasRestore restore(&target, true);
      target.scale((float)extent.width() / (float)m_extent.width(),
                   (float)extent.height() / (float)m_extent.height());
      m_surface->draw(&target, 0, 0, SkSamplingOptions(SkFilterMode::kLinear),
                      nullptr);
    } else if (m_setupPicture) {
      SkAutoCanvasRestore restore(&target, true);
      const float scale = (float)extent.width() / m_spec.size.width();
      target.scale(scale, scale);
      target.drawPicture(m_setupPicture);
    }
    m_extent = extent;
    m_scale = (float)extent.width() / m_spec.size.width();
    m_surface = std::move(surface);
  }

  void paint(SkCanvas& canvas) {
    if (!m_surface) return;
    // The picture is as many pixels across as the surface was formed
    // at and is put back on the declared canvas here; on a canvas at
    // the declared size that is the identity, and the bytes are the
    // plate's.
    SkAutoCanvasRestore restore(&canvas, true);
    canvas.scale(m_spec.size.width() / (float)m_extent.width(),
                 m_spec.size.height() / (float)m_extent.height());
    m_surface->draw(&canvas, 0, 0);
  }

  weave::FontContext& m_fonts;
  Assets& m_assets;
  motion::FrameClock m_clock;
  // Held indirectly so a fresh setup can replace it whole; see runSetup.
  std::unique_ptr<motion::Ticker> m_ticker;
  CanvasSpec m_spec;
  std::unique_ptr<DrawSketch> m_sketch;
  draw::Pen m_pen;
  sk_sp<SkPicture> m_setupPicture;
  sk_sp<SkSurface> m_surface;
  SkISize m_extent{1, 1};
  float m_scale = 1.0f;
  int m_frameCount = 0;
  double m_sinceDraw = 0.0;
  double m_slack = 0.0;
  Input m_input;
  Events m_events;
  Timing m_timing;
  // Reset per frame rather than built per frame, so the laps a frame
  // lays cost no allocation inside the span they are timing.
  measure::Laps m_laps;
  std::array<Lane, 2> m_lanes{};
  bool m_stepping = false;  // the last frame took a stated step
  bool m_deterministic;
};

}  // namespace

std::unique_ptr<Session> DrawKind::open(weave::FontContext& fonts,
                                        Assets& assets,
                                        bool deterministic) const {
  return std::make_unique<DrawSession>(m_factory(), fonts, assets,
                                       deterministic);
}

}  // namespace sigil::sketch
