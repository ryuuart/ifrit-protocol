/** @file
 * A bound UsdPreviewSurface into a Part's material fields: scalar
 * inputs as factors, UsdUVTexture inputs as texture references whose
 * bytes are read from the files beside the stage.
 */

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/shader.h>

#include <fstream>
#include <optional>

#include "ReadContext.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

namespace {

std::optional<std::vector<std::byte>> readBytes(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  std::vector<std::byte> bytes;
  char c;
  while (in.get(c)) bytes.push_back((std::byte)c);
  return bytes;
}

}  // namespace

void readMaterial(const UsdShadeMaterial& material,
                  const std::filesystem::path& stageDir,
                  geometry::decode::Part& part) {
  UsdShadeShader surface = material.ComputeSurfaceSource();
  if (!surface) return;
  const auto image =
      [&](const char* inputName) -> std::optional<UsdShadeShader> {
    UsdShadeInput input = surface.GetInput(TfToken(inputName));
    if (!input) return std::nullopt;
    UsdShadeConnectableAPI source;
    TfToken sourceName;
    UsdShadeAttributeType sourceType;
    if (!input.GetConnectedSource(&source, &sourceName, &sourceType))
      return std::nullopt;
    UsdShadeShader texture(source.GetPrim());
    TfToken id;
    if (!texture || !texture.GetShaderId(&id) || id != TfToken("UsdUVTexture"))
      return std::nullopt;
    return texture;
  };
  const auto fetch = [&](const UsdShadeShader& texture, std::string& uriOut,
                         std::vector<std::byte>& bytesOut) {
    SdfAssetPath asset;
    if (UsdShadeInput file = texture.GetInput(TfToken("file")))
      file.Get(&asset);
    const std::string resolved = asset.GetResolvedPath().empty()
                                     ? asset.GetAssetPath()
                                     : asset.GetResolvedPath();
    if (resolved.empty()) return;
    uriOut = asset.GetAssetPath();
    std::filesystem::path p(resolved);
    if (p.is_relative()) p = stageDir / p;
    if (auto bytes = readBytes(p.string())) bytesOut = std::move(*bytes);
  };
  if (auto tex = image("diffuseColor")) {
    fetch(*tex, part.textureUri, part.textureBytes);
  } else if (UsdShadeInput in = surface.GetInput(TfToken("diffuseColor"))) {
    GfVec3f c;
    if (in.Get(&c)) part.baseColor = {c[0], c[1], c[2], part.baseColor.a};
  }
  const auto scalar = [&](const char* name, float& out) {
    if (UsdShadeInput in = surface.GetInput(TfToken(name))) {
      float v;
      if (in.Get(&v)) out = v;
    }
  };
  if (auto tex = image("roughness")) {
    geometry::decode::Part::TextureRef& ref = part.textures["roughness"];
    fetch(*tex, ref.uri, ref.bytes);
  } else {
    scalar("roughness", part.roughness);
  }
  if (auto tex = image("metallic")) {
    geometry::decode::Part::TextureRef& ref = part.textures["metallic"];
    fetch(*tex, ref.uri, ref.bytes);
  } else {
    scalar("metallic", part.metallic);
  }
  if (auto tex = image("occlusion")) {
    geometry::decode::Part::TextureRef& ref = part.textures["occlusion"];
    fetch(*tex, ref.uri, ref.bytes);
  }
  if (auto tex = image("normal")) {
    geometry::decode::Part::TextureRef& ref = part.textures["normal"];
    fetch(*tex, ref.uri, ref.bytes);
  }
  if (auto tex = image("emissiveColor")) {
    geometry::decode::Part::TextureRef& ref = part.textures["emissive"];
    fetch(*tex, ref.uri, ref.bytes);
    part.emissive = {1, 1, 1, 1};
  } else if (UsdShadeInput in = surface.GetInput(TfToken("emissiveColor"))) {
    GfVec3f c;
    if (in.Get(&c)) part.emissive = {c[0], c[1], c[2], 1};
  }
  if (auto tex = image("opacity")) {
    geometry::decode::Part::TextureRef& ref = part.textures["opacity"];
    fetch(*tex, ref.uri, ref.bytes);
    part.opaque = false;
  } else if (UsdShadeInput in = surface.GetInput(TfToken("opacity"))) {
    float a;
    if (in.Get(&a) && a < 1.0f) {
      part.baseColor.a = a;
      part.opaque = false;
    }
  }
  scalar("ior", part.ior);
  scalar("opacityThreshold", part.alphaCutoff);
  const VtValue transmission =
      material.GetPrim().GetCustomDataByKey(TfToken("sigil:transmission"));
  if (transmission.IsHolding<float>())
    part.transmission = transmission.Get<float>();
}

}  // namespace sigil::usd
