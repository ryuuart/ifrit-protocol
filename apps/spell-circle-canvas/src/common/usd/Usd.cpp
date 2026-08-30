#include "sigilusd/Usd.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <set>

// OpenUSD. Its headers are chatty under strict warnings; the library's
// own diagnostics are routed to stderr by USD itself.
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quatd.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/quath.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/output.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/tokens.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

namespace {

/** A valid, unique USD identifier from any name. */
std::string identifier(std::string_view name) {
  std::string out;
  for (char c : name)
    out += (std::isalnum((unsigned char)c) || c == '_') ? c : '_';
  if (out.empty() || std::isdigit((unsigned char)out[0])) out = "_" + out;
  return out;
}

/** glm's column-major mat4 as USD's row-vector GfMatrix4d: element (row
 *  i, column j) of the USD matrix is glm[i][j] — glm indexes column
 *  first, so this is the transpose USD's convention wants, and the copy
 *  is elementwise. */
GfMatrix4d toGf(const glm::mat4& m) {
  GfMatrix4d out;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) out[i][j] = (double)m[i][j];
  return out;
}

glm::mat4 fromGf(const GfMatrix4d& g) {
  glm::mat4 out;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) out[i][j] = (float)g[i][j];
  return out;
}

bool writePng(const sk_sp<SkImage>& image, const std::filesystem::path& path) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(image->width(), image->height()));
  if (!image->readPixels(nullptr, bm.pixmap(), 0, 0)) return false;
  SkFILEWStream stream(path.string().c_str());
  return stream.isValid() && SkPngEncoder::Encode(&stream, bm.pixmap(), {});
}

}  // namespace

// ---------------------------------------------------------------------------
// Writer

struct Writer::Impl {
  std::filesystem::path file;
  WriteOptions options;
  UsdStageRefPtr stage;
  std::set<std::string> usedPaths;
  /** Materials already authored, by pointer identity of their images and
   *  value of their scalars — the same material placed twice binds one
   *  prim. */
  std::vector<std::pair<world::Material, SdfPath>> materials;
  int textureCounter = 0;
  bool texturesDirReady = false;
  /** One file per image, however many materials share it. */
  std::map<const SkImage*, std::string> writtenImages;
  std::string error;

  std::string uniquePath(std::string_view parent, std::string_view name) {
    std::string base = std::string(parent) + "/" + identifier(name);
    std::string path = base;
    for (int n = 2; usedPaths.count(path); ++n)
      path = base + "_" + std::to_string(n);
    usedPaths.insert(path);
    return path;
  }

  std::filesystem::path textureDir() const {
    if (!options.textureDir.empty()) return options.textureDir;
    return file.stem().string() + "_textures";
  }

  /** Write an image beside the stage; the asset path is relative. */
  std::optional<std::string> textureAsset(const sk_sp<SkImage>& image,
                                          const char* role) {
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
    const std::string asset = (textureDir() / name).generic_string();
    writtenImages[image.get()] = asset;
    return asset;
  }

  /** The material as UsdPreviewSurface, authored once per distinct
   *  material under /World/Materials. Only the base of a layered
   *  material is expressible; the layer count rides as custom data. */
  SdfPath material(const world::Material& m, std::string_view hint) {
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
    if (!texture(m.metallicMap, "metallic", "metallic",
                 SdfValueTypeNames->Float, channelName(m.metallicChannel),
                 false))
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
        surface
            .CreateInput(TfToken("emissiveColor"), SdfValueTypeNames->Color3f)
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

  /** The mesh's lanes onto a UsdGeomMesh (positions, normals, st,
   *  displayColor, prim lanes as uniform primvars). */
  void fillMesh(UsdGeomMesh& usdMesh, const geometry::Mesh& mesh) {
    VtVec3fArray points;
    points.reserve(mesh.positions.size());
    for (const glm::vec3& p : mesh.positions) points.push_back({p.x, p.y, p.z});
    usdMesh.CreatePointsAttr().Set(points);
    VtIntArray counts((int)mesh.triangleCount(), 3);
    VtIntArray indices;
    indices.reserve(mesh.indices.size());
    for (uint32_t i : mesh.indices) indices.push_back((int)i);
    usdMesh.CreateFaceVertexCountsAttr().Set(counts);
    usdMesh.CreateFaceVertexIndicesAttr().Set(indices);
    usdMesh.CreateSubdivisionSchemeAttr().Set(UsdGeomTokens->none);
    if (mesh.normals.size() == mesh.positions.size()) {
      VtVec3fArray normals;
      for (const glm::vec3& n : mesh.normals)
        normals.push_back({n.x, n.y, n.z});
      usdMesh.CreateNormalsAttr().Set(normals);
      usdMesh.SetNormalsInterpolation(UsdGeomTokens->vertex);
    }
    UsdGeomPrimvarsAPI primvars(usdMesh);
    if (mesh.uvs.size() == mesh.positions.size()) {
      // USD's st has v UP the image; ours runs down.
      VtVec2fArray st;
      for (const glm::vec2& uv : mesh.uvs) st.push_back({uv.x, 1.0f - uv.y});
      primvars
          .CreatePrimvar(TfToken("st"), SdfValueTypeNames->TexCoord2fArray,
                         UsdGeomTokens->vertex)
          .Set(st);
    }
    if (mesh.colors.size() == mesh.positions.size()) {
      VtVec3fArray colors;
      VtFloatArray alphas;
      for (const glm::vec4& c : mesh.colors) {
        colors.push_back({c.r, c.g, c.b});
        alphas.push_back(c.a);
      }
      usdMesh.CreateDisplayColorPrimvar(UsdGeomTokens->vertex).Set(colors);
      usdMesh.CreateDisplayOpacityPrimvar(UsdGeomTokens->vertex).Set(alphas);
    }
    for (const auto& [name, lane] : mesh.prims) {
      if (name == "Material" || lane.size() != mesh.triangleCount()) continue;
      VtVec4fArray values;
      for (const glm::vec4& v : lane) values.push_back({v.x, v.y, v.z, v.w});
      primvars
          .CreatePrimvar(TfToken(identifier(name)),
                         SdfValueTypeNames->Float4Array, UsdGeomTokens->uniform)
          .Set(values);
    }
  }

  /** Bind @p slots: one material over the whole mesh, or GeomSubsets by
   *  the "Material" lane. */
  void bind(UsdGeomMesh& usdMesh, const geometry::Mesh& mesh,
            const std::vector<world::Material>& slots, std::string_view hint) {
    if (slots.empty()) return;
    const std::vector<glm::vec4>* lane = mesh.primIf("Material");
    if (slots.size() == 1 || !lane || lane->size() != mesh.triangleCount()) {
      UsdShadeMaterialBindingAPI::Apply(usdMesh.GetPrim())
          .Bind(UsdShadeMaterial(stage->GetPrimAtPath(
              material(slots.front(), std::string(hint) + "_material"))));
      return;
    }
    std::vector<VtIntArray> faces(slots.size());
    for (size_t t = 0; t < lane->size(); ++t) {
      int slot = (int)std::floor((*lane)[t].x + 0.5f);
      slot = std::clamp(slot, 0, (int)slots.size() - 1);
      faces[(size_t)slot].push_back((int)t);
    }
    for (size_t s = 0; s < slots.size(); ++s) {
      if (faces[s].empty()) continue;
      UsdGeomSubset subset = UsdGeomSubset::CreateGeomSubset(
          usdMesh, TfToken("slot" + std::to_string(s)), UsdGeomTokens->face,
          faces[s], UsdShadeTokens->materialBind);
      UsdShadeMaterialBindingAPI::Apply(subset.GetPrim())
          .Bind(UsdShadeMaterial(stage->GetPrimAtPath(material(
              slots[s], std::string(hint) + "_slot" + std::to_string(s)))));
    }
  }
};

Writer::Writer(const std::filesystem::path& file, WriteOptions options)
    : m_impl(std::make_unique<Impl>()) {
  m_impl->file = file;
  m_impl->options = std::move(options);
  m_impl->stage = UsdStage::CreateInMemory();
  if (m_impl->stage) {
    UsdGeomXform world = UsdGeomXform::Define(m_impl->stage, SdfPath("/World"));
    m_impl->stage->SetDefaultPrim(world.GetPrim());
    UsdGeomSetStageUpAxis(m_impl->stage, UsdGeomTokens->y);
    UsdGeomSetStageMetersPerUnit(m_impl->stage, m_impl->options.metersPerUnit);
    UsdGeomScope::Define(m_impl->stage, SdfPath("/World/Materials"));
    m_impl->usedPaths.insert("/World");
    m_impl->usedPaths.insert("/World/Materials");
  }
}

Writer::~Writer() = default;

std::string Writer::mesh(std::string_view name, const geometry::Mesh& mesh,
                         const glm::mat4& model,
                         const std::vector<world::Material>& slots,
                         std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage) return {};
  const std::string path = impl.uniquePath(parent, name);
  UsdGeomMesh usdMesh = UsdGeomMesh::Define(impl.stage, SdfPath(path));
  usdMesh.AddTransformOp().Set(toGf(model));
  impl.fillMesh(usdMesh, mesh);
  impl.bind(usdMesh, mesh, slots, name);
  return path;
}

std::string Writer::stamps(std::string_view name, const geometry::Cloud& cloud,
                           const geometry::Mesh& stamp, const glm::mat4& model,
                           const world::Material& material,
                           std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage) return {};
  const std::string path = impl.uniquePath(parent, name);
  UsdGeomPointInstancer instancer =
      UsdGeomPointInstancer::Define(impl.stage, SdfPath(path));
  instancer.AddTransformOp().Set(toGf(model));
  // The one prototype, under the instancer.
  const std::string protoPath = path + "/Prototypes/stamp";
  UsdGeomScope::Define(impl.stage, SdfPath(path + "/Prototypes"));
  UsdGeomMesh proto = UsdGeomMesh::Define(impl.stage, SdfPath(protoPath));
  impl.fillMesh(proto, stamp);
  impl.bind(proto, stamp, {material}, name);
  instancer.CreatePrototypesRel().AddTarget(SdfPath(protoPath));

  const size_t n = cloud.size();
  VtVec3fArray positions;
  VtIntArray protoIndices((int)n, 0);
  VtVec3fArray scales;
  VtQuathArray orientations;
  const std::vector<float>* size = cloud.scalarIf("size");
  const std::vector<glm::vec3>* dir = cloud.vectorIf("dir");
  if (!dir) dir = cloud.vectorIf("normal");
  const std::vector<glm::vec4>* tint = cloud.colorIf("tint");
  for (size_t i = 0; i < n; ++i) {
    const glm::vec3& p = cloud.positions[i];
    positions.push_back({p.x, p.y, p.z});
    const float s = size && i < size->size() ? (*size)[i] : 1.0f;
    scales.push_back({s, s, s});
    if (dir && i < dir->size()) {
      // The stamp's +z along dir: the rotation taking (0,0,1) to dir.
      const glm::vec3 d = glm::normalize((*dir)[i]);
      const GfRotation rot(GfVec3d(0, 0, 1), GfVec3d(d.x, d.y, d.z));
      const GfQuatd q = rot.GetQuat();
      orientations.push_back(
          GfQuath((GfHalf)q.GetReal(), (GfHalf)q.GetImaginary()[0],
                  (GfHalf)q.GetImaginary()[1], (GfHalf)q.GetImaginary()[2]));
    }
  }
  instancer.CreatePositionsAttr().Set(positions);
  instancer.CreateProtoIndicesAttr().Set(protoIndices);
  instancer.CreateScalesAttr().Set(scales);
  if (orientations.size() == n)
    instancer.CreateOrientationsAttr().Set(orientations);
  if (tint && tint->size() == n) {
    VtVec3fArray colors;
    VtFloatArray alphas;
    for (const glm::vec4& c : *tint) {
      colors.push_back({c.r, c.g, c.b});
      alphas.push_back(c.a);
    }
    UsdGeomPrimvarsAPI primvars(instancer);
    primvars
        .CreatePrimvar(TfToken("displayColor"), SdfValueTypeNames->Color3fArray,
                       UsdGeomTokens->vertex)
        .Set(colors);
    primvars
        .CreatePrimvar(TfToken("displayOpacity"), SdfValueTypeNames->FloatArray,
                       UsdGeomTokens->vertex)
        .Set(alphas);
  }
  return path;
}

std::string Writer::light(std::string_view name,
                          const world::LightComponent& light,
                          std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage) return {};
  const std::string path = impl.uniquePath(parent, name);
  if (light.type == world::LightComponent::Type::Point) {
    UsdLuxSphereLight sphere =
        UsdLuxSphereLight::Define(impl.stage, SdfPath(path));
    sphere.AddTranslateOp().Set(
        GfVec3d(light.position.x, light.position.y, light.position.z));
    sphere.CreateRadiusAttr().Set(1.0f);
    sphere.CreateIntensityAttr().Set(light.intensity);
    sphere.CreateColorAttr().Set(
        GfVec3f(light.color.x, light.color.y, light.color.z));
    sphere.GetPrim().SetCustomDataByKey(TfToken("sigil:range"),
                                        VtValue(light.range));
    return path;
  }
  UsdLuxDistantLight distant =
      UsdLuxDistantLight::Define(impl.stage, SdfPath(path));
  // A distant light shines down its -Z; aim -Z along the direction.
  const glm::vec3 d = glm::normalize(light.direction);
  const GfRotation rot(GfVec3d(0, 0, -1), GfVec3d(d.x, d.y, d.z));
  distant.AddOrientOp().Set(GfQuatf(rot.GetQuat()));
  distant.CreateIntensityAttr().Set(light.intensity);
  distant.CreateColorAttr().Set(
      GfVec3f(light.color.x, light.color.y, light.color.z));
  return path;
}

std::string Writer::sun(std::string_view name, const world::Lighting& lighting,
                        std::string_view parent) {
  world::LightComponent sun;
  sun.type = world::LightComponent::Type::Directional;
  sun.direction = lighting.sunDirection;
  sun.color = lighting.sunColor;
  sun.intensity = lighting.sunIntensity;
  return light(name, sun, parent);
}

std::string Writer::camera(std::string_view name,
                           const geometry::space::Camera& camera,
                           std::string_view parent) {
  Impl& impl = *m_impl;
  if (!impl.stage) return {};
  const std::string path = impl.uniquePath(parent, name);
  UsdGeomCamera cam = UsdGeomCamera::Define(impl.stage, SdfPath(path));
  // Camera-to-world: the inverse of the view matrix.
  cam.AddTransformOp().Set(toGf(glm::inverse(camera.view())));
  // A 24mm-tall aperture with the focal length that gives the vertical
  // fov; the horizontal aperture follows the aspect a consumer sets.
  const float aperture = 24.0f;
  const float focal =
      aperture * 0.5f / std::tan(camera.fovYDeg * (float)M_PI / 360.0f);
  cam.CreateFocalLengthAttr().Set(focal);
  cam.CreateVerticalApertureAttr().Set(aperture);
  cam.CreateHorizontalApertureAttr().Set(aperture * 16.0f / 9.0f);
  cam.CreateClippingRangeAttr().Set(GfVec2f(camera.zNear, camera.zFar));
  return path;
}

bool Writer::save(std::string* error) {
  Impl& impl = *m_impl;
  if (!impl.stage) {
    if (error) *error = "no stage";
    return false;
  }
  std::error_code ec;
  if (!impl.file.parent_path().empty())
    std::filesystem::create_directories(impl.file.parent_path(), ec);
  // Export writes the file the extension asks for: .usdc (crate), .usda
  // (ascii), .usd (crate), .usdz (a package).
  if (!impl.stage->GetRootLayer()->Export(impl.file.string())) {
    if (error) *error = "USD refused to write " + impl.file.string();
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Reader

namespace {

/** One value per face-vertex from a primvar of any interpolation. */
template <class T>
std::vector<T> perFaceVertex(const VtArray<T>& values, const TfToken& interp,
                             const VtIntArray& counts,
                             const VtIntArray& indices,
                             const VtIntArray& primvarIndices) {
  std::vector<T> out;
  out.reserve(indices.size());
  int face = 0, inFace = 0;
  for (size_t fv = 0; fv < indices.size(); ++fv) {
    size_t at;
    if (interp == UsdGeomTokens->constant)
      at = 0;
    else if (interp == UsdGeomTokens->uniform)
      at = (size_t)face;
    else if (interp == UsdGeomTokens->faceVarying)
      at = fv;
    else  // vertex / varying: by point
      at = (size_t)indices[fv];
    if (!primvarIndices.empty() && at < primvarIndices.size())
      at = (size_t)primvarIndices[at];
    out.push_back(at < values.size() ? values[at] : T());
    if (++inFace >= counts[(size_t)face]) {
      inFace = 0;
      ++face;
    }
  }
  return out;
}

std::optional<std::vector<std::byte>> readBytes(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  std::vector<std::byte> bytes;
  char c;
  while (in.get(c)) bytes.push_back((std::byte)c);
  return bytes;
}

/** A UsdPreviewSurface (or the shader a material's surface output
 *  connects to) into a Part's material fields. */
void readMaterial(const UsdShadeMaterial& material,
                  const std::filesystem::path& stageDir,
                  geometry::import::Part& part) {
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
    geometry::import::Part::TextureRef& ref = part.textures["roughness"];
    fetch(*tex, ref.uri, ref.bytes);
  } else {
    scalar("roughness", part.roughness);
  }
  if (auto tex = image("metallic")) {
    geometry::import::Part::TextureRef& ref = part.textures["metallic"];
    fetch(*tex, ref.uri, ref.bytes);
  } else {
    scalar("metallic", part.metallic);
  }
  if (auto tex = image("occlusion")) {
    geometry::import::Part::TextureRef& ref = part.textures["occlusion"];
    fetch(*tex, ref.uri, ref.bytes);
  }
  if (auto tex = image("normal")) {
    geometry::import::Part::TextureRef& ref = part.textures["normal"];
    fetch(*tex, ref.uri, ref.bytes);
  }
  if (auto tex = image("emissiveColor")) {
    geometry::import::Part::TextureRef& ref = part.textures["emissive"];
    fetch(*tex, ref.uri, ref.bytes);
    part.emissive = {1, 1, 1, 1};
  } else if (UsdShadeInput in = surface.GetInput(TfToken("emissiveColor"))) {
    GfVec3f c;
    if (in.Get(&c)) part.emissive = {c[0], c[1], c[2], 1};
  }
  if (auto tex = image("opacity")) {
    geometry::import::Part::TextureRef& ref = part.textures["opacity"];
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
  VtValue transmission;
  if (material.GetPrim()
          .GetCustomDataByKey(TfToken("sigil:transmission"))
          .IsHolding<float>())
    part.transmission = material.GetPrim()
                            .GetCustomDataByKey(TfToken("sigil:transmission"))
                            .Get<float>();
}

}  // namespace

std::optional<geometry::import::Model> readModel(
    const std::filesystem::path& file, std::string* error) {
  return readModel(file, nullptr, error);
}

std::optional<geometry::import::Model> readModel(
    const std::filesystem::path& file, ReadInfo* info, std::string* error) {
  UsdStageRefPtr stage = UsdStage::Open(file.string());
  if (!stage) {
    if (error) *error = "cannot open " + file.string();
    return std::nullopt;
  }
  const std::filesystem::path stageDir = file.parent_path();
  geometry::import::Model model;
  std::vector<std::string> materialNames;
  const auto materialSlot = [&](const UsdShadeMaterial& material) -> int {
    if (!material) return -1;
    const std::string name = material.GetPath().GetString();
    for (size_t i = 0; i < materialNames.size(); ++i)
      if (materialNames[i] == name) return (int)i;
    materialNames.push_back(name);
    return (int)materialNames.size() - 1;
  };
  UsdGeomXformCache xforms;
  for (const UsdPrim& prim : stage->Traverse()) {
    if (prim.IsA<UsdGeomMesh>()) {
      UsdGeomMesh usdMesh(prim);
      VtVec3fArray points;
      VtIntArray counts, indices;
      usdMesh.GetPointsAttr().Get(&points);
      usdMesh.GetFaceVertexCountsAttr().Get(&counts);
      usdMesh.GetFaceVertexIndicesAttr().Get(&indices);
      if (points.empty() || counts.empty()) continue;
      geometry::import::Part part;
      part.name = prim.GetName().GetString();
      geometry::Mesh& mesh = part.mesh;
      // Unwelded per face-vertex, faces fan-triangulated.
      VtVec3fArray normals;
      usdMesh.GetNormalsAttr().Get(&normals);
      const TfToken normalsInterp = usdMesh.GetNormalsInterpolation();
      UsdGeomPrimvarsAPI primvars(usdMesh);
      VtVec2fArray st;
      VtIntArray stIndices;
      TfToken stInterp;
      if (UsdGeomPrimvar pv = primvars.GetPrimvar(TfToken("st"))) {
        pv.Get(&st);
        pv.GetIndices(&stIndices);
        stInterp = pv.GetInterpolation();
      }
      VtVec3fArray colors;
      VtIntArray colorIndices;
      TfToken colorInterp;
      if (UsdGeomPrimvar pv = usdMesh.GetDisplayColorPrimvar()) {
        pv.Get(&colors);
        pv.GetIndices(&colorIndices);
        colorInterp = pv.GetInterpolation();
      }
      const std::vector<GfVec3f> nPerFv =
          normals.empty() ? std::vector<GfVec3f>{}
                          : perFaceVertex(normals, normalsInterp, counts,
                                          indices, VtIntArray());
      const std::vector<GfVec2f> stPerFv =
          st.empty() ? std::vector<GfVec2f>{}
                     : perFaceVertex(st, stInterp, counts, indices, stIndices);
      const std::vector<GfVec3f> cPerFv =
          colors.empty() ? std::vector<GfVec3f>{}
                         : perFaceVertex(colors, colorInterp, counts, indices,
                                         colorIndices);
      // Face -> subset material slot (the "Material" lane), then the
      // fan.
      std::vector<int> faceSlot(counts.size(), -1);
      const std::vector<UsdGeomSubset> subsets = UsdGeomSubset::GetGeomSubsets(
          usdMesh, UsdGeomTokens->face, UsdShadeTokens->materialBind);
      for (const UsdGeomSubset& subset : subsets) {
        const int slot =
            materialSlot(UsdShadeMaterialBindingAPI(subset.GetPrim())
                             .ComputeBoundMaterial());
        VtIntArray faces;
        subset.GetIndicesAttr().Get(&faces);
        for (int f : faces)
          if (f >= 0 && (size_t)f < faceSlot.size()) faceSlot[(size_t)f] = slot;
      }
      const UsdShadeMaterial bound =
          UsdShadeMaterialBindingAPI(prim).ComputeBoundMaterial();
      const int wholeSlot = materialSlot(bound);
      if (bound) {
        readMaterial(bound, stageDir, part);
        part.materialIndex = wholeSlot;
      }
      std::vector<glm::vec4> laneValues;
      size_t fv = 0;
      for (size_t f = 0; f < counts.size(); ++f) {
        const int n = counts[f];
        const uint32_t base = (uint32_t)mesh.positions.size();
        for (int k = 0; k < n; ++k, ++fv) {
          const GfVec3f& p = points[(size_t)indices[fv]];
          mesh.positions.push_back({p[0], p[1], p[2]});
          if (!nPerFv.empty()) {
            const GfVec3f& nn = nPerFv[fv];
            mesh.normals.push_back({nn[0], nn[1], nn[2]});
          }
          if (!stPerFv.empty()) {
            const GfVec2f& uv = stPerFv[fv];
            mesh.uvs.push_back({uv[0], 1.0f - uv[1]});
          }
          if (!cPerFv.empty()) {
            const GfVec3f& c = cPerFv[fv];
            mesh.colors.push_back({c[0], c[1], c[2], 1});
          }
        }
        for (int k = 1; k + 1 < n; ++k) {
          mesh.indices.insert(mesh.indices.end(), {base, base + (uint32_t)k,
                                                   base + (uint32_t)k + 1});
          const int slot = faceSlot[f] >= 0 ? faceSlot[f] : wholeSlot;
          laneValues.push_back({(float)std::max(slot, 0), 0, 0, 0});
        }
      }
      if (!subsets.empty() || wholeSlot >= 0)
        mesh.prim("Material", {0, 0, 0, 0}) = laneValues;
      if (mesh.normals.size() != mesh.positions.size()) mesh.normals.clear();
      if (mesh.uvs.size() != mesh.positions.size()) mesh.uvs.clear();
      if (mesh.colors.size() != mesh.positions.size()) mesh.colors.clear();
      // The subset materials: the FIRST subset's material fills the
      // part's factors when the mesh as a whole binds none.
      if (!bound && !subsets.empty())
        readMaterial(UsdShadeMaterialBindingAPI(subsets.front().GetPrim())
                         .ComputeBoundMaterial(),
                     stageDir, part);
      const glm::mat4 world = fromGf(xforms.GetLocalToWorldTransform(prim));
      mesh.transform(world);
      if (mesh.normals.empty() && !mesh.indices.empty()) mesh.computeNormals();
      if (!mesh.indices.empty()) model.parts.push_back(std::move(part));
    } else if (prim.IsA<UsdGeomPointInstancer>()) {
      UsdGeomPointInstancer instancer(prim);
      VtVec3fArray positions;
      instancer.GetPositionsAttr().Get(&positions);
      if (positions.empty()) continue;
      geometry::import::Part part;
      part.name = prim.GetName().GetString();
      for (const GfVec3f& p : positions)
        part.mesh.positions.push_back({p[0], p[1], p[2]});
      VtVec3fArray scales;
      if (instancer.GetScalesAttr().Get(&scales) &&
          scales.size() == positions.size()) {
        std::vector<float>& size = part.scalarLanes["size"];
        for (const GfVec3f& s : scales) size.push_back(s[0]);
      }
      const glm::mat4 world = fromGf(xforms.GetLocalToWorldTransform(prim));
      part.mesh.transform(world);
      model.parts.push_back(std::move(part));
    }
  }
  if (info) info->materialNames = materialNames;
  return model;
}

}  // namespace sigil::usd
