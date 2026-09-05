/** @file
 * The door's cost in both directions: a node running a pen program of a
 * few hundred circles, and a pen painting a retained card every frame.
 */

#include <benchmark/benchmark.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/draw/Draw.h>

#include "BenchSupport.h"

namespace {

using namespace sigil::compose;
using sigil::draw::Frame;
using sigil::draw::Pen;

void DrawNode(benchmark::State& state) {
  bench::Host host(800, 600);
  host.composer.render(stack().child(pen([](Pen& pen) {
                                       pen.noStroke();
                                       pen.fill(220, 120, 80);
                                       for (int i = 0; i < 300; ++i)
                                         pen.circle(pen.random(800),
                                                    pen.random(600), 10);
                                     })
                                         .width(800)
                                         .height(600)));
  for (int i = 0; i < 4; ++i) host.draw();
  for (auto&& _ : state) host.draw();
}
BENCHMARK(DrawNode)->Unit(benchmark::kMicrosecond);

void RetainedCard(benchmark::State& state) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
  Pen pen;
  int count = 0;
  const Element card =
      box()
          .padding(16)
          .fill(Fill::color({0.2f, 0.3f, 0.5f, 1}))
          .child(text(u8"A retained card, reconciled every frame",
                      sigil::weave::TextStyle{}));
  for (auto&& _ : state) {
    Frame frame;
    frame.width = 800;
    frame.height = 600;
    frame.seconds = ++count / 60.0;
    frame.deltaSeconds = 1.0 / 60.0;
    frame.frameCount = count;
    frame.fonts = &bench::fonts();
    pen.begin(*surface->getCanvas(), frame);
    pen.background(20);
    pen.element(card, SkRect::MakeXYWH(40, 40, 320, 120));
    pen.end();
  }
}
BENCHMARK(RetainedCard)->Unit(benchmark::kMicrosecond);

}  // namespace
