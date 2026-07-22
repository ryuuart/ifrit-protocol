#pragma once
// Split from GalleryScenes.h — see that header for the registry.

#include "GalleryCore.h"

#include <random>

namespace compose_gallery {

// ---- 13: sustained load — cached cards + bound movers (#21) ---------------

struct LoadScene final : Scene {
  std::vector<std::unique_ptr<choreograph::Output<float>>> movers;

  const char *name() const override { return "load"; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    movers.clear();
    auto root = stack().fill(Fill::color({0.04f, 0.04f, 0.08f, 1}));
    std::mt19937 rng{3};
    // 300 static cached cards.
    for (int i = 0; i < 300; ++i) {
      const float x = (float)(rng() % 860), y = (float)(rng() % 600);
      root.child(box().width(34).height(22).corners({4})
                     .inset(x, y, 0, 0).absolute()
                     .fill(Fill::color({0.09f, 0.10f, 0.16f, 1})));
    }
    // 24 binding-driven movers over them (only these paint live).
    for (int i = 0; i < 24; ++i) {
      auto out = std::make_unique<choreograph::Output<float>>(0.0f);
      const float y = 20.0f + 25.0f * (float)i;
      const float phase = (float)i * 0.7f;
      root.child(box().width(46).height(18).corners({4})
                     .inset(0, y, 0, 0).absolute()
                     .translateX(out.get())
                     .fill(Fill::color({0.49f, 0.91f, 1.0f, 0.8f})));
      movers.push_back(std::move(out));
      ticker.add([o = movers.back().get(), phase, t = 0.0](double dt) mutable {
        t += dt;
        *o = 430.0f + 410.0f * (float)std::sin(t * 0.9 + phase);
        return true;
      });
    }
    composer.render(std::move(root));
  }
};


} // namespace compose_gallery
