/** @file
 * A page the sketch lights: the texture pass that paints a compose tree as
 * the maps of a surface, and the surface those maps make.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/scene3d/Page.h>
#include <sigilworld/advanced/TextureProducer.h>

#include <chrono>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace sigil::sketch {

namespace {

/** THE MATERIAL A BODY WEARS TO SHOW @p scene, or the part of it @p region
 *  names: its maps, each marked as painted by the pass called @p name,
 *  dressing a white surface. */
material::Material surfaceOf(compose::SurfaceScene& scene,
                             const std::string& name,
                             std::optional<material::PixelRect> region) {
  material::texture::TextureMaps maps =
      region ? scene.maps(*region) : scene.maps();
  for (auto& [role, texture] : maps.maps)
    texture = world::produced(texture, name);
  return material::surface::program(maps, {.baseColor = {1, 1, 1, 1}});
}

/** The part of its page @p worn shows, read off its base colour map: none
 *  when it shows the whole page. */
std::optional<material::PixelRect> regionWorn(const material::Material& worn) {
  const material::Texture* texture =
      material::surface::map(worn, material::surface::kBaseColorSlot);
  if (!texture) return std::nullopt;
  const auto* marked = texture->source().as<world::ProducedPixels>();
  if (!marked) return std::nullopt;
  const auto* painted = marked->pixels.as<compose::SceneSource>();
  if (!painted || !painted->scene()) return std::nullopt;
  return painted->scene()->cutRect();
}

/** THE PAGE'S PAINT, as a texture pass performs it: the tree rendered
 *  into every map at the frame's time. */
struct PagePaint final : world::TextureProducerOperations {
  PagePaint(std::shared_ptr<compose::SurfaceScene> scene, compose::Element tree,
            std::string name)
      : scene(std::move(scene)), tree(std::move(tree)), name(std::move(name)) {}

  bool produce(std::chrono::duration<double> time) const override {
    if (!scene) return false;
    const uint64_t before = scene->revision();
    scene->render(tree, time.count());
    return scene->revision() != before;
  }

  bool paints(const material::Texture& texture) const override {
    const auto* marked = texture.source().as<world::ProducedPixels>();
    if (!marked) return false;
    const auto* painted = marked->pixels.as<compose::SceneSource>();
    if (!painted || !scene) return false;
    return scene->paints(painted->scene());
  }

  /** A page learns which optional lobes its content states by painting
   *  it, which happens after the frame was described: a body wearing the
   *  page's surface from before the page learned a lobe is drawn wearing
   *  the surface with that lobe's maps. The lobes compared are the ones the
   *  page's maps dress a surface with, which counts the clear coat whose
   *  map every page paints. A coat's own normal is learned the same way:
   *  its map is added once the content states one. */
  std::optional<material::Material> restated(
      const material::Material& worn) const override {
    if (!scene || !material::surface::isSurface(worn)) return std::nullopt;
    material::Material current = surfaceOf(*scene, name, regionWorn(worn));
    const auto coatNormal = [](const material::Material& surface) {
      return material::surface::map(
                 surface, material::surface::kClearcoatNormalSlot) != nullptr;
    };
    // The numbers the maps are read against are learned the same way: an
    // emission past white raises the strength the emissive map is read at,
    // a deeper volume or a stronger distortion the reading of its map, and
    // the first transmitting material sets the volume the page carries.
    const auto readAgainst = [](const material::Material& surface) {
      return std::tuple(surface.get<float>("emissiveStrength"),
                        surface.get<float>("thickness"),
                        surface.get<float>("distortionStrength"),
                        surface.get<float>("ior"),
                        surface.get<float>("attenuationDistance"),
                        surface.get<material::Color>("attenuationColor"),
                        surface.get<float>("dispersion"));
    };
    if (material::surface::lobes(worn) == material::surface::lobes(current) &&
        coatNormal(worn) == coatNormal(current) &&
        readAgainst(worn) == readAgainst(current))
      return std::nullopt;
    return current;
  }

  bool operator==(const PagePaint& other) const {
    return scene == other.scene && name == other.name &&
           compose::sameDescription(tree, other.tree);
  }

  std::shared_ptr<compose::SurfaceScene> scene;
  compose::Element tree;
  std::string name;
};

}  // namespace

Page::Page(std::shared_ptr<compose::SurfaceScene> scene, std::string name)
    : m_scene(std::move(scene)), m_name(std::move(name)) {}

world::Pass Page::render(const compose::Element& tree) const {
  return world::texturePass(
      m_name, world::TextureProducer(PagePaint(m_scene, tree, m_name)));
}

material::Material Page::surface() const {
  if (!m_scene) return material::surface::program();
  // Every map is marked as this page's, so a frame can say which pass is
  // missing when a body wears them without it.
  return surfaceOf(*m_scene, m_name, std::nullopt);
}

material::Material Page::surface(material::PixelRect region) const {
  if (!m_scene) return material::surface::program();
  return surfaceOf(*m_scene, m_name, region);
}

const std::string& Page::name() const {
  static const std::string none;
  return m_scene ? m_name : none;
}

}  // namespace sigil::sketch
