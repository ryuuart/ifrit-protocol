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
    float4 g_Params;      // x ambient, y light count
    float4 g_LightPos[8];   // xyz position (point) / direction (dir), w 1=point
    float4 g_LightColor[8]; // rgb color * intensity, w range (point)
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

// The instanced stream rides buffer slot 1: a 3x4 point transform
// (basis * scale | position) plus a tint, per instance.
struct VSInstIn
{
    float3 Pos    : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV     : ATTRIB2;
    float4 Row0   : ATTRIB3;
    float4 Row1   : ATTRIB4;
    float4 Row2   : ATTRIB5;
    float4 Tint   : ATTRIB6;
};

struct PSIn
{
    float4 Pos    : SV_POSITION;
    float3 World  : WORLDPOS;
    float3 Normal : NORMALX;
    float2 UV     : TEXCOORD0;
    float4 Tint   : COLOR0;
};

void VSMain(in VSIn IN, out PSIn OUT)
{
    float4 world = mul(g_Model, float4(IN.Pos, 1.0));
    OUT.World  = world.xyz;
    OUT.Pos    = mul(g_ViewProj, world);
    OUT.Normal = normalize(mul(g_NormalMat, float4(IN.Normal, 0.0)).xyz);
    OUT.UV     = IN.UV;
    OUT.Tint   = float4(1.0, 1.0, 1.0, 1.0);
}

void VSInstanced(in VSInstIn IN, out PSIn OUT)
{
    float3 local = float3(dot(IN.Row0.xyz, IN.Pos) + IN.Row0.w,
                          dot(IN.Row1.xyz, IN.Pos) + IN.Row1.w,
                          dot(IN.Row2.xyz, IN.Pos) + IN.Row2.w);
    float4 world = mul(g_Model, float4(local, 1.0));
    OUT.World = world.xyz;
    OUT.Pos   = mul(g_ViewProj, world);
    // Uniform per-instance scale: rotate the normal by the same basis,
    // normalize the scale away.
    float3 nrm = float3(dot(IN.Row0.xyz, IN.Normal),
                        dot(IN.Row1.xyz, IN.Normal),
                        dot(IN.Row2.xyz, IN.Normal));
    OUT.Normal = normalize(mul(g_NormalMat, float4(nrm, 0.0)).xyz);
    OUT.UV   = IN.UV;
    OUT.Tint = IN.Tint;
}

float4 PSMain(in PSIn IN) : SV_TARGET
{
    float4 tex  = g_Texture.Sample(g_Texture_sampler, IN.UV);
    float4 base = tex * g_BaseColor * IN.Tint;

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

    // Registry lights: the same GGX lobe per light, point lights with a
    // windowed falloff — (1 - (d/range)^2)^2 — so intensity stays in
    // the sun's 1..5 ballpark instead of inverse-square thousands.
    int lightCount = (int)(g_Params.y + 0.5);
    for (int i = 0; i < lightCount; ++i)
    {
        float3 Li;
        float  atten = 1.0;
        if (g_LightPos[i].w > 0.5)
        {
            float3 toL  = g_LightPos[i].xyz - IN.World;
            float  dist = max(length(toL), 1e-4);
            Li = toL / dist;
            float x = saturate(dist / max(g_LightColor[i].w, 1e-3));
            float win = 1.0 - x * x;
            atten = win * win;
        }
        else
        {
            Li = normalize(-g_LightPos[i].xyz);
        }
        float ndli = saturate(dot(N, Li));
        if (ndli * atten <= 0.0)
            continue;
        float3 Hi   = normalize(Li + V);
        float  ndhi = saturate(dot(N, Hi));
        float  hdvi = saturate(dot(Hi, V));
        float  ddi  = ndhi * ndhi * (a2 - 1.0) + 1.0;
        float  Di   = a2 / max(3.1415926 * ddi * ddi, 1e-4);
        float3 Fi   = F0 + (1.0 - F0) * pow(1.0 - hdvi, 5.0);
        float  Visi = 0.5 / max(lerp(2.0 * ndli * ndv, ndli + ndv, a), 1e-4);
        direct += (albedo / 3.1415926 + Di * Fi * Visi) *
                  g_LightColor[i].rgb * (atten * ndli);
    }

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
  float params[4];                  // x ambient, y light count
  float lightPos[kLightBudget][4];  // xyz pos/dir, w 1=point
  float lightColor[kLightBudget][4]; // rgb premultiplied intensity, w range
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

/** The per-instance vertex stream: 3x4 transform rows + tint —
 *  matches VSInstIn's ATTRIB3..6. */
struct InstanceAttribs {
  float row0[4];
  float row1[4];
  float row2[4];
  float tint[4];
};

/** Orientation basis for a point's direction — the same construction
 *  points::instance() uses (Points.cpp basisFor), so a Cloud renders
 *  identically merged or instanced. */
void basisFor(SkV3 dir, SkV3 up, SkV3 *x, SkV3 *y, SkV3 *z) {
  const float len = dir.length();
  *z = len > 1e-6f ? dir * (1.0f / len) : SkV3{0, 0, 1};
  SkV3 side = SkV3::Cross(up, *z);
  if (side.lengthSquared() < 1e-8f)
    side = SkV3::Cross(SkV3{1, 0, 0}, *z);
  *x = side.normalize();
  *y = SkV3::Cross(*z, *x);
}

std::vector<InstanceAttribs> buildInstances(const shape::Cloud &cloud,
                                            const InstanceLanes &lanes) {
  const std::vector<float> *scaleLane =
      lanes.scaleLane.empty() ? nullptr : cloud.scalarIf(lanes.scaleLane);
  const std::vector<SkColor4f> *tintLane =
      lanes.tintLane.empty() ? nullptr : cloud.colorIf(lanes.tintLane);
  const std::vector<SkV3> *orientLane =
      lanes.orientLane.empty() ? nullptr : cloud.vectorIf(lanes.orientLane);

  std::vector<InstanceAttribs> out(cloud.size());
  for (size_t i = 0; i < cloud.size(); ++i) {
    const float s =
        lanes.scale *
        (scaleLane && i < scaleLane->size() ? (*scaleLane)[i] : 1.0f);
    SkV3 bx{1, 0, 0}, by{0, 1, 0}, bz{0, 0, 1};
    if (orientLane && i < orientLane->size())
      basisFor((*orientLane)[i], lanes.up, &bx, &by, &bz);
    const SkV3 p = cloud.positions[i];
    InstanceAttribs &a = out[i];
    a.row0[0] = bx.x * s;
    a.row0[1] = by.x * s;
    a.row0[2] = bz.x * s;
    a.row0[3] = p.x;
    a.row1[0] = bx.y * s;
    a.row1[1] = by.y * s;
    a.row1[2] = bz.y * s;
    a.row1[3] = p.y;
    a.row2[0] = bx.z * s;
    a.row2[1] = by.z * s;
    a.row2[2] = bz.z * s;
    a.row2[3] = p.z;
    const SkColor4f tint = tintLane && i < tintLane->size()
                               ? (*tintLane)[i]
                               : SkColor4f{1, 1, 1, 1};
    a.tint[0] = tint.fR;
    a.tint[1] = tint.fG;
    a.tint[2] = tint.fB;
    a.tint[3] = tint.fA;
  }
  return out;
}

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

/** The instanced sibling: one stamp's buffers plus the per-instance
 *  stream (addInstanced). instanceBuffer is null while the flock is
 *  empty; setInstances() refreshes or recreates it. */
struct GpuInstancedGeometry {
  dg::RefCntAutoPtr<dg::IBuffer> vertexBuffer;
  dg::RefCntAutoPtr<dg::IBuffer> indexBuffer;
  dg::RefCntAutoPtr<dg::IBuffer> instanceBuffer;
  dg::RefCntAutoPtr<dg::IShaderResourceBinding> srb;
  uint32_t indexCount = 0;
  uint32_t instanceCount = 0;
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
  dg::RefCntAutoPtr<dg::IPipelineState> opaqueInstancedPso;
  dg::RefCntAutoPtr<dg::IPipelineState> blendInstancedPso;
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
  bool createMeshBuffers(const shape::Mesh &mesh,
                         dg::RefCntAutoPtr<dg::IBuffer> &vertexBuffer,
                         dg::RefCntAutoPtr<dg::IBuffer> &indexBuffer);
  dg::RefCntAutoPtr<dg::IBuffer>
  createInstanceBuffer(const std::vector<InstanceAttribs> &instances);
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

  RefCntAutoPtr<IShader> vsInstanced;
  shaderCI.Desc.Name = "sigilworld vs instanced";
  shaderCI.EntryPoint = "VSInstanced";
  device->CreateShader(shaderCI, &vsInstanced);

  RefCntAutoPtr<IShader> ps;
  shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
  shaderCI.Desc.Name = "sigilworld ps";
  shaderCI.EntryPoint = "PSMain";
  device->CreateShader(shaderCI, &ps);

  if (!vs || !vsInstanced || !ps) {
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
  // The instanced variants add buffer slot 1: transform rows + tint,
  // advancing per instance.
  LayoutElement instancedLayout[] = {
      {0, 0, 3, VT_FLOAT32, False},
      {1, 0, 3, VT_FLOAT32, False},
      {2, 0, 2, VT_FLOAT32, False},
      {3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
      {4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
      {5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
      {6, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
  };

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

  // The four-PSO family: {plain, instanced} x {opaque, blended}. Same
  // PS and resource layout everywhere, so one SRB shape serves all.
  auto makePso = [&](const char *name, bool instanced, bool blend,
                     dg::RefCntAutoPtr<IPipelineState> &out) {
    psoCI.PSODesc.Name = name;
    psoCI.pVS = instanced ? vsInstanced : vs;
    gp.InputLayout.LayoutElements = instanced ? instancedLayout : layout;
    gp.InputLayout.NumElements = instanced ? 7u : 3u;
    auto &rt0 = gp.BlendDesc.RenderTargets[0];
    rt0.BlendEnable = blend ? True : False;
    rt0.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    rt0.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    rt0.SrcBlendAlpha = BLEND_FACTOR_ONE;
    rt0.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    gp.DepthStencilDesc.DepthWriteEnable = blend ? False : True;
    device->CreateGraphicsPipelineState(psoCI, &out);
  };
  makePso("sigilworld opaque", false, false, opaquePso);
  makePso("sigilworld blended", false, true, blendPso);
  makePso("sigilworld opaque instanced", true, false, opaqueInstancedPso);
  makePso("sigilworld blended instanced", true, true, blendInstancedPso);

  if (!opaquePso || !blendPso || !opaqueInstancedPso ||
      !blendInstancedPso) {
    if (error)
      *error = "pipeline creation failed";
    return false;
  }

  for (IPipelineState *pso :
       {opaquePso.RawPtr(), blendPso.RawPtr(),
        opaqueInstancedPso.RawPtr(), blendInstancedPso.RawPtr()}) {
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

bool World::Impl::createMeshBuffers(
    const shape::Mesh &mesh, dg::RefCntAutoPtr<dg::IBuffer> &vertexBuffer,
    dg::RefCntAutoPtr<dg::IBuffer> &indexBuffer) {
  using namespace dg;
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

  BufferDesc vbDesc;
  vbDesc.Name = "sigilworld vertices";
  vbDesc.Usage = USAGE_IMMUTABLE;
  vbDesc.BindFlags = BIND_VERTEX_BUFFER;
  vbDesc.Size = (Uint64)(vertices.size() * sizeof(Vertex));
  BufferData vbData{vertices.data(), vbDesc.Size};
  device->CreateBuffer(vbDesc, &vbData, &vertexBuffer);

  BufferDesc ibDesc;
  ibDesc.Name = "sigilworld indices";
  ibDesc.Usage = USAGE_IMMUTABLE;
  ibDesc.BindFlags = BIND_INDEX_BUFFER;
  ibDesc.Size = (Uint64)(mesh.indices.size() * sizeof(uint32_t));
  BufferData ibData{mesh.indices.data(), ibDesc.Size};
  device->CreateBuffer(ibDesc, &ibData, &indexBuffer);

  return vertexBuffer && indexBuffer;
}

dg::RefCntAutoPtr<dg::IBuffer> World::Impl::createInstanceBuffer(
    const std::vector<InstanceAttribs> &instances) {
  using namespace dg;
  RefCntAutoPtr<IBuffer> buffer;
  if (instances.empty())
    return buffer;
  // DEFAULT so setInstances() can UpdateBuffer in place.
  BufferDesc desc;
  desc.Name = "sigilworld instances";
  desc.Usage = USAGE_DEFAULT;
  desc.BindFlags = BIND_VERTEX_BUFFER;
  desc.Size = (Uint64)(instances.size() * sizeof(InstanceAttribs));
  BufferData data{instances.data(), desc.Size};
  device->CreateBuffer(desc, &data, &buffer);
  return buffer;
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

  GpuGeometry geometry;
  geometry.indexCount = (uint32_t)mesh.indices.size();
  if (!impl.createMeshBuffers(mesh, geometry.vertexBuffer,
                              geometry.indexBuffer))
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

uint32_t World::addInstanced(const shape::Mesh &stamp,
                             const shape::Cloud &cloud,
                             const Material &material,
                             const InstanceLanes &lanes) {
  using namespace dg;
  Impl &impl = *m_impl;
  if (stamp.positions.empty() || stamp.indices.empty())
    return 0;

  GpuInstancedGeometry geometry;
  geometry.indexCount = (uint32_t)stamp.indices.size();
  if (!impl.createMeshBuffers(stamp, geometry.vertexBuffer,
                              geometry.indexBuffer))
    return 0;

  const std::vector<InstanceAttribs> instances =
      buildInstances(cloud, lanes);
  geometry.instanceCount = (uint32_t)instances.size();
  geometry.instanceBuffer = impl.createInstanceBuffer(instances);
  if (geometry.instanceCount > 0 && !geometry.instanceBuffer)
    return 0;

  dg::RefCntAutoPtr<dg::ITexture> texture =
      impl.uploadTexture(material.texture);
  impl.opaqueInstancedPso->CreateShaderResourceBinding(&geometry.srb,
                                                       true);
  if (!geometry.srb)
    return 0;
  if (auto *var = geometry.srb->GetVariableByName(SHADER_TYPE_PIXEL,
                                                  "g_Texture"))
    var->Set(texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

  const entt::entity id = impl.registry.create();
  impl.registry.emplace<GpuInstancedGeometry>(id, std::move(geometry));
  // Whole-flock transform starts at identity — setTransform moves the
  // flock as one.
  impl.registry.emplace<TransformComponent>(id, SkM44());
  impl.registry.emplace<MaterialComponent>(id, material);
  return (uint32_t)id;
}

void World::setInstances(uint32_t id, const shape::Cloud &cloud,
                         const InstanceLanes &lanes) {
  using namespace dg;
  Impl &impl = *m_impl;
  const entt::entity e = entity(id);
  if (!impl.registry.valid(e) ||
      !impl.registry.all_of<GpuInstancedGeometry>(e))
    return;
  GpuInstancedGeometry &geometry =
      impl.registry.get<GpuInstancedGeometry>(e);

  const std::vector<InstanceAttribs> instances =
      buildInstances(cloud, lanes);
  if (instances.size() == geometry.instanceCount &&
      geometry.instanceBuffer) {
    impl.context->UpdateBuffer(
        geometry.instanceBuffer, 0,
        (Uint64)(instances.size() * sizeof(InstanceAttribs)),
        instances.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  } else {
    geometry.instanceBuffer = impl.createInstanceBuffer(instances);
    geometry.instanceCount = (uint32_t)instances.size();
  }
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
  if (registry.valid(e) &&
      registry.any_of<GpuGeometry, GpuInstancedGeometry>(e))
    registry.destroy(e);
}

size_t World::surfaceCount() const {
  return m_impl->registry.view<GpuGeometry>().size() +
         m_impl->registry.view<GpuInstancedGeometry>().size();
}

uint32_t World::addLight(const LightComponent &light) {
  entt::registry &registry = m_impl->registry;
  const entt::entity id = registry.create();
  registry.emplace<LightComponent>(id, light);
  return (uint32_t)id;
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

  // Camera precedence: an ACTIVE CameraComponent entity overrides
  // setCamera; with none, the fallback camera drives the frame.
  shape::space::Camera cam = impl.camera;
  for (auto [e, camComponent] :
       impl.registry.view<CameraComponent>().each()) {
    if (camComponent.active) {
      cam = camComponent.camera;
      break;
    }
  }

  // Frame constants.
  {
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

    // Registry lights, first kLightBudget the view yields.
    int lightCount = 0;
    for (auto [e, light] : impl.registry.view<LightComponent>().each()) {
      if (lightCount >= kLightBudget)
        break;
      float *pos = constants->lightPos[lightCount];
      float *color = constants->lightColor[lightCount];
      if (light.type == LightComponent::Type::Point) {
        pos[0] = light.position.x;
        pos[1] = light.position.y;
        pos[2] = light.position.z;
        pos[3] = 1;
      } else {
        pos[0] = light.direction.x;
        pos[1] = light.direction.y;
        pos[2] = light.direction.z;
        pos[3] = 0;
      }
      color[0] = light.color.fR * light.intensity;
      color[1] = light.color.fG * light.intensity;
      color[2] = light.color.fB * light.intensity;
      color[3] = light.range;
      ++lightCount;
    }
    constants->params[1] = (float)lightCount;
  }

  // Gather from the registry: opaque then blended (back-to-front by
  // view depth). The alpha test reads the LIVE MaterialComponent, so
  // mutating alpha through registry() re-routes the pass correctly.
  // Instanced flocks route by their material alpha as a whole.
  struct DrawItem {
    const GpuGeometry *geometry = nullptr;
    const GpuInstancedGeometry *instanced = nullptr;
    const SkM44 *model = nullptr;
    const Material *material = nullptr;
  };
  std::vector<DrawItem> opaque, blended;
  for (auto [e, geometry, transform, material] :
       impl.registry
           .view<GpuGeometry, TransformComponent, MaterialComponent>()
           .each()) {
    DrawItem item;
    item.geometry = &geometry;
    item.model = &transform.model;
    item.material = &material.material;
    (material.material.baseColor.fA < 1.0f ? blended : opaque)
        .push_back(item);
  }
  for (auto [e, geometry, transform, material] :
       impl.registry
           .view<GpuInstancedGeometry, TransformComponent,
                 MaterialComponent>()
           .each()) {
    if (geometry.instanceCount == 0)
      continue;
    DrawItem item;
    item.instanced = &geometry;
    item.model = &transform.model;
    item.material = &material.material;
    (material.material.baseColor.fA < 1.0f ? blended : opaque)
        .push_back(item);
  }
  if (!blended.empty()) {
    const SkM44 view = cam.view();
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
                      IPipelineState *plainPso,
                      IPipelineState *instancedPso) {
    IPipelineState *bound = nullptr;
    for (const DrawItem &item : list) {
      IPipelineState *pso = item.instanced ? instancedPso : plainPso;
      if (pso != bound) {
        impl.context->SetPipelineState(pso);
        bound = pso;
      }
      impl.writeDrawConstants(*item.model, *item.material);
      DrawIndexedAttribs attribs;
      attribs.IndexType = VT_UINT32;
      attribs.Flags = DRAW_FLAG_VERIFY_ALL;
      if (item.instanced) {
        const GpuInstancedGeometry &geometry = *item.instanced;
        impl.context->CommitShaderResources(
            geometry.srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        IBuffer *buffers[] = {geometry.vertexBuffer,
                              geometry.instanceBuffer};
        const Uint64 offsets[] = {0, 0};
        impl.context->SetVertexBuffers(
            0, 2, buffers, offsets,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            SET_VERTEX_BUFFERS_FLAG_RESET);
        impl.context->SetIndexBuffer(
            geometry.indexBuffer, 0,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        attribs.NumIndices = geometry.indexCount;
        attribs.NumInstances = geometry.instanceCount;
      } else {
        const GpuGeometry &geometry = *item.geometry;
        impl.context->CommitShaderResources(
            geometry.srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        IBuffer *vb = geometry.vertexBuffer;
        const Uint64 offset = 0;
        impl.context->SetVertexBuffers(
            0, 1, &vb, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            SET_VERTEX_BUFFERS_FLAG_RESET);
        impl.context->SetIndexBuffer(
            geometry.indexBuffer, 0,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        attribs.NumIndices = geometry.indexCount;
      }
      impl.context->DrawIndexed(attribs);
    }
  };
  drawList(opaque, impl.opaquePso, impl.opaqueInstancedPso);
  drawList(blended, impl.blendPso, impl.blendInstancedPso);

  // A zero-draw frame never begins a render pass, so the Vulkan
  // backend keeps the clears deferred until the next flush — flush
  // BEFORE the resolve or it republishes the previous frame.
  if (opaque.empty() && blended.empty())
    impl.context->Flush();

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
