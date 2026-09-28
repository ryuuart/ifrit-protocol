/** A material facsimile of an illuminated leaf. The digitized hand and
 *  ornament remain registered while a moving light catches only gold. */
// TAGS: Studies/Manuscripts, Typography/Calligraphy, Materials/Metal,
// Media/Images

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilio/advanced/Decoding.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Resource.h>
#include <sigilmedia/core/Image.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <cmath>
#include <string>

namespace material = sigil::material;
namespace media = sigil::media;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
struct PageParameters {
  glm::vec2 sourceSize = {1427, 2020};
};

Text caption(std::string words, float size) {
  return text(std::move(words))
      .font({.face = weave::ports::face({"Helvetica Neue", "Arial"}),
             .size = size})
      .ink(material::hexColor(0xB1AD95));
}
}  // namespace

struct Manuscript {
  motion::Animatable<float> direction = motion::animatable(120.0f);
  motion::Animatable<float> elevation = motion::animatable(56.0f);
  std::shared_ptr<TextureScene> lightScene;
  Element lightField;
  Element pageFrame;
  material::Material coverage = material::Color{0, 0, 0, 0};

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1000, 1430);
    ctx.background(material::hexColor(0x151916));
    ctx.captureAt(3.0);
    const auto page = ctx.assets.hub().load<media::Image>(
        ctx.local("data/black-hours-019v.png"));
    const auto goldScene = ctx.textureScene({910, 1288});
    goldScene->render(box().width(910).height(1288).fill(
        material::shader(ctx.assets.hub(), ctx.local("gold.sksl"),
                         PageParameters{}, {.textures = {{"uPage", page}}})));
    const auto normalScene = ctx.textureScene({114, 161});
    normalScene->render(box().width(114).height(161).fill(
        material::shader(ctx.assets.hub(), ctx.local("leaf-normal.sksl"))));
    const auto response =
        material::from(material::Color{0.74f, 0.56f, 0.22f, 1})
            .surface(
                {.metallic = 1.0f,
                 .roughness = 0.43f,
                 .normal = material::image(normalScene->texture().source())});
    const auto light = material::studio({.direction = direction,
                                         .elevation = elevation,
                                         .color = {1.0f, 0.96f, 0.82f, 1.0f},
                                         .intensity = 0.8f,
                                         .ambient = 0.84f});
    // The full-resolution image and coverage preserve the script and
    // worn foil. Only the smooth illumination needs a smaller surface.
    lightScene = ctx.textureScene({114, 161});
    lightField = box().width(114).height(161).fill(response).lighting(light);
    lightScene->render(lightField);
    coverage = material::image(goldScene->texture().source());
    pageFrame = positioned().width(1000).height(1430).children(
        {kit::at(image(page), 45, 34, 910, 1288),
         kit::at(caption("THE BLACK HOURS", 20), 46, 1350, 550, 33),
         kit::at(caption("MS M.493 · FOL. 19v", 14), 742, 1355, 221, 27),
         kit::at(caption("Bruges, c. 1480 · The Morgan Library & Museum", 14),
                 46, 1386, 720, 28)});
    ctx.composer.render(describe());
  }

  Element describe() {
    return positioned().width(1000).height(1430).children(
        {pageFrame,
         kit::at(image(lightScene->texture().source(), material::Fit::Stretch)
                     .mask(by::alpha(coverage)),
                 45, 34, 910, 1288)});
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    direction = 120.0f + 70.0f * std::sin(float(elapsed) * 0.36f);
    elevation = 55.0f + 17.0f * std::sin(float(elapsed) * 0.36f + 1.0f);
    lightScene->render(lightField, elapsed);
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH_AS(Manuscript, "manuscript", "Study · Type",
                "The Black Hours, Morgan MS M.493 fol. 19v: a faithful "
                "facsimile with selectively relit gold leaf")
