/** @file
 * A Material as UsdPreviewSurface: one prim per distinct material under
 * /World/Materials, textures as UsdUVTexture nodes reading `st`, and the
 * images themselves written as PNG files beside the stage.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/output.h>
#include <pxr/usd/usdShade/shader.h>

#include "WriterImpl.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

namespace {

bool writePng(const sk_sp<SkImage>& image, const std::filesystem::path& path) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(image->width(), image->height()));
  if (!image->readPixels(nullptr, bm.pixmap(), 0, 0)) return false;
  SkFILEWStream stream(path.string().c_str());
  return stream.isValid() && SkPngEncoder::Encode(&stream, bm.pixmap(), {});
}

}  // namespace

std::filesystem::path Writer::Impl::textureDir() const {
  if (!options.textureDir.empty()) return options.textureDir;
  return file.stem().string() + "_textures";
}

std::optional<std::string> Writer::Impl::textureAsset(
    const sk_sp<SkImage>& image, const char* role) {
  if (!image) return std::nullopt;
  if (auto it = writtenImages.find(image.get()); it != writtenImages.end())
    return it->second;
  const std::filesystem::path dir = file.parent_path() / textureDir();
  if (!texturesDirReady) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    texturesDirReady = true;
  }
  const std::string name =
      std::to_string(++textureCounter) + "_" + role + ".png";
  if (!writePng(image, dir / name)) return std::nullopt;
  std::string asset = (textureDir() / name).generic_string();
  writtenImages[image.get()] = asset;
  return asset;
}

SdfPath Writer::Impl::material(const world::Material& m,
                               std::string_view hint) {
  for (const auto& [known, path] : materials)
    if (known == m) return path;
  const std::string path = uniquePath("/World/Materials", hint);
  UsdShadeMaterial material = UsdShadeMaterial::Define(stage, SdfPath(path));
  UsdShadeShader surface =
      UsdShadeShader::Define(stage, SdfPath(path + "/PreviewSurface"));
  surface.CreateIdAttr(VtValue(TfToken("UsdPreviewSurface")));
  material.CreateSurfaceOutput().ConnectToSource(surface.ConnectableAPI(),
                                                 TfToken("surface"));

  // st reader shared by every texture node of this material.
  UsdShadeShader stReader;
  const auto reader = [&]() -> UsdShadeShader& {
    if (!stReader) {
      stReader = UsdShadeShader::Define(stage, SdfPath(path + "/stReader"));
      stReader.CreateIdAttr(VtValue(TfToken("UsdPrimvarReader_float2")));
      stReader.CreateInput(TfToken("varname"), SdfValueTypeNames->Token)
          .Set(TfToken("st"));
      stReader.CreateOutput(TfToken("result"), SdfValueTypeNames->Float2);
    }
    return stReader;
  };
  // A texture node reading `role` bound to `input` of the surface.
  const auto texture = [&](const sk_sp<SkImage>& image, const char* role,
                           const char* input, const SdfValueTypeName& type,
                           const char* channel, bool srgb) -> bool {
    const std::optional<std::string> asset = textureAsset(image, role);
    if (!asset) return false;
    UsdShadeShader node = UsdShadeShader::Define(
        stage, SdfPath(path + "/" + std::string(role) + "Texture"));
    node.CreateIdAttr(VtValue(TfToken("UsdUVTexture")));
    node.CreateInput(TfToken("file"), SdfValueTypeNames->Asset)
        .Set(SdfAssetPath(*asset));
    node.CreateInput(TfToken("st"), SdfValueTypeNames->Float2)
        .ConnectToSource(reader().ConnectableAPI(), TfToken("result"));
    node.CreateInput(TfToken("wrapS"), SdfValueTypeNames->Token)
        .Set(TfToken(m.tile ? "repeat" : "clamp"));
    node.CreateInput(TfToken("wrapT"), SdfValueTypeNames->Token)
        .Set(TfToken(m.tile ? "repeat" : "clamp"));
    node.CreateInput(TfToken("sourceColorSpace"), SdfValueTypeNames->Token)
        .Set(TfToken(srgb ? "sRGB" : "raw"));
    node.CreateOutput(TfToken(channel), type);
    surface.CreateInput(TfToken(input), type)
        .ConnectToSource(node.ConnectableAPI(), TfToken(channel));
    return true;
  };
  const auto channelName = [](int c) {
    return c == 1 ? "g" : c == 2 ? "b" : c == 3 ? "a" : "r";
  };

  if (!texture(m.texture, "baseColor", "diffuseColor",
               SdfValueTypeNames->Color3f, "rgb", true))
    surface.CreateInput(TfToken("diffuseColor"), SdfValueTypeNames->Color3f)
        .Set(GfVec3f(m.baseColor.x, m.baseColor.y, m.baseColor.z));
  if (m.texture) {
    // The base colour factor still multiplies in the shader here; USD
    // has no multiplier input, so it rides as custom data.
    surface.GetPrim().SetCustomDataByKey(
        TfToken("sigil:baseColorFactor"),
        VtValue(GfVec3f(m.baseColor.x, m.baseColor.y, m.baseColor.z)));
  }
  if (!texture(m.roughnessMap, "roughness", "roughness",
               SdfValueTypeNames->Float, channelName(m.roughnessChannel),
               false))
    surface.CreateInput(TfToken("roughness"), SdfValueTypeNames->Float)
        .Set(m.roughness);
  if (!texture(m.metallicMap, "metallic", "metallic", SdfValueTypeNames->Float,
               channelName(m.metallicChannel), false))
    surface.CreateInput(TfToken("metallic"), SdfValueTypeNames->Float)
        .Set(m.metallic);
  texture(m.occlusionMap, "occlusion", "occlusion", SdfValueTypeNames->Float,
          channelName(m.occlusionChannel), false);
  if (m.normalMap) {
    // UsdUVTexture can remap [0,1] to [-1,1] itself.
    const std::optional<std::string> asset =
        textureAsset(m.normalMap, "normal");
    if (asset) {
      UsdShadeShader node =
          UsdShadeShader::Define(stage, SdfPath(path + "/normalTexture"));
      node.CreateIdAttr(VtValue(TfToken("UsdUVTexture")));
      node.CreateInput(TfToken("file"), SdfValueTypeNames->Asset)
          .Set(SdfAssetPath(*asset));
      node.CreateInput(TfToken("st"), SdfValueTypeNames->Float2)
          .ConnectToSource(reader().ConnectableAPI(), TfToken("result"));
      node.CreateInput(TfToken("sourceColorSpace"), SdfValueTypeNames->Token)
          .Set(TfToken("raw"));
      node.CreateInput(TfToken("scale"), SdfValueTypeNames->Float4)
          .Set(GfVec4f(2, m.normalMapDirectX ? -2.0f : 2.0f, 2, 1));
      node.CreateInput(TfToken("bias"), SdfValueTypeNames->Float4)
          .Set(GfVec4f(-1, m.normalMapDirectX ? 1.0f : -1.0f, -1, 0));
      node.CreateOutput(TfToken("rgb"), SdfValueTypeNames->Normal3f);
      surface.CreateInput(TfToken("normal"), SdfValueTypeNames->Normal3f)
          .ConnectToSource(node.ConnectableAPI(), TfToken("rgb"));
    }
  }
  if (m.emissiveStrength > 0) {
    const glm::vec3 e = glm::vec3(m.emissive) * m.emissiveStrength;
    if (!texture(m.emissiveMap, "emissive", "emissiveColor",
                 SdfValueTypeNames->Color3f, "rgb", true))
      surface.CreateInput(TfToken("emissiveColor"), SdfValueTypeNames->Color3f)
          .Set(GfVec3f(e.x, e.y, e.z));
  }
  if (!texture(m.opacityMap, "opacity", "opacity", SdfValueTypeNames->Float,
               channelName(m.opacityChannel), false))
    if (m.baseColor.w < 1.0f)
      surface.CreateInput(TfToken("opacity"), SdfValueTypeNames->Float)
          .Set(m.baseColor.w);
  if (m.alphaCutoff > 0)
    surface.CreateInput(TfToken("opacityThreshold"), SdfValueTypeNames->Float)
        .Set(m.alphaCutoff);
  surface.CreateInput(TfToken("ior"), SdfValueTypeNames->Float).Set(m.ior);
  surface.CreateInput(TfToken("useSpecularWorkflow"), SdfValueTypeNames->Int)
      .Set(0);
  if (m.transmission > 0)
    material.GetPrim().SetCustomDataByKey(TfToken("sigil:transmission"),
                                          VtValue(m.transmission));
  if (!m.layers.empty())
    material.GetPrim().SetCustomDataByKey(TfToken("sigil:layers"),
                                          VtValue((int)m.layers.size()));
  if (m.unlit)
    material.GetPrim().SetCustomDataByKey(TfToken("sigil:unlit"),
                                          VtValue(true));
  materials.emplace_back(m, SdfPath(path));
  return SdfPath(path);
}

}  // namespace sigil::usd
