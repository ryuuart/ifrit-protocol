#include "sigilworld/World.h"

#include "sigilworld/Components.h"

#include <Common/interface/RefCntAutoPtr.hpp>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <Graphics/GraphicsTools/interface/MapHelper.hpp>

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

namespace sigil::world {

namespace dg = Diligent;

namespace {

// ---------------------------------------------------------------------------
// Matrix upload convention — chosen to dodge glslang's HLSL row_major
// quirks entirely: shaders use column-vector math (mul(M, v)) with
// DEFAULT cbuffer packing, which reads column-major memory; SkM44's
// native storage IS column-major, so uploads are raw dumps with no
// transposes anywhere.

struct Mat4 {
  float m[16];
};

Mat4 colMajor(const SkM44 &src) {
  Mat4 out;
  src.getColMajor(out.m);
  return out;
}

/** Perspective (RH, y-up eye space, depth 0..1) as a column-vector
 *  SkM44. No Vulkan clip-y flip here: Diligent's Vulkan backend
 *  normalizes to the GL/D3D convention (negative viewport internally),
 *  so +y up in clip space is already correct. */
SkM44 perspectiveVk(float fovYDeg, float aspect, float zNear, float zFar) {
  const float f = 1.0f / std::tan(fovYDeg * (float)M_PI / 360.0f);
  return SkM44(f / aspect, 0, 0, 0,     //
               0, f, 0, 0,              //
               0, 0, zFar / (zNear - zFar),
               zNear * zFar / (zNear - zFar), //
               0, 0, -1, 0);
}

/** Classic normal matrix: (M^-1)^T, column-major memory — which is
 *  exactly M^-1 dumped row-major. */
Mat4 normalMatrix(const SkM44 &model) {
  SkM44 inv;
  if (!model.invert(&inv))
    inv = SkM44();
  Mat4 out;
  inv.getRowMajor(out.m);
  return out;
}

// ---------------------------------------------------------------------------
// Shaders

constexpr char kShaderSource[] = R"(
cbuffer FrameConstants
{
    float4x4 g_ViewProj;
    float4 g_CamPos;
    float4 g_SunDir;      // xyz direction toward scene, w intensity
    float4 g_SunColor;
    float4 g_SkyColor;
    float4 g_GroundColor;
    float4 g_Params;      // x ambient
};

cbuffer DrawConstants
{
    float4x4 g_Model;
    float4x4 g_NormalMat;
    float4 g_BaseColor;
    float4 g_Emissive;    // rgb emissive, a unused
    float4 g_MatParams;   // x metallic, y roughness, z emissiveStrength, w unlit
};

Texture2D    g_Texture;
SamplerState g_Texture_sampler;

struct VSIn
{
    float3 Pos    : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV     : ATTRIB2;
};

struct PSIn
{
    float4 Pos    : SV_POSITION;
    float3 World  : WORLDPOS;
    float3 Normal : NORMALX;
    float2 UV     : TEXCOORD0;
};

void VSMain(in VSIn IN, out PSIn OUT)
{
    float4 world = mul(g_Model, float4(IN.Pos, 1.0));
    OUT.World  = world.xyz;
    OUT.Pos    = mul(g_ViewProj, world);
    OUT.Normal = normalize(mul(g_NormalMat, float4(IN.Normal, 0.0)).xyz);
    OUT.UV     = IN.UV;
}

float4 PSMain(in PSIn IN) : SV_TARGET
{
    float4 tex  = g_Texture.Sample(g_Texture_sampler, IN.UV);
    float4 base = tex * g_BaseColor;

    if (g_MatParams.w > 0.5)
    {
        // Unlit screens: skip lighting/tonemap but still re-encode the
        // linearized sRGB sample for the UNORM target.
        float3 unlit = base.rgb + g_Emissive.rgb * g_MatParams.z;
        return float4(pow(max(unlit, 0.0), 1.0 / 2.2), base.a);
    }

    float3 N = normalize(IN.Normal);
    float3 V = normalize(g_CamPos.xyz - IN.World);
    if (dot(N, V) < 0.0)  // two-sided panels shade on both faces
        N = -N;

    float  metallic = g_MatParams.x;
    float  rough    = clamp(g_MatParams.y, 0.045, 1.0);
    float3 F0       = lerp(float3(0.04, 0.04, 0.04), base.rgb, metallic);
    float3 albedo   = base.rgb * (1.0 - metallic);

    float3 L   = normalize(-g_SunDir.xyz);
    float3 H   = normalize(L + V);
    float  ndl = saturate(dot(N, L));
    float  ndv = saturate(dot(N, V));
    float  ndh = saturate(dot(N, H));
    float  hdv = saturate(dot(H, V));

    // GGX + Schlick fresnel + Karis visibility.
    float  a   = rough * rough;
    float  a2  = a * a;
    float  dd  = ndh * ndh * (a2 - 1.0) + 1.0;
    float  D   = a2 / max(3.1415926 * dd * dd, 1e-4);
    float3 F   = F0 + (1.0 - F0) * pow(1.0 - hdv, 5.0);
    float  Vis = 0.5 / max(lerp(2.0 * ndl * ndv, ndl + ndv, a), 1e-4);

    float3 sun    = g_SunColor.rgb * g_SunDir.w;
    float3 direct = (albedo / 3.1415926 + D * F * Vis) * sun * ndl;

    // Hemisphere ambient, with a reflection-oriented lobe for spec.
    float3 hemiN = lerp(g_GroundColor.rgb, g_SkyColor.rgb, N.y * 0.5 + 0.5);
    float3 R     = reflect(-V, N);
    float3 hemiR = lerp(g_GroundColor.rgb, g_SkyColor.rgb, R.y * 0.5 + 0.5);
    float3 ambient = g_Params.x *
        (hemiN * albedo + hemiR * F0 * (1.0 - rough) * (0.4 + 0.6 * pow(1.0 - ndv, 2.0)));

    float3 color = direct + ambient + g_Emissive.rgb * g_MatParams.z;

    // Filmic-ish tonemap + gamma.
    color = color / (color + 1.0);
    color = pow(color, 1.0 / 2.2);
    return float4(color, base.a);
}
)";

struct FrameConstants {
  Mat4 viewProj;
  float camPos[4];
  float sunDir[4];
  float sunColor[4];
  float skyColor[4];
  float groundColor[4];
  float params[4];
};

struct DrawConstants {
  Mat4 model;
  Mat4 normalMat;
  float baseColor[4];
  float emissive[4];
  float matParams[4];
};

struct Vertex {
  float pos[3];
  float normal[3];
  float uv[2];
};

} // namespace

// ---------------------------------------------------------------------------

/** The private GPU component: device objects for one surface entity.
 *  Public state (transform, material) lives in the public components —
 *  see Components.h. */
struct GpuGeometry {
  dg::RefCntAutoPtr<dg::IBuffer> vertexBuffer;
  dg::RefCntAutoPtr<dg::IBuffer> indexBuffer;
  dg::RefCntAutoPtr<dg::IShaderResourceBinding> srb;
  uint32_t indexCount = 0;
};

struct World::Impl {
  WorldConfig config;
  shape::space::Camera camera;
  Lighting lighting;

  dg::RefCntAutoPtr<dg::IRenderDevice> device;
  dg::RefCntAutoPtr<dg::IDeviceContext> context;

  dg::RefCntAutoPtr<dg::ITexture> colorTarget;   // MSAA when enabled
  dg::RefCntAutoPtr<dg::ITexture> resolveTarget; // single-sample
  dg::RefCntAutoPtr<dg::ITexture> depthTarget;
  dg::RefCntAutoPtr<dg::ITexture> stagingTarget;
  int sampleCount = 1;

  dg::RefCntAutoPtr<dg::IPipelineState> opaquePso;
  dg::RefCntAutoPtr<dg::IPipelineState> blendPso;
  dg::RefCntAutoPtr<dg::IBuffer> frameCB;
  dg::RefCntAutoPtr<dg::IBuffer> drawCB;
  dg::RefCntAutoPtr<dg::ITexture> whiteTexture;

  /** Surfaces are entities; ids handed to callers are entity values.
   *  Entity 0 is reserved at init so a valid surface id is never 0. */
  entt::registry registry;
  bool rendered = false;

  bool init(std::string *error);
  bool createTargets(std::string *error);
  bool createPipelines(std::string *error);
  dg::RefCntAutoPtr<dg::ITexture> uploadTexture(const sk_sp<SkImage> &image);
  void writeDrawConstants(const SkM44 &model, const Material &material);
};

bool World::Impl::init(std::string *error) {
  using namespace dg;
  IEngineFactoryVk *factory = GetEngineFactoryVk();
  if (!factory) {
    if (error)
      *error = "Diligent Vulkan factory unavailable";
    return false;
  }
  EngineVkCreateInfo engineCI;
  if (config.validation)
    engineCI.SetValidationLevel(VALIDATION_LEVEL_1);
  IRenderDevice *rawDevice = nullptr;
  IDeviceContext *rawContext = nullptr;
  factory->CreateDeviceAndContextsVk(engineCI, &rawDevice, &rawContext);
  if (!rawDevice || !rawContext) {
    if (error)
      *error = "Vulkan device creation failed (is MoltenVK installed? "
               "brew install molten-vk vulkan-loader)";
    return false;
  }
  device.Attach(rawDevice);
  context.Attach(rawContext);
  // Reserve entity 0: callers read a 0 surface id as failure.
  (void)registry.create();
  return createTargets(error) && createPipelines(error);
}

bool World::Impl::createTargets(std::string *error) {
  using namespace dg;
  const TEXTURE_FORMAT colorFormat = TEX_FORMAT_RGBA8_UNORM;

  sampleCount = std::max(config.sampleCount, 1);
  const TextureFormatInfoExt &fmtInfo =
      device->GetTextureFormatInfoExt(colorFormat);
  while (sampleCount > 1 && !(fmtInfo.SampleCounts & sampleCount))
    sampleCount /= 2;

  TextureDesc desc;
  desc.Type = RESOURCE_DIM_TEX_2D;
  desc.Width = (Uint32)config.width;
  desc.Height = (Uint32)config.height;
  desc.MipLevels = 1;

  desc.Name = "sigilworld color";
  desc.Format = colorFormat;
  desc.BindFlags = BIND_RENDER_TARGET;
  desc.SampleCount = (Uint32)sampleCount;
  device->CreateTexture(desc, nullptr, &colorTarget);

  desc.Name = "sigilworld resolve";
  desc.SampleCount = 1;
  device->CreateTexture(desc, nullptr, &resolveTarget);

  desc.Name = "sigilworld depth";
  desc.Format = TEX_FORMAT_D32_FLOAT;
  desc.BindFlags = BIND_DEPTH_STENCIL;
  desc.SampleCount = (Uint32)sampleCount;
  device->CreateTexture(desc, nullptr, &depthTarget);

  desc.Name = "sigilworld staging";
  desc.Format = colorFormat;
  desc.BindFlags = BIND_NONE;
  desc.SampleCount = 1;
  desc.Usage = USAGE_STAGING;
  desc.CPUAccessFlags = CPU_ACCESS_READ;
  device->CreateTexture(desc, nullptr, &stagingTarget);

  if (!colorTarget || !resolveTarget || !depthTarget || !stagingTarget) {
    if (error)
      *error = "offscreen target creation failed";
    return false;
  }
  return true;
}

bool World::Impl::createPipelines(std::string *error) {
  using namespace dg;

  ShaderCreateInfo shaderCI;
  shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  shaderCI.Desc.UseCombinedTextureSamplers = true;
  shaderCI.Source = kShaderSource;

  RefCntAutoPtr<IShader> vs;
  shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
  shaderCI.Desc.Name = "sigilworld vs";
  shaderCI.EntryPoint = "VSMain";
  device->CreateShader(shaderCI, &vs);

  RefCntAutoPtr<IShader> ps;
  shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
  shaderCI.Desc.Name = "sigilworld ps";
  shaderCI.EntryPoint = "PSMain";
  device->CreateShader(shaderCI, &ps);

  if (!vs || !ps) {
    if (error)
      *error = "shader compilation failed";
    return false;
  }

  BufferDesc cbDesc;
  cbDesc.Name = "sigilworld frame cb";
  cbDesc.Size = sizeof(FrameConstants);
  cbDesc.Usage = USAGE_DYNAMIC;
  cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
  cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  device->CreateBuffer(cbDesc, nullptr, &frameCB);
  cbDesc.Name = "sigilworld draw cb";
  cbDesc.Size = sizeof(DrawConstants);
  device->CreateBuffer(cbDesc, nullptr, &drawCB);
  if (!frameCB || !drawCB) {
    if (error)
      *error = "constant buffer creation failed";
    return false;
  }

  GraphicsPipelineStateCreateInfo psoCI;
  psoCI.PSODesc.Name = "sigilworld opaque";
  auto &gp = psoCI.GraphicsPipeline;
  gp.NumRenderTargets = 1;
  gp.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
  gp.DSVFormat = TEX_FORMAT_D32_FLOAT;
  gp.SmplDesc.Count = (Uint8)sampleCount;
  gp.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  gp.RasterizerDesc.CullMode = CULL_MODE_NONE; // two-sided panels
  gp.DepthStencilDesc.DepthEnable = True;
  gp.DepthStencilDesc.DepthWriteEnable = True;

  LayoutElement layout[] = {
      {0, 0, 3, VT_FLOAT32, False}, // position
      {1, 0, 3, VT_FLOAT32, False}, // normal
      {2, 0, 2, VT_FLOAT32, False}, // uv
  };
  gp.InputLayout.LayoutElements = layout;
  gp.InputLayout.NumElements = 3;

  psoCI.pVS = vs;
  psoCI.pPS = ps;

  ShaderResourceVariableDesc variables[] = {
      {SHADER_TYPE_PIXEL, "g_Texture",
       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
  };
  psoCI.PSODesc.ResourceLayout.Variables = variables;
  psoCI.PSODesc.ResourceLayout.NumVariables = 1;

  SamplerDesc samplerDesc;
  samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
  samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
  samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
  samplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
  samplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
  ImmutableSamplerDesc samplers[] = {
      {SHADER_TYPE_PIXEL, "g_Texture", samplerDesc},
  };
  psoCI.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
  psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 1;

  device->CreateGraphicsPipelineState(psoCI, &opaquePso);

  psoCI.PSODesc.Name = "sigilworld blended";
  auto &rt0 = gp.BlendDesc.RenderTargets[0];
  rt0.BlendEnable = True;
  rt0.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
  rt0.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
  rt0.SrcBlendAlpha = BLEND_FACTOR_ONE;
  rt0.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
  gp.DepthStencilDesc.DepthWriteEnable = False;
  device->CreateGraphicsPipelineState(psoCI, &blendPso);

  if (!opaquePso || !blendPso) {
    if (error)
      *error = "pipeline creation failed";
    return false;
  }

  for (IPipelineState *pso :
       {opaquePso.RawPtr(), blendPso.RawPtr()}) {
    for (SHADER_TYPE stage : {SHADER_TYPE_VERTEX, SHADER_TYPE_PIXEL}) {
      if (auto *var = pso->GetStaticVariableByName(stage, "FrameConstants"))
        var->Set(frameCB);
      if (auto *var = pso->GetStaticVariableByName(stage, "DrawConstants"))
        var->Set(drawCB);
    }
  }

  // 1x1 white fallback so untextured materials sample identity.
  {
    TextureDesc desc;
    desc.Name = "sigilworld white";
    desc.Type = RESOURCE_DIM_TEX_2D;
    desc.Width = 1;
    desc.Height = 1;
    desc.Format = TEX_FORMAT_RGBA8_UNORM;
    desc.BindFlags = BIND_SHADER_RESOURCE;
    desc.MipLevels = 1;
    const Uint32 white = 0xffffffffu;
    TextureSubResData subres{&white, 4};
    TextureData data{&subres, 1};
    device->CreateTexture(desc, &data, &whiteTexture);
  }
  return whiteTexture != nullptr;
}

dg::RefCntAutoPtr<dg::ITexture>
World::Impl::uploadTexture(const sk_sp<SkImage> &image) {
  using namespace dg;
  if (!image)
    return whiteTexture;
  const int w = image->width(), h = image->height();
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(w, h, kRGBA_8888_SkColorType,
                                       kUnpremul_SkAlphaType));
  if (!image->readPixels(nullptr, bitmap.pixmap(), 0, 0))
    return whiteTexture;

  TextureDesc desc;
  desc.Name = "sigilworld surface texture";
  desc.Type = RESOURCE_DIM_TEX_2D;
  desc.Width = (Uint32)w;
  desc.Height = (Uint32)h;
  // UI content is authored sRGB; the sRGB view linearizes on sample.
  desc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
  desc.BindFlags = BIND_SHADER_RESOURCE;
  desc.MipLevels = 1;
  TextureSubResData subres{bitmap.getPixels(),
                           (Uint64)bitmap.rowBytes()};
  TextureData data{&subres, 1};
  RefCntAutoPtr<ITexture> texture;
  device->CreateTexture(desc, &data, &texture);
  return texture ? texture : whiteTexture;
}

void World::Impl::writeDrawConstants(const SkM44 &model,
                                     const Material &m) {
  using namespace dg;
  MapHelper<DrawConstants> constants(context, drawCB, MAP_WRITE,
                                     MAP_FLAG_DISCARD);
  constants->model = colMajor(model);
  constants->normalMat = normalMatrix(model);
  constants->baseColor[0] = m.baseColor.fR;
  constants->baseColor[1] = m.baseColor.fG;
  constants->baseColor[2] = m.baseColor.fB;
  constants->baseColor[3] = m.baseColor.fA;
  constants->emissive[0] = m.emissive.fR;
  constants->emissive[1] = m.emissive.fG;
  constants->emissive[2] = m.emissive.fB;
  constants->emissive[3] = 1;
  constants->matParams[0] = m.metallic;
  constants->matParams[1] = m.roughness;
  constants->matParams[2] = m.emissiveStrength;
  constants->matParams[3] = m.unlit ? 1.0f : 0.0f;
}

// ---------------------------------------------------------------------------

World::World() : m_impl(std::make_unique<Impl>()) {}
World::~World() = default;

std::unique_ptr<World> World::create(const WorldConfig &config,
                                     std::string *error) {
  std::unique_ptr<World> world(new World());
  world->m_impl->config = config;
  if (!world->m_impl->init(error))
    return nullptr;
  return world;
}

uint32_t World::addSurface(const shape::Mesh &mesh, const SkM44 &model,
                           const Material &material) {
  using namespace dg;
  Impl &impl = *m_impl;
  if (mesh.positions.empty() || mesh.indices.empty())
    return 0;

  std::vector<Vertex> vertices(mesh.positions.size());
  for (size_t i = 0; i < mesh.positions.size(); ++i) {
    Vertex &v = vertices[i];
    v.pos[0] = mesh.positions[i].x;
    v.pos[1] = mesh.positions[i].y;
    v.pos[2] = mesh.positions[i].z;
    const SkV3 n =
        i < mesh.normals.size() ? mesh.normals[i] : SkV3{0, 0, 1};
    v.normal[0] = n.x;
    v.normal[1] = n.y;
    v.normal[2] = n.z;
    const SkPoint uv =
        i < mesh.uvs.size() ? mesh.uvs[i] : SkPoint{0, 0};
    v.uv[0] = uv.fX;
    v.uv[1] = uv.fY;
  }

  GpuGeometry geometry;
  geometry.indexCount = (uint32_t)mesh.indices.size();

  BufferDesc vbDesc;
  vbDesc.Name = "sigilworld vertices";
  vbDesc.Usage = USAGE_IMMUTABLE;
  vbDesc.BindFlags = BIND_VERTEX_BUFFER;
  vbDesc.Size = (Uint64)(vertices.size() * sizeof(Vertex));
  BufferData vbData{vertices.data(), vbDesc.Size};
  impl.device->CreateBuffer(vbDesc, &vbData, &geometry.vertexBuffer);

  BufferDesc ibDesc;
  ibDesc.Name = "sigilworld indices";
  ibDesc.Usage = USAGE_IMMUTABLE;
  ibDesc.BindFlags = BIND_INDEX_BUFFER;
  ibDesc.Size = (Uint64)(mesh.indices.size() * sizeof(uint32_t));
  BufferData ibData{mesh.indices.data(), ibDesc.Size};
  impl.device->CreateBuffer(ibDesc, &ibData, &geometry.indexBuffer);

  if (!geometry.vertexBuffer || !geometry.indexBuffer)
    return 0;

  dg::RefCntAutoPtr<dg::ITexture> texture =
      impl.uploadTexture(material.texture);
  impl.opaquePso->CreateShaderResourceBinding(&geometry.srb, true);
  if (!geometry.srb)
    return 0;
  if (auto *var = geometry.srb->GetVariableByName(SHADER_TYPE_PIXEL,
                                                  "g_Texture"))
    var->Set(texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

  const entt::entity id = impl.registry.create();
  impl.registry.emplace<GpuGeometry>(id, std::move(geometry));
  impl.registry.emplace<TransformComponent>(id, model);
  impl.registry.emplace<MaterialComponent>(id, material);
  return (uint32_t)id;
}

void World::setTransform(uint32_t id, const SkM44 &model) {
  entt::registry &registry = m_impl->registry;
  const entt::entity e = entity(id);
  if (registry.valid(e) && registry.all_of<TransformComponent>(e))
    registry.get<TransformComponent>(e).model = model;
}

void World::removeSurface(uint32_t id) {
  entt::registry &registry = m_impl->registry;
  const entt::entity e = entity(id);
  if (registry.valid(e) && registry.all_of<GpuGeometry>(e))
    registry.destroy(e);
}

size_t World::surfaceCount() const {
  return m_impl->registry.view<GpuGeometry>().size();
}

entt::registry &World::registry() {
  return m_impl->registry;
}

const entt::registry &World::registry() const {
  return m_impl->registry;
}

void World::setCamera(const shape::space::Camera &camera) {
  m_impl->camera = camera;
}

void World::setLighting(const Lighting &lighting) {
  m_impl->lighting = lighting;
}

bool World::render() {
  using namespace dg;
  Impl &impl = *m_impl;
  if (!impl.context)
    return false;

  const bool msaa = impl.sampleCount > 1;
  ITextureView *rtv = (msaa ? impl.colorTarget : impl.resolveTarget)
                          ->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
  ITextureView *dsv =
      impl.depthTarget->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
  impl.context->SetRenderTargets(1, &rtv, dsv,
                                 RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float clear[4] = {impl.config.clearColor.fR,
                          impl.config.clearColor.fG,
                          impl.config.clearColor.fB,
                          impl.config.clearColor.fA};
  impl.context->ClearRenderTarget(rtv, clear,
                                  RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  impl.context->ClearDepthStencil(dsv, CLEAR_DEPTH_FLAG, 1.0f, 0,
                                  RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Frame constants.
  {
    const shape::space::Camera &cam = impl.camera;
    const float aspect =
        impl.config.height > 0
            ? (float)impl.config.width / (float)impl.config.height
            : 1.0f;
    // Column-vector chain: clip = proj * view * model.
    SkM44 viewProj =
        perspectiveVk(cam.fovYDeg, aspect, cam.zNear, cam.zFar);
    viewProj.preConcat(cam.view());

    MapHelper<FrameConstants> constants(impl.context, impl.frameCB,
                                        MAP_WRITE, MAP_FLAG_DISCARD);
    constants->viewProj = colMajor(viewProj);
    constants->camPos[0] = cam.eye.x;
    constants->camPos[1] = cam.eye.y;
    constants->camPos[2] = cam.eye.z;
    constants->camPos[3] = 1;
    SkV3 sunDir = impl.lighting.sunDirection;
    const float len = sunDir.length();
    if (len > 1e-6f)
      sunDir = sunDir * (1.0f / len);
    constants->sunDir[0] = sunDir.x;
    constants->sunDir[1] = sunDir.y;
    constants->sunDir[2] = sunDir.z;
    constants->sunDir[3] = impl.lighting.sunIntensity;
    auto putColor = [](float *dst, const SkColor4f &c, float alpha) {
      dst[0] = c.fR;
      dst[1] = c.fG;
      dst[2] = c.fB;
      dst[3] = alpha;
    };
    putColor(constants->sunColor, impl.lighting.sunColor, 1);
    putColor(constants->skyColor, impl.lighting.skyColor, 1);
    putColor(constants->groundColor, impl.lighting.groundColor, 1);
    constants->params[0] = impl.lighting.ambient;
    constants->params[1] = constants->params[2] = constants->params[3] = 0;
  }

  // Gather from the registry: opaque then blended (back-to-front by
  // view depth). The alpha test reads the LIVE MaterialComponent, so
  // mutating alpha through registry() re-routes the pass correctly.
  struct DrawItem {
    const GpuGeometry *geometry;
    const SkM44 *model;
    const Material *material;
  };
  std::vector<DrawItem> opaque, blended;
  for (auto [e, geometry, transform, material] :
       impl.registry
           .view<GpuGeometry, TransformComponent, MaterialComponent>()
           .each()) {
    const DrawItem item{&geometry, &transform.model,
                        &material.material};
    (material.material.baseColor.fA < 1.0f ? blended : opaque)
        .push_back(item);
  }
  if (!blended.empty()) {
    const SkM44 view = impl.camera.view();
    auto viewZ = [&](const DrawItem &item) {
      const SkV4 origin = (view * *item.model) * SkV4{0, 0, 0, 1};
      return origin.z;
    };
    std::sort(blended.begin(), blended.end(),
              [&](const DrawItem &a, const DrawItem &b) {
                return viewZ(a) < viewZ(b);
              });
  }

  auto drawList = [&](const std::vector<DrawItem> &list,
                      IPipelineState *pso) {
    if (list.empty())
      return;
    impl.context->SetPipelineState(pso);
    for (const DrawItem &item : list) {
      impl.writeDrawConstants(*item.model, *item.material);
      impl.context->CommitShaderResources(
          item.geometry->srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      IBuffer *vb = item.geometry->vertexBuffer;
      const Uint64 offset = 0;
      impl.context->SetVertexBuffers(
          0, 1, &vb, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
          SET_VERTEX_BUFFERS_FLAG_RESET);
      impl.context->SetIndexBuffer(
          item.geometry->indexBuffer, 0,
          RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      DrawIndexedAttribs attribs;
      attribs.NumIndices = item.geometry->indexCount;
      attribs.IndexType = VT_UINT32;
      attribs.Flags = DRAW_FLAG_VERIFY_ALL;
      impl.context->DrawIndexed(attribs);
    }
  };
  drawList(opaque, impl.opaquePso);
  drawList(blended, impl.blendPso);

  if (msaa) {
    ResolveTextureSubresourceAttribs resolve;
    resolve.SrcTextureTransitionMode =
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    resolve.DstTextureTransitionMode =
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    impl.context->ResolveTextureSubresource(impl.colorTarget,
                                            impl.resolveTarget, resolve);
  }
  impl.context->Flush();
  impl.rendered = true;
  return true;
}

sk_sp<SkImage> World::readback() {
  using namespace dg;
  Impl &impl = *m_impl;
  if (!impl.rendered)
    return nullptr;

  CopyTextureAttribs copy;
  copy.pSrcTexture = impl.resolveTarget;
  copy.pDstTexture = impl.stagingTarget;
  copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  impl.context->CopyTexture(copy);
  impl.context->WaitForIdle();

  MappedTextureSubresource mapped;
  impl.context->MapTextureSubresource(impl.stagingTarget, 0, 0, MAP_READ,
                                      MAP_FLAG_DO_NOT_WAIT, nullptr,
                                      mapped);
  if (!mapped.pData)
    return nullptr;

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(impl.config.width,
                                       impl.config.height,
                                       kRGBA_8888_SkColorType,
                                       kOpaque_SkAlphaType));
  const uint8_t *src = (const uint8_t *)mapped.pData;
  for (int y = 0; y < impl.config.height; ++y)
    std::memcpy(bitmap.getAddr32(0, y), src + (size_t)y * mapped.Stride,
                (size_t)impl.config.width * 4);
  impl.context->UnmapTextureSubresource(impl.stagingTarget, 0, 0);
  bitmap.setImmutable();
  return bitmap.asImage();
}

bool World::savePng(const std::filesystem::path &path) {
  sk_sp<SkImage> image = readback();
  if (!image)
    return false;
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(image->width(), image->height(),
                                       kRGBA_8888_SkColorType,
                                       kOpaque_SkAlphaType));
  if (!image->readPixels(nullptr, bitmap.pixmap(), 0, 0))
    return false;
  SkFILEWStream stream(path.string().c_str());
  return stream.isValid() &&
         SkPngEncoder::Encode(&stream, bitmap.pixmap(), {});
}

const char *World::backendName() const {
  return m_impl->device ? "Vulkan" : "none";
}

} // namespace sigil::world
