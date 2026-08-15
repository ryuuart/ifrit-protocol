#pragma once

/** @file
 * SigilWorld texture sets — the folder a material authoring tool
 * exports, read back into a Material.
 *
 * Substance Painter and Designer, Poly Haven, ambientCG, Quixel and
 * glTF-style pipelines all ship a PBR material the same way: one image
 * per map, named `<set>_<role>[_<size>].<ext>`. The role words differ
 * per tool (`BaseColor` / `diff` / `Color` / `albedo`; `Normal` /
 * `nor_gl` / `NormalGL`; `OcclusionRoughnessMetallic` / `arm` / `orm`)
 * but the shape is one convention. This header knows the words: it
 * classifies a file name into a Role, groups a directory's files into
 * sets by their shared stem, and builds a Material from a set — with a
 * packed occlusion-roughness-metallic image wired to all three channel
 * slots and a DirectX-convention normal map flagged as such.
 *
 * Decoding is not done here. SigilWorld owns no image decoder; the
 * caller supplies one (SigilImage's decode, or anything returning an
 * SkImage for a path), exactly as SigilShape's importer takes a Resolver.
 */

#include <sigilshape/Import.h>
#include <sigilworld/World.h>

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::world::textures {

/** What a map is FOR. Packed is a three-channel occlusion (R),
 *  roughness (G), metallic (B) image — glTF's layout, and what
 *  Substance's glTF template, Poly Haven's `_arm` and the common
 *  `_orm` write. Height, Opacity and Specular are recognized so a set's
 *  files all classify, but Material has no slot for them yet. */
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

/** One material's worth of files, keyed by role. When a set carries
 *  both a Packed image and separate roughness/metallic/occlusion
 *  images, the separate ones win. */
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

/** Build a Material from a set: every recognized map decoded through
 *  @p decode and placed in its slot, a Packed image wired to the
 *  roughness (channel 1), metallic (channel 2) and occlusion
 *  (channel 0) slots unless a separate map exists for that role, the
 *  normal convention flagged, and `tile` set — a scanned material is
 *  meant to repeat. Scalars start from @p base: a set with a metallic
 *  map wants `metallic = 1` so the map's values come through, which is
 *  what the default `base` carries when the set has one. */
Material material(const TextureSet& set, const Decoder& decode,
                  Material base = {});

/** Build a Material from images already decoded and keyed by usage word
 *  — what a rendered Substance graph hands over (`Graph::outputsByUsage`)
 *  or any pipeline that names its maps by channel. Same slot rules as
 *  the file-set overload; the pack (`"orm"`/`"arm"` key) fills whatever
 *  separate maps are missing. Substance graphs author DirectX normals
 *  unless their `$normalformat` input says otherwise, hence the default
 *  for @p normalDirectX. */
Material material(const std::map<std::string, sk_sp<SkImage>>& byUsage,
                  Material base = {}, bool normalDirectX = true);

/** Decodes encoded image bytes (a name hint sharpens format detection);
 *  null when it cannot. */
using BytesDecoder = std::function<sk_sp<SkImage>(
    const std::vector<std::byte>& bytes, std::string_view nameHint)>;

/** Build a Material from an imported model part: its base colour
 *  texture and factor, its metallic / roughness / emissive factors, and
 *  every map its `textures` carry (glTF's normal, packed
 *  metallicRoughness, occlusion, emissive), decoded through @p decode,
 *  plus its transmission, ior and alpha cutoff. glTF normals are
 *  OpenGL-convention; the sampler tiles. The part's factors are kept as
 *  the scalars the maps multiply. */
Material material(const shape::import::Part& part, const BytesDecoder& decode,
                  Material base = {});

}  // namespace sigil::world::textures
