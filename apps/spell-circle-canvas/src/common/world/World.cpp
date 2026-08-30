#include "sigilworld/World.h"

#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <sigilgeometry/mesh/Vec.h>

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
      "placeSweep, placeChain and placeChainOn return 0; prop "
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

// The graphics shader is assembled at pipeline creation: the fixed text
// below, with the per-slot texture declarations and samplers generated
// for kSlots material slots (the base and up to Material::kMaxLayers
// layers), so the layer count lives in one constant.
constexpr int kSlots = 1 + Material::kMaxLayers;

constexpr char kShaderHead[] = R"(
cbuffer FrameConstants
{
    float4x4 g_ViewProj;
    float4 g_CamPos;
    float4 g_SunDir;      // xyz direction toward scene, w intensity
    float4 g_SunColor;
    float4 g_SkyColor;
    float4 g_GroundColor;
    float4 g_Params;      // x ambient, y light count, zw 1/target size
    float4 g_LightPos[8];   // xyz position (point) / direction (dir), w 1=point
    float4 g_LightColor[8]; // rgb color * intensity, w range (point)
    float4 g_Env;         // x intensity, y rotation (radians), z top mip, w 1 = present
};

// One material's dials — the base's in slot 0, each layer's in the
// slots after it.
struct SlotConstants
{
    float4 BaseColor;
    float4 Emissive;      // rgb emissive, a unused
    float4 MatParams;     // x metallic, y roughness, z emissiveStrength, w unlit (slot 0)
    float4 UvScaleOffset; // xy uv scale, zw uv offset
    float4 MapParams;     // x normalScale, y occlusionStrength, z 1 = DirectX normal, w transmission
    float4 Channels;      // x roughness, y metallic, z occlusion, w opacity channel index
    float4 Glass;         // x ior, y thickness, z alphaCutoff (slot 0), w scene-colour top mip (slot 0)
    float4 Flags;         // x 1 = tile (repeat) the slot's maps
};

cbuffer DrawConstants
{
    float4x4 g_Model;
    float4x4 g_NormalMat;
    SlotConstants g_Slot[SLOTS];
    // Layer i (1..) reads g_Layer*[i - 1]:
    float4 g_LayerA[LAYERS];  // x mask source, y channel or constant, z low, w high
    float4 g_LayerB[LAYERS];  // xyz mask axis, w invert
    float4 g_LayerC[LAYERS];  // xy mask uv scale, zw mask uv offset
    float4 g_LayerD[LAYERS];  // x blend mode, y 1 = layer present, z 1 = tile the mask
};

// Three samplers serve every texture (the device caps samplers per
// stage far below textures): clamp, repeat, and the panorama's
// wrap-u/clamp-v with mips. Which one a map takes is a per-slot flag.
SamplerState g_Clamp;
SamplerState g_Wrap;
SamplerState g_Panorama;

// The frame's OPAQUE pass, resolved and mipped, for glass to look
// through. Bound to every prop (a 1x1 black stand-in until a
// transmissive prop exists in the frame).
Texture2D    g_SceneColor;
// The environment panorama, bound like a material map (a 1x1 black
// stand-in when the lighting has none) so the frame's light rides the
// same binding shape as everything else.
Texture2D    g_Environment;

float4 sampleMap(Texture2D t, float2 uv, float tile)
{
    if (tile > 0.5) return t.Sample(g_Wrap, uv);
    return t.Sample(g_Clamp, uv);
}
)";

// Per slot k: the texture set. Every slot is bound — a 1x1 white (or
// flat-normal) stand-in where the material has no map — so the shader
// multiplies unconditionally and never branches on presence.
constexpr char kShaderSlotTextures[] = R"(
Texture2D    g_Texture$;
Texture2D    g_NormalMap$;
Texture2D    g_RoughnessMap$;
Texture2D    g_MetallicMap$;
Texture2D    g_OcclusionMap$;
Texture2D    g_EmissiveMap$;
Texture2D    g_OpacityMap$;
)";

// Per layer i: its mask image (white when the mask is not a map).
constexpr char kShaderLayerTextures[] = R"(
Texture2D    g_Mask$;
)";

constexpr char kShaderCommon[] = R"(
// Equirectangular lookup: u = 0.5 faces -Z, v = 0 is straight up, with
// the panorama turned about +Y by g_Env.y.
float3 envSample(float3 d, float lod)
{
    float c = cos(g_Env.y), s = sin(g_Env.y);
    float3 r = float3(c * d.x - s * d.z, d.y, s * d.x + c * d.z);
    float u = atan2(r.x, -r.z) / 6.2831853 + 0.5;
    float v = acos(clamp(r.y, -1.0, 1.0)) / 3.1415926;
    return g_Environment.SampleLevel(g_Panorama, float2(u, v), lod).rgb
           * g_Env.x;
}

float pick(float4 v, float channel)
{
    int c = (int)(channel + 0.5);
    return c == 0 ? v.x : (c == 1 ? v.y : (c == 2 ? v.z : v.w));
}

// The tangent frame from screen-space derivatives — no vertex tangents.
// T follows increasing u, B increasing v; with v running DOWN the image
// (uv origin top-left) an OpenGL-convention map's green axis is -B.
// Every slot's map is brought to the OpenGL convention before blending,
// so one frame serves the blended normal.
float3 perturbNormal(float3 N, float3 P, float2 uv, float3 mapN)
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
    return normalize(T * mapN.x - B * mapN.y + N * mapN.z);
}

// The parameters a pixel is shaded with — one slot's, or several
// slots' blended. Blending happens HERE, before shading, which is what
// makes a layered material one material rather than several drawn over
// each other.
struct Surf
{
    float4 base;        // linear rgb, alpha
    float3 nTS;         // tangent-space normal, OpenGL convention
    float  rough;
    float  metal;
    float  occ;
    float3 emissive;
    float  transmission;
    float  ior;
    float  thickness;
};

Surf blendSurf(Surf a, Surf b, float m, int mode)
{
    Surf r = a;
    if (mode == 1)       // Add
    {
        r.base.rgb = a.base.rgb + b.base.rgb * m;
        r.emissive = a.emissive + b.emissive * m;
    }
    else if (mode == 2)  // Multiply
    {
        r.base.rgb = a.base.rgb * lerp(float3(1.0, 1.0, 1.0), b.base.rgb, m);
        r.emissive = lerp(a.emissive, b.emissive, m);
    }
    else                 // Mix
    {
        r.base.rgb = lerp(a.base.rgb, b.base.rgb, m);
        r.emissive = lerp(a.emissive, b.emissive, m);
    }
    r.base.a       = lerp(a.base.a, b.base.a, m);
    r.nTS          = normalize(lerp(a.nTS, b.nTS, m));
    r.rough        = lerp(a.rough, b.rough, m);
    r.metal        = lerp(a.metal, b.metal, m);
    r.occ          = lerp(a.occ, b.occ, m);
    r.transmission = lerp(a.transmission, b.transmission, m);
    r.ior          = lerp(a.ior, b.ior, m);
    r.thickness    = lerp(a.thickness, b.thickness, m);
    return r;
}

// A mask's shaping, shared by every source: fit the raw value onto
// [low, high] and clamp (low == high steps at low), then invert.
float shapeMask(float raw, float4 A, float4 B)
{
    float m = A.w != A.z ? saturate((raw - A.z) / (A.w - A.z))
                         : (raw >= A.z ? 1.0 : 0.0);
    return B.w > 0.5 ? 1.0 - m : m;
}
)";

// Per slot k: sample the slot's texture set into a Surf.
constexpr char kShaderSlotSample[] = R"(
Surf sampleSlot$(float2 uvIn, float4 tint)
{
    SlotConstants c = g_Slot[$];
    float2 uv = uvIn * c.UvScaleOffset.xy + c.UvScaleOffset.zw;
    float tile = c.Flags.x;
    Surf s;
    s.base = sampleMap(g_Texture$, uv, tile) * c.BaseColor * tint;
    s.base.a *= pick(sampleMap(g_OpacityMap$, uv, tile), c.Channels.w);
    float3 n = sampleMap(g_NormalMap$, uv, tile).xyz * 2.0 - 1.0;
    n.xy *= c.MapParams.x;
    if (c.MapParams.z > 0.5) n.y = -n.y;   // DirectX green down -> OpenGL up
    s.nTS = n;
    s.rough = c.MatParams.y *
        pick(sampleMap(g_RoughnessMap$, uv, tile), c.Channels.x);
    s.metal = c.MatParams.x *
        pick(sampleMap(g_MetallicMap$, uv, tile), c.Channels.y);
    s.occ = lerp(1.0, pick(sampleMap(g_OcclusionMap$, uv, tile), c.Channels.z),
                 c.MapParams.y);
    s.emissive = c.Emissive.rgb * c.MatParams.z *
        sampleMap(g_EmissiveMap$, uv, tile).rgb;
    s.transmission = c.MapParams.w;
    s.ior = c.Glass.x;
    s.thickness = c.Glass.y;
    return s;
}
)";

// Per layer i (1-based; reads g_Layer*[i-1] and g_Mask$ where $ = i):
// the mask's raw value from its source, then shaped.
constexpr char kShaderLayerMask[] = R"(
float maskValue$(float2 uvIn, float4 vcolor, float3 Ngeom, float3 P)
{
    float4 A = g_LayerA[$ - 1];
    float4 B = g_LayerB[$ - 1];
    float4 C = g_LayerC[$ - 1];
    int source = (int)(A.x + 0.5);
    float raw = A.y;                                             // Constant
    if (source == 1)
        raw = pick(sampleMap(g_Mask$, uvIn * C.xy + C.zw, g_LayerD[$ - 1].z), A.y);  // Map
    else if (source == 2)
        raw = pick(vcolor, A.y);                                 // VertexColor
    else if (source == 3)
        raw = dot(Ngeom, normalize(B.xyz));                      // Slope
    else if (source == 4)
        raw = dot(P, B.xyz);                                     // Height
    return shapeMask(raw, A, B);
}
)";

constexpr char kShaderBody[] = R"(
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
    float4 Tint   : COLOR0;   // mesh colour x instance tint: multiplies base
    float4 VColor : COLOR1;   // the mesh's own colour lane, raw: mask source
};

void VSMain(in VSIn IN, out PSIn OUT)
{
    float4 world = mul(g_Model, float4(IN.Pos, 1.0));
    OUT.World  = world.xyz;
    OUT.Pos    = mul(g_ViewProj, world);
    OUT.Normal = normalize(mul(g_NormalMat, float4(IN.Normal, 0.0)).xyz);
    OUT.UV     = IN.UV;
    OUT.Tint   = IN.Color;
    OUT.VColor = IN.Color;
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
    OUT.VColor = IN.Color;
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
    float3 Ngeom = normalize(IN.Normal);
    float3 V = normalize(g_CamPos.xyz - IN.World);
    // Culling is off, so every prop is two-sided. Flip a backfacing
    // normal rather than dropping the fragment: winding does not decide
    // visibility here, and a single-sided panel would go black from
    // behind.
    if (dot(Ngeom, V) < 0.0)
        Ngeom = -Ngeom;

    // The material: slot 0, then each present layer blended in where
    // its mask says.
    Surf s = sampleSlot0(IN.UV, IN.Tint);
LAYER_BLENDS
    float4 base = s.base;
    // The cutout: below the cutoff the fragment is not there at all.
    if (g_Slot[0].Glass.z > 0.0 && base.a < g_Slot[0].Glass.z)
        discard;
    float3 emissive = s.emissive;

    if (g_Slot[0].MatParams.w > 0.5)
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

    // The blended tangent-space normal, applied once in the derivative
    // frame.
    float3 N = perturbNormal(Ngeom, IN.World, IN.UV, s.nTS);

    float  metallic = s.metal;
    float  rough    = clamp(s.rough, 0.045, 1.0);
    float  occlusion = s.occ;
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
    // Diffuse and specular kept apart: glass keeps its specular on top
    // of what it transmits and gives up its diffuse.
    float3 diffuse  = (albedo / 3.1415926) * sun * ndl;
    float3 specular = (D * F * Vis) * sun * ndl;

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
        diffuse  += (albedo / 3.1415926) * g_LightColor[i].rgb * (atten * ndli);
        specular += (Di * Fi * Visi) * g_LightColor[i].rgb * (atten * ndli);
    }

    // Ambient: the panorama when the lighting carries one, otherwise the
    // sky/ground hemisphere with a reflection-oriented lobe for spec.
    float3 R = reflect(-V, N);
    float3 ambientDiffuse, ambientSpecular;
    if (g_Env.w > 0.5)
    {
        // Diffuse: a five-tap cosine-weighted hemisphere read from a
        // blurred level (the top levels of a panorama's mip chain are
        // too few texels to stand in for a convolution on their own);
        // specular along R at a roughness-chosen level, weighted by the
        // split-sum environment BRDF (the analytic fit, no lookup
        // texture).
        float dlod = max(g_Env.z - 3.0, 0.0);
        float3 tN = abs(N.y) < 0.9 ? cross(N, float3(0.0, 1.0, 0.0))
                                   : cross(N, float3(1.0, 0.0, 0.0));
        tN = normalize(tN);
        float3 bN = cross(N, tN);
        float3 irradiance = envSample(N, dlod) * 0.36;
        irradiance += envSample(normalize(N * 0.5 + tN * 0.866), dlod) * 0.16;
        irradiance += envSample(normalize(N * 0.5 - tN * 0.866), dlod) * 0.16;
        irradiance += envSample(normalize(N * 0.5 + bN * 0.866), dlod) * 0.16;
        irradiance += envSample(normalize(N * 0.5 - bN * 0.866), dlod) * 0.16;
        float3 prefiltered = envSample(R, rough * g_Env.z);
        float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022);
        float4 c1 = float4(1.0, 0.0425, 1.04, -0.04);
        float4 rr = rough * c0 + c1;
        float a004 = min(rr.x * rr.x, exp2(-9.28 * ndv)) * rr.x + rr.y;
        float2 AB = float2(-1.04, 1.04) * a004 + rr.zw;
        ambientDiffuse  = g_Params.x * occlusion * irradiance * albedo;
        ambientSpecular = g_Params.x * occlusion * prefiltered * (F0 * AB.x + AB.y);
    }
    else
    {
        float3 hemiN = lerp(g_GroundColor.rgb, g_SkyColor.rgb, N.y * 0.5 + 0.5);
        float3 hemiR = lerp(g_GroundColor.rgb, g_SkyColor.rgb, R.y * 0.5 + 0.5);
        ambientDiffuse  = g_Params.x * occlusion * hemiN * albedo;
        ambientSpecular = g_Params.x * occlusion * hemiR * F0 * (1.0 - rough) *
                          (0.4 + 0.6 * pow(1.0 - ndv, 2.0));
    }
    diffuse  += ambientDiffuse;
    specular += ambientSpecular;

    // Tonemap, then the same sRGB encode the unlit branch uses — one
    // transfer function for the whole target.
    float3 color = diffuse + specular + emissive;
    float3 encoded = LinearToSrgb(color / (color + 1.0));
    float  alpha = base.a;

    float transmission = s.transmission;
    if (transmission > 0.0)
    {
        // Glass: what lies behind, read from the opaque pass where the
        // refracted view ray exits a slab `thickness` deep, blurred by
        // roughness, tinted by the base colour; the surface's own
        // diffuse gives way to it and its specular rides on top. The
        // scene colour is already encoded and tonemapped, so the mix
        // happens in encoded space and the pixel is written opaque —
        // the background has been composed here.
        float3 rd = refract(-V, N, 1.0 / max(s.ior, 1.0));
        float3 exitP = IN.World + rd * s.thickness;
        float4 clip = mul(g_ViewProj, float4(exitP, 1.0));
        float2 suv = clip.xy / max(clip.w, 1e-4) * float2(0.5, -0.5) + 0.5;
        suv = clamp(suv, 0.0, 1.0);
        float3 seen = g_SceneColor.SampleLevel(g_Clamp, suv,
                                               rough * g_Slot[0].Glass.w).rgb;
        float3 tinted = seen * base.rgb;
        // Specular AND emission ride on top of what is transmitted:
        // glass shines, and edge-lit or neon glass glows, at any
        // transmission. Only the diffuse gives way.
        float3 specE = LinearToSrgb(specular / (specular + 1.0));
        float3 emitE = LinearToSrgb(emissive / (emissive + 1.0));
        float3 ownE = LinearToSrgb(diffuse / (diffuse + 1.0));
        encoded = lerp(ownE, tinted, transmission) + specE + emitE;
        alpha = lerp(alpha, 1.0, transmission);
    }
    return float4(encoded, alpha);
}
)";

/** The graphics shader with its slot and layer sections generated. */
std::string buildShaderSource() {
  const auto stamp = [](const char* text, int index) {
    std::string out;
    for (const char* p = text; *p; ++p)
      if (*p == '$')
        out += std::to_string(index);
      else
        out += *p;
    return out;
  };
  std::string head = kShaderHead;
  const auto replaceAll = [](std::string& s, const std::string& from,
                             const std::string& to) {
    for (size_t at = s.find(from); at != std::string::npos;
         at = s.find(from, at + to.size()))
      s.replace(at, from.size(), to);
  };
  replaceAll(head, "SLOTS", std::to_string(kSlots));
  replaceAll(head, "LAYERS", std::to_string(Material::kMaxLayers));
  std::string source = head;
  for (int k = 0; k < kSlots; ++k) source += stamp(kShaderSlotTextures, k);
  for (int i = 1; i <= Material::kMaxLayers; ++i)
    source += stamp(kShaderLayerTextures, i);
  source += kShaderCommon;
  for (int k = 0; k < kSlots; ++k) source += stamp(kShaderSlotSample, k);
  for (int i = 1; i <= Material::kMaxLayers; ++i)
    source += stamp(kShaderLayerMask, i);
  std::string body = kShaderBody;
  std::string blends;
  for (int i = 1; i <= Material::kMaxLayers; ++i)
    blends += "    if (g_LayerD[" + std::to_string(i - 1) +
              "].y > 0.5)\n        s = blendSurf(s, sampleSlot" +
              std::to_string(i) + "(IN.UV, IN.Tint), maskValue" +
              std::to_string(i) +
              "(IN.UV, IN.VColor, Ngeom, IN.World), (int)(g_LayerD[" +
              std::to_string(i - 1) + "].x + 0.5));\n";
  replaceAll(body, "LAYER_BLENDS\n", blends);
  source += body;
  return source;
}

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
  float env[4];  // x intensity, y rotation, z top mip, w present
};

/** One material slot's dials — the SlotConstants struct of the shader,
 *  member for member. */
struct SlotConstants {
  float baseColor[4];
  float emissive[4];
  float matParams[4];
  float uvScaleOffset[4];
  float mapParams[4];
  float channels[4];
  float glass[4];  // x ior, y thickness, z alphaCutoff, w scene top mip
  float flags[4];  // x tile
};

struct DrawConstants {
  Mat4 model;
  Mat4 normalMat;
  SlotConstants slot[kSlots];
  float layerA[Material::kMaxLayers]
              [4];  // mask source, channel/const, low, high
  float layerB[Material::kMaxLayers][4];  // mask axis, invert
  float layerC[Material::kMaxLayers][4];  // mask uv scale, offset
  float layerD[Material::kMaxLayers][4];  // blend mode, present
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
using geometry::detail::basisFor;

std::vector<InstanceAttribs> buildInstances(const geometry::Cloud& cloud,
                                            const StampLanes& lanes) {
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
  /** Every image the binding was built from, in slot order — the base's
   *  maps, then each layer's maps and its mask — plus each slot's tile
   *  flag, and the panorama (the frame's light is bound per prop, so a
   *  new panorama rebinds every prop once). */
  std::vector<const SkImage*> images;
  std::vector<bool> tiles;
  const SkImage* environment = nullptr;
  bool valid = false;  ///< false until the first bind
  static MaterialBinding of(const Material& m, const SkImage* environment) {
    MaterialBinding b;
    const auto one = [&](const Material& slot) {
      for (const sk_sp<SkImage>* image :
           {&slot.texture, &slot.normalMap, &slot.roughnessMap,
            &slot.metallicMap, &slot.occlusionMap, &slot.emissiveMap,
            &slot.opacityMap})
        b.images.push_back(image->get());
      b.tiles.push_back(slot.tile);
    };
    one(m);
    for (const Material::Layer& layer : m.layers) {
      one(layer.material);
      b.images.push_back(layer.mask.map.get());
      b.tiles.push_back(layer.mask.tile);
    }
    b.environment = environment;
    b.valid = true;
    return b;
  }
  bool operator==(const MaterialBinding&) const = default;
};

/** Every texture variable the pixel shader declares, in the generated
 *  scheme buildShaderSource() uses: seven maps per slot, one mask per
 *  layer, the panorama, the scene colour. */
std::vector<std::string> materialTextureNames() {
  std::vector<std::string> names;
  for (int k = 0; k < kSlots; ++k)
    for (const char* base :
         {"g_Texture", "g_NormalMap", "g_RoughnessMap", "g_MetallicMap",
          "g_OcclusionMap", "g_EmissiveMap", "g_OpacityMap"})
      names.push_back(std::string(base) + std::to_string(k));
  for (int i = 1; i <= Material::kMaxLayers; ++i)
    names.push_back("g_Mask" + std::to_string(i));
  names.emplace_back("g_Environment");
  names.emplace_back("g_SceneColor");
  return names;
}

struct GpuGeometry {
  dg::RefCntAutoPtr<dg::IBuffer> vertexBuffer;
  dg::RefCntAutoPtr<dg::IBuffer> indexBuffer;
  /** One index range per material slot, in slot order: range 0 wears
   *  MaterialComponent::material, range i its slots[i - 1]. A mesh with
   *  no "Material" prim lane is one range. The index buffer is uploaded
   *  in this order, triangles grouped by slot. */
  struct Range {
    uint32_t first = 0;
    uint32_t count = 0;
    dg::RefCntAutoPtr<dg::IShaderResourceBinding> srb;
    MaterialBinding bound;
  };
  std::vector<Range> ranges;
  uint32_t indexCount = 0;
};

/** The mesh's triangles grouped by material slot: @p order receives the
 *  index buffer contents (triangles reordered so each slot's are
 *  contiguous) and the returned ranges say where each slot's begin. The
 *  slot of a triangle is the .x of the mesh's "Material" prim lane,
 *  clamped into [0, slotCount); without that lane, or with one slot,
 *  everything is slot 0. */
std::vector<std::pair<uint32_t, uint32_t>> groupBySlot(
    const geometry::Mesh& mesh, int slotCount, std::vector<uint32_t>& order) {
  const std::vector<glm::vec4>* lane = mesh.primIf("Material");
  const size_t tris = mesh.triangleCount();
  slotCount = std::max(slotCount, 1);
  if (!lane || lane->size() != tris || slotCount == 1) {
    order = mesh.indices;
    return {{0u, (uint32_t)mesh.indices.size()}};
  }
  std::vector<std::vector<uint32_t>> buckets((size_t)slotCount);
  for (size_t t = 0; t < tris; ++t) {
    int slot = (int)std::floor((*lane)[t].x + 0.5f);
    slot = std::clamp(slot, 0, slotCount - 1);
    buckets[(size_t)slot].push_back((uint32_t)t);
  }
  order.clear();
  order.reserve(mesh.indices.size());
  std::vector<std::pair<uint32_t, uint32_t>> ranges;
  for (const std::vector<uint32_t>& bucket : buckets) {
    const uint32_t first = (uint32_t)order.size();
    for (uint32_t t : bucket)
      for (int k = 0; k < 3; ++k)
        order.push_back(mesh.indices[t * 3 + (size_t)k]);
    ranges.emplace_back(first, (uint32_t)order.size() - first);
  }
  return ranges;
}

/** The instanced sibling: one stamp's buffers plus the per-instance
 *  stream (placeStamps). instanceBuffer is null while the stamps are
 *  empty; setStamps() refreshes or recreates it. */
struct GpuInstancedGeometry {
  dg::RefCntAutoPtr<dg::IBuffer> vertexBuffer;
  dg::RefCntAutoPtr<dg::IBuffer> indexBuffer;
  dg::RefCntAutoPtr<dg::IBuffer> instanceBuffer;
  dg::RefCntAutoPtr<dg::IShaderResourceBinding> srb;
  MaterialBinding bound;
  uint32_t indexCount = 0;
  uint32_t instanceCount = 0;
};

/** placeSweep()'s private state: the loop resident on the GPU and the
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

/** placeChain()'s private state: the chain (a value — the
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
          if constexpr (std::is_same_v<T, World::pop::Fill>) note(o.attr);
          if constexpr (std::is_same_v<T, World::pop::Lookup>) {
            // Both ends: a lookup may read one custom lane and write
            // another, and each needs its own slot.
            note(o.from);
            note(o.to);
          }
          if constexpr (std::is_same_v<T, World::pop::Select>) {
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
          if constexpr (std::is_same_v<T, World::pop::PointSet>)
            for (const std::string& name :
                 geometry::popops::seedCustomNames(o.cloud))
              note(World::pop::AttrRef{name});
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
  geometry::space::Camera camera;
  Lighting lighting;

  dg::RefCntAutoPtr<dg::IRenderDevice> device;
  dg::RefCntAutoPtr<dg::IDeviceContext> context;

  dg::RefCntAutoPtr<dg::ITexture> colorTarget;    // MSAA when enabled
  dg::RefCntAutoPtr<dg::ITexture> resolveTarget;  // single-sample
  dg::RefCntAutoPtr<dg::ITexture> depthTarget;
  dg::RefCntAutoPtr<dg::ITexture> stagingTarget;
  /** The opaque pass, resolved and mipped, for transmissive surfaces
   *  to read; refreshed between the passes only in frames that have
   *  one. */
  dg::RefCntAutoPtr<dg::ITexture> sceneColor;
  int sceneColorMips = 1;
  int sampleCount = 1;

  dg::RefCntAutoPtr<dg::IPipelineState> opaquePso;
  dg::RefCntAutoPtr<dg::IPipelineState> blendPso;
  dg::RefCntAutoPtr<dg::IPipelineState> opaqueInstancedPso;
  dg::RefCntAutoPtr<dg::IPipelineState> blendInstancedPso;
  dg::RefCntAutoPtr<dg::IBuffer> frameCB;
  dg::RefCntAutoPtr<dg::IBuffer> drawCB;
  // The sweep generator, created lazily on the first placeSweep().
  dg::RefCntAutoPtr<dg::IPipelineState> sweepPso;
  dg::RefCntAutoPtr<dg::IBuffer> sweepCB;
  bool ensureSweepPipeline();
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
  dg::RefCntAutoPtr<dg::ISampler> panoramaSampler;  // wrap u, clamp v, mips
  dg::RefCntAutoPtr<dg::ITexture> blackTexture;
  /** The uploaded panorama and the image it came from (by pointer):
   *  setLighting compares and re-uploads only on a new image. */
  dg::RefCntAutoPtr<dg::ITexture> environmentTexture;
  const SkImage* environmentKey = nullptr;
  int environmentMips = 1;
  bool uploadEnvironment(const sk_sp<SkImage>& image);
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
  bool createMeshBuffers(const geometry::Mesh& mesh,
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

  desc.Name = "sigilworld scene colour";
  desc.Format = colorFormat;
  desc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
  desc.MiscFlags = MISC_TEXTURE_FLAG_GENERATE_MIPS;
  desc.SampleCount = 1;
  sceneColorMips = 1;
  for (int mw = config.width, mh = config.height; mw > 1 || mh > 1;
       ++sceneColorMips) {
    mw = std::max(mw / 2, 1);
    mh = std::max(mh / 2, 1);
  }
  desc.MipLevels = (Uint32)sceneColorMips;
  device->CreateTexture(desc, nullptr, &sceneColor);
  desc.MiscFlags = MISC_TEXTURE_FLAG_NONE;
  desc.MipLevels = 1;

  desc.Name = "sigilworld staging";
  desc.Format = colorFormat;
  desc.BindFlags = BIND_NONE;
  desc.SampleCount = 1;
  desc.Usage = USAGE_STAGING;
  desc.CPUAccessFlags = CPU_ACCESS_READ;
  device->CreateTexture(desc, nullptr, &stagingTarget);

  if (!colorTarget || !resolveTarget || !depthTarget || !stagingTarget ||
      !sceneColor) {
    if (error) *error = "offscreen target creation failed";
    return false;
  }
  return true;
}

bool World::Impl::createPipelines(std::string* error) {
  using namespace dg;

  ShaderCreateInfo shaderCI;
  shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  // Separate samplers: three shared ones for every texture, because
  // the device caps samplers per stage far below textures.
  shaderCI.Desc.UseCombinedTextureSamplers = false;
  const std::string shaderSource = buildShaderSource();
  shaderCI.Source = shaderSource.c_str();

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

  // Every texture the pixel shader samples is a mutable variable: the
  // per-slot sets, the per-layer masks, the panorama and the scene
  // colour. Named here in the same generated scheme the shader source
  // uses.
  const std::vector<std::string> textureNames = materialTextureNames();
  std::vector<ShaderResourceVariableDesc> variables;
  for (const std::string& name : textureNames)
    variables.push_back({SHADER_TYPE_PIXEL, name.c_str(),
                         SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE});
  psoCI.PSODesc.ResourceLayout.Variables = variables.data();
  psoCI.PSODesc.ResourceLayout.NumVariables = (Uint32)variables.size();

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
    // The panorama: seamless around (wrap u), pinned at the poles
    // (clamp v), and its mip chain is the roughness blur.
    samplerDesc.AddressU = TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
    samplerDesc.Name = "sigilworld panorama";
    device->CreateSampler(samplerDesc, &panoramaSampler);
    if (!clampSampler || !wrapSampler || !panoramaSampler) {
      if (error) *error = "sampler creation failed";
      return false;
    }
    sceneColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
        ->SetSampler(clampSampler);
  }
  // The three samplers are immutable on every graphics pipeline, under
  // the names the shader declares; a map picks clamp or repeat by a
  // per-slot flag at sample time.
  SamplerDesc clampDesc;
  clampDesc.MinFilter = FILTER_TYPE_LINEAR;
  clampDesc.MagFilter = FILTER_TYPE_LINEAR;
  clampDesc.MipFilter = FILTER_TYPE_LINEAR;
  clampDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
  clampDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
  SamplerDesc wrapDesc = clampDesc;
  wrapDesc.AddressU = TEXTURE_ADDRESS_WRAP;
  wrapDesc.AddressV = TEXTURE_ADDRESS_WRAP;
  SamplerDesc panoramaDesc = clampDesc;
  panoramaDesc.AddressU = TEXTURE_ADDRESS_WRAP;
  const ImmutableSamplerDesc immutableSamplers[] = {
      {SHADER_TYPE_PIXEL, "g_Clamp", clampDesc},
      {SHADER_TYPE_PIXEL, "g_Wrap", wrapDesc},
      {SHADER_TYPE_PIXEL, "g_Panorama", panoramaDesc},
  };
  psoCI.PSODesc.ResourceLayout.ImmutableSamplers = immutableSamplers;
  psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 3;

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
    desc.Name = "sigilworld black";
    const Uint32 black = 0xff000000u;
    TextureSubResData blackSub{&black, 4};
    TextureData blackData{&blackSub, 1};
    device->CreateTexture(desc, &blackData, &blackTexture);
  }
  if (!whiteTexture || !flatNormalTexture || !blackTexture) return false;
  whiteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
      ->SetSampler(clampSampler);
  flatNormalTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
      ->SetSampler(clampSampler);
  blackTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)
      ->SetSampler(clampSampler);
  return true;
}

bool World::Impl::uploadEnvironment(const sk_sp<SkImage>& image) {
  using namespace dg;
  environmentTexture.Release();
  environmentKey = image.get();
  environmentMips = 1;
  if (!image) return true;
  // Half float keeps an HDR panorama's range; the full mip chain is the
  // roughness blur the shader walks. Only level 0 is supplied; the
  // device generates the rest.
  const int w = image->width(), h = image->height();
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::Make(w, h, kRGBA_F16_SkColorType, kUnpremul_SkAlphaType));
  if (!image->readPixels(nullptr, bitmap.pixmap(), 0, 0)) return false;
  int mips = 1;
  for (int mw = w, mh = h; mw > 1 || mh > 1; ++mips) {
    mw = std::max(mw / 2, 1);
    mh = std::max(mh / 2, 1);
  }
  TextureDesc desc;
  desc.Name = "sigilworld environment";
  desc.Type = RESOURCE_DIM_TEX_2D;
  desc.Width = (Uint32)w;
  desc.Height = (Uint32)h;
  desc.Format = TEX_FORMAT_RGBA16_FLOAT;
  desc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
  desc.MiscFlags = MISC_TEXTURE_FLAG_GENERATE_MIPS;
  desc.MipLevels = (Uint32)mips;
  device->CreateTexture(desc, nullptr, &environmentTexture);
  if (!environmentTexture) return false;
  // Level 0 is written after creation, then the chain is generated —
  // creating with partial initial data (only level 0 supplied) is not a
  // path the device takes.
  TextureSubResData subres{bitmap.getPixels(), (Uint64)bitmap.rowBytes()};
  Box box(0, (Uint32)w, 0, (Uint32)h);
  context->UpdateTexture(environmentTexture, 0, 0, box, subres,
                         RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                         RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  ITextureView* view =
      environmentTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
  view->SetSampler(panoramaSampler);
  context->GenerateMips(view);
  environmentMips = mips;
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
  const auto set = [&](const std::string& name, const sk_sp<SkImage>& image,
                       bool srgb, bool tile, ITexture* standIn) {
    RefCntAutoPtr<ITexture> texture = image ? uploadTexture(image, srgb, tile)
                                            : RefCntAutoPtr<ITexture>(standIn);
    if (auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, name.c_str()))
      var->Set(texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
  };
  // Slot k: the base (k = 0) or layer k - 1; unused slots take the
  // stand-ins so every variable is bound.
  for (int k = 0; k < kSlots; ++k) {
    const Material* slot = k == 0
                               ? &material
                               : (k - 1 < (int)material.layers.size()
                                      ? &material.layers[(size_t)k - 1].material
                                      : nullptr);
    static const Material kNone;
    const Material& m = slot ? *slot : kNone;
    const std::string sk = std::to_string(k);
    set("g_Texture" + sk, m.texture, true, m.tile, whiteTexture);
    set("g_NormalMap" + sk, m.normalMap, false, m.tile, flatNormalTexture);
    set("g_RoughnessMap" + sk, m.roughnessMap, false, m.tile, whiteTexture);
    set("g_MetallicMap" + sk, m.metallicMap, false, m.tile, whiteTexture);
    set("g_OcclusionMap" + sk, m.occlusionMap, false, m.tile, whiteTexture);
    set("g_EmissiveMap" + sk, m.emissiveMap, true, m.tile, whiteTexture);
    set("g_OpacityMap" + sk, m.opacityMap, false, m.tile, whiteTexture);
  }
  for (int i = 1; i <= Material::kMaxLayers; ++i) {
    const Mask* mask = i - 1 < (int)material.layers.size()
                           ? &material.layers[(size_t)i - 1].mask
                           : nullptr;
    set("g_Mask" + std::to_string(i), mask ? mask->map : nullptr, false,
        mask ? mask->tile : false, whiteTexture);
  }
  if (auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_SceneColor"))
    var->Set(sceneColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
  if (auto* var = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_Environment"))
    var->Set((environmentTexture ? environmentTexture : blackTexture)
                 ->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
  bound = MaterialBinding::of(material, environmentKey);
  return true;
}

void World::Impl::writeDrawConstants(const glm::mat4& model,
                                     const Material& m) {
  using namespace dg;
  MapHelper<DrawConstants> constants(context, drawCB, MAP_WRITE,
                                     MAP_FLAG_DISCARD);
  constants->model = colMajor(model);
  constants->normalMat = normalMatrix(model);
  const auto channel = [](int c) { return (float)std::clamp(c, 0, 3); };
  const auto writeSlot = [&](SlotConstants& c, const Material& s) {
    c.baseColor[0] = s.baseColor.x;
    c.baseColor[1] = s.baseColor.y;
    c.baseColor[2] = s.baseColor.z;
    c.baseColor[3] = s.baseColor.w;
    c.emissive[0] = s.emissive.x;
    c.emissive[1] = s.emissive.y;
    c.emissive[2] = s.emissive.z;
    c.emissive[3] = 1;
    c.matParams[0] = s.metallic;
    c.matParams[1] = s.roughness;
    c.matParams[2] = s.emissiveStrength;
    c.matParams[3] = 0;
    c.uvScaleOffset[0] = s.uvScale.x;
    c.uvScaleOffset[1] = s.uvScale.y;
    c.uvScaleOffset[2] = s.uvOffset.x;
    c.uvScaleOffset[3] = s.uvOffset.y;
    c.mapParams[0] = s.normalMap ? s.normalScale : 0.0f;
    c.mapParams[1] = s.occlusionMap ? s.occlusionStrength : 0.0f;
    c.mapParams[2] = s.normalMapDirectX ? 1.0f : 0.0f;
    c.mapParams[3] = std::clamp(s.transmission, 0.0f, 1.0f);
    c.channels[0] = channel(s.roughnessChannel);
    c.channels[1] = channel(s.metallicChannel);
    c.channels[2] = channel(s.occlusionChannel);
    c.channels[3] = channel(s.opacityChannel);
    c.glass[0] = s.ior;
    c.glass[1] = s.thickness;
    c.glass[2] = 0;
    c.glass[3] = 0;
    c.flags[0] = s.tile ? 1.0f : 0.0f;
    c.flags[1] = c.flags[2] = c.flags[3] = 0;
  };
  static const Material kNone;
  for (int k = 0; k < kSlots; ++k) {
    const Material& slot = k == 0 ? m
                                  : (k - 1 < (int)m.layers.size()
                                         ? m.layers[(size_t)k - 1].material
                                         : kNone);
    writeSlot(constants->slot[k], slot);
  }
  // Prop-wide dials ride slot 0.
  constants->slot[0].matParams[3] = m.unlit ? 1.0f : 0.0f;
  constants->slot[0].glass[2] = m.alphaCutoff;
  constants->slot[0].glass[3] = (float)std::max(sceneColorMips - 1, 0);
  for (int i = 0; i < Material::kMaxLayers; ++i) {
    float* A = constants->layerA[i];
    float* B = constants->layerB[i];
    float* C = constants->layerC[i];
    float* D = constants->layerD[i];
    A[0] = A[1] = A[2] = A[3] = 0;
    B[0] = B[1] = B[2] = B[3] = 0;
    C[0] = C[1] = C[2] = C[3] = 0;
    D[0] = D[1] = D[2] = D[3] = 0;
    if (i >= (int)m.layers.size()) continue;
    const Material::Layer& layer = m.layers[(size_t)i];
    const Mask& mask = layer.mask;
    A[0] = (float)mask.source;
    A[1] = mask.source == Mask::Source::Constant ? mask.value
                                                 : channel(mask.channel);
    A[2] = mask.low;
    A[3] = mask.high;
    B[0] = mask.axis.x;
    B[1] = mask.axis.y;
    B[2] = mask.axis.z;
    B[3] = mask.inverted ? 1.0f : 0.0f;
    C[0] = mask.uvScale.x;
    C[1] = mask.uvScale.y;
    C[2] = mask.uvOffset.x;
    C[3] = mask.uvOffset.y;
    D[0] = (float)layer.blend;
    D[1] = 1;
    D[2] = mask.tile ? 1.0f : 0.0f;
  }
}

namespace {

std::vector<Vertex> packVertices(const geometry::Mesh& mesh) {
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
    const geometry::Mesh& mesh, dg::RefCntAutoPtr<dg::IBuffer>& vertexBuffer,
    dg::RefCntAutoPtr<dg::IBuffer>& indexBuffer) {
  using namespace dg;
  const std::vector<Vertex> vertices = packVertices(mesh);

  // DEFAULT rather than IMMUTABLE usage, so setMesh() can update
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
  // DEFAULT usage, so setStamps() can update it in place when the
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
    "CSSplineScatter", "CSJitter",   "CSNoise",    "CSRamp",    "CSVary",
    "CSLookAt",        "CSMath",     "CSRelax",    "CSFill",    "CSAtlas",
    "CSLookup",        "CSSelect",   "CSAffine",   "CSPeak",    "CSDeform",
    "CSMix",           "CSPointSet", "CSCopyBack", "CSPopPack",
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
    16,            // 19 PointSet — its lanes are the arena's initial
                   //             upload; the kernel is empty
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
World::~World() {
  // Anything recorded but not submitted (an add with no render after it)
  // is flushed before the context goes, so teardown never leaves
  // commands behind.
  if (m_impl && m_impl->context) m_impl->context->Flush();
}

std::unique_ptr<World> World::create(const WorldConfig& config,
                                     std::string* error) {
  std::unique_ptr<World> world(new World());
  world->m_impl->config = config;
  if (!world->m_impl->init(error)) return nullptr;
  return world;
}

uint32_t World::place(const geometry::Mesh& mesh, const glm::mat4& model,
                      const Material& material) {
  return place(mesh, model, std::vector<Material>{material});
}

uint32_t World::place(const geometry::Mesh& mesh, const glm::mat4& model,
                      const std::vector<Material>& slots) {
  using namespace dg;
  Impl& impl = *m_impl;
  if (mesh.positions.empty() || mesh.indices.empty() || slots.empty()) return 0;

  // Triangles grouped by slot; the index buffer holds them in that
  // order and each range draws with its slot's material.
  std::vector<uint32_t> order;
  const std::vector<std::pair<uint32_t, uint32_t>> ranges =
      groupBySlot(mesh, (int)slots.size(), order);
  geometry::Mesh grouped = mesh;
  grouped.indices = std::move(order);

  GpuGeometry geometry;
  geometry.indexCount = (uint32_t)grouped.indices.size();
  if (!impl.createMeshBuffers(grouped, geometry.vertexBuffer,
                              geometry.indexBuffer))
    return 0;
  for (size_t i = 0; i < ranges.size(); ++i) {
    GpuGeometry::Range range;
    range.first = ranges[i].first;
    range.count = ranges[i].second;
    if (!impl.bindMaterial(impl.opaquePso, slots[i], range.srb, range.bound))
      return 0;
    geometry.ranges.push_back(std::move(range));
  }

  MaterialComponent component;
  component.material = slots.front();
  component.slots.assign(slots.begin() + 1, slots.end());
  const entt::entity id = impl.registry.create();
  impl.registry.emplace<GpuGeometry>(id, std::move(geometry));
  impl.registry.emplace<TransformComponent>(id, model);
  impl.registry.emplace<MaterialComponent>(id, std::move(component));
  return (uint32_t)id;
}

uint32_t World::placeStamps(const geometry::Mesh& stamp,
                            const geometry::Cloud& cloud,
                            const Material& material, const StampLanes& lanes) {
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
  // One transform for all the stamps, starting at identity: the points
  // carry their own placement, and setTransform moves all of them
  // together.
  impl.registry.emplace<TransformComponent>(id, glm::mat4(1.0f));
  impl.registry.emplace<MaterialComponent>(id, material);
  return (uint32_t)id;
}

void World::setStamps(uint32_t id, const geometry::Cloud& cloud,
                      const StampLanes& lanes) {
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

void World::setMesh(uint32_t id, const geometry::Mesh& mesh) {
  using namespace dg;
  Impl& impl = *m_impl;
  const entt::entity e = entity(id);
  if (!impl.registry.valid(e) || !impl.registry.all_of<GpuGeometry>(e)) return;
  if (mesh.positions.empty() || mesh.indices.empty()) return;
  GpuGeometry& geometry = impl.registry.get<GpuGeometry>(e);
  // The new mesh's triangles grouped by the slots the prop already
  // wears; a slot count that stays put keeps its bindings.
  const int slotCount = (int)geometry.ranges.size();
  std::vector<uint32_t> order;
  const std::vector<std::pair<uint32_t, uint32_t>> ranges =
      groupBySlot(mesh, slotCount, order);
  for (size_t i = 0; i < ranges.size() && i < geometry.ranges.size(); ++i) {
    geometry.ranges[i].first = ranges[i].first;
    geometry.ranges[i].count = ranges[i].second;
  }
  const Uint64 vertexBytes = (Uint64)(mesh.positions.size() * sizeof(Vertex));
  const Uint64 indexBytes = (Uint64)(order.size() * sizeof(uint32_t));
  if (geometry.vertexBuffer && geometry.indexBuffer &&
      geometry.vertexBuffer->GetDesc().Size == vertexBytes &&
      geometry.indexBuffer->GetDesc().Size == indexBytes) {
    const std::vector<Vertex> vertices = packVertices(mesh);
    impl.context->UpdateBuffer(geometry.vertexBuffer, 0, vertexBytes,
                               vertices.data(),
                               RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    impl.context->UpdateBuffer(geometry.indexBuffer, 0, indexBytes,
                               order.data(),
                               RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    return;
  }
  geometry::Mesh grouped = mesh;
  grouped.indices = std::move(order);
  geometry.indexCount = (uint32_t)grouped.indices.size();
  geometry.vertexBuffer.Release();
  geometry.indexBuffer.Release();
  impl.createMeshBuffers(grouped, geometry.vertexBuffer, geometry.indexBuffer);
}

uint32_t World::placeSweep(const SweepDesc& desc, const Material& material) {
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

  GpuGeometry::Range whole;
  whole.first = 0;
  whole.count = geometry.indexCount;
  if (!impl.bindMaterial(impl.opaquePso, material, whole.srb, whole.bound))
    return 0;
  geometry.ranges.push_back(std::move(whole));

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
  if (const auto* given = std::get_if<World::pop::PointSet>(&chain.front()))
    return (int)given->cloud.size();
  return 0;
}

/** The arena's initial contents for a chain led by a PointSet: the
 *  cloud laid out by geometry::popops::seedAttrs into the builtin slots and
 *  the chain's custom slots, in the same slot order slotFor() reads. */
std::vector<float> seededLanes(const geometry::Cloud& cloud, int count,
                               const std::vector<std::string>& customNames) {
  std::map<std::string, std::vector<glm::vec4>, std::less<>> lanes;
  geometry::popops::seedAttrs(cloud, lanes);
  const int slots = World::pop::kBuiltinSlots + (int)customNames.size();
  std::vector<float> data((size_t)count * (size_t)slots * 4, 0.0f);
  const auto pour = [&](int slot, const std::vector<glm::vec4>& lane) {
    for (int i = 0; i < count && (size_t)i < lane.size(); ++i) {
      float* dst = &data[((size_t)slot * (size_t)count + (size_t)i) * 4];
      dst[0] = lane[(size_t)i].x;
      dst[1] = lane[(size_t)i].y;
      dst[2] = lane[(size_t)i].z;
      dst[3] = lane[(size_t)i].w;
    }
  };
  for (const auto& [name, lane] : lanes) {
    const int32_t builtin = World::pop::builtinIndex(name);
    if (builtin >= 0) {
      pour(builtin, lane);
      continue;
    }
    for (size_t c = 0; c < customNames.size(); ++c)
      if (customNames[c] == name)
        pour(World::pop::kBuiltinSlots + (int)c, lane);
  }
  return data;
}

dg::RefCntAutoPtr<dg::IBuffer> createLaneBuffer(
    dg::IRenderDevice* device, int count, int slots, const char* name,
    const std::vector<float>* initial = nullptr) {
  using namespace dg;
  // Zero-filled so custom lanes start at {0,0,0,0}, matching the CPU
  // executor's default for an untouched attribute; the builtin lanes are
  // initialized by the generator kernel before anything reads them — or
  // uploaded here outright when the chain is led by a point set.
  const std::vector<float> zeros(
      initial ? *initial
              : std::vector<float>((size_t)count * (size_t)slots * 4, 0.0f));
  BufferDesc desc;
  desc.Name = name;
  desc.Usage = USAGE_DEFAULT;
  // SHADER_RESOURCE as well as UAV, because a downstream chain built
  // with placeChainOn reads this arena's position slot as its own loop.
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
 *  setChain takes the structural path when it sees a different table
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

uint32_t World::placeChain(const geometry::Mesh& stamp, const pop::Chain& chain,
                           const Material& material) {
  using namespace dg;
  Impl& impl = *m_impl;
  const int count = popChainCount(chain);
  if (stamp.positions.empty() || stamp.indices.empty() || count < 1) return 0;
  if (!impl.ensurePopPipelines()) return 0;

  PopComponent points;
  points.chain = chain;
  points.count = count;
  points.customNames = popCustomNames(chain);
  const int slots = pop::kBuiltinSlots + (int)points.customNames.size();
  if (const auto* scatter = std::get_if<pop::SplineScatter>(&chain.front())) {
    points.loopCount = (int)scatter->loop.size();
    points.loop = impl.createLoopBuffer(scatter->loop);
    points.lanes = createLaneBuffer(impl.device, count, slots, "pop lanes");
  } else {
    // A point set: no loop to walk (the binding still wants a buffer),
    // and the arena arrives filled.
    const auto& given = std::get<pop::PointSet>(chain.front());
    points.loopCount = 0;
    points.loop = impl.createLoopBuffer({{0, 0, 0}});
    const std::vector<float> seeded =
        seededLanes(given.cloud, count, points.customNames);
    points.lanes =
        createLaneBuffer(impl.device, count, slots, "pop lanes", &seeded);
  }
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

uint32_t World::placeChainOn(uint32_t upstream, const geometry::Mesh& stamp,
                             const pop::Chain& chain,
                             const Material& material) {
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

void World::setChainWindow(uint32_t id, float head, float span) {
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

void World::setChain(uint32_t id, const pop::Chain& chain) {
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
  // A point set's cloud IS the arena's contents: any re-describe of such
  // a chain re-uploads, so it always takes the structural path.
  const auto* given = std::get_if<pop::PointSet>(&chain.front());
  const bool sameShape =
      !given && count == points.count && chain.size() == points.chain.size() &&
      popCustomNames(chain) == points.customNames &&
      table == points.tableData &&
      std::equal(chain.begin(), chain.end(), points.chain.begin(),
                 [](const pop::Op& a, const pop::Op& b) {
                   return a.index() == b.index();
                 });
  // The loop rides its own immutable buffer, uploaded when the surface
  // was added, so a re-describe with MOVED control points must recreate
  // it or the kernels go on cooking the old loop. Compared before the
  // chain is overwritten. A surface created with placeChainOn carries an
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
    std::vector<float> seeded;
    if (given) seeded = seededLanes(given->cloud, count, points.customNames);
    points.lanes = createLaneBuffer(
        impl.device, count, pop::kBuiltinSlots + (int)points.customNames.size(),
        "pop lanes", given ? &seeded : nullptr);
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
      // Allocation failed partway. Zero the count so readChain() cannot
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

geometry::Cloud World::readChain(uint32_t id) {
  using namespace dg;
  Impl& impl = *m_impl;
  geometry::Cloud out;
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

void World::remove(uint32_t id) {
  entt::registry& registry = m_impl->registry;
  const entt::entity e = entity(id);
  if (registry.valid(e) &&
      registry.any_of<GpuGeometry, GpuInstancedGeometry>(e))
    registry.destroy(e);
}

size_t World::propCount() const {
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

void World::setCamera(const geometry::space::Camera& camera) {
  m_impl->camera = camera;
}

void World::setLighting(const Lighting& lighting) {
  m_impl->lighting = lighting;
  if (lighting.environment.get() != m_impl->environmentKey)
    m_impl->uploadEnvironment(lighting.environment);
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
        // tangent; must match the point kernels' value below.
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
        // Refreshed every cook, so a setChain() on the upstream that
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
        // Old state UNKNOWN: taken from the resource itself. The scratch
        // buffer sits in the copy state its upload left it in until the
        // first Relax touches it, so a fixed "was UAV" would be wrong
        // for every chain without one.
        StateTransitionDesc barriers[] = {
            {points.lanes, RESOURCE_STATE_UNKNOWN,
             RESOURCE_STATE_UNORDERED_ACCESS,
             STATE_TRANSITION_FLAG_UPDATE_STATE},
            {points.scratch, RESOURCE_STATE_UNKNOWN,
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
              } else if constexpr (std::is_same_v<T, pop::Fill>) {
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
              } else if constexpr (std::is_same_v<T, pop::Select>) {
                put3(params.a, op.center);
                put3(params.b, op.size);
                params.c[0] = op.feather;
                params.c[1] = (float)op.shape;
                params.c[2] = op.invert ? 1.0f : 0.0f;
                params.c[3] = (float)op.combine;
                params.d[1] = (float)points.slotFor(pop::AttrRef{op.to});
                params.m[1] = (float)points.slotFor(op.from);
              } else if constexpr (std::is_same_v<T, pop::Affine>) {
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
                geometry::popops::deformFrame(op, &axis, &dir, &side);
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
  geometry::space::Camera cam = impl.camera;
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
    constants->params[1] = 0;
    constants->params[2] = 1.0f / (float)impl.config.width;
    constants->params[3] = 1.0f / (float)impl.config.height;

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
    constants->env[0] = impl.lighting.environmentIntensity;
    constants->env[1] =
        impl.lighting.environmentRotationDeg * (float)M_PI / 180.0f;
    constants->env[2] = (float)std::max(impl.environmentMips - 1, 0);
    constants->env[3] = impl.environmentTexture ? 1.0f : 0.0f;
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
    size_t range = 0;  ///< which slot's index range, for GpuGeometry
  };
  std::vector<DrawItem> opaque, blended;
  for (auto [e, geometry, transform, material] :
       impl.registry.view<GpuGeometry, TransformComponent, MaterialComponent>()
           .each()) {
    // One item per slot range, each routed by its own material.
    for (size_t r = 0; r < geometry.ranges.size(); ++r) {
      if (geometry.ranges[r].count == 0) continue;
      DrawItem item;
      item.geometry = &geometry;
      item.model = &transform.model;
      item.material =
          r == 0 ? &material.material
                 : (r - 1 < material.slots.size() ? &material.slots[r - 1]
                                                  : &material.material);
      item.range = r;
      (item.material->blended() ? blended : opaque).push_back(item);
    }
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
    (material.material.blended() ? blended : opaque).push_back(item);
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
      const MaterialBinding want =
          MaterialBinding::of(*item.material, impl.environmentKey);
      if (item.instanced && !(item.instanced->bound == want))
        impl.bindMaterial(impl.opaqueInstancedPso, *item.material,
                          item.instanced->srb, item.instanced->bound);
      if (item.geometry && !(item.geometry->ranges[item.range].bound == want))
        impl.bindMaterial(impl.opaquePso, *item.material,
                          item.geometry->ranges[item.range].srb,
                          item.geometry->ranges[item.range].bound);
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
        const GpuGeometry::Range& range = geometry.ranges[item.range];
        impl.context->CommitShaderResources(
            range.srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        IBuffer* vb = geometry.vertexBuffer;
        const Uint64 offset = 0;
        impl.context->SetVertexBuffers(
            0, 1, &vb, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            SET_VERTEX_BUFFERS_FLAG_RESET);
        impl.context->SetIndexBuffer(geometry.indexBuffer, 0,
                                     RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        attribs.FirstIndexLocation = range.first;
        attribs.NumIndices = range.count;
      }
      impl.context->DrawIndexed(attribs);
    }
  };
  drawList(opaque, impl.opaquePso, impl.opaqueInstancedPso);
  bool anyGlass = false;
  for (const DrawItem& item : blended)
    anyGlass |= item.material->transmission > 0;
  if (anyGlass) {
    // Glass looks through the opaque pass: resolve (or copy) it into
    // the scene-colour texture, build its mip chain for the roughness
    // blur, and rebind the targets — the resolve unbinds them.
    if (opaque.empty()) impl.context->Flush();
    if (msaa) {
      ResolveTextureSubresourceAttribs resolve;
      resolve.SrcTextureTransitionMode =
          RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
      resolve.DstTextureTransitionMode =
          RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
      impl.context->ResolveTextureSubresource(impl.colorTarget, impl.sceneColor,
                                              resolve);
    } else {
      CopyTextureAttribs copy;
      copy.pSrcTexture = impl.resolveTarget;
      copy.pDstTexture = impl.sceneColor;
      copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
      copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
      impl.context->CopyTexture(copy);
    }
    impl.context->GenerateMips(
        impl.sceneColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    impl.context->SetRenderTargets(1, &rtv, dsv,
                                   RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }
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
