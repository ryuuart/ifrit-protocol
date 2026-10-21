// WHAT A FILTERED STROKE'S LAYER COSTS AT THE SIZE IT IS GIVEN: strokes
// laid in page coordinates on page-sized boxes, each blurred and faded, as
// a height map is drawn, against the same strokes on boxes fitted to them,
// and against page-sized boxes whose node also carries a mark laid on its
// box, which keeps the box as the node's size. The first and the second
// open layers at each stroke's ink; the third opens them at the page. Each
// set is run on a raster surface, and again on a Graphite Metal surface
// when the binary carries one.

#include <include/effects/SkImageFilters.h>
#include <sigilcompose/Compose.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Transform.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "BenchSupport.h"

using namespace sigil::compose;
using sigil::compose::bench::Host;

namespace {

namespace path = sigil::geometry::path;

constexpr int kPageWidth = 1500, kPageHeight = 1000;

/** How each stroke is placed on the page. */
enum class Placement {
  /** On a box the size of the page, its spine in page coordinates. */
  PageBox,
  /** On a box fitted to its spine, the spine moved into the box. */
  FittedBox,
  /** On a page-sized box that also carries a mark laid on the box. */
  PageBoxWithBoxMark,
};

/** A MARK THAT SAYS NOTHING ABOUT WHERE IT STANDS and draws nothing: a
 *  node carrying it is sized to its box, whatever its shape. */
struct MarkOnTheBox {
  bool operator==(const MarkOnTheBox&) const = default;
  void paint(sigil::draw::Pen&, const PaintContext&) const {}
};

/** The spine of stroke @p index: a short wandering run of three legs
 *  somewhere on the page, the same run every time it is asked for. */
std::string spineOf(int index) {
  const float across = std::fmod(97.0f * (float)index, kPageWidth - 240.0f);
  const float down = std::fmod(61.0f * (float)index, kPageHeight - 160.0f);
  float x = 120 + across, y = 80 + down;
  std::string spine = "M" + std::to_string(x) + " " + std::to_string(y);
  float heading = 0.7f * (float)index;
  for (int leg = 0; leg < 3; ++leg) {
    heading += 0.6f;
    x += std::cos(heading) * 60.0f;
    y += std::sin(heading) * 60.0f;
    spine += " L" + std::to_string(x) + " " + std::to_string(y);
  }
  return spine;
}

Element strokesOn(int count, Placement placement) {
  std::vector<Element> strokes;
  strokes.reserve((size_t)count);
  for (int index = 0; index < count; ++index) {
    path::Outline spine = path::Outline::svg(spineOf(index));
    float left = 0, top = 0, width = kPageWidth, height = kPageHeight;
    if (placement == Placement::FittedBox) {
      const path::Rect bounds = spine.bounds();
      left = bounds.left();
      top = bounds.top();
      width = std::max(bounds.width(), 1.0f);
      height = std::max(bounds.height(), 1.0f);
      spine = spine.transformed(path::Transform::translate({-left, -top}));
    }
    Element stroke =
        box()
            .absolute()
            .left(left)
            .top(top)
            .width(width)
            .height(height)
            .shape(heldPath(std::move(spine)))
            .stroke(sigil::material::Material(
                        sigil::material::Color{0.9f, 0.9f, 0.9f, 1}),
                    {.width = 4})
            .opacity(0.5f)
            .filter(sigil::material::skia::filter(
                SkImageFilters::Blur(5, 5, nullptr)));
    if (placement == Placement::PageBoxWithBoxMark)
      stroke.foreground(MarkOnTheBox{});
    strokes.push_back(std::move(stroke));
  }
  return box()
      .width(kPageWidth)
      .height(kPageHeight)
      .fill(Fill::color({0, 0, 0, 1}))
      .children(std::move(strokes));
}

void inkBoundsArm(benchmark::State& state, Placement placement) {
  Host host(kPageWidth, kPageHeight);
  host.composer.render(strokesOn((int)state.range(0), placement));
  host.draw();
  for ([[maybe_unused]] auto iteration : state) host.draw();
}

}  // namespace

static void BM_Draw_BlurredStrokes_PageBox(benchmark::State& state) {
  inkBoundsArm(state, Placement::PageBox);
}
BENCHMARK(BM_Draw_BlurredStrokes_PageBox)->Arg(50)->Arg(250);

static void BM_Draw_BlurredStrokes_FittedBox(benchmark::State& state) {
  inkBoundsArm(state, Placement::FittedBox);
}
BENCHMARK(BM_Draw_BlurredStrokes_FittedBox)->Arg(50)->Arg(250);

static void BM_Draw_BlurredStrokes_PageBoxWithBoxMark(benchmark::State& state) {
  inkBoundsArm(state, Placement::PageBoxWithBoxMark);
}
BENCHMARK(BM_Draw_BlurredStrokes_PageBoxWithBoxMark)->Arg(50)->Arg(250);

#ifdef SIGIL_BENCH_GPU

using sigil::compose::bench::GraphiteTarget;

namespace {

void inkBoundsGraphiteArm(benchmark::State& state, Placement placement) {
  GraphiteTarget target(state, kPageWidth, kPageHeight);
  if (!target.ok()) return;
  Host host(kPageWidth, kPageHeight);
  host.composer.render(strokesOn((int)state.range(0), placement));
  host.composer.draw(target.canvas());
  target.submitSynced();
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.draw(target.canvas());
    target.submitSynced();
  }
}

}  // namespace

static void BM_Draw_BlurredStrokes_PageBox_Graphite(benchmark::State& state) {
  inkBoundsGraphiteArm(state, Placement::PageBox);
}
BENCHMARK(BM_Draw_BlurredStrokes_PageBox_Graphite)->Arg(50)->Arg(250);

static void BM_Draw_BlurredStrokes_FittedBox_Graphite(
    benchmark::State& state) {
  inkBoundsGraphiteArm(state, Placement::FittedBox);
}
BENCHMARK(BM_Draw_BlurredStrokes_FittedBox_Graphite)->Arg(50)->Arg(250);

static void BM_Draw_BlurredStrokes_PageBoxWithBoxMark_Graphite(
    benchmark::State& state) {
  inkBoundsGraphiteArm(state, Placement::PageBoxWithBoxMark);
}
BENCHMARK(BM_Draw_BlurredStrokes_PageBoxWithBoxMark_Graphite)
    ->Arg(50)
    ->Arg(250);

#endif
