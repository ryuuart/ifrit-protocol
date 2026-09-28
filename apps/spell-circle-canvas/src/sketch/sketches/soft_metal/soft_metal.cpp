/** A material typography poster. A folded normal field and a revolving
 *  studio reflection dress the glyphs themselves, including their edges. */
// TAGS: Studies/Graphic Design, Typography/Posters, Materials/Metal,
// Motion/Bindings

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <cmath>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
const auto cream = material::hexColor(0xE5F0C7);
const auto ground = material::hexColor(0x47756A);

Text type(std::string words, float size, bool heavy = false) {
  return text(std::move(words))
      .font({.face = weave::ports::face({"Helvetica Neue", "Arial"},
                                        heavy ? 700 : 400),
             .size = size,
             .track = heavy ? -size * 0.055f : 0.0f})
      .ink(cream);
}
}  // namespace

struct SoftMetal {
  motion::Animatable<float> reflection = motion::animatable(0.0f);
  motion::Animatable<float> direction = motion::animatable(120.0f);
  std::shared_ptr<TextureScene> lightScene;
  Element lightField;
  Element groundLayer;
  material::Material reflected = material::Color{0, 0, 0, 0};

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1080, 1400);
    ctx.background(ground);
    ctx.captureAt(3.0);
    const auto normalScene = ctx.textureScene({192, 84});
    normalScene->render(box().width(192).height(84).fill(
        material::shader(ctx.assets.hub(), ctx.local("folds.sksl"))));
    const auto normal = material::image(normalScene->texture().source());
    const auto roomScene = ctx.textureScene({512, 256});
    roomScene->render(box().width(512).height(256).fill(
        material::shader(ctx.assets.hub(), ctx.local("studio.sksl"))));
    const auto room = material::image(roomScene->texture().source());
    const material::Lighting light{
        material::studio({.direction = direction,
                          .elevation = 55,
                          .intensity = 0.55f,
                          .ambient = 0.36f}),
        material::environment(
            room,
            {.rotation = reflection, .intensity = 1.15f, .size = {512, 256}})};
    const auto silver =
        material::from(material::hexColor(0xD8E0DA))
            .surface({.metallic = 1.0f, .roughness = 0.28f, .normal = normal});
    // The reflection varies smoothly across a letter. Shade that field
    // at its own frequency, then apply full-resolution glyph coverage.
    lightScene = ctx.textureScene({192, 84});
    lightField = box().width(192).height(84).fill(silver).lighting(light);
    lightScene->render(lightField);
    reflected = material::shader(ctx.assets.hub(), ctx.local("reflection.sksl"),
                                 {.textures = {{"uLight", {}}}});
    std::vector<Element> poster{
        kit::at(0, 0, 1080, 1400)
            .fill(material::from(ground).layer(
                material::noise(0.8f, {.seed = 8, .grain = true}),
                {.blend = material::BlendMode::Multiply, .opacity = 0.055f}))
            .cache(Cache::Texture),
        kit::at(type("MATTER / LETTER", 23, true), 53, 48, 700, 40),
        kit::at(type("VOLUME 03\nREFLECTIVE STUDIES", 14), 804, 49, 225, 44),
        kit::at(54, 122, 972, 1).fill(cream),
        kit::at(type("HARD MATERIAL.\nFLUID LETTERFORM.", 58, true), 54, 959,
                700, 154),
        kit::at(type("LIGHT BECOMES\nTHE PRINTING INK.", 16), 803, 1000, 228,
                66),
        kit::at(54, 1187, 972, 1).fill(cream),
        kit::at(type("A SPECIMEN IN CHROME\nFOLDED / POLISHED / REFLECTED", 17),
                55, 1221, 552, 64),
        kit::at(type("SM–03", 80, true), 744, 1211, 295, 103),
        kit::at(type("SURFACE & COUNTERFORM", 12), 55, 1346, 700, 24),
        kit::at(type("EDITION 001", 12), 932, 1346, 108, 24)};
    for (int stripe = 0; stripe < 32; ++stripe) {
      poster.push_back(
          kit::at(57 + stripe * 30.9f, 869, stripe % 4 == 0 ? 4 : 1, 31)
              .fill(cream));
    }
    groundLayer = positioned()
                      .width(1080)
                      .height(1400)
                      .children(std::move(poster))
                      .cache(Cache::Texture);
    ctx.composer.render(describe());
  }

  static Element headline(const char* word, float size,
                          const material::Material& ink, float left, float top,
                          float width, float height) {
    return kit::at(
        type(word, size, true).ink(ink).decorationOutline(Boundary::Glyphs),
        left, top, width, height);
  }

  Element describe() {
    auto ink = reflected;
    ink.slot("uLight", material::image(lightScene->texture().source(),
                                       {.repeat = material::Repeat::Pad}));
    ink.effects(
        material::Filter::bevel({.depth = 2,
                                 .size = 3,
                                 .angleDegrees = 115,
                                 .highlight = {1, 1, 0.92f, 0.8f},
                                 .shadow = {0.02f, 0.05f, 0.03f, 0.8f}}));
    return positioned().width(1080).height(1400).children(
        {groundLayer, headline("SOFT", 370, ink, 31, 145, 1020, 440),
         headline("METAL", 300, ink, 35, 499, 1014, 365)});
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    reflection = float(elapsed) * 18.0f;
    direction = 115.0f + 35.0f * std::sin(float(elapsed) * 0.4f);
    lightScene->render(lightField, elapsed);
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(SoftMetal, "Study · Type",
             "Reflective type poster: folded chrome, bevelled glyphs and a "
             "revolving studio light")
