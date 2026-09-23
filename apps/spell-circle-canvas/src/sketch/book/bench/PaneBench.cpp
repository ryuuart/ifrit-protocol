/** @file
 * What one frame of the window's pane costs as the reader zooms in, for
 * a scene whose tiling is drawn afresh every frame.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <memory>

#include "../CanvasView.h"

namespace {

using namespace sigil::sketch;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

/** The sketch's own files stand beside it, so the store mounts the
 *  directory the registry's sketches stand in. */
Assets& assets() {
  static auto* store = new Assets("", SIGIL_SKETCH_DIR);
  return *store;
}

/** A pane a Retina window gives the canvas, in device pixels, and the
 *  density the window declares for a sketch's bakes. */
constexpr SkISize kPane = {1600, 1200};
constexpr float kScreenDensity = 2.0f;

/** THE REGISTRY'S `penrose_paving`, opened the way the window opens a
 *  sketch: its bakes declared at the screen's density, and stepped past
 *  its entrance so what is measured is the scene standing. Its tiling is
 *  declared uncached, so every frame draws it afresh. */
struct Paving {
  std::unique_ptr<Session> session;
  SkSize canvas = SkSize::MakeEmpty();
  SkColor4f ground = SkColors::kTransparent;
  sk_sp<SkSurface> pane;

  bool open(benchmark::State& state) {
    const int index = find("penrose_paving");
    if (index < 0) {
      state.SkipWithError("penrose_paving is not in this binary's registry");
      return false;
    }
    const Entry& entry = registry()[(size_t)index];
    session = entry.kind()->open(fonts(), assets(), false, entry.key);
    session->setBakeDensity(kScreenDensity);
    canvas = session->canvas().size;
    ground = sigil::material::skia::toSkColor(session->canvas().background);
    pane = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kPane));
    if (!pane) {
      state.SkipWithError("the pane could not be allocated");
      return false;
    }
    for (int step = 0; step < 12; ++step) frame(CanvasView{}, 0.5);
    return true;
  }

  /** The view that magnifies the whole-canvas fit by @p zoom. */
  [[nodiscard]] CanvasView zoomed(float zoom, SkPoint offset) const {
    return {placeCanvas(canvas, SkSize::Make(kPane), {}).scale * zoom, offset};
  }

  void frame(const CanvasView& view, double step) {
    drawPane(*pane->getCanvas(), SkSize::Make(kPane), canvas, view,
             [&](SkCanvas& into) {
               into.clear(ground);
               session->frame(into, step);
             });
  }
};

/** ONE FRAME OF THE PANE at the zoom the argument names, magnified about
 *  the pane's centre. The target stays the pane, so a deeper zoom draws
 *  less of the canvas into the same pixels. */
void PaneFrame(benchmark::State& state) {
  Paving paving;
  if (!paving.open(state)) return;
  const CanvasView view = paving.zoomed((float)state.range(0), {0, 0});
  for (auto&& _ : state) paving.frame(view, 1.0 / 60.0);
  state.counters["targetPixels"] =
      (double)kPane.width() * (double)kPane.height();
}
BENCHMARK(PaneFrame)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

/** THE SAME PANE WHILE THE READER DRAGS IT, at the zoom the argument
 *  names: the view moves by a pixel every frame, so a cache that asked
 *  whether its node stands exactly where it stood would be taken again
 *  on every one of them, and this would read above the still pane. */
void PaneFramePanning(benchmark::State& state) {
  Paving paving;
  if (!paving.open(state)) return;
  const float zoom = (float)state.range(0);
  float drag = 0.0f;
  for (auto&& _ : state) {
    paving.frame(paving.zoomed(zoom, {drag, drag * 0.5f}), 1.0 / 60.0);
    drag += 1.0f;
  }
}
BENCHMARK(PaneFramePanning)
    ->Arg(2)
    ->Unit(benchmark::kMillisecond)
    ->UseRealTime();

}  // namespace
