/** @file
 * What one frame of the window's pane costs: still at a zoom, panned,
 * zoomed at every frame with and without the scale held, and drawn the
 * way a pane that grew with the zoom drew it, for comparison.
 */

// Registered the way a sketch file is, so the bench opens its scene
// through the registry exactly as the window does. It has to stand
// before the prelude: the macro chooses its form at include time.
#define SIGIL_SKETCH_STATIC "pane_bench_probe"

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

#include "../CanvasView.h"

namespace {

using namespace sigil::sketch;
namespace compose = sigil::compose;

/** THE SCENE: a field of tiles drawn afresh every frame, over which a
 *  `graphics()` buffer the canvas's size accumulates strokes. The field
 *  is what a frame costs at any scale; the buffer is formed at the scale
 *  the sketch is drawn at, so it is what a change of that scale makes
 *  again. */
struct PaneProbe {
  static constexpr int kColumns = 30;
  static constexpr int kRows = 24;

  void setup(SketchContext& ctx) {
    ctx.canvas(900, 720);
    ctx.background({0.08f, 0.08f, 0.1f, 1});
    std::vector<compose::Element> tiles;
    tiles.reserve((size_t)(kColumns * kRows));
    const float width = 900.0f / kColumns;
    const float height = 720.0f / kRows;
    for (int row = 0; row < kRows; ++row)
      for (int column = 0; column < kColumns; ++column) {
        const float shade = (float)((row * 7 + column * 13) % 17) / 17.0f;
        tiles.push_back(
            compose::kit::at(column * width + 1.0f, row * height + 1.0f,
                             width - 2.0f, height - 2.0f)
                .fill(compose::linearGradient(
                    {0, 0}, {width, height},
                    {{0.3f + 0.5f * shade, 0.35f, 0.6f - 0.3f * shade, 1},
                     {0.1f, 0.1f + 0.4f * shade, 0.3f, 1}})));
      }
    ctx.composer.render(
        compose::box()
            .inset(0)
            .children(
                {compose::positioned()
                     .inset(0)
                     .cache(compose::Cache::None)
                     .children(std::move(tiles)),
                 compose::graphics("pane_bench_probe.trail",
                                   [](sigil::draw::Pen& pen) {
                                     const float turn =
                                         (float)pen.frameCount * 0.07f;
                                     pen.stroke(1.0f, 0.9f, 0.6f, 0.5f);
                                     pen.strokeWeight(2.0f);
                                     pen.line(450.0f, 360.0f,
                                              450.0f + 340.0f * std::cos(turn),
                                              360.0f + 340.0f * std::sin(turn));
                                   })
                     .inset(0)}));
  }
};

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

Assets& assets() {
  static auto* store = new Assets("");
  return *store;
}

/** A pane a Retina window gives the canvas, in device pixels, and the
 *  density the window declares for a sketch's bakes. */
constexpr SkISize kPane = {1600, 1200};
constexpr float kScreenDensity = 2.0f;
constexpr double kFrameStep = 1.0 / 60.0;

/** THE PROBE, opened the way the window opens a sketch: its bakes
 *  declared at the screen's density, and stepped a few frames so what is
 *  measured is the scene standing. */
struct Pane {
  std::unique_ptr<Session> session;
  SkSize canvas = SkSize::MakeEmpty();
  SkColor4f ground = SkColors::kTransparent;
  sk_sp<SkSurface> pane;
  PaneLayer layer;

  bool open(benchmark::State& state, SkISize extent = kPane) {
    const int index = find("pane_bench_probe");
    if (index < 0) {
      state.SkipWithError("the probe is not in this binary's registry");
      return false;
    }
    const Entry& entry = registry()[(size_t)index];
    session = entry.kind()->open(fonts(), assets(), false, entry.key);
    session->setBakeDensity(kScreenDensity);
    canvas = session->canvas().size;
    ground = sigil::material::skia::toSkColor(session->canvas().background);
    pane = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(extent));
    if (!pane) {
      state.SkipWithError("the pane could not be allocated");
      return false;
    }
    for (int step = 0; step < 6; ++step) frame(CanvasView{});
    return true;
  }

  /** The scale, in pane pixels per canvas unit, that magnifies the
   *  whole-canvas fit by @p zoom. */
  [[nodiscard]] float zoomed(float zoom) const {
    return placeCanvas(canvas, SkSize::Make(kPane), {}).scale * zoom;
  }

  void frame(const CanvasView& view, float drawnScale = 0.0f) {
    const SkISize extent = pane->imageInfo().dimensions();
    drawPane(
        *pane->getCanvas(), SkSize::Make(extent), canvas, view,
        [&](SkCanvas& into) {
          into.clear(ground);
          session->frame(into, kFrameStep);
        },
        drawnScale, &layer);
  }
};

/** ONE FRAME OF THE PANE at the zoom the argument names, magnified about
 *  the pane's centre. The target stays the pane, so a deeper zoom draws
 *  less of the canvas into the same pixels. */
void PaneFrame(benchmark::State& state) {
  Pane pane;
  if (!pane.open(state)) return;
  const CanvasView view{pane.zoomed((float)state.range(0)), {0, 0}};
  for (auto&& _ : state) pane.frame(view);
  state.counters["targetPixels"] =
      (double)kPane.width() * (double)kPane.height();
}
BENCHMARK(PaneFrame)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

/** THE SAME FRAME DRAWN THE WAY A PANE THAT GREW WITH THE ZOOM DREW IT:
 *  a target the pane's size times the zoom each way, the whole canvas
 *  fitted into it, which the scene graph then showed a pane's worth of.
 *  What the view above is measured against. */
void GrownPaneFrame(benchmark::State& state) {
  const int zoom = (int)state.range(0);
  Pane pane;
  if (!pane.open(state, {kPane.width() * zoom, kPane.height() * zoom}))
    return;
  for (auto&& _ : state) pane.frame(CanvasView{});
  state.counters["targetPixels"] = (double)kPane.width() * zoom *
                                   (double)kPane.height() * zoom;
}
BENCHMARK(GrownPaneFrame)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

/** THE SAME PANE WHILE THE READER DRAGS IT, at the zoom the argument
 *  names: the view moves by a pixel every frame. */
void PaneFramePanning(benchmark::State& state) {
  Pane pane;
  if (!pane.open(state)) return;
  const float scale = pane.zoomed((float)state.range(0));
  float drag = 0.0f;
  for (auto&& _ : state) {
    pane.frame(CanvasView{scale, {drag, drag * 0.5f}});
    drag += 1.0f;
  }
}
BENCHMARK(PaneFramePanning)
    ->Arg(2)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

/** The zoom of a pinch at frame @p index: from the fit to four times it
 *  and back, three percent a frame. */
float pinchAt(int index) {
  constexpr int kLeg = 47;  // 1.03 to the 47th is four, near enough
  const int phase = index % (2 * kLeg);
  const int step = phase < kLeg ? phase : 2 * kLeg - phase;
  return std::pow(1.03f, (float)step);
}

/** THE SAME PANE WHILE THE READER PINCHES, the scale the sketch is drawn
 *  at following the view at every frame: whatever the scene keeps at
 *  the scale it is drawn at is made again at each of them. */
void PaneFrameZooming(benchmark::State& state) {
  Pane pane;
  if (!pane.open(state)) return;
  int index = 0;
  for (auto&& _ : state)
    pane.frame(CanvasView{pane.zoomed(pinchAt(index++)), {0, 0}});
}
BENCHMARK(PaneFrameZooming)->Unit(benchmark::kMillisecond)->UseRealTime();

/** THE SAME PINCH AS THE WINDOW DRAWS IT, the scale held while the view
 *  moves and let go past twice or half of it: a frame of the gesture
 *  follows the view only at those crossings. The clock is the gesture's
 *  own, a sixtieth of a second a frame, so the hold never settles. */
void PaneFrameZoomingHeld(benchmark::State& state) {
  Pane pane;
  if (!pane.open(state)) return;
  ZoomHold hold;
  ZoomHold::Clock::time_point now{};
  int index = 0;
  for (auto&& _ : state) {
    const float scale = pane.zoomed(pinchAt(index++));
    now += std::chrono::microseconds(16667);
    pane.frame(CanvasView{scale, {0, 0}}, hold.drawnScale(scale, now));
  }
}
BENCHMARK(PaneFrameZoomingHeld)->Unit(benchmark::kMillisecond)->UseRealTime();

}  // namespace

SIGIL_SKETCH(PaneProbe, "Bench", "the pane bench's own fixture")
