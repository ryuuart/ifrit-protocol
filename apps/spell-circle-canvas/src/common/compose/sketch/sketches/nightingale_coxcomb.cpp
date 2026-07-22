#include <sigilsketch/Sketch.h>
#include <sigilweave/FontContext.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>
#include <cstdio>
#include <cstdlib>
using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;

struct Min : sigil::compose::sketch::Sketch {
  Pattern grainP;
  Material m1, m2;
  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(1900, 1032);
    ctx.background({0.94f, 0.88f, 0.87f, 1});
    const int st = std::getenv("NC_ST") ? atoi(std::getenv("NC_ST")) : 0;
    fprintf(stderr, "[min] st=%d\n", st);
    if (st >= 1) {
      grainP = patterns::speckle(64, 470, 0.42f, 1.25f, {{0.3f,0.5f,0.55f,1}});
      grainP.seed(11);
      m1 = grainP.material();
      fprintf(stderr, "[min] speckle ok\n");
    }
    if (st >= 2) {
      m2 = patterns::grain(0.011f, 4, 5.0f);
      fprintf(stderr, "[min] grain ok\n");
    }
    if (st >= 3) {
      m1 = Material::blend({{Material::solid({0.8f,0.85f,0.85f,1}), SkBlendMode::kSrc},
                            {grainP.material(), SkBlendMode::kSrcOver},
                            {patterns::grain(0.01f, 3, 7.0f), SkBlendMode::kSoftLight}});
      fprintf(stderr, "[min] blend ok\n");
    }
    auto root = stack().fill(Fill::color({0.94f, 0.88f, 0.87f, 1}));
    if (st == 4)  // sector outline + plain colour
      root.child(box().absolute().left(200.0f).top(200.0f).width(600.0f).height(400.0f)
                     .outline(shapes::sector(-90.0f, 30.0f)).fill(Fill::color({0.4f,0.5f,0.6f,1})));
    if (st == 5)  // plain box + blended material (no sector)
      root.child(box().absolute().left(200.0f).top(200.0f).width(600.0f).height(400.0f).fill(m1));
    if (st == 6)  // plain box + speckle material only
      root.child(box().absolute().left(200.0f).top(200.0f).width(600.0f).height(400.0f)
                     .fill(grainP.material()));
    if (st == 7)  // plain box + grain material only
      root.child(box().absolute().left(200.0f).top(200.0f).width(600.0f).height(400.0f)
                     .fill(patterns::grain(0.01f, 3, 7.0f)));
    if (st == 8)  // sector + speckle
      root.child(box().absolute().left(200.0f).top(200.0f).width(600.0f).height(400.0f)
                     .outline(shapes::sector(-90.0f, 30.0f)).fill(grainP.material()));
    ctx.composer.render(root);
    fprintf(stderr, "[min] render ok\n");
  }
};
SIGIL_SKETCH(Min)
