/** @file
 * The maps of a surface, one texture scene each over the same tree.
 */

#include "sigilcompose/texture/SurfaceScene.h"

#include <sigilcompose/core/Composer.h>

#include <array>
#include <utility>

namespace sigil::compose {

namespace {

using material::texture::Role;

constexpr std::array kRoles{Role::BaseColor, Role::Normal,    Role::Roughness,
                            Role::Metallic,  Role::Occlusion, Role::Emissive};

}  // namespace

SurfaceScene::SurfaceScene() = default;
SurfaceScene::~SurfaceScene() = default;

std::shared_ptr<SurfaceScene> SurfaceScene::make(SkISize size,
                                                 weave::FontContext& fonts) {
  std::shared_ptr<SurfaceScene> surface(new SurfaceScene());
  for (Role role : kRoles) {
    std::shared_ptr<TextureScene> scene = TextureScene::make(size, fonts);
    if (!scene) return nullptr;
    scene->setSurfaceMap(role);
    surface->m_scenes.emplace(role, std::move(scene));
  }
  return surface;
}

bool SurfaceScene::useDevice(core::hardware::GpuDevice& device,
                             skia::GraphiteContext& context) {
  // Every map stands on the device or none does: a set split across two
  // places would be sampled from both.
  for (auto& [role, scene] : m_scenes)
    if (!scene->useDevice(device, context)) return false;
  return true;
}

void SurfaceScene::render(const Element& root, double seconds) {
  bool painted = false;
  for (auto& [role, scene] : m_scenes) {
    const uint64_t before = scene->revision();
    scene->render(root, seconds);
    painted |= scene->revision() != before;
  }
  if (painted) ++m_revision;
}

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
