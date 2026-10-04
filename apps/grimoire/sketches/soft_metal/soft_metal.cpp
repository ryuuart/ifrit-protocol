/** A machined film title: original vector letter contours, recessed edges,
 *  brushed steel and slowly moving firelight over two closing plate halves. */
// TAGS: Studies/Film, Typography/Lettering, Materials/Metal,
// Materials/Lighting, Motion/Transitions

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Lettering.h"

namespace geometry = sigil::geometry;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
using namespace sigil::compose;

struct SoftMetal {
  std::shared_ptr<TextureScene> plateScene, heatScene, fireScene;
  Element heatField, fireField;
  bool assembled = false;
  motion::Animatable<float> upper = motion::animatable(-306.0f);
  motion::Animatable<float> lower = motion::animatable(306.0f);

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1440, 612);
    ctx.background({0.014f, 0.018f, 0.023f, 1});
    ctx.captureAt(4.0);
    plateScene = ctx.textureScene({1440, 612});
    const auto steel =
        material::shader(ctx.assets.hub(), ctx.local("steel.sksl"));
    const auto cavity =
        material::from(material::hexColor(0x30323B))
            .layer(material::noise(0.22f, {.octaves = 2,
                                           .seed = 39,
                                           .grain = true,
                                           .stretch = 0.07f}),
                   {.blend = material::BlendMode::Multiply, .opacity = 0.13f})
            .effects(material::Filter::shadow(
                         {0, 0, 0, 0.82f},
                         {.blur = 1.2f, .offset = {0, 3}, .inside = true})
                         .then(material::Filter::bevel(
                             {.depth = -2.3f,
                              .size = 1.4f,
                              .angleDegrees = 100,
                              .highlight = {0.84f, 0.9f, 1, 0.72f},
                              .shadow = {0.02f, 0.025f, 0.04f, 0.9f}})));
    const auto main =
        geometry::shapes::fitted(title::inscription("TERMINATOR 2"));
    const auto sub =
        geometry::shapes::fitted(title::inscription("JUDGMENT DAY"));
    const auto edge =
        material::linearGradient({0, 0}, {0, 1},
                                 {{0.0f, material::hexColor(0xFAF6EB)},
                                  {0.48f, material::hexColor(0x66656B)},
                                  {1.0f, material::hexColor(0xC5CECE)}});
    std::vector<Element> plate{
        kit::at(0, 0, 1440, 612).fill(steel),
        kit::at(88, 194, 1262, 90)
            .shape(main)
            .fill(edge)
            .stroke(stroke(3.8f, Fill(edge))),
        kit::at(88, 194, 1262, 90).shape(main).fill(cavity),
        kit::at(213, 340, 1015, 68)
            .shape(sub)
            .fill(edge)
            .stroke(stroke(3.1f, Fill(edge))),
        kit::at(213, 340, 1015, 68).shape(sub).fill(cavity),
        kit::at(0, 306, 1440, 2.4f).fill(material::hexColor(0x0D0E15)),
        kit::at(0, 309, 1440, 1.2f).fill(material::hexColor(0x8B949E)),
        kit::at(0, 0, 1440, 5).fill(material::hexColor(0x11151D)),
        kit::at(0, 607, 1440, 5).fill(material::hexColor(0x11151D))};
    plateScene->render(
        positioned().width(1440).height(612).children(std::move(plate)));
    heatScene = ctx.textureScene({128, 54});
    heatField = box().width(128).height(54).fill(
        material::shader(ctx.assets.hub(), ctx.local("heat.sksl")));
    heatScene->render(heatField);
    fireScene = ctx.textureScene({192, 36});
    fireField = box().width(192).height(36).fill(
        material::shader(ctx.assets.hub(), ctx.local("fire.sksl")));
    fireScene->render(fireField);
    ctx.composer.render(describe());
  }

  Element describe() {
    const auto plate = plateScene->texture().source();
    const auto light = heatScene->texture().source();
    const auto half = [&](float sourceTop, float top, float height) {
      return kit::at(
          positioned()
              .overflow(Overflow::Clip)
              .children({kit::at(image(plate, material::Fit::Stretch), 0,
                                 sourceTop, 1440, 612)}),
          0, top, 1440, height);
    };
    std::vector<Element> frame;
    if (assembled) {
      frame.push_back(
          kit::at(image(plate, material::Fit::Stretch), 0, 0, 1440, 612));
    } else {
      frame.push_back(half(0, 0, 308).translateY(upper));
      frame.push_back(half(-308, 308, 304).translateY(lower));
    }
    // The illumination stays in the room while the plates move through it.
    // Native compositing preserves the engraving's full-resolution pixels.
    frame.push_back(
        kit::at(image(light, material::Fit::Stretch), 0, 0, 1440, 612)
            .blendMode(material::BlendMode::Multiply)
            .key("light"));
    // Flame coverage exists only in the lower part of the frame.
    frame.push_back(
        kit::at(image(fireScene->texture().source(), material::Fit::Stretch), 0,
                342.72f, 1440, 269.28f)
            .key("flames"));
    return positioned().width(1440).height(612).children(std::move(frame));
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const float cycle = std::fmod(float(elapsed), 12.0f);
    const float progress = std::clamp(cycle / 1.8f, 0.0f, 1.0f);
    const float eased = 1.0f - std::pow(1.0f - progress, 4.0f);
    const float retreat = std::clamp((cycle - 10.2f) / 1.8f, 0.0f, 1.0f);
    const float distance = 306 * (1.0f - eased + retreat * retreat * retreat);
    assembled = distance < 0.001f;
    upper = -distance;
    lower = distance;
    heatScene->render(heatField, elapsed);
    fireScene->render(fireField, elapsed);
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(SoftMetal, "Study · Type",
             "Terminator 2 title study: custom machined lettering, recessed "
             "bevels, brushed steel, reflected fire and closing plate halves")
