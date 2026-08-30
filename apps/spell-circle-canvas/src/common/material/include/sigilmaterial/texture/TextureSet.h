#pragma once

/** @file
 * Texture sets — the folder a material authoring tool exports, read
 * back as textures by role.
 *
 * Substance Painter and Designer, Poly Haven, ambientCG, Quixel and
 * glTF-style pipelines all ship a PBR material the same way: one image
 * per map, named `<set>_<role>[_<size>].<ext>`. The role words differ
 * per tool (`BaseColor` / `diff` / `Color` / `albedo`; `Normal` /
 * `nor_gl` / `NormalGL`; `OcclusionRoughnessMetallic` / `arm` / `orm`)
 * but the shape is one convention. This header knows the words: it
 * classifies a file name into a Role, groups a directory's files into
 * sets by their shared stem, and decodes a set into one Texture per role.
 *
 * Decoding is not done here: the caller supplies a Decoder — anything
 * returning an image for a path — so the library owns no file access.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <sigilmaterial/texture/Texture.h>

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::material::textures {

/** What a map is FOR. Packed is a three-channel occlusion (R),
 *  roughness (G), metallic (B) image — glTF's layout, and what
 *  Substance's glTF template, Poly Haven's `_arm` and the common
 *  `_orm` write. */
enum class Role {
  Unknown,
  BaseColor,
  Normal,
  Roughness,
  Metallic,
  Occlusion,
  Emissive,
  Packed,
  Height,
  Opacity,
  Specular,
};

/** The role's name as messages and keys spell it. */
std::string_view name(Role role);

/** What one texture file was read to be: the channel it feeds, the set
 *  of sibling files it belongs to, and any convention its name
 *  declares. Produced by inspecting the filename, so a directory of
 *  maps from any of the usual sources can be sorted into materials
 *  without being told the naming scheme up front. */
struct Classified {
  Role role = Role::Unknown;
  /** The set this file belongs to: the file stem with the role word and
   *  any trailing size token (`1k`, `2K`, `4096`) removed. */
  std::string set;
  /** For Role::Normal: the map's green channel points DOWN the image
   *  (DirectX). Set from `_NormalDX`, `_nor_dx`, `_Normal_DirectX` and
   *  the like; plain `_Normal` / `_nor` read as OpenGL. */
  bool directX = false;
};

/** Classify one file name (path or bare name; the extension is
 *  ignored). Matching is case-insensitive on the last one or two
 *  underscore- or hyphen-separated tokens before an optional size
 *  token. */
Classified classify(std::string_view filename);

/** The role a channel USAGE word names — the vocabulary a Substance
 *  graph tags its outputs with ("baseColor", "diffuse", "normal",
 *  "roughness", "metallic", "ambientOcclusion", "emissive", "height",
 *  "opacity", "specular") and glTF's material slots share. Unknown for
 *  anything else. */
Role roleForUsage(std::string_view usage);

/** One material's worth of files, keyed by role. */
struct TextureSet {
  std::string name;
  std::map<Role, std::filesystem::path> files;
  bool normalDirectX = false;
};

/** Every set under @p directory (not recursive): image files grouped by
 *  the stem their role word leaves behind. Files with no recognized
 *  role are ignored. Sorted by name. */
std::vector<TextureSet> discover(const std::filesystem::path& directory);

/** Decodes a path into an image; null when it cannot. */
using Decoder = std::function<sk_sp<SkImage>(const std::filesystem::path&)>;

/** A set decoded: one repeating texture per role that decoded, and the
 *  normal convention the set declared. */
struct TextureMaps {
  std::string name;
  std::map<Role, Texture> maps;
  bool normalDirectX = false;

  /** The texture for @p role, or null when the set has none. */
  const Texture* map(Role role) const {
    auto it = maps.find(role);
    return it == maps.end() ? nullptr : &it->second;
  }
};

/** Every file of @p set decoded through @p decode into a texture that
 *  repeats on both axes — a scanned material is meant to tile. A file
 *  that fails to decode leaves its role out. */
TextureMaps fromFiles(const TextureSet& set, const Decoder& decode);

/** Images already decoded and keyed by usage word — what a rendered
 *  Substance graph hands over or any pipeline that names its maps by
 *  channel — as textures by role. The first usage word naming a role
 *  wins, so "diffuse" and "baseColor" both land on BaseColor. Substance
 *  graphs author DirectX normals unless told otherwise, hence the
 *  default for @p normalDirectX. */
TextureMaps fromUsageMap(const std::map<std::string, sk_sp<SkImage>>& byUsage,
                         bool normalDirectX = true);

}  // namespace sigil::material::textures
