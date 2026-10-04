/** @file
 * surface_components — properties and children make a reusable VFX card.
 *
 * One component accepts a solid, a live fill, or a material. The same
 * gel style follows two resolved heights under texture caching. Five
 * cards occupy a three-column grid without widening the last row.
 *
 * EDIT THESE FIRST
 *   Card — the component's properties; content arrives as an Element.
 *   columns — the number of equal tracks in the panel grid.
 */

// TAGS: Materials/Compositing

#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <utility>

#include "y2k_chrome/Aqua.h"

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace material = sigil::material;
using namespace sigil::compose;

namespace {

struct Card {
  std::u8string title;
  sigil::motion::Animatable<Fill> ground;
};

Element card(const Card& properties, Element content) {
  return kit::well(
             {.height = 204},
             box().column().padding(18).gap(12).borderRadius({12}).children(
                 {document::label(properties.title),
                  std::move(content).flexGrow(1)}))
      .fill(properties.ground);
}

Element gel(float height) {
  return kit::centred(box()
                          .key("gel")
                          .width(112)
                          .height(height)
                          .borderRadius({height / 2})
                          .fill(y2k::aquaGel({0.10f, 0.64f, 0.96f, 1}, height))
                          .foreground(y2k::aquaLens())
                          .cache(Cache::Texture));
}

struct SurfaceComponents {
  sigil::motion::Animatable<Fill> ink =
      sigil::motion::animatable<Fill>(Fill::color({0.10f, 0.30f, 0.40f, 1}));
  int sizeStep = -1;

  Element describe(float height) {
    const sketch::kit::Provide presentation(sketch::kit::featureTheme());
    const Fill slate = Fill::color({0.10f, 0.13f, 0.18f, 1});
    const Fill ramp = sigil::material::linearGradient(
        {0, 0}, {1, 1},
        {{0, {0.28f, 0.10f, 0.38f, 1}}, {1, {0.07f, 0.28f, 0.35f, 1}}});
    return sketch::kit::page(
        {
            .title = "Components for VFX",
            .subtitle = "Props + children",
            .footer = "One card · three paints · responsive styles · "
                      "a shared grid",
        },
        sketch::kit::panelGrid(
            {.cells = {card({u8"Solid", slate},
                            document::caption("A plain surface prop.")),
                       card({u8"Material", ramp},
                            document::caption(
                                "The same prop accepts a recipe.")),
                       card({u8"Live fill", ink},
                            document::caption("The binding updates in place.")),
                       card({u8"Gel · fixed size", slate}, gel(36)),
                       card({u8"Gel · resizing", slate}, gel(height))},
             .columns = 3,
             .gap = 18,
             .rowGap = 18}));
  }

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::featureTheme());
    sketch::kit::stage(ctx, {.size = {1020, 620}, .captureAt = 2.5});
    ctx.composer.render(describe(80));
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const float wave = 0.5f + 0.5f * std::sin((float)elapsed);
    ink = Fill::color({0.08f, 0.18f + wave * 0.18f, 0.28f + wave * 0.20f, 1});
    const int step = (int)elapsed % 4;
    if (step != sizeStep) {
      sizeStep = step;
      ctx.composer.render(describe(step < 2 ? 36.0f : 80.0f));
    }
  }
};

}  // namespace

SIGIL_SKETCH(SurfaceComponents, "Kit · API",
             "reusable cards, live surface properties and resizing gel styles")
