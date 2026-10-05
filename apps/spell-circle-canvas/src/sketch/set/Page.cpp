/** @file
 * A page the set lights: the texture pass that paints a compose tree as
 * the maps of a surface, and the surface those maps make.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/set/Page.h>
#include <sigilworld/advanced/TextureProducer.h>

#include <chrono>
#include <optional>
#include <string>
#include <utility>

namespace sigil::sketch {

namespace {

/** THE MATERIAL A BODY WEARS TO SHOW @p scene: its maps, each marked as
 *  painted by the pass called @p name, dressing a white surface. */
material::Material surfaceOf(const compose::SurfaceScene& scene,
                             const std::string& name) {
  material::texture::TextureMaps maps = scene.maps();
  for (auto& [role, texture] : maps.maps)
    texture = world::produced(texture, name);
  return material::surface::program(maps, {.baseColor = {1, 1, 1, 1}});
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
    for (const auto& [role, map] : scene->maps().maps) {
      const auto* own = map.source().as<compose::SceneSource>();
      if (own && own->scene() == painted->scene()) return true;
    }
    return false;
  }

  /** A page learns which optional lobes its content states by painting
   *  it, which happens after the frame was described: a body wearing the
   *  page's surface from before the page learned a lobe is drawn wearing
   *  the surface with that lobe's maps. */
  std::optional<material::Material> restated(
      const material::Material& worn) const override {
    if (!scene || !material::surface::isSurface(worn)) return std::nullopt;
    if (material::surface::lobes(worn) == scene->lobes()) return std::nullopt;
    return surfaceOf(*scene, name);
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
  return surfaceOf(*m_scene, m_name);
}

const std::string& Page::name() const {
  static const std::string none;
  return m_scene ? m_name : none;
}

}  // namespace sigil::sketch
