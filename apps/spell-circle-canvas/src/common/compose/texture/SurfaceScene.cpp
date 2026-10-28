/** @file
 * The maps of a surface, one texture scene each over the same tree.
 */

#include "sigilcompose/texture/SurfaceScene.h"

#include <sigilcompose/core/Composer.h>
#include <sigilmaterial/skia/SurfaceMaps.h>

#include <algorithm>
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
 *  brushing's direction and strength, the film's weight and thickness, the
 *  transmission with the thickness, index, dispersion and attenuation of its
 *  volume, the distortion — and the
 *  coat's own normal, once a material states one. */
std::vector<Role> lobeRoles(const material::Lobes& lobes,
                            bool clearcoatNormal) {
  std::vector<Role> out;
  if (clearcoatNormal) out.push_back(Role::ClearcoatNormal);
  if (lobes.sheen) out.push_back(Role::Sheen);
  if (lobes.sheen || lobes.specular) out.push_back(Role::SheenSpecular);
  if (lobes.specular) out.push_back(Role::SpecularColor);
  if (lobes.anisotropy) out.push_back(Role::Anisotropy);
  if (lobes.iridescence) out.push_back(Role::Iridescence);
  if (lobes.transmission) {
    out.push_back(Role::Transmission);
    out.push_back(Role::Thickness);
    out.push_back(Role::Volume);
    out.push_back(Role::Attenuation);
  }
  if (lobes.distortion) out.push_back(Role::Distortion);
  return out;
}

}  // namespace

SurfaceScene::SurfaceScene() = default;
SurfaceScene::~SurfaceScene() = default;

std::shared_ptr<SurfaceScene> SurfaceScene::make(SkISize size,
                                                 weave::FontContext& fonts,
                                                 SurfaceRange range) {
  std::shared_ptr<SurfaceScene> surface(new SurfaceScene());
  surface->m_size = size;
  surface->m_fonts = &fonts;
  surface->m_range = range;
  // A stated reading stands from the first paint.
  if (range.emission > 0)
    surface->m_emissionScale = std::max(range.emission, 1.0f);
  if (range.thickness > 0) surface->m_thicknessScale = range.thickness;
  if (range.distortion > 0) surface->m_distortionScale = range.distortion;
  for (Role role : kRoles)
    if (!surface->add(role)) return nullptr;
  return surface;
}

bool SurfaceScene::add(Role role) {
  if (m_scenes.count(role)) return true;
  std::shared_ptr<TextureScene> scene = TextureScene::make(m_size, *m_fonts);
  if (!scene) return false;
  scene->setSurfaceMap(role);
  scene->setSurfaceMapScale(scaleOf(role));
  // A map added after the scene adopted a device stands there too.
  if (m_device && !scene->useDevice(*m_device, *m_context)) return false;
  m_scenes.emplace(role, std::move(scene));
  return true;
}

bool SurfaceScene::useDevice(gpu::GpuDevice& device,
                             skia::GraphiteContext& context) {
  // Every map stands on the device or none does: a set split across two
  // places would be sampled from both. The cuts stand where the maps do.
  for (auto& [role, scene] : m_scenes)
    if (!scene->useDevice(device, context)) return false;
  for (auto& [region, cuts] : m_cuts)
    for (auto& [role, cut] : cuts)
      if (!cut->useDevice(device, context)) return false;
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
  m_lobes.transmission |= heard.transmission;
  m_lobes.distortion |= heard.distortion;
  m_clearcoatNormal |= listener.heardClearcoatNormal();
  // WHAT A MAP'S WHITE STANDS FOR rises with what the page states: the
  // brightest emission, the deepest volume, the strongest distortion. A map
  // whose reading rose is painted again against it in this same render.
  // A reading the author stated stands whatever the content says.
  const material::skia::MapReach& reach = listener.heardReach();
  if (m_range.emission <= 0)
    m_emissionScale = std::max({m_emissionScale, reach.emission, 1.0f});
  if (m_range.thickness <= 0)
    m_thicknessScale = std::max(m_thicknessScale, reach.thickness);
  if (m_range.distortion <= 0)
    m_distortionScale = std::max(m_distortionScale, reach.distortion);
  for (auto& [role, scene] : m_scenes) {
    const float scale = scaleOf(role);
    if (scene->composer().surfaceMapScale() == scale) continue;
    scene->setSurfaceMapScale(scale);
    paint(*scene);
  }
  for (Role role : lobeRoles(m_lobes, m_clearcoatNormal)) {
    if (m_scenes.count(role)) continue;
    if (!add(role)) continue;
    paint(*m_scenes.at(role));
    painted = true;
  }
  // Each cut copies its map once the map has painted since.
  for (auto& [region, cuts] : m_cuts)
    for (auto& [role, cut] : cuts) cut->render(root, seconds);
  if (painted) ++m_revision;
}

material::Lobes SurfaceScene::lobes() const { return m_lobes; }

float SurfaceScene::scaleOf(Role role) const {
  switch (role) {
    case Role::Emissive:
      return m_emissionScale;
    case Role::Thickness:
      return m_thicknessScale > 0 ? m_thicknessScale : 1;
    case Role::Distortion:
      return m_distortionScale > 0 ? m_distortionScale : 1;
    default:
      return 1;
  }
}

material::texture::TextureMaps SurfaceScene::readings() const {
  material::texture::TextureMaps maps;
  maps.name = "compose surface";
  maps.normalDirectX = false;
  maps.emissiveStrength = m_emissionScale;
  maps.thickness = m_thicknessScale;
  maps.distortionStrength = m_distortionScale;
  return maps;
}

material::texture::TextureMaps SurfaceScene::maps() const {
  material::texture::TextureMaps maps = readings();
  for (const auto& [role, scene] : m_scenes)
    maps.maps.emplace(role, scene->texture());
  return maps;
}

material::texture::TextureMaps SurfaceScene::maps(material::PixelRect region) {
  material::texture::TextureMaps maps = readings();
  auto& cuts = m_cuts[{region.x, region.y, region.width, region.height}];
  for (const auto& [role, scene] : m_scenes) {
    auto found = cuts.find(role);
    if (found == cuts.end()) {
      std::shared_ptr<TextureScene> cut =
          TextureScene::cut(scene, region, *m_fonts);
      if (!cut) continue;
      // A cut asked for after the page stood on the device stands there
      // too, and copies what the page holds now.
      if (m_device && !cut->useDevice(*m_device, *m_context)) continue;
      if (scene->image()) cut->render(Element());
      found = cuts.emplace(role, std::move(cut)).first;
    }
    maps.maps.emplace(role, found->second->texture());
  }
  return maps;
}

bool SurfaceScene::paints(const TextureScene* scene) const {
  for (const auto& [role, map] : m_scenes)
    if (map.get() == scene) return true;
  for (const auto& [region, cuts] : m_cuts)
    for (const auto& [role, cut] : cuts)
      if (cut.get() == scene) return true;
  return false;
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
