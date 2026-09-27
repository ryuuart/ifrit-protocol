/** @file
 * What a kit component costs a frame that describes it again: describe,
 * render and draw on a raster surface.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Factories.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <string>

namespace {

namespace kit = sigil::sketch::kit;
namespace compose = sigil::compose;
namespace measure = sigil::measure;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

/** A run of @p rows checks, a heading every eight and a failure every
 *  five — the proof table a study draws on its plate. */
measure::CheckTable proof(int rows) {
  measure::CheckTable table;
  for (int row = 0; row < rows; ++row) {
    if (row % 8 == 0) table.add(measure::heading("GROUP " + std::to_string(row)));
    table.add(measure::check("check " + std::to_string(row), 1.0,
                             row % 5 == 0 ? 1.5 : 1.0, 0.01));
  }
  return table;
}

void describeRenderDraw(benchmark::State& state, bool every) {
  sigil::motion::Engine engine;
  compose::Composer composer{engine, fonts()};
  composer.setSize({480.0f, 900.0f});
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(480, 900));
  const measure::CheckTable table = proof((int)state.range(0));
  const kit::Theme& look = kit::houseTheme();
  for (auto _ : state) {
    composer.render(compose::box()
                        .applyStyleSheet(look.styleSheet())
                        .children({kit::verdict(
                            table, {.rows = every ? kit::VerdictRows::Every
                                                  : kit::VerdictRows::Failures,
                                    .summary = true})}));
    composer.draw(*surface->getCanvas());
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}

void BM_KitVerdictEveryRow(benchmark::State& state) {
  describeRenderDraw(state, true);
}
BENCHMARK(BM_KitVerdictEveryRow)->Arg(12)->Arg(48);

void BM_KitVerdictFailuresOnly(benchmark::State& state) {
  describeRenderDraw(state, false);
}
BENCHMARK(BM_KitVerdictFailuresOnly)->Arg(48);

}  // namespace
