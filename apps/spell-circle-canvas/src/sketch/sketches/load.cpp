/** @file
 * load — sustained load: three hundred cached cards under two dozen
 * binding-driven movers, so what paints live is exactly the movers and
 * the rest is proved to be sitting still.
 */

#include <sigilsketch/canvas/Sketch.h>

#include <random>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {

// ---- 13: sustained load — cached cards + bound movers (#21) ---------------

struct Load final : sketch::Sketch {
  std::vector<std::unique_ptr<choreograph::Output<float>>> movers;

  void setup(sketch::SketchContext& ctx) override {
    ctx.background({0, 0, 0, 1});
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    movers.clear();
    auto root = stack().fill(Fill::color({0.04f, 0.04f, 0.08f, 1}));
    // a fixed seed; the scene must render the same on every run
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    std::mt19937 rng{3};
    // 300 static cached cards.
    for (int i = 0; i < 300; ++i) {
      const float x = (float)(rng() % 860), y = (float)(rng() % 600);
      root.child(box()
                     .width(34)
                     .height(22)
                     .corners({4})
                     .inset(x, y, 0, 0)
                     .fill(Fill::color({0.09f, 0.10f, 0.16f, 1})));
    }
    // 24 binding-driven movers over them (only these paint live).
    for (int i = 0; i < 24; ++i) {
      auto out = std::make_unique<choreograph::Output<float>>(0.0f);
      const float y = 20.0f + 25.0f * (float)i;
      const float phase = (float)i * 0.7f;
      root.child(box()
                     .width(46)
                     .height(18)
                     .corners({4})
                     .inset(0, y, 0, 0)
                     .translateX(out.get())
                     .fill(Fill::color({0.49f, 0.91f, 1.0f, 0.8f})));
      movers.push_back(std::move(out));
      ticker.add([o = movers.back().get(), phase, t = 0.0](double dt) mutable {
        t += dt;
        *o = 430.0f + 410.0f * (float)std::sin(t * 0.9 + phase);
        return true;
      });
    }
    composer.render(root);
  }
};

}  // namespace

SIGIL_SKETCH_AS(Load, "load", "Catalog \xc2\xb7 Scale", "#21 sustained load")
