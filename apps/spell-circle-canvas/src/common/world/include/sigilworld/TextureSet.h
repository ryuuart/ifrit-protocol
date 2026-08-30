#pragma once

/** @file
 * SigilWorld texture sets — a material authoring tool's export, read
 * back into a world Material. The vocabulary — the roles a map plays,
 * the classifier over the tools' file names, the directory walk into
 * sets and the decode into textures by role — is SigilMaterial's, spelled
 * here under the same names; this header adds the world's own slot rules:
 * a packed occlusion-roughness-metallic image wired to all three channel
 * slots, a DirectX-convention normal flagged as such, and the scalar a
 * map multiplies started at one so the map's values come through.
 *
 * Decoding is not done here. SigilWorld owns no image decoder; the
 * caller supplies one (SigilImage's decode, or anything returning an
 * SkImage for a path), exactly as SigilGeometry's importer takes a Resolver.
 */

#include <sigilgeometry/codec/Decode.h>
#include <sigilmaterial/texture/TextureSet.h>
#include <sigilworld/World.h>

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::world::textures {

using material::textures::Classified;
using material::textures::classify;
using material::textures::Decoder;
using material::textures::discover;
using material::textures::Role;
using material::textures::roleForUsage;
using material::textures::TextureSet;

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
Material material(const geometry::decode::Part& part,
                  const BytesDecoder& decode, Material base = {});

/** The model's material SLOTS: one Material per index the parts name
 *  (`Part::materialIndex`), built from the first part wearing each, a
 *  default for any index no part wears — the list `World::place(mesh,
 *  model, slots)` takes beside `Model::merged()`. Empty when no part
 *  names a material. */
std::vector<Material> materials(const geometry::decode::Model& model,
                                const BytesDecoder& decode, Material base = {});

}  // namespace sigil::world::textures
