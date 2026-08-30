/** @file
 * A material as UsdPreviewSurface: one prim per distinct material under
 * /World/Materials, the surface recipe's params read by name and its map
 * slots as UsdUVTexture nodes reading `st`, and the images themselves
 * written as PNG files beside the stage.
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
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/kit/Surface.h>

#include <cmath>

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

namespace {

/** A params field's value when the recipe declares it, else @p fallback
 *  — so a material built from some other recipe still writes a valid
 *  preview surface rather than a surface of zeros. */
float scalar(const material::Material& m, std::string_view name,
             float fallback) {
  const material::Field* field = m.recipe().params().find(name);
  return field && field->kind == material::Kind::Float ? m.get<float>(name)
                                                       : fallback;
}

material::Color tint(const material::Material& m, std::string_view name,
                     material::Color fallback) {
  const material::Field* field = m.recipe().params().find(name);
  return field && field->kind == material::Kind::Color
             ? m.get<material::Color>(name)
             : fallback;
}

/** The material at the bottom of a stack: what UsdPreviewSurface can
 *  express. */
const material::Material& bottom(const material::Material& m) {
  const material::Material* p = &m;
  while (material::under(*p) != p) p = material::under(*p);
  return *p;
}

}  // namespace

SdfPath Writer::Impl::material(const material::Material& top,
                               std::string_view hint) {
  for (const auto& [known, path] : materials)
    if (known == top) return path;
  // Stacking is a live composition UsdPreviewSurface cannot hold: the
  // material at the bottom is written and the depth rides as custom data.
  const material::Material& m = bottom(top);
  const int depth = material::stackDepth(top);
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
  // A texture node reading the map in `slot`, bound to `input` of the
  // surface. The map's own tiling decides the wrap mode.
  const auto texture = [&](std::string_view slot, const char* role,
                           const char* input, const SdfValueTypeName& type,
                           const char* channel, bool srgb) -> bool {
    const material::Texture* map = material::kit::map(m, slot);
    if (!map) return false;
    const std::optional<std::string> asset = textureAsset(map->image(), role);
    if (!asset) return false;
    const bool repeat = map->tileX() == SkTileMode::kRepeat;
    UsdShadeShader node = UsdShadeShader::Define(
        stage, SdfPath(path + "/" + std::string(role) + "Texture"));
    node.CreateIdAttr(VtValue(TfToken("UsdUVTexture")));
    node.CreateInput(TfToken("file"), SdfValueTypeNames->Asset)
        .Set(SdfAssetPath(*asset));
    node.CreateInput(TfToken("st"), SdfValueTypeNames->Float2)
        .ConnectToSource(reader().ConnectableAPI(), TfToken("result"));
    node.CreateInput(TfToken("wrapS"), SdfValueTypeNames->Token)
        .Set(TfToken(repeat ? "repeat" : "clamp"));
    node.CreateInput(TfToken("wrapT"), SdfValueTypeNames->Token)
        .Set(TfToken(map->tileY() == SkTileMode::kRepeat ? "repeat" : "clamp"));
    node.CreateInput(TfToken("sourceColorSpace"), SdfValueTypeNames->Token)
        .Set(TfToken(srgb ? "sRGB" : "raw"));
    node.CreateOutput(TfToken(channel), type);
    surface.CreateInput(TfToken(input), type)
        .ConnectToSource(node.ConnectableAPI(), TfToken(channel));
    return true;
  };
  const auto channelName = [](float c) {
    const int i = (int)std::lround(c);
    return i == 1 ? "g" : i == 2 ? "b" : i == 3 ? "a" : "r";
  };

  const material::Color baseColor =
      tint(m, "baseColor", material::Color{0.8f, 0.8f, 0.8f, 1});
  if (!texture(material::kit::kBaseColorSlot, "baseColor", "diffuseColor",
               SdfValueTypeNames->Color3f, "rgb", true))
    surface.CreateInput(TfToken("diffuseColor"), SdfValueTypeNames->Color3f)
        .Set(GfVec3f(baseColor.r, baseColor.g, baseColor.b));
  else
    // The base colour factor still multiplies in the shader here; USD
    // has no multiplier input, so it rides as custom data.
    surface.GetPrim().SetCustomDataByKey(
        TfToken("sigil:baseColorFactor"),
        VtValue(GfVec3f(baseColor.r, baseColor.g, baseColor.b)));

  if (!texture(material::kit::kRoughnessSlot, "roughness", "roughness",
               SdfValueTypeNames->Float,
               channelName(scalar(m, "roughnessChannel", 0)), false))
    surface.CreateInput(TfToken("roughness"), SdfValueTypeNames->Float)
        .Set(scalar(m, "roughness", 0.5f));
  if (!texture(material::kit::kMetallicSlot, "metallic", "metallic",
               SdfValueTypeNames->Float,
               channelName(scalar(m, "metallicChannel", 0)), false))
    surface.CreateInput(TfToken("metallic"), SdfValueTypeNames->Float)
        .Set(scalar(m, "metallic", 0.0f));
  texture(material::kit::kOcclusionSlot, "occlusion", "occlusion",
          SdfValueTypeNames->Float,
          channelName(scalar(m, "occlusionChannel", 0)), false);

  if (const material::Texture* normal =
          material::kit::map(m, material::kit::kNormalSlot)) {
    // UsdUVTexture can remap [0,1] to [-1,1] itself.
    const std::optional<std::string> asset =
        textureAsset(normal->image(), "normal");
    if (asset) {
      const bool directX = scalar(m, "normalDirectX", 0) > 0.5f;
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
          .Set(GfVec4f(2, directX ? -2.0f : 2.0f, 2, 1));
      node.CreateInput(TfToken("bias"), SdfValueTypeNames->Float4)
          .Set(GfVec4f(-1, directX ? 1.0f : -1.0f, -1, 0));
      node.CreateOutput(TfToken("rgb"), SdfValueTypeNames->Normal3f);
      surface.CreateInput(TfToken("normal"), SdfValueTypeNames->Normal3f)
          .ConnectToSource(node.ConnectableAPI(), TfToken("rgb"));
    }
  }

  const float emissiveStrength = scalar(m, "emissiveStrength", 0);
  if (emissiveStrength > 0) {
    const material::Color e = tint(m, "emissive", material::Color{0, 0, 0, 1});
    if (!texture(material::kit::kEmissiveSlot, "emissive", "emissiveColor",
                 SdfValueTypeNames->Color3f, "rgb", true))
      surface.CreateInput(TfToken("emissiveColor"), SdfValueTypeNames->Color3f)
          .Set(GfVec3f(e.r * emissiveStrength, e.g * emissiveStrength,
                       e.b * emissiveStrength));
  }
  if (!texture(material::kit::kOpacitySlot, "opacity", "opacity",
               SdfValueTypeNames->Float,
               channelName(scalar(m, "opacityChannel", 0)), false))
    if (baseColor.a < 1.0f)
      surface.CreateInput(TfToken("opacity"), SdfValueTypeNames->Float)
          .Set(baseColor.a);
  const float alphaCutoff = scalar(m, "alphaCutoff", 0);
  if (alphaCutoff > 0)
    surface.CreateInput(TfToken("opacityThreshold"), SdfValueTypeNames->Float)
        .Set(alphaCutoff);
  surface.CreateInput(TfToken("ior"), SdfValueTypeNames->Float)
      .Set(scalar(m, "ior", 1.5f));
  surface.CreateInput(TfToken("useSpecularWorkflow"), SdfValueTypeNames->Int)
      .Set(0);
  const float transmission = scalar(m, "transmission", 0);
  if (transmission > 0)
    material.GetPrim().SetCustomDataByKey(TfToken("sigil:transmission"),
                                          VtValue(transmission));
  if (depth > 0)
    material.GetPrim().SetCustomDataByKey(TfToken("sigil:layers"),
                                          VtValue(depth));
  if (material::kit::isUnlit(m))
    material.GetPrim().SetCustomDataByKey(TfToken("sigil:unlit"),
                                          VtValue(true));
  materials.emplace_back(top, SdfPath(path));
  return SdfPath(path);
}

}  // namespace sigil::usd
