/** A material facsimile of an illuminated leaf. The digitized hand and
 *  ornament remain registered under a gold-leaf material on the gilding. */
// TAGS: Studies/Manuscripts, Typography/Calligraphy, Materials/Metal,
// Media/Images

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilio/advanced/Decoding.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Resource.h>
#include <sigilmedia/core/Image.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <string>

namespace material = sigil::material;
namespace media = sigil::media;
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
  std::shared_ptr<TextureScene> leafScene;
  Element leafField;
  Element pageFrame;
  material::Material coverage = material::Color{0, 0, 0, 0};

  void setup(sketch::Scene2DContext& ctx) {
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
    // The full-resolution image and coverage preserve the script and
    // worn foil. Only the smooth gold field needs a smaller surface.
    leafScene = ctx.textureScene({114, 161});
    leafField = box().width(114).height(161).fill(response);
    leafScene->render(leafField);
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
         kit::at(image(leafScene->texture().source(), material::Fit::Stretch)
                     .mask(by::alpha(coverage)),
                 45, 34, 910, 1288)});
  }

  void update(double elapsed, sketch::Scene2DContext& ctx) {
    leafScene->render(leafField, elapsed);
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH_AS(Manuscript, "manuscript", "Study · Type",
                "The Black Hours, Morgan MS M.493 fol. 19v: a faithful "
                "facsimile with a gold-leaf material on the gilding")
