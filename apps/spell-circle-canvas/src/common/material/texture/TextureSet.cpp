/** @file
 * The role vocabulary the tools' file names use, the classifier over it,
 * the directory walk that groups files into sets, and the decode of a
 * set into textures by role.
 */

#include "sigilmaterial/texture/TextureSet.h"

#include <algorithm>
#include <boost/container/flat_map.hpp>
#include <cctype>

namespace sigil::material::textures {

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

std::string_view name(Role role) {
  switch (role) {
    case Role::Unknown:
      return "unknown";
    case Role::BaseColor:
      return "baseColor";
    case Role::Normal:
      return "normal";
    case Role::Roughness:
      return "roughness";
    case Role::Metallic:
      return "metallic";
    case Role::Occlusion:
      return "occlusion";
    case Role::Emissive:
      return "emissive";
    case Role::Packed:
      return "packed";
    case Role::Height:
      return "height";
    case Role::Opacity:
      return "opacity";
    case Role::Specular:
      return "specular";
  }
  return "?";
}

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
  boost::container::flat_map<std::string, TextureSet> sets;
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

TextureMaps fromFiles(const TextureSet& set, const Decoder& decode) {
  TextureMaps out;
  out.name = set.name;
  out.normalDirectX = set.normalDirectX;
  if (!decode) return out;
  for (const auto& [role, path] : set.files)
    if (sk_sp<SkImage> image = decode(path))
      out.maps.emplace(role,
                       Texture::of(std::move(image)).tile(SkTileMode::kRepeat));
  return out;
}

TextureMaps fromUsageMap(
    const boost::container::map<std::string, sk_sp<SkImage>>& byUsage,
    bool normalDirectX) {
  TextureMaps out;
  out.normalDirectX = normalDirectX;
  for (const auto& [usage, image] : byUsage) {
    const Role role = roleForUsage(usage);
    if (role == Role::Unknown || !image || out.maps.count(role)) continue;
    out.maps.emplace(role, Texture::of(image).tile(SkTileMode::kRepeat));
  }
  return out;
}

}  // namespace sigil::material::textures
