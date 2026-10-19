/** A panel wearing a stack of the library's own filters and nothing of
 *  its own: every program its draws ask for is one the host can write
 *  down and stand up again before the panel next opens. */
#include <sigilcompose/core/Core.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilsketch/scene2d/Scene2D.h>

namespace {

constexpr float kWidth = 640;
constexpr float kHeight = 420;

struct StackProbe {
  void setup(sigil::sketch::Scene2DContext& ctx) {
    using namespace sigil::compose;
    using sigil::material::Color;
    using sigil::material::Filter;
    ctx.canvas(kWidth, kHeight);
    ctx.composer.render(
        stack()
            .width(kWidth)
            .height(kHeight)
            .fill(Fill::color(Color{0.03f, 0.04f, 0.06f, 1}))
            .alignItems(Align::Center)
            .justifyContent(Justify::Center)
            .children({box()
                           .width(320)
                           .height(160)
                           .fill(Fill::color(Color{0.9f, 0.6f, 0.2f, 1}))
                           .filter(Filter::glow(Color{0.2f, 0.7f, 0.9f, 1}, 12)
                                       .then(Filter::saturate(1.4f))
                                       .then(Filter::brightness(0.1f)))}));
  }
};

}  // namespace

SIGIL_SKETCH(StackProbe, "Test",
             "a panel wearing a stack of the library's own filters")
