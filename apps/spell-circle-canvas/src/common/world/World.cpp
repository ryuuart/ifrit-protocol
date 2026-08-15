#include "sigilworld/World.h"

#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <sigilshape/detail/VecMath.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <Graphics/GraphicsTools/interface/MapHelper.hpp>

#include "sigilworld/Animation.h"
#include "sigilworld/Components.h"

#ifdef SIGILWORLD_POP_SPIRV
#include <glm/gtc/type_ptr.hpp>

#include "shaders/WorldShaderParams.h"
#include "world_pop_spirv.h"

namespace {
/** The compiled compute kernel for @p entry; null when absent. */
const unsigned char* findSpirv(const char* entry, size_t* size) {
  for (const auto& blob : kWorldPopSpirv)
    if (std::strcmp(blob.name, entry) == 0) {
      *size = blob.size;
      return blob.data;
    }
  *size = 0;
  return nullptr;
}
}  // namespace
#endif

#ifndef SIGILWORLD_POP_SPIRV
#include <include/core/SkTypes.h>  // SkDebugf — the missing-kernels diagnostic

namespace {
/** Announces once per process that the compute generators cannot run.
 *  Every generator entry point fails the same way, so one line covers
 *  all of them. */
void warnComputeKernelsUnavailable() {
  static bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "[world] compute kernels unavailable: this build was configured "
      "without slangc, so no SPIR-V compute shaders were embedded. "
      "addSweep, addFlock, addPoints and addPointsOn return 0; surface "
      "rendering is unaffected.\n");
}
}  // namespace
#endif

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
// MATRIX UPLOAD CONVENTION. The shaders use column-vector math —
// mul(M, v) — with default cbuffer packing, which reads column-major
// memory. glm stores column-major natively, so every matrix upload is a
// raw memcpy and there is no transpose anywhere in this file. Keep it
// that way: introducing one transpose means rechecking all of them, and
// the HLSL row_major annotations that would otherwise be needed are
// unevenly supported across shader translators.

struct Mat4 {
  float m[16];
};

Mat4 colMajor(const glm::mat4& src) {
  Mat4 out;
  std::memcpy(out.m, glm::value_ptr(src), sizeof(out.m));
  return out;
}

/** Perspective projection as a column-vector matrix: RIGHT-HANDED, y-up
 *  eye space, depth mapped to 0..1.
 *
 *  There is deliberately NO Vulkan clip-y flip. The backend normalizes to
 *  the GL/D3D convention internally, so +y up in clip space is already
 *  correct and adding a flip here would render the scene upside down. */
glm::mat4 perspectiveVk(float fovYDeg, float aspect, float zNear, float zFar) {
  const float f = 1.0f / std::tan(fovYDeg * (float)M_PI / 360.0f);
  glm::mat4 m(0.0f);  // glm indexes [column][row]
  m[0][0] = f / aspect;
  m[1][1] = f;
  m[2][2] = zFar / (zNear - zFar);
  m[3][2] = zNear * zFar / (zNear - zFar);
  m[2][3] = -1;
  return m;
}

/** Classic normal matrix: (M^-1)^T in column-major memory. */
Mat4 normalMatrix(const glm::mat4& model) {
  const float det = glm::determinant(model);
  const glm::mat4 inv =
      std::abs(det) < 1e-12f ? glm::mat4(1.0f) : glm::inverse(model);
  return colMajor(glm::transpose(inv));
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
    float4 g_UvScaleOffset; // xy uv scale, zw uv offset
    float4 g_MapParams;   // x normalScale, y occlusionStrength, z 1 = DirectX normal
    float4 g_Channels;    // x roughness, y metallic, z occlusion channel index
};

// The texture set. Every slot is bound — a 1x1 white (or flat-normal)
// stand-in where the material has no map — so the shader multiplies
// unconditionally and never branches on presence.
Texture2D    g_Texture;
SamplerState g_Texture_sampler;
Texture2D    g_NormalMap;
SamplerState g_NormalMap_sampler;
Texture2D    g_RoughnessMap;
SamplerState g_RoughnessMap_sampler;
Texture2D    g_MetallicMap;
SamplerState g_MetallicMap_sampler;
Texture2D    g_OcclusionMap;
SamplerState g_OcclusionMap_sampler;
Texture2D    g_EmissiveMap;
SamplerState g_EmissiveMap_sampler;

float pick(float4 v, float channel)
{
    int c = (int)(channel + 0.5);
    return c == 0 ? v.x : (c == 1 ? v.y : (c == 2 ? v.z : v.w));
}

// The tangent frame from screen-space derivatives — no vertex tangents.
// T follows increasing u, B increasing v; with v running DOWN the image
// (uv origin top-left) an OpenGL-convention map's green axis is -B, and a
// DirectX map's is +B.
float3 perturbNormal(float3 N, float3 P, float2 uv, float3 mapN, float dx)
{
    // Solve the 2x2 for dP/du and dP/dv outright rather than through
    // the cross-product shortcut: the direct solve is invariant to the
    // handedness of the screen-space derivative pair, so the frame's
    // signs do not depend on which way the backend's ddy points.
    float3 dp1 = ddx(P);
    float3 dp2 = ddy(P);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    float det = duv1.x * duv2.y - duv2.x * duv1.y;
    if (abs(det) < 1e-12)
        return N;  // no uv gradient here: keep the geometric normal
    float3 dPdu = (dp1 * duv2.y - dp2 * duv1.y) / det;
    float3 dPdv = (dp2 * duv1.x - dp1 * duv2.x) / det;
    float3 T = dPdu - N * dot(N, dPdu);
    float3 B = dPdv - N * dot(N, dPdv);
    float tl = length(T), bl = length(B);
    if (tl < 1e-12 || bl < 1e-12)
        return N;
    T /= tl;
    B /= bl;
    float3 up = dx > 0.5 ? B : -B;
    return normalize(T * mapN.x + up * mapN.y + N * mapN.z);
}

struct VSIn
{
    float3 Pos    : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV     : ATTRIB2;
    float4 Color  : ATTRIB3; // baked mesh color lane, white default
};

// The instanced stream rides buffer slot 1: a 3x4 point transform
// (basis * scale | position) plus a tint, per instance.
struct VSInstIn
{
    float3 Pos    : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV     : ATTRIB2;
    float4 Color  : ATTRIB3;
    float4 Row0   : ATTRIB4;
    float4 Row1   : ATTRIB5;
    float4 Row2   : ATTRIB6;
    float4 Tint   : ATTRIB7;
    float4 Tex    : ATTRIB8; // uv window: xy offset, zw scale
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
    OUT.Tint   = IN.Color;
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
    OUT.UV   = IN.UV * IN.Tex.zw + IN.Tex.xy;
    OUT.Tint = IN.Color * IN.Tint;
}

// The EXACT inverse of the hardware sRGB decode, and it must stay exact.
//
// Panel textures are created RGBA8_UNORM_SRGB, so the sampler linearizes
// with the piecewise standard sRGB curve. The render target is plain
// RGBA8_UNORM, so the encode is ours to write — and it has to be that
// same piecewise curve, or an unlit panel is not the byte-for-byte
// pass-through it is supposed to be. pow(c, 1/2.2) is NOT that curve: it
// is visibly off through the dark-to-mid range, where the linear toe
// lives. step(c, k) is 1 where k >= c, i.e. where c <= k, which is the
// inclusive branch point the standard specifies.
float3 LinearToSrgb(float3 c)
{
    c = clamp(c, 0.0, 1.0);
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow(c, 1.0 / 2.4) - 0.055;
    return lerp(hi, lo, step(c, 0.0031308));
}

float4 PSMain(in PSIn IN) : SV_TARGET
{
    float2 uv   = IN.UV * g_UvScaleOffset.xy + g_UvScaleOffset.zw;
    float4 tex  = g_Texture.Sample(g_Texture_sampler, uv);
    float4 base = tex * g_BaseColor * IN.Tint;
    float3 emissiveMap = g_EmissiveMap.Sample(g_EmissiveMap_sampler, uv).rgb;
    float3 emissive = g_Emissive.rgb * g_MatParams.z * emissiveMap;

    if (g_MatParams.w > 0.5)
    {
        // Unlit screens: skip lighting and tonemapping, but still
        // re-encode the linearized sample for the UNORM target — the
        // same curve the lit branch ends with, so there is one transfer
        // function for the whole target. With the exact inverse curve
        // this branch is a true pass-through: an untinted texel lands in
        // the readback as its own byte.
        float3 unlit = base.rgb + emissive;
        return float4(LinearToSrgb(unlit), base.a);
    }

    float3 N = normalize(IN.Normal);
    float3 V = normalize(g_CamPos.xyz - IN.World);
    // Culling is off, so every surface is two-sided. Flip a backfacing
    // normal rather than dropping the fragment: winding does not decide
    // visibility here, and a single-sided panel would go black from
    // behind.
    if (dot(N, V) < 0.0)
        N = -N;

    // The normal map: decoded from [0,1] to [-1,1], its tilt scaled by
    // normalScale (0 = flat), applied in the derivative frame.
    float3 mapN = g_NormalMap.Sample(g_NormalMap_sampler, uv).xyz * 2.0 - 1.0;
    mapN.xy *= g_MapParams.x;
    N = perturbNormal(N, IN.World, uv, mapN, g_MapParams.z);

    float  metallic = g_MatParams.x *
        pick(g_MetallicMap.Sample(g_MetallicMap_sampler, uv), g_Channels.y);
    float  rough    = clamp(g_MatParams.y *
        pick(g_RoughnessMap.Sample(g_RoughnessMap_sampler, uv), g_Channels.x),
        0.045, 1.0);
    float  occlusion = lerp(1.0,
        pick(g_OcclusionMap.Sample(g_OcclusionMap_sampler, uv), g_Channels.z),
        g_MapParams.y);
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

    // Registry lights: the same GGX lobe per light, with point lights on
    // a windowed falloff — (1 - (d/range)^2)^2 — rather than a physical
    // inverse square. That keeps authored intensities in the same small
    // range as the sun's instead of running to thousands.
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
    float3 ambient = g_Params.x * occlusion *
        (hemiN * albedo + hemiR * F0 * (1.0 - rough) * (0.4 + 0.6 * pow(1.0 - ndv, 2.0)));

    float3 color = direct + ambient + emissive;

    // Tonemap, then the same sRGB encode the unlit branch uses — one
    // transfer function for the whole target.
    color = color / (color + 1.0);
    return float4(LinearToSrgb(color), base.a);
}
)";

struct FrameConstants {
  Mat4 viewProj;
  float camPos[4];
  float sunDir[4];
  float sunColor[4];
  float skyColor[4];
  float groundColor[4];
  float params[4];                    // x ambient, y light count
  float lightPos[kLightBudget][4];    // xyz pos/dir, w 1=point
  float lightColor[kLightBudget][4];  // rgb premultiplied intensity, w range
};

struct DrawConstants {
  Mat4 model;
  Mat4 normalMat;
  float baseColor[4];
  float emissive[4];
  float matParams[4];
  float uvScaleOffset[4];
  float mapParams[4];
  float channels[4];
};

struct Vertex {
  float pos[3];
  float normal[3];
  float uv[2];
  float color[4];  // baked mesh color lane, white when absent
};

/** The per-instance vertex stream: 3x4 transform rows + tint —
 *  matches VSInstIn's ATTRIB4..8. */
struct InstanceAttribs {
  float row0[4];
  float row1[4];
  float row2[4];
  float tint[4];
  float tex[4];  // uv window: xy offset, zw scale (identity default)
};

// Orientation basis for a point's direction. This is the same function
// the CPU-side instancing uses, shared rather than reimplemented, so a
// Cloud renders identically whether its stamps were merged into one mesh
// or drawn as GPU instances.
using shape::detail::basisFor;

std::vector<InstanceAttribs> buildInstances(const shape::Cloud& cloud,
                                            const InstanceLanes& lanes) {
  const std::vector<float>* scaleLane =
      lanes.scaleLane.empty() ? nullptr : cloud.scalarIf(lanes.scaleLane);
  const std::vector<glm::vec4>* tintLane =
      lanes.tintLane.empty() ? nullptr : cloud.colorIf(lanes.tintLane);
  const std::vector<glm::vec3>* orientLane =
      lanes.orientLane.empty() ? nullptr : cloud.vectorIf(lanes.orientLane);
  const std::vector<glm::vec4>* texLane = cloud.colorIf("Tex");

  std::vector<InstanceAttribs> out(cloud.size());
  for (size_t i = 0; i < cloud.size(); ++i) {
    const float s =
        lanes.scale *
        (scaleLane && i < scaleLane->size() ? (*scaleLane)[i] : 1.0f);
    glm::vec3 bx{1, 0, 0}, by{0, 1, 0}, bz{0, 0, 1};
    if (orientLane && i < orientLane->size())
      basisFor((*orientLane)[i], lanes.up, &bx, &by, &bz);
    const glm::vec3 p = cloud.positions[i];
    InstanceAttribs& a = out[i];
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
    const glm::vec4 tint = tintLane && i < tintLane->size()
                               ? (*tintLane)[i]
                               : glm::vec4{1, 1, 1, 1};
    const glm::vec4 tex =
        texLane && i < texLane->size() ? (*texLane)[i] : glm::vec4{0, 0, 1, 1};
    a.tex[0] = tex.x;
    a.tex[1] = tex.y;
    a.tex[2] = tex.z;
    a.tex[3] = tex.w;
    a.tint[0] = tint.x;
    a.tint[1] = tint.y;
    a.tint[2] = tint.z;
    a.tint[3] = tint.w;
  }
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------

/** The private GPU component: device objects for one surface entity.
 *  Public state (transform, material) lives in the public components —
 *  see Components.h. */
/** Which images (by pointer) and which addressing a surface's shader
 *  resource binding was built from. render() compares this against the
 *  live MaterialComponent and rebinds when a pointer moved, which is
 *  what makes swapping a material's texture live. */
struct MaterialBinding {
  const SkImage* base = nullptr;
  const SkImage* normal = nullptr;
  const SkImage* roughness = nullptr;
  const SkImage* metallic = nullptr;
  const SkImage* occlusion = nullptr;
  const SkImage* emissive = nullptr;
  bool tile = false;
  bool valid = false;  ///< false until the first bind
  static MaterialBinding of(const Material& m) {
    return {m.texture.get(),
            m.normalMap.get(),
            m.roughnessMap.get(),
            m.metallicMap.get(),
            m.occlusionMap.get(),
            m.emissiveMap.get(),
            m.tile,
            true};
  }
  bool operator==(const MaterialBinding&) const = default;
};

struct GpuGeometry {
  dg::RefCntAutoPtr<dg::IBuffer> vertexBuffer;
  dg::RefCntAutoPtr<dg::IBuffer> indexBuffer;
  dg::RefCntAutoPtr<dg::IShaderResourceBinding> srb;
  MaterialBinding bound;
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
  MaterialBinding bound;
  uint32_t indexCount = 0;
  uint32_t instanceCount = 0;
};

/** addSweep()'s private state: the loop resident on the GPU and the
 *  compute binding that lets the sweep kernel rewrite the surface's
 *  vertex buffer in place. `dirty` batches any number of window moves
 *  into one dispatch at the next render(). */
struct SweepComponent {
  dg::RefCntAutoPtr<dg::IBuffer> points;
  dg::RefCntAutoPtr<dg::IShaderResourceBinding> srb;
  float head = 1, span = 1, width = 0;
  int sections = 0;
  int pointCount = 0;
  bool dirty = true;
};

// The parameter blocks come from shaders/WorldShaderParams.h, which is
// the same header the kernels compile against — so the C++ and shader
// views of these structs cannot drift apart.
using SweepParams = shaderparams::SweepParamsData;

/** addFlock()'s private state, the flock counterpart of SweepComponent:
 *  the loop on the GPU plus the compute binding that packs the instanced
 *  draw stream in place. */
struct FlockComponent {
  dg::RefCntAutoPtr<dg::IBuffer> points;
  dg::RefCntAutoPtr<dg::IShaderResourceBinding> srb;
  float head = 1, span = 1;
  float radius = 0, scale = 1;
  float noiseAmplitude = 0, noiseFrequency = 0.01f, seed = 7;
  float tintTail[4] = {1, 1, 1, 1};
  float tintHead[4] = {1, 1, 1, 1};
  int count = 0;
  int pointCount = 0;
  bool dirty = true;
};

using FlockParams = shaderparams::FlockParamsData;

/** addPoints()'s private state: the chain (a value — the
 *  nondestructive description), the GPU attribute lanes it cooks into,
 *  and one shader resource binding per operator. The bindings cannot be
 *  shared: they are pipeline-specific, and dead-resource elimination
 *  makes each operator's layout differ. */
struct PopComponent {
  World::pop::Chain chain;
  dg::RefCntAutoPtr<dg::IBuffer> loop;
  /** The lane ARENA: slotCount lanes of count float4s each — builtins in
   *  fixed slots 0..kBuiltinSlots-1, custom names above them, where
   *  customNames[i] owns slot kBuiltinSlots+i. One buffer, one element
   *  type: the GPU counterpart of the CPU executor's named attribute
   *  store. */
  dg::RefCntAutoPtr<dg::IBuffer> lanes;
  dg::RefCntAutoPtr<dg::IBuffer> scratch;  // Relax reads/writes alternately
  /** Every Lookup op's stop table, concatenated in chain order; each
   *  dispatch gets its own (offset, count). Always non-null — a
   *  chain with no Lookup still binds a one-element placeholder. */
  dg::RefCntAutoPtr<dg::IBuffer> table;
  std::vector<glm::vec4> tableData;  ///< exactly what `table` holds
  std::vector<std::string> customNames;
  std::vector<dg::RefCntAutoPtr<dg::IShaderResourceBinding>> srbs;
  entt::entity upstream = entt::null;  ///< device-resident compose
  int count = 0;
  int loopCount = 0;
  bool dirty = true;

  int slotFor(const World::pop::AttrRef& attr) const {
    const int32_t builtin = World::pop::builtinIndex(attr);
    if (builtin >= 0) return builtin;
    for (size_t i = 0; i < customNames.size(); ++i)
      if (customNames[i] == attr.name)
        return World::pop::kBuiltinSlots + (int)i;
    return 0;  // unreachable when customNames covers the chain
  }
};

/** Every attribute name a chain touches beyond the builtins, in
 *  first-appearance order — the arena's custom slot assignment. */
std::vector<std::string> popCustomNames(const World::pop::Chain& chain) {
  std::vector<std::string> names;
  const auto note = [&](const World::pop::AttrRef& attr) {
    if (World::pop::builtinIndex(attr) >= 0) return;
    for (const std::string& existing : names)
      if (existing == attr.name) return;
    names.push_back(attr.name);
  };
  for (const World::pop::Op& op : chain)
    std::visit(
        [&](const auto& o) {
          using T = std::decay_t<decltype(o)>;
          if constexpr (requires { o.lane; }) note(o.lane);
          if constexpr (std::is_same_v<T, World::pop::Set>) note(o.attr);
          if constexpr (std::is_same_v<T, World::pop::Lookup>) {
            // Both ends: a lookup may read one custom lane and write
            // another, and each needs its own slot.
            note(o.from);
            note(o.to);
          }
          if constexpr (std::is_same_v<T, World::pop::Group>) {
            note(o.from);
            note(World::pop::AttrRef{o.to});
          }
          if constexpr (std::is_same_v<T, World::pop::Peak>) note(o.along);
          if constexpr (std::is_same_v<T, World::pop::Mix>) {
            note(o.a);
            note(o.b);
            note(o.to);
            if (!o.factorLane.empty()) note(World::pop::AttrRef{o.factorLane});
          }
          // The mask is a lane read like any other; an unnamed mask
          // (empty) is "everyone" and owns no slot.
          if constexpr (requires { o.mask; })
            if (!o.mask.empty()) note(World::pop::AttrRef{o.mask});
        },
        op);
  return names;
}

using PopParams = shaderparams::PopParamsData;

struct World::Impl {
  WorldConfig config;
  shape::space::Camera camera;
  Lighting lighting;

  dg::RefCntAutoPtr<dg::IRenderDevice> device;
  dg::RefCntAutoPtr<dg::IDeviceContext> context;

  dg::RefCntAutoPtr<dg::ITexture> colorTarget;    // MSAA when enabled
  dg::RefCntAutoPtr<dg::ITexture> resolveTarget;  // single-sample
  dg::RefCntAutoPtr<dg::ITexture> depthTarget;
  dg::RefCntAutoPtr<dg::ITexture> stagingTarget;
  int sampleCount = 1;

  dg::RefCntAutoPtr<dg::IPipelineState> opaquePso;
  dg::RefCntAutoPtr<dg::IPipelineState> blendPso;
  dg::RefCntAutoPtr<dg::IPipelineState> opaqueInstancedPso;
  dg::RefCntAutoPtr<dg::IPipelineState> blendInstancedPso;
  dg::RefCntAutoPtr<dg::IBuffer> frameCB;
  dg::RefCntAutoPtr<dg::IBuffer> drawCB;
  // The sweep generator, created lazily on the first addSweep().
  dg::RefCntAutoPtr<dg::IPipelineState> sweepPso;
  dg::RefCntAutoPtr<dg::IBuffer> sweepCB;
  bool ensureSweepPipeline();
  // The flock generator, created lazily on the first addFlock().
  dg::RefCntAutoPtr<dg::IPipelineState> flockPso;
  dg::RefCntAutoPtr<dg::IBuffer> flockCB;
  bool ensureFlockPipeline();
  dg::RefCntAutoPtr<dg::IBuffer> createLoopBuffer(
      const std::vector<glm::vec3>& loop);
  // The pop kernel: one PSO per operator entry point (index = the
  // Op variant index; the last is the pack sink), lazily created.
  std::vector<dg::RefCntAutoPtr<dg::IPipelineState>> popPsos;
  dg::RefCntAutoPtr<dg::IBuffer> popCB;
  bool ensurePopPipelines();
  bool bindPopSrbs(PopComponent& points, dg::IBuffer* instanceBuffer);
  dg::RefCntAutoPtr<dg::ITexture> whiteTexture;
  dg::RefCntAutoPtr<dg::ITexture> flatNormalTexture;  // (0.5, 0.5, 1)
  // The two addressing modes a material can ask for. Assigned to each
  // uploaded texture's view, which is how a combined texture-sampler
  // shader variable picks its sampler here.
  dg::RefCntAutoPtr<dg::ISampler> clampSampler;
  dg::RefCntAutoPtr<dg::ISampler> wrapSampler;
  /** Upload every map of @p material into a fresh binding on @p pso and
   *  record what was bound in @p bound. */
  bool bindMaterial(dg::IPipelineState* pso, const Material& material,
                    dg::RefCntAutoPtr<dg::IShaderResourceBinding>& srb,
                    MaterialBinding& bound);

  /** Surfaces are entities, and the ids handed to callers are entity
   *  values. Entity 0 is reserved at init so that a valid surface id is
   *  never 0 and callers can read 0 as failure. */
  entt::registry registry;
  bool rendered = false;

  bool init(std::string* error);
  bool createTargets(std::string* error);
  bool createPipelines(std::string* error);
  /** @p srgb decodes the image as display-encoded colour (base colour,
   *  emissive); data maps (normal, roughness, metallic, occlusion) pass
   *  false. @p tile picks the repeat sampler over clamp. */
  dg::RefCntAutoPtr<dg::ITexture> uploadTexture(const sk_sp<SkImage>& image,
                                                bool srgb, bool tile);
  void writeDrawConstants(const glm::mat4& model, const Material& material);
  bool createMeshBuffers(const shape::Mesh& mesh,
                         dg::RefCntAutoPtr<dg::IBuffer>& vertexBuffer,
                         dg::RefCntAutoPtr<dg::IBuffer>& indexBuffer);
  dg::RefCntAutoPtr<dg::IBuffer> createInstanceBuffer(
      const std::vector<InstanceAttribs>& instances);
};

bool World::Impl::init(std::string* error) {
  using namespace dg;
  IEngineFactoryVk* factory = GetEngineFactoryVk();
  if (!factory) {
    if (error) *error = "Diligent Vulkan factory unavailable";
    return false;
  }
  EngineVkCreateInfo engineCI;
  if (config.validation) engineCI.SetValidationLevel(VALIDATION_LEVEL_1);
  IRenderDevice* rawDevice = nullptr;
  IDeviceContext* rawContext = nullptr;
  factory->CreateDeviceAndContextsVk(engineCI, &rawDevice, &rawContext);
  if (!rawDevice || !rawContext) {
    if (error)
      *error =
          "Vulkan device creation failed (is MoltenVK installed? "
          "brew install molten-vk vulkan-loader)";
    return false;
  }
  device.Attach(rawDevice);
  context.Attach(rawContext);
  // Burn entity 0 so no real surface can ever have id 0, which every
  // caller reads as failure.
  (void)registry.create();
  return createTargets(error) && createPipelines(error);
}

bool World::Impl::createTargets(std::string* error) {
  using namespace dg;
  const TEXTURE_FORMAT colorFormat = TEX_FORMAT_RGBA8_UNORM;

  sampleCount = std::max(config.sampleCount, 1);
  const TextureFormatInfoExt& fmtInfo =
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
    if (error) *error = "offscreen target creation failed";
    return false;
  }
  return true;
}

bool World::Impl::createPipelines(std::string* error) {
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
    if (error) *error = "shader compilation failed";
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
    if (error) *error = "constant buffer creation failed";
    return false;
  }

  GraphicsPipelineStateCreateInfo psoCI;
  psoCI.PSODesc.Name = "sigilworld opaque";
  auto& gp = psoCI.GraphicsPipeline;
  gp.NumRenderTargets = 1;
  gp.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
  gp.DSVFormat = TEX_FORMAT_D32_FLOAT;
  gp.SmplDesc.Count = (Uint8)sampleCount;
  gp.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  // No culling anywhere: every surface is two-sided and the pixel shader
  // flips a backfacing normal, so triangle winding never decides
  // visibility.
  gp.RasterizerDesc.CullMode = CULL_MODE_NONE;
  gp.DepthStencilDesc.DepthEnable = True;
  gp.DepthStencilDesc.DepthWriteEnable = True;

  LayoutElement layout[] = {
      {0, 0, 3, VT_FLOAT32, False},  // position
      {1, 0, 3, VT_FLOAT32, False},  // normal
      {2, 0, 2, VT_FLOAT32, False},  // uv
      {3, 0, 4, VT_FLOAT32, False},  // baked color
  };
  // The instanced variants add buffer slot 1: transform rows + tint,
  // advancing per instance.
  LayoutElement instancedLayout[] = {
      {0, 0, 3, VT_FLOAT32, False},
      {1, 0, 3, VT_FLOAT32, False},
      {2, 0, 2, VT_FLOAT32, False},
      {3, 0, 4, VT_FLOAT32, False},
      {4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
      {5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
      {6, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
      {7, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
      {8, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
  };

  psoCI.pPS = ps;

  ShaderResourceVariableDesc variables[] = {
      {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
      {SHADER_TYPE_PIXEL, "g_NormalMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
      {SHADER_TYPE_PIXEL, "g_RoughnessMap",
       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
      {SHADER_TYPE_PIXEL, "g_MetallicMap",
       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
      {SHADER_TYPE_PIXEL, "g_OcclusionMap",
       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
      {SHADER_TYPE_PIXEL, "g_EmissiveMap",
       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
  };
  psoCI.PSODesc.ResourceLayout.Variables = variables;
  psoCI.PSODesc.ResourceLayout.NumVariables = (Uint32)std::size(variables);

  // No immutable samplers: each texture VIEW carries its own sampler
  // (clamp or repeat, per Material::tile), and the combined
  // texture-sampler variables pick it up from the view. Two samplers
  // exist in the whole device.
  {
    SamplerDesc samplerDesc;
    samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
    samplerDesc.Name = "sigilworld clamp";
    device->CreateSampler(samplerDesc, &clampSampler);
    samplerDesc.AddressU = TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = TEXTURE_ADDRESS_WRAP;
    samplerDesc.Name = "sigilworld wrap";
    device->CreateSampler(samplerDesc, &wrapSampler);
    if (!clampSampler || !wrapSampler) {
      if (error) *error = "sampler creation failed";
      return false;
    }
  }

  // Four pipelines: {plain, instanced} x {opaque, blended}. The pixel
  // shader and resource layout are identical across all four, so one
  // shader-resource-binding shape serves every surface.
  auto makePso = [&](const char* name, bool instanced, bool blend,
                     dg::RefCntAutoPtr<IPipelineState>& out) {
    psoCI.PSODesc.Name = name;
    psoCI.pVS = instanced ? vsInstanced : vs;
    gp.InputLayout.LayoutElements = instanced ? instancedLayout : layout;
    gp.InputLayout.NumElements = instanced ? 9u : 4u;
    auto& rt0 = gp.BlendDesc.RenderTargets[0];
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

  if (!opaquePso || !blendPso || !opaqueInstancedPso || !blendInstancedPso) {
    if (error) *error = "pipeline creation failed";
    return false;
  }

  for (IPipelineState* pso :
       {opaquePso.RawPtr(), blendPso.RawPtr(), opaqueInstancedPso.RawPtr(),
        blendInstancedPso.RawPtr()}) {
    for (SHADER_TYPE stage : {SHADER_TYPE_VERTEX, SHADER_TYPE_PIXEL}) {
      if (auto* var = pso->GetStaticVariableByName(stage, "FrameConstants"))
        var->Set(frameCB);
      if (auto* var = pso->GetStaticVariableByName(stage, "DrawConstants"))
        var->Set(drawCB);
    }
  }

  // A 1x1 white texture, bound wherever a material carries no image, so
  // the shader's texture multiply is an identity rather than a branch.
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
    // ...and its normal-map twin: (0.5, 0.5, 1) decodes to the
    // unperturbed normal, so a material without a normal map shades
    // exactly as if the map path were not there.
    desc.Name = "sigilworld flat normal";
    const Uint32 flat = 0xffff8080u;  // ABGR in memory: R=0x80 G=0x80 B=0xff
    TextureSubResData flatSub{&flat, 4};
    TextureData flatData{&flatSub, 1};
    device->CreateTexture(desc, &flatData, &flatNormalTexture);
  }
  if (!whiteTexture || !flatNormalTexture) return false;
  whiteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
      ->SetSampler(clampSampler);
  flatNormalTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
      ->SetSampler(clampSampler);
  return true;
}

dg::RefCntAutoPtr<dg::ITexture> World::Impl::uploadTexture(
    const sk_sp<SkImage>& image, bool srgb, bool tile) {
  using namespace dg;
  if (!image) return whiteTexture;
  // Every source flattens to 8-bit unpremultiplied RGBA here, so a float
  // or HDR image loses its range at this point. No mipmaps are built
  // either — the sampler's mip filter has nothing to choose between.
  const int w = image->width(), h = image->height();
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
  if (!image->readPixels(nullptr, bitmap.pixmap(), 0, 0)) return whiteTexture;

  TextureDesc desc;
  desc.Name = "sigilworld surface texture";
  desc.Type = RESOURCE_DIM_TEX_2D;
  desc.Width = (Uint32)w;
  desc.Height = (Uint32)h;
  // Colour content is authored in sRGB, so an sRGB view linearizes it
  // on sample and the shader can work in linear throughout. The pixel
  // shader's own encode is the exact inverse of this decode. Data maps
  // (normals, roughness, metallic, occlusion) are numbers, not colours,
  // and upload as plain UNORM.
  desc.Format = srgb ? TEX_FORMAT_RGBA8_UNORM_SRGB : TEX_FORMAT_RGBA8_UNORM;
  desc.BindFlags = BIND_SHADER_RESOURCE;
  desc.MipLevels = 1;
  TextureSubResData subres{bitmap.getPixels(), (Uint64)bitmap.rowBytes()};
  TextureData data{&subres, 1};
  RefCntAutoPtr<ITexture> texture;
  device->CreateTexture(desc, &data, &texture);
  if (!texture) return whiteTexture;
  texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
      ->SetSampler(tile ? wrapSampler : clampSampler);
  return texture;
}

bool World::Impl::bindMaterial(
    dg::IPipelineState* pso, const Material& material,
    dg::RefCntAutoPtr<dg::IShaderResourceBinding>& srb,
    MaterialBinding& bound) {
  using namespace dg;
  srb.Release();
  pso->CreateShaderResourceBinding(&srb, true);
  if (!srb) return false;
  const auto set = [&](const char* name, const sk_sp<SkImage>& image, bool srgb,
                       ITexture* standIn) {
    RefCntAutoPtr<ITexture> texture =
        image ? uploadTexture(image, srgb, material.tile)
              : RefCntAutoPtr<ITexture>(standIn);
    if (auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, name))
      var->Set(texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
  };
  set("g_Texture", material.texture, true, whiteTexture);
  set("g_NormalMap", material.normalMap, false, flatNormalTexture);
  set("g_RoughnessMap", material.roughnessMap, false, whiteTexture);
  set("g_MetallicMap", material.metallicMap, false, whiteTexture);
  set("g_OcclusionMap", material.occlusionMap, false, whiteTexture);
  set("g_EmissiveMap", material.emissiveMap, true, whiteTexture);
  bound = MaterialBinding::of(material);
  return true;
}

void World::Impl::writeDrawConstants(const glm::mat4& model,
                                     const Material& m) {
  using namespace dg;
  MapHelper<DrawConstants> constants(context, drawCB, MAP_WRITE,
                                     MAP_FLAG_DISCARD);
  constants->model = colMajor(model);
  constants->normalMat = normalMatrix(model);
  constants->baseColor[0] = m.baseColor.x;
  constants->baseColor[1] = m.baseColor.y;
  constants->baseColor[2] = m.baseColor.z;
  constants->baseColor[3] = m.baseColor.w;
  constants->emissive[0] = m.emissive.x;
  constants->emissive[1] = m.emissive.y;
  constants->emissive[2] = m.emissive.z;
  constants->emissive[3] = 1;
  constants->matParams[0] = m.metallic;
  constants->matParams[1] = m.roughness;
  constants->matParams[2] = m.emissiveStrength;
  constants->matParams[3] = m.unlit ? 1.0f : 0.0f;
  constants->uvScaleOffset[0] = m.uvScale.x;
  constants->uvScaleOffset[1] = m.uvScale.y;
  constants->uvScaleOffset[2] = m.uvOffset.x;
  constants->uvScaleOffset[3] = m.uvOffset.y;
  constants->mapParams[0] = m.normalMap ? m.normalScale : 0.0f;
  constants->mapParams[1] = m.occlusionMap ? m.occlusionStrength : 0.0f;
  constants->mapParams[2] = m.normalMapDirectX ? 1.0f : 0.0f;
  constants->mapParams[3] = 0;
  const auto channel = [](int c) { return (float)std::clamp(c, 0, 3); };
  constants->channels[0] = channel(m.roughnessChannel);
  constants->channels[1] = channel(m.metallicChannel);
  constants->channels[2] = channel(m.occlusionChannel);
  constants->channels[3] = 0;
}

namespace {

std::vector<Vertex> packVertices(const shape::Mesh& mesh) {
  std::vector<Vertex> vertices(mesh.positions.size());
  for (size_t i = 0; i < mesh.positions.size(); ++i) {
    Vertex& v = vertices[i];
    v.pos[0] = mesh.positions[i].x;
    v.pos[1] = mesh.positions[i].y;
    v.pos[2] = mesh.positions[i].z;
    const glm::vec3 n =
        i < mesh.normals.size() ? mesh.normals[i] : glm::vec3{0, 0, 1};
    v.normal[0] = n.x;
    v.normal[1] = n.y;
    v.normal[2] = n.z;
    const glm::vec2 uv = i < mesh.uvs.size() ? mesh.uvs[i] : glm::vec2{0, 0};
    v.uv[0] = uv.x;
    v.uv[1] = uv.y;
    const glm::vec4 c =
        i < mesh.colors.size() ? mesh.colors[i] : glm::vec4{1, 1, 1, 1};
    v.color[0] = c.x;
    v.color[1] = c.y;
    v.color[2] = c.z;
    v.color[3] = c.w;
  }
  return vertices;
}

}  // namespace

bool World::Impl::createMeshBuffers(
    const shape::Mesh& mesh, dg::RefCntAutoPtr<dg::IBuffer>& vertexBuffer,
    dg::RefCntAutoPtr<dg::IBuffer>& indexBuffer) {
  using namespace dg;
  const std::vector<Vertex> vertices = packVertices(mesh);

  // DEFAULT rather than IMMUTABLE usage, so setSurfaceMesh() can update
  // these buffers in place when the topology is unchanged.
  BufferDesc vbDesc;
  vbDesc.Name = "sigilworld vertices";
  vbDesc.Usage = USAGE_DEFAULT;
  vbDesc.BindFlags = BIND_VERTEX_BUFFER;
  vbDesc.Size = (Uint64)(vertices.size() * sizeof(Vertex));
  BufferData vbData{vertices.data(), vbDesc.Size};
  device->CreateBuffer(vbDesc, &vbData, &vertexBuffer);

  BufferDesc ibDesc;
  ibDesc.Name = "sigilworld indices";
  ibDesc.Usage = USAGE_DEFAULT;
  ibDesc.BindFlags = BIND_INDEX_BUFFER;
  ibDesc.Size = (Uint64)(mesh.indices.size() * sizeof(uint32_t));
  BufferData ibData{mesh.indices.data(), ibDesc.Size};
  device->CreateBuffer(ibDesc, &ibData, &indexBuffer);

  return vertexBuffer && indexBuffer;
}

dg::RefCntAutoPtr<dg::IBuffer> World::Impl::createInstanceBuffer(
    const std::vector<InstanceAttribs>& instances) {
  using namespace dg;
  RefCntAutoPtr<IBuffer> buffer;
  if (instances.empty()) return buffer;
  // DEFAULT usage, so setInstances() can update it in place when the
  // instance count is unchanged.
  BufferDesc desc;
  desc.Name = "sigilworld instances";
  desc.Usage = USAGE_DEFAULT;
  desc.BindFlags = BIND_VERTEX_BUFFER;
  desc.Size = (Uint64)(instances.size() * sizeof(InstanceAttribs));
  BufferData data{instances.data(), desc.Size};
  device->CreateBuffer(desc, &data, &buffer);
  return buffer;
}

bool World::Impl::ensureSweepPipeline() {
  using namespace dg;
  if (sweepPso) return true;

#ifndef SIGILWORLD_POP_SPIRV
  // The compute generators exist only as SPIR-V compiled at build time;
  // they have no fallback shader. The graphics path still works — this
  // door fails to create and the caller returns 0.
  warnComputeKernelsUnavailable();
  return false;
#else
  ShaderCreateInfo shaderCI;
  size_t byteCodeSize = 0;
  shaderCI.ByteCode = findSpirv("CSSweep", &byteCodeSize);
  shaderCI.ByteCodeSize = byteCodeSize;
  if (!shaderCI.ByteCode) return false;
  shaderCI.EntryPoint = "CSSweep";
  shaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
  shaderCI.Desc.Name = "sigilworld sweep cs";
  RefCntAutoPtr<IShader> cs;
  device->CreateShader(shaderCI, &cs);
  if (!cs) return false;

  BufferDesc cbDesc;
  cbDesc.Name = "sigilworld sweep params";
  cbDesc.Usage = USAGE_DYNAMIC;
  cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
  cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  cbDesc.Size = sizeof(SweepParams);
  device->CreateBuffer(cbDesc, nullptr, &sweepCB);
  if (!sweepCB) return false;

  ComputePipelineStateCreateInfo psoCI;
  psoCI.PSODesc.Name = "sigilworld sweep";
  psoCI.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
  ShaderResourceVariableDesc variables[] = {
      {SHADER_TYPE_COMPUTE, "g_Points", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
      {SHADER_TYPE_COMPUTE, "g_Vertices",
       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
  };
  psoCI.PSODesc.ResourceLayout.Variables = variables;
  psoCI.PSODesc.ResourceLayout.NumVariables = 2;
  psoCI.pCS = cs;
  device->CreateComputePipelineState(psoCI, &sweepPso);
  if (!sweepPso) return false;
  if (auto* var =
          sweepPso->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "SweepParams"))
    var->Set(sweepCB);
  return true;
#endif
}

bool World::Impl::ensureFlockPipeline() {
  using namespace dg;
  if (flockPso) return true;

#ifndef SIGILWORLD_POP_SPIRV
  // The compute generators exist only as SPIR-V compiled at build time;
  // they have no fallback shader. The graphics path still works — this
  // door fails to create and the caller returns 0.
  warnComputeKernelsUnavailable();
  return false;
#else
  ShaderCreateInfo shaderCI;
  size_t byteCodeSize = 0;
  shaderCI.ByteCode = findSpirv("CSFlock", &byteCodeSize);
  shaderCI.ByteCodeSize = byteCodeSize;
  if (!shaderCI.ByteCode) return false;
  shaderCI.EntryPoint = "CSFlock";
  shaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
  shaderCI.Desc.Name = "sigilworld flock cs";
  RefCntAutoPtr<IShader> cs;
  device->CreateShader(shaderCI, &cs);
  if (!cs) return false;

  BufferDesc cbDesc;
  cbDesc.Name = "sigilworld flock params";
  cbDesc.Usage = USAGE_DYNAMIC;
  cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
  cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  cbDesc.Size = sizeof(FlockParams);
  device->CreateBuffer(cbDesc, nullptr, &flockCB);
  if (!flockCB) return false;

  ComputePipelineStateCreateInfo psoCI;
  psoCI.PSODesc.Name = "sigilworld flock";
  psoCI.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
  ShaderResourceVariableDesc variables[] = {
      {SHADER_TYPE_COMPUTE, "g_Points", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
      {SHADER_TYPE_COMPUTE, "g_Instances",
       SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
  };
  psoCI.PSODesc.ResourceLayout.Variables = variables;
  psoCI.PSODesc.ResourceLayout.NumVariables = 2;
  psoCI.pCS = cs;
  device->CreateComputePipelineState(psoCI, &flockPso);
  if (!flockPso) return false;
  if (auto* var =
          flockPso->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "FlockParams"))
    var->Set(flockCB);
  return true;
#endif
}

dg::RefCntAutoPtr<dg::IBuffer> World::Impl::createLoopBuffer(
    const std::vector<glm::vec3>& loop) {
  using namespace dg;
  std::vector<float> pointData;
  pointData.reserve(loop.size() * 4);
  for (const glm::vec3& p : loop) {
    pointData.push_back(p.x);
    pointData.push_back(p.y);
    pointData.push_back(p.z);
    pointData.push_back(0);
  }
  BufferDesc desc;
  desc.Name = "sigilworld loop";
  desc.Usage = USAGE_IMMUTABLE;
  desc.BindFlags = BIND_SHADER_RESOURCE;
  desc.Mode = BUFFER_MODE_STRUCTURED;
  desc.ElementByteStride = 4 * sizeof(float);
  desc.Size = (Uint64)(pointData.size() * sizeof(float));
  BufferData data{pointData.data(), desc.Size};
  RefCntAutoPtr<IBuffer> buffer;
  device->CreateBuffer(desc, &data, &buffer);
  return buffer;
}

namespace {

/** Every compute entry point, in pipeline order. The copy-back and pack
 *  sinks come last because they belong to no operator. */
constexpr const char* kPopEntries[] = {
    "CSSplineScatter", "CSJitter",   "CSNoise",     "CSRamp", "CSVary",
    "CSLookAt",        "CSMath",     "CSRelax",     "CSSet",  "CSAtlas",
    "CSLookup",        "CSGroup",    "CSTransform", "CSPeak", "CSDeform",
    "CSMix",           "CSCopyBack", "CSPopPack",
};
constexpr size_t kPopCopyBackIndex = std::size(kPopEntries) - 2;
constexpr size_t kPopPackIndex = std::size(kPopEntries) - 1;

/** No compute kernel: the operator runs on the CPU only, and every door
 *  into the GPU executor DECLINES a chain holding it rather than
 *  dropping the operator and cooking something subtly wrong. */
constexpr size_t kPopNoKernel = (size_t)-1;
/** Operator variant index -> kPopEntries index. ONE ROW PER ALTERNATIVE,
 *  index-aligned with pop::Op.
 *
 *  The variant order is effectively ABI: this table is indexed by it, so
 *  inserting an alternative anywhere but the end silently repoints every
 *  later operator at another operator's kernel. Append only.
 *
 *  A table rather than arithmetic over the index, because arithmetic
 *  cannot express more than one hole without being wrong by a fencepost
 *  somewhere. The static_assert below turns appending an operator
 *  without deciding its row into a build failure instead of a runtime
 *  mystery. */
constexpr size_t kPopOpPso[] = {
    0,             // 0  SplineScatter
    1,             // 1  Jitter
    2,             // 2  Noise
    3,             // 3  Ramp
    4,             // 4  Vary
    5,             // 5  LookAt
    6,             // 6  Math
    7,             // 7  Relax
    kPopNoKernel,  // 8  MeshScatter — CPU only: seeds from a Mesh
    8,             // 9  Set
    9,             // 10 Atlas
    kPopNoKernel,  // 11 Promote     — CPU only: writes the primitive
                   //                  class, which this executor has no
                   //                  arena for
    10,            // 12 Lookup
    kPopNoKernel,  // 13 Sort        — CPU only: a permutation of the
                   //                  whole point set is not a per-point
                   //                  map, so no kernel can express it
    11,            // 14 Group
    12,            // 15 Transform
    13,            // 16 Peak
    14,            // 17 Deform
    15,            // 18 Mix
};
static_assert(
    std::size(kPopOpPso) == std::variant_size_v<World::pop::Op>,
    "every pop::Op alternative needs a row here — appending "
    "appending one anywhere but the end would land it on another op's "
    "kernel");
size_t popPsoIndex(size_t variantIndex) {
  return variantIndex < std::size(kPopOpPso) ? kPopOpPso[variantIndex]
                                             : kPopNoKernel;
}
/** The boundary check reads the SAME table the dispatcher maps through,
 *  so what the executor declines and what it can actually run cannot
 *  drift apart. */
bool popOpRunsOnGpu(const World::pop::Op& op) {
  return popPsoIndex(op.index()) != kPopNoKernel;
}
bool popChainRunsOnGpu(const World::pop::Chain& chain) {
  for (const World::pop::Op& op : chain)
    if (!popOpRunsOnGpu(op)) return false;
  return true;
}
/** Every Lookup op's stop table, concatenated in chain order. This is
 *  the layout g_Table is uploaded with, and the cook pass walks the
 *  chain in this same order so each Lookup dispatch gets the offset its
 *  own stops landed at. */
std::vector<glm::vec4> popTable(const World::pop::Chain& chain) {
  std::vector<glm::vec4> stops;
  for (const World::pop::Op& op : chain)
    if (const auto* lookup = std::get_if<World::pop::Lookup>(&op))
      stops.insert(stops.end(), lookup->stops.begin(), lookup->stops.end());
  return stops;
}

}  // namespace

bool World::Impl::ensurePopPipelines() {
#ifndef SIGILWORLD_POP_SPIRV
  // The compute generators exist only as SPIR-V compiled at build time;
  // they have no fallback shader. The graphics path still works — this
  // door fails to create and the caller returns 0.
  warnComputeKernelsUnavailable();
  return false;
#else
  using namespace dg;
  if (!popPsos.empty()) return true;

  BufferDesc cbDesc;
  cbDesc.Name = "sigilworld pop params";
  cbDesc.Usage = USAGE_DYNAMIC;
  cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
  cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  cbDesc.Size = sizeof(PopParams);
  device->CreateBuffer(cbDesc, nullptr, &popCB);
  if (!popCB) return false;

  for (const char* entry : kPopEntries) {
    ShaderCreateInfo shaderCI;
    size_t byteCodeSize = 0;
    shaderCI.ByteCode = findSpirv(entry, &byteCodeSize);
    shaderCI.ByteCodeSize = byteCodeSize;
    if (!shaderCI.ByteCode) return false;
    shaderCI.EntryPoint = entry;
    shaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
    shaderCI.Desc.Name = entry;
    RefCntAutoPtr<IShader> cs;
    device->CreateShader(shaderCI, &cs);
    if (!cs) return false;

    ComputePipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = entry;
    psoCI.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
    psoCI.PSODesc.ResourceLayout.DefaultVariableType =
        SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
    ShaderResourceVariableDesc variables[] = {
        {SHADER_TYPE_COMPUTE, "PopParams",
         SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    };
    psoCI.PSODesc.ResourceLayout.Variables = variables;
    psoCI.PSODesc.ResourceLayout.NumVariables = 1;
    psoCI.pCS = cs;
    RefCntAutoPtr<IPipelineState> pso;
    device->CreateComputePipelineState(psoCI, &pso);
    if (!pso) return false;
    if (auto* var =
            pso->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "PopParams"))
      var->Set(popCB);
    popPsos.push_back(pso);
  }
  return true;
#endif
}

bool World::Impl::bindPopSrbs(PopComponent& points,
                              dg::IBuffer* instanceBuffer) {
  using namespace dg;
  points.srbs.clear();
  if (!points.table)
    return false;  // the lookup binding needs it; a null view is not legal
  dg::IBuffer* loopBuffer = points.loop;
  if (!loopBuffer && registry.valid(points.upstream) &&
      registry.all_of<PopComponent>(points.upstream))
    loopBuffer = registry.get<PopComponent>(points.upstream).lanes;
  if (!loopBuffer) return false;
  const auto bindOne = [&](size_t psoIndex) -> bool {
    RefCntAutoPtr<IShaderResourceBinding> srb;
    popPsos[psoIndex]->CreateShaderResourceBinding(&srb, true);
    if (!srb) return false;
    const auto set = [&](const char* name, IBuffer* buffer,
                         BUFFER_VIEW_TYPE view) {
      if (auto* var = srb->GetVariableByName(SHADER_TYPE_COMPUTE, name))
        var->Set(buffer->GetDefaultView(view));
    };
    set("g_Points", loopBuffer, BUFFER_VIEW_SHADER_RESOURCE);
    set("g_Lanes", points.lanes, BUFFER_VIEW_UNORDERED_ACCESS);
    set("g_Instances", instanceBuffer, BUFFER_VIEW_UNORDERED_ACCESS);
    set("g_Scratch", points.scratch, BUFFER_VIEW_UNORDERED_ACCESS);
    set("g_Table", points.table, BUFFER_VIEW_SHADER_RESOURCE);
    points.srbs.push_back(srb);
    return true;
  };
  for (const pop::Op& op : points.chain) {
    const size_t psoIndex = popPsoIndex(op.index());
    // Defence in depth behind the public doors' validation: an operator
    // with no kernel gets NO binding here rather than another
    // operator's.
    if (psoIndex == kPopNoKernel || !bindOne(psoIndex)) return false;
  }
  // The Relax copy-back is second from last; the pack sink is last.
  return bindOne(kPopCopyBackIndex) && bindOne(kPopPackIndex);
}

// ---------------------------------------------------------------------------

World::World() : m_impl(std::make_unique<Impl>()) {}
World::~World() = default;

std::unique_ptr<World> World::create(const WorldConfig& config,
                                     std::string* error) {
  std::unique_ptr<World> world(new World());
  world->m_impl->config = config;
  if (!world->m_impl->init(error)) return nullptr;
  return world;
}

uint32_t World::addSurface(const shape::Mesh& mesh, const glm::mat4& model,
                           const Material& material) {
  using namespace dg;
  Impl& impl = *m_impl;
  if (mesh.positions.empty() || mesh.indices.empty()) return 0;

  GpuGeometry geometry;
  geometry.indexCount = (uint32_t)mesh.indices.size();
  if (!impl.createMeshBuffers(mesh, geometry.vertexBuffer,
                              geometry.indexBuffer))
    return 0;

  if (!impl.bindMaterial(impl.opaquePso, material, geometry.srb,
                         geometry.bound))
    return 0;

  const entt::entity id = impl.registry.create();
  impl.registry.emplace<GpuGeometry>(id, std::move(geometry));
  impl.registry.emplace<TransformComponent>(id, model);
  impl.registry.emplace<MaterialComponent>(id, material);
  return (uint32_t)id;
}

uint32_t World::addInstanced(const shape::Mesh& stamp,
                             const shape::Cloud& cloud,
                             const Material& material,
                             const InstanceLanes& lanes) {
  using namespace dg;
  Impl& impl = *m_impl;
  if (stamp.positions.empty() || stamp.indices.empty()) return 0;

  GpuInstancedGeometry geometry;
  geometry.indexCount = (uint32_t)stamp.indices.size();
  if (!impl.createMeshBuffers(stamp, geometry.vertexBuffer,
                              geometry.indexBuffer))
    return 0;

  const std::vector<InstanceAttribs> instances = buildInstances(cloud, lanes);
  geometry.instanceCount = (uint32_t)instances.size();
  geometry.instanceBuffer = impl.createInstanceBuffer(instances);
  if (geometry.instanceCount > 0 && !geometry.instanceBuffer) return 0;

  if (!impl.bindMaterial(impl.opaqueInstancedPso, material, geometry.srb,
                         geometry.bound))
    return 0;

  const entt::entity id = impl.registry.create();
  impl.registry.emplace<GpuInstancedGeometry>(id, std::move(geometry));
  // One transform for the whole flock, starting at identity: the points
  // carry their own placement, and setTransform moves all of them
  // together.
  impl.registry.emplace<TransformComponent>(id, glm::mat4(1.0f));
  impl.registry.emplace<MaterialComponent>(id, material);
  return (uint32_t)id;
}

void World::setInstances(uint32_t id, const shape::Cloud& cloud,
                         const InstanceLanes& lanes) {
  using namespace dg;
  Impl& impl = *m_impl;
  const entt::entity e = entity(id);
  if (!impl.registry.valid(e) || !impl.registry.all_of<GpuInstancedGeometry>(e))
    return;
  GpuInstancedGeometry& geometry = impl.registry.get<GpuInstancedGeometry>(e);

  const std::vector<InstanceAttribs> instances = buildInstances(cloud, lanes);
  if (instances.size() == geometry.instanceCount && geometry.instanceBuffer) {
    impl.context->UpdateBuffer(
        geometry.instanceBuffer, 0,
        (Uint64)(instances.size() * sizeof(InstanceAttribs)), instances.data(),
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  } else {
    geometry.instanceBuffer = impl.createInstanceBuffer(instances);
    geometry.instanceCount = (uint32_t)instances.size();
  }
}

void World::setTransform(uint32_t id, const glm::mat4& model) {
  entt::registry& registry = m_impl->registry;
  const entt::entity e = entity(id);
  if (registry.valid(e) && registry.all_of<TransformComponent>(e))
    registry.get<TransformComponent>(e).model = model;
}

void World::setSurfaceMesh(uint32_t id, const shape::Mesh& mesh) {
  using namespace dg;
  Impl& impl = *m_impl;
  const entt::entity e = entity(id);
  if (!impl.registry.valid(e) || !impl.registry.all_of<GpuGeometry>(e)) return;
  if (mesh.positions.empty() || mesh.indices.empty()) return;
  GpuGeometry& geometry = impl.registry.get<GpuGeometry>(e);
  const Uint64 vertexBytes = (Uint64)(mesh.positions.size() * sizeof(Vertex));
  const Uint64 indexBytes = (Uint64)(mesh.indices.size() * sizeof(uint32_t));
  if (geometry.vertexBuffer && geometry.indexBuffer &&
      geometry.vertexBuffer->GetDesc().Size == vertexBytes &&
      geometry.indexBuffer->GetDesc().Size == indexBytes) {
    const std::vector<Vertex> vertices = packVertices(mesh);
    impl.context->UpdateBuffer(geometry.vertexBuffer, 0, vertexBytes,
                               vertices.data(),
                               RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    impl.context->UpdateBuffer(geometry.indexBuffer, 0, indexBytes,
                               mesh.indices.data(),
                               RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    return;
  }
  geometry.indexCount = (uint32_t)mesh.indices.size();
  geometry.vertexBuffer.Release();
  geometry.indexBuffer.Release();
  impl.createMeshBuffers(mesh, geometry.vertexBuffer, geometry.indexBuffer);
}

uint32_t World::addSweep(const SweepDesc& desc, const Material& material) {
  using namespace dg;
  Impl& impl = *m_impl;
  if (desc.loop.size() < 3 || desc.sections < 2) return 0;
  if (!impl.ensureSweepPipeline()) return 0;

  SweepComponent sweep;
  sweep.head = desc.head;
  sweep.span = desc.span;
  sweep.width = desc.width;
  sweep.sections = desc.sections;
  sweep.pointCount = (int)desc.loop.size();

  // The loop, resident on the GPU: one float4 per control point.
  sweep.points = impl.createLoopBuffer(desc.loop);

  // The vertex buffer the sweep kernel rewrites: bound both as a
  // structured unordered-access view and as a vertex buffer.
  GpuGeometry geometry;
  BufferDesc vbDesc;
  vbDesc.Name = "sigilworld sweep vertices";
  vbDesc.Usage = USAGE_DEFAULT;
  vbDesc.BindFlags = BIND_VERTEX_BUFFER | BIND_UNORDERED_ACCESS;
  vbDesc.Mode = BUFFER_MODE_STRUCTURED;
  vbDesc.ElementByteStride = sizeof(Vertex);
  vbDesc.Size = (Uint64)(desc.sections * 2) * sizeof(Vertex);
  impl.device->CreateBuffer(vbDesc, nullptr, &geometry.vertexBuffer);

  std::vector<uint32_t> indices;
  indices.reserve(((size_t)desc.sections - 1) * 6);
  for (int i = 0; i + 1 < desc.sections; ++i) {
    const uint32_t a = (uint32_t)(i * 2);
    indices.insert(indices.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
  }
  geometry.indexCount = (uint32_t)indices.size();
  BufferDesc ibDesc;
  ibDesc.Name = "sigilworld sweep indices";
  ibDesc.Usage = USAGE_IMMUTABLE;
  ibDesc.BindFlags = BIND_INDEX_BUFFER;
  ibDesc.Size = (Uint64)(indices.size() * sizeof(uint32_t));
  BufferData ibData{indices.data(), ibDesc.Size};
  impl.device->CreateBuffer(ibDesc, &ibData, &geometry.indexBuffer);
  if (!sweep.points || !geometry.vertexBuffer || !geometry.indexBuffer)
    return 0;

  impl.sweepPso->CreateShaderResourceBinding(&sweep.srb, true);
  if (!sweep.srb) return 0;
  if (auto* var = sweep.srb->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Points"))
    var->Set(sweep.points->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
  if (auto* var =
          sweep.srb->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Vertices"))
    var->Set(
        geometry.vertexBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

  if (!impl.bindMaterial(impl.opaquePso, material, geometry.srb,
                         geometry.bound))
    return 0;

  const entt::entity id = impl.registry.create();
  impl.registry.emplace<GpuGeometry>(id, std::move(geometry));
  impl.registry.emplace<TransformComponent>(id, glm::mat4(1.0f));
  impl.registry.emplace<MaterialComponent>(id, material);
  impl.registry.emplace<SweepComponent>(id, std::move(sweep));
  return (uint32_t)id;
}

void World::setSweepWindow(uint32_t id, float head, float span) {
  Impl& impl = *m_impl;
  const entt::entity e = entity(id);
  if (!impl.registry.valid(e) || !impl.registry.all_of<SweepComponent>(e))
    return;
  SweepComponent& sweep = impl.registry.get<SweepComponent>(e);
  sweep.head = head;
  sweep.span = span;
  sweep.dirty = true;
}

uint32_t World::addFlock(const shape::Mesh& stamp, const FlockDesc& desc,
                         const Material& material) {
  using namespace dg;
  Impl& impl = *m_impl;
  if (stamp.positions.empty() || stamp.indices.empty() ||
      desc.loop.size() < 3 || desc.count < 1)
    return 0;
  if (!impl.ensureFlockPipeline()) return 0;

  FlockComponent flock;
  flock.head = desc.head;
  flock.span = desc.span;
  flock.radius = desc.radius;
  flock.scale = desc.scale;
  flock.noiseAmplitude = desc.noiseAmplitude;
  flock.noiseFrequency = desc.noiseFrequency;
  flock.seed = desc.seed;
  flock.count = desc.count;
  flock.pointCount = (int)desc.loop.size();
  const glm::vec4& tail = desc.tintTail;
  const glm::vec4& headTint = desc.tintHead;
  flock.tintTail[0] = tail.x;
  flock.tintTail[1] = tail.y;
  flock.tintTail[2] = tail.z;
  flock.tintTail[3] = tail.w;
  flock.tintHead[0] = headTint.x;
  flock.tintHead[1] = headTint.y;
  flock.tintHead[2] = headTint.z;
  flock.tintHead[3] = headTint.w;
  flock.points = impl.createLoopBuffer(desc.loop);

  GpuInstancedGeometry geometry;
  geometry.indexCount = (uint32_t)stamp.indices.size();
  geometry.instanceCount = (uint32_t)desc.count;
  if (!impl.createMeshBuffers(stamp, geometry.vertexBuffer,
                              geometry.indexBuffer))
    return 0;

  // The instanced stream the compute kernel packs: bound both as an
  // unordered-access view and as a per-instance vertex buffer.
  BufferDesc ibDesc;
  ibDesc.Name = "sigilworld flock instances";
  ibDesc.Usage = USAGE_DEFAULT;
  ibDesc.BindFlags = BIND_VERTEX_BUFFER | BIND_UNORDERED_ACCESS;
  ibDesc.Mode = BUFFER_MODE_STRUCTURED;
  ibDesc.ElementByteStride = sizeof(InstanceAttribs);
  ibDesc.Size = (Uint64)desc.count * sizeof(InstanceAttribs);
  impl.device->CreateBuffer(ibDesc, nullptr, &geometry.instanceBuffer);
  if (!flock.points || !geometry.instanceBuffer) return 0;

  impl.flockPso->CreateShaderResourceBinding(&flock.srb, true);
  if (!flock.srb) return 0;
  if (auto* var = flock.srb->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Points"))
    var->Set(flock.points->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
  if (auto* var =
          flock.srb->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Instances"))
    var->Set(
        geometry.instanceBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

  if (!impl.bindMaterial(impl.opaqueInstancedPso, material, geometry.srb,
                         geometry.bound))
    return 0;

  const entt::entity id = impl.registry.create();
  impl.registry.emplace<GpuInstancedGeometry>(id, std::move(geometry));
  impl.registry.emplace<TransformComponent>(id, glm::mat4(1.0f));
  impl.registry.emplace<MaterialComponent>(id, material);
  impl.registry.emplace<FlockComponent>(id, std::move(flock));
  return (uint32_t)id;
}

void World::setFlockWindow(uint32_t id, float head, float span) {
  Impl& impl = *m_impl;
  const entt::entity e = entity(id);
  if (!impl.registry.valid(e) || !impl.registry.all_of<FlockComponent>(e))
    return;
  FlockComponent& flock = impl.registry.get<FlockComponent>(e);
  flock.head = head;
  flock.span = span;
  flock.dirty = true;
}

namespace {

/** A chain's point count comes from its generator head. Answers 0 for a
 *  chain this executor declines, which every caller reads as failure. */
int popChainCount(const World::pop::Chain& chain) {
  if (chain.empty()) return 0;
  // Declined, not silently stripped: dropping an operator would cook
  // geometry that looks plausible and is wrong. kPopOpPso says which
  // operators have no kernel and why.
  if (!popChainRunsOnGpu(chain)) return 0;
  if (const auto* scatter =
          std::get_if<World::pop::SplineScatter>(&chain.front()))
    return scatter->loop.size() >= 3 ? scatter->count : 0;
  return 0;
}

dg::RefCntAutoPtr<dg::IBuffer> createLaneBuffer(dg::IRenderDevice* device,
                                                int count, int slots,
                                                const char* name) {
  using namespace dg;
  // Zero-filled so custom lanes start at {0,0,0,0}, matching the CPU
  // executor's default for an untouched attribute; the builtin lanes are
  // initialized by the generator kernel before anything reads them.
  const std::vector<float> zeros((size_t)count * (size_t)slots * 4, 0.0f);
  BufferDesc desc;
  desc.Name = name;
  desc.Usage = USAGE_DEFAULT;
  // SHADER_RESOURCE as well as UAV, because a downstream chain built
  // with addPointsOn reads this arena's position slot as its own loop.
  desc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
  desc.Mode = BUFFER_MODE_STRUCTURED;
  desc.ElementByteStride = 4 * sizeof(float);
  desc.Size = (Uint64)zeros.size() * sizeof(float);
  BufferData data{zeros.data(), desc.Size};
  RefCntAutoPtr<IBuffer> buffer;
  device->CreateBuffer(desc, &data, &buffer);
  return buffer;
}

/** The Lookup stop tables, concatenated. IMMUTABLE, because the stops
 *  are part of the chain value: editing them is a re-describe, and
 *  setPoints takes the structural path when it sees a different table
 *  even if every operator kind still lines up. A chain with no Lookup
 *  still gets a one-element placeholder, since the binding is not
 *  optional and a null buffer view is not legal. */
dg::RefCntAutoPtr<dg::IBuffer> createTableBuffer(
    dg::IRenderDevice* device, const std::vector<glm::vec4>& stops) {
  using namespace dg;
  const std::vector<glm::vec4> contents =
      stops.empty() ? std::vector<glm::vec4>{glm::vec4{0, 0, 0, 0}} : stops;
  BufferDesc desc;
  desc.Name = "sigilworld pop lookup table";
  desc.Usage = USAGE_IMMUTABLE;
  desc.BindFlags = BIND_SHADER_RESOURCE;
  desc.Mode = BUFFER_MODE_STRUCTURED;
  desc.ElementByteStride = 4 * sizeof(float);
  desc.Size = (Uint64)contents.size() * 4 * sizeof(float);
  BufferData data{contents.data(), desc.Size};
  RefCntAutoPtr<IBuffer> buffer;
  device->CreateBuffer(desc, &data, &buffer);
  return buffer;
}

}  // namespace

uint32_t World::addPoints(const shape::Mesh& stamp, const pop::Chain& chain,
                          const Material& material) {
  using namespace dg;
  Impl& impl = *m_impl;
  const int count = popChainCount(chain);
  if (stamp.positions.empty() || stamp.indices.empty() || count < 1) return 0;
  if (!impl.ensurePopPipelines()) return 0;

  PopComponent points;
  points.chain = chain;
  points.count = count;
  const auto& scatter = std::get<pop::SplineScatter>(chain.front());
  points.loopCount = (int)scatter.loop.size();
  points.loop = impl.createLoopBuffer(scatter.loop);
  points.customNames = popCustomNames(chain);
  const int slots = pop::kBuiltinSlots + (int)points.customNames.size();
  points.lanes = createLaneBuffer(impl.device, count, slots, "pop lanes");
  points.scratch = createLaneBuffer(impl.device, count, 1, "pop scratch");
  points.tableData = popTable(chain);
  points.table = createTableBuffer(impl.device, points.tableData);
  if (!points.loop || !points.lanes || !points.scratch || !points.table)
    return 0;

  GpuInstancedGeometry geometry;
  geometry.indexCount = (uint32_t)stamp.indices.size();
  geometry.instanceCount = (uint32_t)count;
  if (!impl.createMeshBuffers(stamp, geometry.vertexBuffer,
                              geometry.indexBuffer))
    return 0;
  BufferDesc ibDesc;
  ibDesc.Name = "sigilworld pop instances";
  ibDesc.Usage = USAGE_DEFAULT;
  ibDesc.BindFlags = BIND_VERTEX_BUFFER | BIND_UNORDERED_ACCESS;
  ibDesc.Mode = BUFFER_MODE_STRUCTURED;
  ibDesc.ElementByteStride = sizeof(InstanceAttribs);
  ibDesc.Size = (Uint64)count * sizeof(InstanceAttribs);
  impl.device->CreateBuffer(ibDesc, nullptr, &geometry.instanceBuffer);
  if (!geometry.instanceBuffer) return 0;
  if (!impl.bindPopSrbs(points, geometry.instanceBuffer)) return 0;

  if (!impl.bindMaterial(impl.opaqueInstancedPso, material, geometry.srb,
                         geometry.bound))
    return 0;

  const entt::entity id = impl.registry.create();
  impl.registry.emplace<GpuInstancedGeometry>(id, std::move(geometry));
  impl.registry.emplace<TransformComponent>(id, glm::mat4(1.0f));
  impl.registry.emplace<MaterialComponent>(id, material);
  impl.registry.emplace<PopComponent>(id, std::move(points));
  return (uint32_t)id;
}

uint32_t World::addPointsOn(uint32_t upstream, const shape::Mesh& stamp,
                            const pop::Chain& chain, const Material& material) {
  using namespace dg;
  Impl& impl = *m_impl;
  const entt::entity up = entity(upstream);
  if (!impl.registry.valid(up) || !impl.registry.all_of<PopComponent>(up))
    return 0;
  const PopComponent& source = impl.registry.get<PopComponent>(up);
  if (source.count < 3) return 0;
  if (chain.empty())  // no generator to ride the upstream arena
    return 0;
  const auto* scatter = std::get_if<pop::SplineScatter>(&chain.front());
  if (!scatter || scatter->count < 1 || stamp.positions.empty() ||
      stamp.indices.empty())
    return 0;
  // The same boundary popChainCount draws, off the same table: an
  // operator with no kernel is declined outright, never dropped.
  if (!popChainRunsOnGpu(chain)) return 0;
  if (!impl.ensurePopPipelines()) return 0;

  const int count = scatter->count;
  PopComponent points;
  points.chain = chain;
  points.count = count;
  points.upstream = up;
  points.loopCount = source.count;  // refreshed at cook time
  points.customNames = popCustomNames(chain);
  const int slots = pop::kBuiltinSlots + (int)points.customNames.size();
  points.lanes = createLaneBuffer(impl.device, count, slots, "pop lanes");
  points.scratch = createLaneBuffer(impl.device, count, 1, "pop scratch");
  points.tableData = popTable(chain);
  points.table = createTableBuffer(impl.device, points.tableData);
  if (!points.lanes || !points.scratch || !points.table) return 0;

  GpuInstancedGeometry geometry;
  geometry.indexCount = (uint32_t)stamp.indices.size();
  geometry.instanceCount = (uint32_t)count;
  if (!impl.createMeshBuffers(stamp, geometry.vertexBuffer,
                              geometry.indexBuffer))
    return 0;
  BufferDesc ibDesc;
  ibDesc.Name = "sigilworld pop instances";
  ibDesc.Usage = USAGE_DEFAULT;
  ibDesc.BindFlags = BIND_VERTEX_BUFFER | BIND_UNORDERED_ACCESS;
  ibDesc.Mode = BUFFER_MODE_STRUCTURED;
  ibDesc.ElementByteStride = sizeof(InstanceAttribs);
  ibDesc.Size = (Uint64)count * sizeof(InstanceAttribs);
  impl.device->CreateBuffer(ibDesc, nullptr, &geometry.instanceBuffer);
  if (!geometry.instanceBuffer) return 0;
  if (!impl.bindPopSrbs(points, geometry.instanceBuffer)) return 0;

  if (!impl.bindMaterial(impl.opaqueInstancedPso, material, geometry.srb,
                         geometry.bound))
    return 0;

  const entt::entity id = impl.registry.create();
  impl.registry.emplace<GpuInstancedGeometry>(id, std::move(geometry));
  impl.registry.emplace<TransformComponent>(id, glm::mat4(1.0f));
  impl.registry.emplace<MaterialComponent>(id, material);
  impl.registry.emplace<PopComponent>(id, std::move(points));
  return (uint32_t)id;
}

void World::setPointsWindow(uint32_t id, float head, float span) {
  Impl& impl = *m_impl;
  const entt::entity e = entity(id);
  if (!impl.registry.valid(e) || !impl.registry.all_of<PopComponent>(e)) return;
  PopComponent& points = impl.registry.get<PopComponent>(e);
  if (points.chain.empty()) return;
  auto* scatter = std::get_if<pop::SplineScatter>(&points.chain.front());
  if (!scatter) return;
  scatter->head = head;
  scatter->span = span;
  points.dirty = true;
}

void World::setPoints(uint32_t id, const pop::Chain& chain) {
  using namespace dg;
  Impl& impl = *m_impl;
  const entt::entity e = entity(id);
  if (!impl.registry.valid(e) || !impl.registry.all_of<PopComponent>(e)) return;
  const int count = popChainCount(chain);
  if (count < 1) return;
  PopComponent& points = impl.registry.get<PopComponent>(e);
  GpuInstancedGeometry& geometry = impl.registry.get<GpuInstancedGeometry>(e);

  // Same operator kinds and count: a parameter edit, so keep the
  // buffers and bindings and just re-cook. Anything structural rebuilds
  // the lanes and rebinds. A lookup table edit counts as structural even
  // when the operator kinds line up, because the table rides an
  // immutable buffer — the same reason the loop-content check below
  // treats moved control points as structural.
  const std::vector<glm::vec4> table = popTable(chain);
  const bool sameShape =
      count == points.count && chain.size() == points.chain.size() &&
      popCustomNames(chain) == points.customNames &&
      table == points.tableData &&
      std::equal(chain.begin(), chain.end(), points.chain.begin(),
                 [](const pop::Op& a, const pop::Op& b) {
                   return a.index() == b.index();
                 });
  // The loop rides its own immutable buffer, uploaded when the surface
  // was added, so a re-describe with MOVED control points must recreate
  // it or the kernels go on cooking the old loop. Compared before the
  // chain is overwritten. A surface created with addPointsOn carries an
  // empty loop — its generator reads the upstream arena instead — and so
  // never takes this path.
  const auto* scatter = std::get_if<pop::SplineScatter>(&chain.front());
  const auto* stored =
      points.chain.empty()
          ? nullptr
          : std::get_if<pop::SplineScatter>(&points.chain.front());
  const bool loopChanged = scatter && !scatter->loop.empty() &&
                           (!stored || stored->loop != scatter->loop);
  points.chain = chain;
  if (loopChanged) {
    points.loop = impl.createLoopBuffer(scatter->loop);
    points.loopCount = (int)scatter->loop.size();
  }
  if (!sameShape) {
    points.count = count;
    points.customNames = popCustomNames(chain);
    points.lanes = createLaneBuffer(
        impl.device, count, pop::kBuiltinSlots + (int)points.customNames.size(),
        "pop lanes");
    points.scratch = createLaneBuffer(impl.device, count, 1, "pop scratch");
    points.tableData = table;
    points.table = createTableBuffer(impl.device, points.tableData);
    geometry.instanceCount = (uint32_t)count;
    geometry.instanceBuffer.Release();
    BufferDesc ibDesc;
    ibDesc.Name = "sigilworld pop instances";
    ibDesc.Usage = USAGE_DEFAULT;
    ibDesc.BindFlags = BIND_VERTEX_BUFFER | BIND_UNORDERED_ACCESS;
    ibDesc.Mode = BUFFER_MODE_STRUCTURED;
    ibDesc.ElementByteStride = sizeof(InstanceAttribs);
    ibDesc.Size = (Uint64)count * sizeof(InstanceAttribs);
    impl.device->CreateBuffer(ibDesc, nullptr, &geometry.instanceBuffer);
    if (!points.lanes || !points.scratch || !points.table ||
        !geometry.instanceBuffer) {
      // Allocation failed partway. Zero the count so readPoints() cannot
      // later size a copy against lanes that do not exist.
      points.count = 0;
      return;
    }
    impl.bindPopSrbs(points, geometry.instanceBuffer);
  } else if (loopChanged) {
    // The loop binds into every per-operator binding, so a fresh buffer
    // needs a rebind even on the parameter-only path.
    impl.bindPopSrbs(points, geometry.instanceBuffer);
  }
  points.dirty = true;
}

shape::Cloud World::readPoints(uint32_t id) {
  using namespace dg;
  Impl& impl = *m_impl;
  shape::Cloud out;
  const entt::entity e = entity(id);
  if (!impl.registry.valid(e) || !impl.registry.all_of<PopComponent>(e))
    return out;
  PopComponent& points = impl.registry.get<PopComponent>(e);
  const Uint64 laneBytes = (Uint64)points.count * 4 * sizeof(float);
  const size_t slots = (size_t)pop::kBuiltinSlots + points.customNames.size();

  BufferDesc desc;
  desc.Name = "sigilworld lane readback";
  desc.Usage = USAGE_STAGING;
  desc.CPUAccessFlags = CPU_ACCESS_READ;
  desc.BindFlags = BIND_NONE;
  desc.Size = laneBytes * slots;
  RefCntAutoPtr<IBuffer> staging;
  impl.device->CreateBuffer(desc, nullptr, &staging);
  if (!staging) return out;
  impl.context->CopyBuffer(
      points.lanes, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, staging, 0,
      laneBytes * slots, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  impl.context->WaitForIdle();

  PVoid mapped = nullptr;
  impl.context->MapBuffer(staging, MAP_READ, MAP_FLAG_NONE, mapped);
  if (!mapped) return out;
  const float* f = static_cast<const float*>(mapped);
  const size_t n = (size_t)points.count;
  out.positions.resize(n);
  std::vector<float>& t = out.scalar("t");
  std::vector<glm::vec3>& dir = out.vector("dir");
  std::vector<float>& size = out.scalar("size", 1);
  std::vector<glm::vec4>& tint = out.color("tint");
  std::vector<glm::vec4>& tex = out.color("Tex");
  const auto slot = [&](size_t s, size_t i) { return f + (s * n + i) * 4; };
  for (size_t i = 0; i < n; ++i) {
    const float* p = slot(0, i);
    out.positions[i] = {p[0], p[1], p[2]};
    t[i] = slot(1, i)[0];
    const float* d = slot(2, i);
    dir[i] = {d[0], d[1], d[2]};
    size[i] = slot(3, i)[0];
    const float* c = slot(4, i);
    tint[i] = {c[0], c[1], c[2], c[3]};
    const float* x = slot(5, i);
    tex[i] = {x[0], x[1], x[2], x[3]};
  }
  for (size_t s = 0; s < points.customNames.size(); ++s) {
    std::vector<glm::vec4>& lane = out.color(points.customNames[s]);
    for (size_t i = 0; i < n; ++i) {
      const float* v = slot((size_t)pop::kBuiltinSlots + s, i);
      lane[i] = {v[0], v[1], v[2], v[3]};
    }
  }
  impl.context->UnmapBuffer(staging, MAP_READ);
  return out;
}

void World::removeSurface(uint32_t id) {
  entt::registry& registry = m_impl->registry;
  const entt::entity e = entity(id);
  if (registry.valid(e) &&
      registry.any_of<GpuGeometry, GpuInstancedGeometry>(e))
    registry.destroy(e);
}

size_t World::surfaceCount() const {
  return m_impl->registry.view<GpuGeometry>().size() +
         m_impl->registry.view<GpuInstancedGeometry>().size();
}

uint32_t World::addLight(const LightComponent& light) {
  entt::registry& registry = m_impl->registry;
  const entt::entity id = registry.create();
  registry.emplace<LightComponent>(id, light);
  return (uint32_t)id;
}

entt::registry& World::registry() { return m_impl->registry; }

const entt::registry& World::registry() const { return m_impl->registry; }

void World::setCamera(const shape::space::Camera& camera) {
  m_impl->camera = camera;
}

void World::setLighting(const Lighting& lighting) {
  m_impl->lighting = lighting;
}

bool World::render() {
  using namespace dg;
  Impl& impl = *m_impl;
  if (!impl.context) return false;

  // Declared motion first, before anything reads a component: resolve
  // every animated lane into the Transform / Material / Light / Camera
  // component or generator window it drives (Animation.h). The camera
  // lanes land here too, so the camera pick further down reads this
  // frame's values. A frame with no Animated* component walks five empty
  // views and writes nothing, leaving the imperative setters untouched.
  //
  // NO CLOCK is stepped here, deliberately: the caller owns the ticker,
  // which is what keeps a render a pure function of what the Outputs
  // hold and makes the same frame index reproduce byte for byte.
  resolveAnimation(*this);

  // GPU sweeps first: rewrite every dirty sweep surface's vertices in
  // place, then draw them like any other surface. Binding the vertex
  // buffers in TRANSITION mode inserts the unordered-access to vertex
  // barrier, so no explicit barrier is needed between the two.
  {
    auto sweeps = impl.registry.view<SweepComponent>();
    bool bound = false;
    for (entt::entity e : sweeps) {
      SweepComponent& sweep = sweeps.get<SweepComponent>(e);
      if (!sweep.dirty) continue;
      if (!bound) {
        impl.context->SetPipelineState(impl.sweepPso);
        bound = true;
      }
      {
        MapHelper<SweepParams> params(impl.context, impl.sweepCB, MAP_WRITE,
                                      MAP_FLAG_DISCARD);
        params->window[0] = sweep.head;
        params->window[1] = sweep.span;
        params->window[2] = sweep.width;
        params->window[3] = (float)sweep.sections;
        params->loop[0] = (float)sweep.pointCount;
        // Parameter step the kernel uses for its centred-difference
        // tangent; must match the flock and point kernels' value below.
        params->loop[1] = 0.002f;
        params->loop[2] = 0;
        params->loop[3] = 0;
      }
      impl.context->CommitShaderResources(
          sweep.srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      DispatchComputeAttribs dispatch((Uint32)((sweep.sections + 63) / 64), 1,
                                      1);
      impl.context->DispatchCompute(dispatch);
      sweep.dirty = false;
    }
  }
  {
    auto flocks = impl.registry.view<FlockComponent>();
    bool bound = false;
    for (entt::entity e : flocks) {
      FlockComponent& flock = flocks.get<FlockComponent>(e);
      if (!flock.dirty) continue;
      if (!bound) {
        impl.context->SetPipelineState(impl.flockPso);
        bound = true;
      }
      {
        MapHelper<FlockParams> params(impl.context, impl.flockCB, MAP_WRITE,
                                      MAP_FLAG_DISCARD);
        params->windowA[0] = flock.head;
        params->windowA[1] = flock.span;
        params->windowA[2] = flock.radius;
        params->windowA[3] = (float)flock.count;
        params->windowB[0] = flock.scale;
        params->windowB[1] = flock.noiseAmplitude;
        params->windowB[2] = flock.noiseFrequency;
        params->windowB[3] = flock.seed;
        std::memcpy(params->tintTail, flock.tintTail, sizeof(flock.tintTail));
        std::memcpy(params->tintHead, flock.tintHead, sizeof(flock.tintHead));
        params->loop[0] = (float)flock.pointCount;
        params->loop[1] = 0.002f;
        params->loop[2] = 0;
        params->loop[3] = 0;
      }
      impl.context->CommitShaderResources(
          flock.srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      DispatchComputeAttribs dispatch((Uint32)((flock.count + 63) / 64), 1, 1);
      impl.context->DispatchCompute(dispatch);
      flock.dirty = false;
    }
  }
  {
    // Point chains: one dispatch per operator with a barrier between
    // each, because every operator reads lanes the previous one wrote.
    // The pack sink runs last and fills the instanced draw stream.
    auto popView = impl.registry.view<PopComponent>();
    // Dependency-ordered: an entity's upstream cooks first, and a cooked
    // upstream forces its dependents to re-cook, their input having
    // changed.
    std::vector<entt::entity> cooked;
    std::function<void(entt::entity)> cookOne;
    cookOne = [&](entt::entity e) {
      PopComponent& points = popView.get<PopComponent>(e);
      const bool upstreamCooked = std::find(cooked.begin(), cooked.end(),
                                            points.upstream) != cooked.end();
      if (impl.registry.valid(points.upstream) &&
          impl.registry.all_of<PopComponent>(points.upstream)) {
        PopComponent& up = impl.registry.get<PopComponent>(points.upstream);
        if (up.dirty) cookOne(points.upstream);
        // Refreshed every cook, so a setPoints() on the upstream that
        // changed its point count cannot leave this one reading a stale
        // length.
        points.loopCount = up.count;
      }
      if ((!points.dirty && !upstreamCooked &&
           std::find(cooked.begin(), cooked.end(), points.upstream) ==
               cooked.end()) ||
          points.srbs.empty())
        return;
      const auto laneBarrier = [&] {
        StateTransitionDesc barriers[] = {
            {points.lanes, RESOURCE_STATE_UNORDERED_ACCESS,
             RESOURCE_STATE_UNORDERED_ACCESS,
             STATE_TRANSITION_FLAG_UPDATE_STATE},
            {points.scratch, RESOURCE_STATE_UNORDERED_ACCESS,
             RESOURCE_STATE_UNORDERED_ACCESS,
             STATE_TRANSITION_FLAG_UPDATE_STATE},
        };
        impl.context->TransitionResourceStates(2, barriers);
      };
      const DispatchComputeAttribs dispatch((Uint32)((points.count + 63) / 64),
                                            1, 1);
      const auto runOp = [&](size_t psoIndex, size_t srbIndex,
                             const PopParams& params) {
        impl.context->SetPipelineState(impl.popPsos[psoIndex]);
        {
          MapHelper<PopParams> mapped(impl.context, impl.popCB, MAP_WRITE,
                                      MAP_FLAG_DISCARD);
          *mapped = params;
        }
        impl.context->CommitShaderResources(
            points.srbs[srbIndex], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        impl.context->DispatchCompute(dispatch);
      };
      const auto base = [&] {
        PopParams params = {};
        params.d[0] = (float)points.count;
        params.d[2] = (float)points.loopCount;
        params.d[3] = 0.002f;
        params.m[0] = -1;  // no mask
        return params;
      };
      const auto put4 = [](float* dst, const glm::vec4& v) {
        dst[0] = v.x;
        dst[1] = v.y;
        dst[2] = v.z;
        dst[3] = v.w;
      };
      const auto put3 = [](float* dst, const glm::vec3& v) {
        dst[0] = v.x;
        dst[1] = v.y;
        dst[2] = v.z;
        dst[3] = 0;
      };
      // Walks the chain in the SAME order popTable() concatenated in, so
      // each Lookup dispatch gets the offset its own stops landed at.
      int tableCursor = 0;
      for (size_t i = 0; i < points.chain.size(); ++i) {
        PopParams params = base();
        std::visit(
            [&](const auto& op) {
              using T = std::decay_t<decltype(op)>;
              if constexpr (std::is_same_v<T, pop::SplineScatter>) {
                params.a[0] = op.head;
                params.a[1] = op.span;
                params.a[2] = op.radius;
                params.a[3] = (float)op.seed;
              } else if constexpr (std::is_same_v<T, pop::Jitter>) {
                params.a[0] = op.amplitude;
                params.a[1] = (float)op.seed;
                params.d[1] = (float)points.slotFor(op.lane);
              } else if constexpr (std::is_same_v<T, pop::Noise>) {
                params.a[0] = op.amplitude;
                params.a[1] = op.frequency;
                params.a[2] = op.seed;
                params.d[1] = (float)points.slotFor(op.lane);
              } else if constexpr (std::is_same_v<T, pop::Ramp>) {
                params.a[0] = op.from.x;
                params.a[1] = op.from.y;
                params.a[2] = op.from.z;
                params.a[3] = op.from.w;
                params.b[0] = op.to.x;
                params.b[1] = op.to.y;
                params.b[2] = op.to.z;
                params.b[3] = op.to.w;
                params.d[1] = (float)points.slotFor(op.lane);
              } else if constexpr (std::is_same_v<T, pop::Vary>) {
                params.a[0] = op.base;
                params.a[1] = op.spread;
                params.a[2] = (float)op.seed;
                params.d[1] = (float)points.slotFor(op.lane);
              } else if constexpr (std::is_same_v<T, pop::LookAt>) {
                params.a[0] = op.target.x;
                params.a[1] = op.target.y;
                params.a[2] = op.target.z;
              } else if constexpr (std::is_same_v<T, pop::Relax>) {
                params.a[0] = op.strength;
                params.d[1] = (float)points.slotFor(op.lane);
              } else if constexpr (std::is_same_v<T, pop::Set>) {
                params.a[0] = op.value.x;
                params.a[1] = op.value.y;
                params.a[2] = op.value.z;
                params.a[3] = op.value.w;
                params.d[1] = (float)points.slotFor(op.attr);
              } else if constexpr (std::is_same_v<T, pop::Atlas>) {
                params.a[0] = (float)op.cols;
                params.a[1] = (float)op.rows;
                params.a[2] = (float)op.seed;
              } else if constexpr (std::is_same_v<T, pop::Lookup>) {
                params.a[0] = op.weights.x;
                params.a[1] = op.weights.y;
                params.a[2] = op.weights.z;
                params.a[3] = op.weights.w;
                params.b[0] = (float)points.slotFor(op.from);
                params.b[1] = (float)tableCursor;
                params.b[2] = (float)op.stops.size();
                params.c[0] = op.low;
                params.c[1] = op.high;
                params.d[1] = (float)points.slotFor(op.to);
                tableCursor += (int)op.stops.size();
              } else if constexpr (std::is_same_v<T, pop::Math>) {
                params.a[0] = op.mul.x;
                params.a[1] = op.mul.y;
                params.a[2] = op.mul.z;
                params.a[3] = op.mul.w;
                params.b[0] = op.add.x;
                params.b[1] = op.add.y;
                params.b[2] = op.add.z;
                params.b[3] = op.add.w;
                params.d[1] = (float)points.slotFor(op.lane);
              } else if constexpr (std::is_same_v<T, pop::Group>) {
                put3(params.a, op.center);
                put3(params.b, op.size);
                params.c[0] = op.feather;
                params.c[1] = (float)op.shape;
                params.c[2] = op.invert ? 1.0f : 0.0f;
                params.c[3] = (float)op.combine;
                params.d[1] = (float)points.slotFor(pop::AttrRef{op.to});
                params.m[1] = (float)points.slotFor(op.from);
              } else if constexpr (std::is_same_v<T, pop::Transform>) {
                params.a[0] = op.direction ? 1.0f : 0.0f;
                put4(params.e, op.matrix[0]);
                put4(params.f, op.matrix[1]);
                put4(params.g, op.matrix[2]);
                put4(params.h, op.matrix[3]);
                params.d[1] = (float)points.slotFor(op.lane);
              } else if constexpr (std::is_same_v<T, pop::Peak>) {
                params.a[0] = op.distance;
                params.d[1] = (float)points.slotFor(op.lane);
                params.m[1] = (float)points.slotFor(op.along);
              } else if constexpr (std::is_same_v<T, pop::Deform>) {
                glm::vec3 axis, dir, side;
                shape::popops::deformFrame(op, &axis, &dir, &side);
                params.a[0] = (float)op.kind;
                params.a[1] = op.amount;
                params.a[2] = op.low;
                params.a[3] = op.high;
                params.b[0] = op.amount * 3.14159265f / 180.0f;
                params.b[1] = op.high - op.low;
                put3(params.e, axis);
                put3(params.f, op.origin);
                put3(params.g, dir);
                put3(params.h, side);
                params.d[1] = (float)points.slotFor(op.lane);
              } else if constexpr (std::is_same_v<T, pop::Mix>) {
                params.a[0] = op.factor;
                params.b[0] = (float)points.slotFor(op.a);
                params.b[1] = (float)points.slotFor(op.b);
                params.b[2] =
                    op.factorLane.empty()
                        ? -1.0f
                        : (float)points.slotFor(pop::AttrRef{op.factorLane});
                params.d[1] = (float)points.slotFor(op.to);
              }
              // The mask slot rides every filter that carries one.
              if constexpr (requires { op.mask; })
                if (!op.mask.empty())
                  params.m[0] = (float)points.slotFor(pop::AttrRef{op.mask});
            },
            points.chain[i]);
        if (const auto* relax = std::get_if<pop::Relax>(&points.chain[i])) {
          for (int pass = 0; pass < relax->iterations; ++pass) {
            runOp(popPsoIndex(points.chain[i].index()), i, params);
            laneBarrier();
            runOp(kPopCopyBackIndex, points.srbs.size() - 2, params);
            laneBarrier();
          }
        } else {
          runOp(popPsoIndex(points.chain[i].index()), i, params);
          laneBarrier();
        }
      }
      runOp(kPopPackIndex, points.srbs.size() - 1, base());
      points.dirty = false;
      cooked.push_back(e);
    };
    for (entt::entity e : popView) cookOne(e);
  }

  const bool msaa = impl.sampleCount > 1;
  ITextureView* rtv = (msaa ? impl.colorTarget : impl.resolveTarget)
                          ->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
  ITextureView* dsv =
      impl.depthTarget->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
  impl.context->SetRenderTargets(1, &rtv, dsv,
                                 RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float clear[4] = {impl.config.clearColor.x, impl.config.clearColor.y,
                          impl.config.clearColor.z, impl.config.clearColor.w};
  impl.context->ClearRenderTarget(rtv, clear,
                                  RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  impl.context->ClearDepthStencil(dsv, CLEAR_DEPTH_FLAG, 1.0f, 0,
                                  RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Camera precedence: an entity with an ACTIVE CameraComponent
  // outranks setCamera(), whenever either was set. With none active the
  // fallback camera drives the frame; with several, the first the
  // registry iterates wins.
  shape::space::Camera cam = impl.camera;
  for (auto [e, camComponent] : impl.registry.view<CameraComponent>().each()) {
    if (camComponent.active) {
      cam = camComponent.camera;
      break;
    }
  }

  // Frame constants.
  {
    const float aspect = impl.config.height > 0 ? (float)impl.config.width /
                                                      (float)impl.config.height
                                                : 1.0f;
    // Column-vector chain: clip = proj * view * model.
    const glm::mat4 viewProj =
        perspectiveVk(cam.fovYDeg, aspect, cam.zNear, cam.zFar) * cam.view();

    MapHelper<FrameConstants> constants(impl.context, impl.frameCB, MAP_WRITE,
                                        MAP_FLAG_DISCARD);
    constants->viewProj = colMajor(viewProj);
    constants->camPos[0] = cam.eye.x;
    constants->camPos[1] = cam.eye.y;
    constants->camPos[2] = cam.eye.z;
    constants->camPos[3] = 1;
    glm::vec3 sunDir = impl.lighting.sunDirection;
    const float len = glm::length(sunDir);
    if (len > 1e-6f) sunDir = sunDir * (1.0f / len);
    constants->sunDir[0] = sunDir.x;
    constants->sunDir[1] = sunDir.y;
    constants->sunDir[2] = sunDir.z;
    constants->sunDir[3] = impl.lighting.sunIntensity;
    auto putColor = [](float* dst, const glm::vec4& c, float alpha) {
      dst[0] = c.x;
      dst[1] = c.y;
      dst[2] = c.z;
      dst[3] = alpha;
    };
    putColor(constants->sunColor, impl.lighting.sunColor, 1);
    putColor(constants->skyColor, impl.lighting.skyColor, 1);
    putColor(constants->groundColor, impl.lighting.groundColor, 1);
    constants->params[0] = impl.lighting.ambient;
    constants->params[1] = constants->params[2] = constants->params[3] = 0;

    // Registry lights: the first kLightBudget the view yields, in
    // registry iteration order. Any beyond that are silently ignored.
    int lightCount = 0;
    for (auto [e, light] : impl.registry.view<LightComponent>().each()) {
      if (lightCount >= kLightBudget) break;
      float* pos = constants->lightPos[lightCount];
      float* color = constants->lightColor[lightCount];
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
      color[0] = light.color.x * light.intensity;
      color[1] = light.color.y * light.intensity;
      color[2] = light.color.z * light.intensity;
      color[3] = light.range;
      ++lightCount;
    }
    constants->params[1] = (float)lightCount;
  }

  // Gather from the registry: opaque first, then blended, sorted back to
  // front by view depth. The alpha test reads the LIVE
  // MaterialComponent, so mutating alpha through registry() re-routes
  // the surface between passes on the next frame. An instanced surface
  // routes as a whole and counts as ONE sorted item, so its instances
  // are not sorted against each other.
  struct DrawItem {
    GpuGeometry* geometry = nullptr;
    GpuInstancedGeometry* instanced = nullptr;
    const glm::mat4* model = nullptr;
    const Material* material = nullptr;
  };
  std::vector<DrawItem> opaque, blended;
  for (auto [e, geometry, transform, material] :
       impl.registry.view<GpuGeometry, TransformComponent, MaterialComponent>()
           .each()) {
    DrawItem item;
    item.geometry = &geometry;
    item.model = &transform.model;
    item.material = &material.material;
    (material.material.baseColor.w < 1.0f ? blended : opaque).push_back(item);
  }
  for (auto [e, geometry, transform, material] :
       impl.registry
           .view<GpuInstancedGeometry, TransformComponent, MaterialComponent>()
           .each()) {
    if (geometry.instanceCount == 0) continue;
    DrawItem item;
    item.instanced = &geometry;
    item.model = &transform.model;
    item.material = &material.material;
    (material.material.baseColor.w < 1.0f ? blended : opaque).push_back(item);
  }
  if (!blended.empty()) {
    const glm::mat4 view = cam.view();
    auto viewZ = [&](const DrawItem& item) {
      const glm::vec4 origin = (view * *item.model) * glm::vec4{0, 0, 0, 1};
      return origin.z;
    };
    std::sort(blended.begin(), blended.end(),
              [&](const DrawItem& a, const DrawItem& b) {
                return viewZ(a) < viewZ(b);
              });
  }

  auto drawList = [&](const std::vector<DrawItem>& list,
                      IPipelineState* plainPso, IPipelineState* instancedPso) {
    IPipelineState* bound = nullptr;
    for (const DrawItem& item : list) {
      IPipelineState* pso = item.instanced ? instancedPso : plainPso;
      if (pso != bound) {
        impl.context->SetPipelineState(pso);
        bound = pso;
      }
      impl.writeDrawConstants(*item.model, *item.material);
      // A material whose images moved since the binding was built gets
      // a fresh binding here — the live half of "every field is live".
      // The bindings are pipeline-compatible across all four PSOs, so
      // the plain PSO builds every plain one and the instanced PSO
      // every instanced one.
      const MaterialBinding want = MaterialBinding::of(*item.material);
      if (item.instanced && !(item.instanced->bound == want))
        impl.bindMaterial(impl.opaqueInstancedPso, *item.material,
                          item.instanced->srb, item.instanced->bound);
      if (item.geometry && !(item.geometry->bound == want))
        impl.bindMaterial(impl.opaquePso, *item.material, item.geometry->srb,
                          item.geometry->bound);
      DrawIndexedAttribs attribs;
      attribs.IndexType = VT_UINT32;
      attribs.Flags = DRAW_FLAG_VERIFY_ALL;
      if (item.instanced) {
        const GpuInstancedGeometry& geometry = *item.instanced;
        impl.context->CommitShaderResources(
            geometry.srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        IBuffer* buffers[] = {geometry.vertexBuffer, geometry.instanceBuffer};
        const Uint64 offsets[] = {0, 0};
        impl.context->SetVertexBuffers(
            0, 2, buffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            SET_VERTEX_BUFFERS_FLAG_RESET);
        impl.context->SetIndexBuffer(geometry.indexBuffer, 0,
                                     RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        attribs.NumIndices = geometry.indexCount;
        attribs.NumInstances = geometry.instanceCount;
      } else {
        const GpuGeometry& geometry = *item.geometry;
        impl.context->CommitShaderResources(
            geometry.srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        IBuffer* vb = geometry.vertexBuffer;
        const Uint64 offset = 0;
        impl.context->SetVertexBuffers(
            0, 1, &vb, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            SET_VERTEX_BUFFERS_FLAG_RESET);
        impl.context->SetIndexBuffer(geometry.indexBuffer, 0,
                                     RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        attribs.NumIndices = geometry.indexCount;
      }
      impl.context->DrawIndexed(attribs);
    }
  };
  drawList(opaque, impl.opaquePso, impl.opaqueInstancedPso);
  drawList(blended, impl.blendPso, impl.blendInstancedPso);

  // A frame with no draws never begins a render pass, so the backend
  // leaves the clears deferred. Flush here, BEFORE the resolve below, or
  // the resolve publishes the previous frame's contents.
  if (opaque.empty() && blended.empty()) impl.context->Flush();

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
  Impl& impl = *m_impl;
  if (!impl.rendered) return nullptr;

  CopyTextureAttribs copy;
  copy.pSrcTexture = impl.resolveTarget;
  copy.pDstTexture = impl.stagingTarget;
  copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  impl.context->CopyTexture(copy);
  impl.context->WaitForIdle();

  MappedTextureSubresource mapped;
  impl.context->MapTextureSubresource(impl.stagingTarget, 0, 0, MAP_READ,
                                      MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
  if (!mapped.pData) return nullptr;

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(impl.config.width, impl.config.height,
                                       kRGBA_8888_SkColorType,
                                       kOpaque_SkAlphaType));
  const uint8_t* src = (const uint8_t*)mapped.pData;
  for (int y = 0; y < impl.config.height; ++y)
    std::memcpy(bitmap.getAddr32(0, y), src + (size_t)y * mapped.Stride,
                (size_t)impl.config.width * 4);
  impl.context->UnmapTextureSubresource(impl.stagingTarget, 0, 0);
  bitmap.setImmutable();
  return bitmap.asImage();
}

bool World::savePng(const std::filesystem::path& path) {
  sk_sp<SkImage> image = readback();
  if (!image) return false;
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(image->width(), image->height(),
                                       kRGBA_8888_SkColorType,
                                       kOpaque_SkAlphaType));
  if (!image->readPixels(nullptr, bitmap.pixmap(), 0, 0)) return false;
  SkFILEWStream stream(path.string().c_str());
  return stream.isValid() && SkPngEncoder::Encode(&stream, bitmap.pixmap(), {});
}

const char* World::backendName() const {
  return m_impl->device ? "Vulkan" : "none";
}

}  // namespace sigil::world
