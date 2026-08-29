#include "sigilworld/TextureSet.h"

#include <algorithm>
#include <cctype>

namespace sigil::world::textures {

namespace {

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = (char)std::tolower((unsigned char)c);
  return out;
}

/** The role vocabulary, one entry per word the tools use. Longer words
 *  are tried first so `normaldx` beats `normal`. */
struct Word {
  const char* word;
  Role role;
  bool directX;
};
constexpr Word kWords[] = {
    // packed occlusion / roughness / metallic
    {"occlusionroughnessmetallic", Role::Packed, false},
    {"orm", Role::Packed, false},
    {"arm", Role::Packed, false},
    {"rma", Role::Packed, false},
    // normals — DirectX spellings before the bare word
    {"normaldirectx", Role::Normal, true},
    {"normal_directx", Role::Normal, true},
    {"normaldx", Role::Normal, true},
    {"nor_dx", Role::Normal, true},
    {"nrm_dx", Role::Normal, true},
    {"normalopengl", Role::Normal, false},
    {"normal_opengl", Role::Normal, false},
    {"normalgl", Role::Normal, false},
    {"nor_gl", Role::Normal, false},
    {"nrm_gl", Role::Normal, false},
    {"normal", Role::Normal, false},
    {"normals", Role::Normal, false},
    {"nor", Role::Normal, false},
    {"nrm", Role::Normal, false},
    {"n", Role::Normal, false},
    // base colour
    {"basecolor", Role::BaseColor, false},
    {"basecolour", Role::BaseColor, false},
    {"base_color", Role::BaseColor, false},
    {"albedo", Role::BaseColor, false},
    {"diffuse", Role::BaseColor, false},
    {"diff", Role::BaseColor, false},
    {"color", Role::BaseColor, false},
    {"colour", Role::BaseColor, false},
    {"col", Role::BaseColor, false},
    {"alb", Role::BaseColor, false},
    {"d", Role::BaseColor, false},
    // roughness / metallic
    {"roughness", Role::Roughness, false},
    {"rough", Role::Roughness, false},
    {"rgh", Role::Roughness, false},
    {"r", Role::Roughness, false},
    {"metallic", Role::Metallic, false},
    {"metalness", Role::Metallic, false},
    {"metal", Role::Metallic, false},
    {"met", Role::Metallic, false},
    {"m", Role::Metallic, false},
    // occlusion
    {"ambientocclusion", Role::Occlusion, false},
    {"ambient_occlusion", Role::Occlusion, false},
    {"occlusion", Role::Occlusion, false},
    {"ao", Role::Occlusion, false},
    // emissive
    {"emissive", Role::Emissive, false},
    {"emission", Role::Emissive, false},
    {"emit", Role::Emissive, false},
    {"e", Role::Emissive, false},
    // recognized but slotless
    {"displacement", Role::Height, false},
    {"height", Role::Height, false},
    {"disp", Role::Height, false},
    {"bump", Role::Height, false},
    {"opacity", Role::Opacity, false},
    {"alpha", Role::Opacity, false},
    {"specular", Role::Specular, false},
    {"spec", Role::Specular, false},
};

/** `1k`, `2K`, `4k`, `8k`, `512`, `1024`, `2048`, `4096`, `8192`. */
bool isSizeToken(std::string_view t) {
  if (t.size() == 2 && std::isdigit((unsigned char)t[0]) &&
      (t[1] == 'k' || t[1] == 'K'))
    return true;
  static constexpr const char* kSizes[] = {"256",  "512",  "1024",
                                           "2048", "4096", "8192"};
  for (const char* s : kSizes)
    if (t == s) return true;
  return false;
}

/** Split on '_' and '-' */
std::vector<std::string> tokens(std::string_view stem) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : stem) {
    if (c == '_' || c == '-') {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

const Word* lookup(std::string_view word) {
  const std::string w = lower(word);
  for (const Word& entry : kWords)
    if (w == entry.word) return &entry;
  return nullptr;
}

std::string join(const std::vector<std::string>& parts, size_t count) {
  std::string out;
  for (size_t i = 0; i < count; ++i) {
    if (i) out += '_';
    out += parts[i];
  }
  return out;
}

bool isImageExtension(const std::filesystem::path& p) {
  const std::string ext = lower(p.extension().string());
  return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" ||
         ext == ".tif" || ext == ".tiff" || ext == ".exr" || ext == ".hdr" ||
         ext == ".avif" || ext == ".gif" || ext == ".bmp" || ext == ".tga";
}

}  // namespace

Classified classify(std::string_view filename) {
  Classified out;
  const std::filesystem::path path{std::string(filename)};
  std::vector<std::string> parts = tokens(path.stem().string());
  // Trailing size token, if any, is not part of the role or the set.
  if (!parts.empty() && isSizeToken(parts.back())) parts.pop_back();
  if (parts.empty()) return out;
  // Two-token role words first (`nor_gl`, `normal_directx`, `base_color`),
  // then one.
  if (parts.size() >= 3) {
    const std::string two = parts[parts.size() - 2] + "_" + parts.back();
    if (const Word* w = lookup(two)) {
      out.role = w->role;
      out.directX = w->directX;
      out.set = join(parts, parts.size() - 2);
      return out;
    }
  }
  if (parts.size() >= 2) {
    if (const Word* w = lookup(parts.back())) {
      out.role = w->role;
      out.directX = w->directX;
      out.set = join(parts, parts.size() - 1);
      return out;
    }
  }
  return out;
}

Role roleForUsage(std::string_view usage) {
  const Word* w = lookup(usage);
  return w ? w->role : Role::Unknown;
}

std::vector<TextureSet> discover(const std::filesystem::path& directory) {
  std::map<std::string, TextureSet> sets;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
    if (!entry.is_regular_file() || !isImageExtension(entry.path())) continue;
    const Classified c = classify(entry.path().filename().string());
    if (c.role == Role::Unknown) continue;
    TextureSet& set = sets[c.set];
    set.name = c.set;
    // A set naming one role twice (`_diff.png` beside `_diff.jpg`)
    // keeps the lexicographically first path, so the result does not
    // depend on directory iteration order.
    auto it = set.files.find(c.role);
    if (it == set.files.end() || entry.path() < it->second)
      set.files[c.role] = entry.path();
    if (c.role == Role::Normal) set.normalDirectX = c.directX;
  }
  std::vector<TextureSet> out;
  out.reserve(sets.size());
  for (auto& [name, set] : sets) out.push_back(std::move(set));
  return out;
}

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

Material material(const TextureSet& set, const Decoder& decode, Material base) {
  return fill(
      std::move(base),
      [&](Role role) -> sk_sp<SkImage> {
        auto it = set.files.find(role);
        if (it == set.files.end() || !decode) return nullptr;
        return decode(it->second);
      },
      set.normalDirectX);
}

Material material(const std::map<std::string, sk_sp<SkImage>>& byUsage,
                  Material base, bool normalDirectX) {
  return fill(
      std::move(base),
      [&](Role role) -> sk_sp<SkImage> {
        // First usage word that names the role wins; "diffuse" and
        // "baseColor" both land on BaseColor, so a graph tagged either
        // way is read.
        for (const auto& [usage, image] : byUsage)
          if (roleForUsage(usage) == role && image) return image;
        return nullptr;
      },
      normalDirectX);
}

Material material(const geometry::import::Part& part, const BytesDecoder& decode,
                  Material base) {
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

std::vector<Material> materials(const geometry::import::Model& model,
                                const BytesDecoder& decode, Material base) {
  std::vector<Material> slots((size_t)model.materialSlotCount(), base);
  std::vector<bool> filled(slots.size(), false);
  for (const geometry::import::Part& part : model.parts) {
    if (part.materialIndex < 0 || (size_t)part.materialIndex >= slots.size() ||
        filled[(size_t)part.materialIndex])
      continue;
    slots[(size_t)part.materialIndex] = material(part, decode, base);
    filled[(size_t)part.materialIndex] = true;
  }
  return slots;
}

}  // namespace sigil::world::textures
