/** @file
 * The maps of a surface, one texture scene each over the same tree.
 */

#include "sigilcompose/texture/SurfaceScene.h"

#include <sigilcompose/core/Composer.h>
#include <sigilmaterial/skia/SurfaceMaps.h>

#include <array>
#include <utility>
#include <vector>

namespace sigil::compose {

namespace {

using material::texture::Role;

/** The maps a page is painted as. The three scalars a lit surface reads
 *  share one image, occlusion, roughness and metallic in glTF's order, and
 *  the coat's two share another, so a page costs five paints rather than
 *  eight. */
constexpr std::array kRoles{Role::BaseColor, Role::Normal, Role::Packed,
                            Role::Emissive, Role::Clearcoat};

/** THE MAPS AN OPTIONAL LOBE NEEDS, painted only once a material on the
 *  page states the lobe: the sheen's colour and its roughness beside the
 *  specular strength, the specular colour with that same shared image, the
 *  brushing's direction and strength, the film's weight and thickness. */
std::vector<Role> lobeRoles(const material::Lobes& lobes) {
  std::vector<Role> out;
  if (lobes.sheen) out.push_back(Role::Sheen);
  if (lobes.sheen || lobes.specular) out.push_back(Role::SheenSpecular);
  if (lobes.specular) out.push_back(Role::SpecularColor);
  if (lobes.anisotropy) out.push_back(Role::Anisotropy);
  if (lobes.iridescence) out.push_back(Role::Iridescence);
  return out;
}

}  // namespace

SurfaceScene::SurfaceScene() = default;
SurfaceScene::~SurfaceScene() = default;

std::shared_ptr<SurfaceScene> SurfaceScene::make(SkISize size,
                                                 weave::FontContext& fonts) {
  std::shared_ptr<SurfaceScene> surface(new SurfaceScene());
  surface->m_size = size;
  surface->m_fonts = &fonts;
  for (Role role : kRoles)
    if (!surface->add(role)) return nullptr;
  return surface;
}

bool SurfaceScene::add(Role role) {
  if (m_scenes.count(role)) return true;
  std::shared_ptr<TextureScene> scene = TextureScene::make(m_size, *m_fonts);
  if (!scene) return false;
  scene->setSurfaceMap(role);
  // A map added after the scene adopted a device stands there too.
  if (m_device && !scene->useDevice(*m_device, *m_context)) return false;
  m_scenes.emplace(role, std::move(scene));
  return true;
}

bool SurfaceScene::useDevice(core::hardware::GpuDevice& device,
                             skia::GraphiteContext& context) {
  // Every map stands on the device or none does: a set split across two
  // places would be sampled from both.
  for (auto& [role, scene] : m_scenes)
    if (!scene->useDevice(device, context)) return false;
  m_device = &device;
  m_context = &context;
  return true;
}

void SurfaceScene::render(const Element& root, double seconds) {
  bool painted = false;
  // The materials painted as maps say which optional lobes they state; a
  // lobe heard for the first time gets its maps, painted in this same
  // render, and keeps them for as long as the scene lives.
  material::skia::LobeListener listener;
  const auto paint = [&](TextureScene& scene) {
    const uint64_t before = scene.revision();
    scene.render(root, seconds);
    painted |= scene.revision() != before;
  };
  for (auto& [role, scene] : m_scenes) paint(*scene);
  const material::Lobes heard = listener.heard();
  m_lobes.sheen |= heard.sheen;
  m_lobes.anisotropy |= heard.anisotropy;
  m_lobes.iridescence |= heard.iridescence;
  m_lobes.specular |= heard.specular;
  for (Role role : lobeRoles(m_lobes)) {
    if (m_scenes.count(role)) continue;
    if (!add(role)) continue;
    paint(*m_scenes.at(role));
    painted = true;
  }
  if (painted) ++m_revision;
}

material::Lobes SurfaceScene::lobes() const { return m_lobes; }

material::texture::TextureMaps SurfaceScene::maps() const {
  material::texture::TextureMaps maps;
  maps.name = "compose surface";
  for (const auto& [role, scene] : m_scenes)
    maps.maps.emplace(role, scene->texture());
  maps.normalDirectX = false;
  return maps;
}

material::Texture SurfaceScene::texture(Role role) const {
  const auto found = m_scenes.find(role);
  return found == m_scenes.end() ? material::Texture{}
                                 : found->second->texture();
}

uint64_t SurfaceScene::revision() const { return m_revision; }

SkISize SurfaceScene::size() const {
  return m_scenes.empty() ? SkISize::MakeEmpty()
                          : m_scenes.begin()->second->size();
}

bool SurfaceScene::isRunning() const {
  for (const auto& [role, scene] : m_scenes)
    if (scene->isRunning()) return true;
  return false;
}

const TextureScene* SurfaceScene::scene(Role role) const {
  const auto found = m_scenes.find(role);
  return found == m_scenes.end() ? nullptr : found->second.get();
}

}  // namespace sigil::compose
