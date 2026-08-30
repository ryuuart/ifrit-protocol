/** @file
 * The world's slot rules over a decoded texture set: the packed channels,
 * the normal convention, the scalars a map multiplies, and the doors from
 * files, usage words and imported parts.
 */

#include "sigilworld/TextureSet.h"

#include <utility>

namespace sigil::world::textures {

namespace {

/** The slot rules, shared by both material() overloads: @p image
 *  yields the decoded map for a role or null. */
Material fill(Material m, const std::function<sk_sp<SkImage>(Role)>& image,
              bool normalDirectX) {
  m.texture = image(Role::BaseColor);
  m.normalMap = image(Role::Normal);
  m.normalMapDirectX = normalDirectX;
  m.emissiveMap = image(Role::Emissive);
  m.roughnessMap = image(Role::Roughness);
  m.metallicMap = image(Role::Metallic);
  m.occlusionMap = image(Role::Occlusion);
  m.roughnessChannel = m.metallicChannel = m.occlusionChannel = 0;
  if (!m.roughnessMap || !m.metallicMap || !m.occlusionMap) {
    if (sk_sp<SkImage> packed = image(Role::Packed)) {
      if (!m.occlusionMap) {
        m.occlusionMap = packed;
        m.occlusionChannel = 0;
      }
      if (!m.roughnessMap) {
        m.roughnessMap = packed;
        m.roughnessChannel = 1;
      }
      if (!m.metallicMap) {
        m.metallicMap = packed;
        m.metallicChannel = 2;
      }
    }
  }
  // A map's values must be able to reach the shader: the scalar it
  // multiplies starts at 1 when the set carries that map — unless the
  // caller's base already says otherwise (a glTF factor, say), in
  // which case the factor multiplies the map as authored.
  const Material stock;
  if (m.metallicMap && m.metallic == stock.metallic) m.metallic = 1;
  if (m.roughnessMap && m.roughness == stock.roughness) m.roughness = 1;
  if (m.emissiveMap) {
    if (m.emissiveStrength <= 0) m.emissiveStrength = 1;
    if (m.emissive == glm::vec4{0, 0, 0, 1}) m.emissive = {1, 1, 1, 1};
  }
  m.tile = true;
  return m;
}

}  // namespace

namespace {

Material fromMaps(Material base, const material::textures::TextureMaps& maps) {
  return fill(
      std::move(base),
      [&](Role role) -> sk_sp<SkImage> {
        const material::Texture* t = maps.map(role);
        return t ? t->image() : nullptr;
      },
      maps.normalDirectX);
}

}  // namespace

Material material(const TextureSet& set, const Decoder& decode, Material base) {
  return fromMaps(std::move(base), material::textures::fromFiles(set, decode));
}

Material material(const std::map<std::string, sk_sp<SkImage>>& byUsage,
                  Material base, bool normalDirectX) {
  return fromMaps(std::move(base),
                  material::textures::fromUsageMap(byUsage, normalDirectX));
}

Material material(const geometry::decode::Part& part,
                  const BytesDecoder& decode, Material base) {
  Material m = std::move(base);
  m.baseColor = part.baseColor;
  m.metallic = part.metallic;
  m.roughness = part.roughness;
  m.emissive = part.emissive;
  m.transmission = part.transmission;
  m.ior = part.ior;
  m.alphaCutoff = part.alphaCutoff;
  if (part.emissive.r + part.emissive.g + part.emissive.b > 0 &&
      m.emissiveStrength <= 0)
    m.emissiveStrength = 1;
  std::map<std::string, sk_sp<SkImage>> byUsage;
  if (!part.textureBytes.empty() && decode)
    if (sk_sp<SkImage> base = decode(part.textureBytes, part.textureUri))
      byUsage["baseColor"] = base;
  for (const auto& [usage, ref] : part.textures)
    if (!ref.bytes.empty() && decode)
      if (sk_sp<SkImage> image = decode(ref.bytes, ref.uri))
        byUsage[usage] = image;
  // A file may reference the same bytes for "orm" and "occlusion":
  // decode once, share the image.
  auto orm = part.textures.find("orm");
  auto occ = part.textures.find("occlusion");
  if (orm != part.textures.end() && occ != part.textures.end() &&
      orm->second.bytes == occ->second.bytes && byUsage.count("orm"))
    byUsage["occlusion"] = byUsage["orm"];
  Material out = material(byUsage, std::move(m), /*normalDirectX=*/false);
  return out;
}

std::vector<Material> materials(const geometry::decode::Model& model,
                                const BytesDecoder& decode, Material base) {
  std::vector<Material> slots((size_t)model.materialSlotCount(), base);
  std::vector<bool> filled(slots.size(), false);
  for (const geometry::decode::Part& part : model.parts) {
    if (part.materialIndex < 0 || (size_t)part.materialIndex >= slots.size() ||
        filled[(size_t)part.materialIndex])
      continue;
    slots[(size_t)part.materialIndex] = material(part, decode, base);
    filled[(size_t)part.materialIndex] = true;
  }
  return slots;
}

}  // namespace sigil::world::textures
