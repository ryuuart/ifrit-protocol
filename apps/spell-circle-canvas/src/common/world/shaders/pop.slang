// The pop kernel, authored in Slang over the sigilspline module. All
// attributes — builtins AND custom names — live in ONE lane arena:
// g_Lanes holds slotCount lanes of count float4s each, addressed
// slot * count + i. Builtin slots are fixed (P=0, T=1, Dir=2,
// Scale=3, Color=4, Tex=5); the CPU side assigns custom names the
// slots above. This mirrors shape::popops' named Attrs store exactly
// — one description, two executors, identical semantics (the numeric
// parity test is the gate). Whole-element writes and a single
// element-struct per buffer keep MoltenVK/SPIRV-Cross honest.
import sigilspline;

#include "WorldShaderParams.h"
ConstantBuffer<PopParamsData> PopParams;

StructuredBuffer<LoopPoint> g_Points;
struct Lane4 { float x, y, z, w; }
RWStructuredBuffer<Lane4> g_Lanes;
struct Instance { float4 r0, r1, r2, tint, tex; }
RWStructuredBuffer<Instance> g_Instances;
struct LaneScratch { float x, y, z, w; }
RWStructuredBuffer<LaneScratch> g_Scratch;

uint pointTotal() { return uint(PopParams.d.x); }
int loopCount() { return int(PopParams.d.z); }

float4 loadSlot(int slot, uint i)
{
    Lane4 v = g_Lanes[uint(slot) * pointTotal() + i];
    return float4(v.x, v.y, v.z, v.w);
}

void storeSlot(int slot, uint i, float4 v)
{
    Lane4 c;
    c.x = v.x; c.y = v.y; c.z = v.z; c.w = v.w;
    g_Lanes[uint(slot) * pointTotal() + i] = c;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSSplineScatter(uint3 id: SV_DispatchThreadID)
{
    uint count = pointTotal();
    if (id.x >= count) return;
    int n = loopCount();
    uint seed = uint(PopParams.a.w);
    float u0 = (float(id.x) + 0.5) / float(count);
    float t = PopParams.a.x - PopParams.a.y + PopParams.a.y * u0 +
              (hash1(id.x * 3u + seed) - 0.5) *
                  (PopParams.a.y / float(count)) * 4.0;
    float3 p = splineAt(g_Points, n, t);
    float3 tangent = splineAt(g_Points, n, t + PopParams.d.w) -
                     splineAt(g_Points, n, t - PopParams.d.w);
    float len = length(tangent);
    tangent = len > 1e-6 ? tangent / len : float3(1.0, 0.0, 0.0);
    float3 n0 = abs(tangent.y) < 0.9
                    ? cross(tangent, float3(0.0, 1.0, 0.0))
                    : cross(tangent, float3(1.0, 0.0, 0.0));
    n0 = normalize(n0);
    float3 b0 = cross(tangent, n0);
    float ang = hash1(id.x * 7u + seed + 2u) * 6.2831853;
    float rad = sqrt(hash1(id.x * 5u + seed + 3u)) * PopParams.a.z;
    p += (n0 * cos(ang) + b0 * sin(ang)) * rad;
    storeSlot(0, id.x, float4(p, 0.0));                    // P
    storeSlot(1, id.x, float4(u0, 0.0, 0.0, 0.0));          // T
    storeSlot(2, id.x, float4(tangent, 0.0));               // Dir
    storeSlot(3, id.x, float4(1.0, 1.0, 1.0, 1.0));         // Scale
    storeSlot(4, id.x, float4(1.0, 1.0, 1.0, 1.0));         // Color
    storeSlot(5, id.x, float4(0.0, 0.0, 1.0, 1.0));         // Tex
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSJitter(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    int slot = int(PopParams.d.y);
    uint seed = uint(PopParams.a.y);
    float4 v = loadSlot(slot, id.x);
    v.x += (hash1(id.x * 3u + seed) - 0.5) * 2.0 * PopParams.a.x;
    v.y += (hash1(id.x * 5u + seed + 1u) - 0.5) * 2.0 * PopParams.a.x;
    v.z += (hash1(id.x * 7u + seed + 2u) - 0.5) * 2.0 * PopParams.a.x;
    storeSlot(slot, id.x, v);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSNoise(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    int slot = int(PopParams.d.y);
    float4 pv = loadSlot(0, id.x);
    float3 dd = drift(float3(pv.x, pv.y, pv.z), PopParams.a.y,
                      PopParams.a.z) *
                PopParams.a.x;
    float4 v = loadSlot(slot, id.x);
    v.x += dd.x; v.y += dd.y; v.z += dd.z;
    storeSlot(slot, id.x, v);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSRamp(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    float t = loadSlot(1, id.x).x;
    storeSlot(int(PopParams.d.y), id.x,
              lerp(PopParams.a, PopParams.b, t));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSVary(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    uint seed = uint(PopParams.a.z);
    float v = PopParams.a.x *
              (1.0 + PopParams.a.y * (hash1(id.x * 11u + seed) * 2.0 - 1.0));
    storeSlot(int(PopParams.d.y), id.x, float4(v, v, v, v));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSLookAt(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    float4 pv = loadSlot(0, id.x);
    float3 d = float3(PopParams.a.x, PopParams.a.y, PopParams.a.z) -
               float3(pv.x, pv.y, pv.z);
    float len = length(d);
    d = len > 1e-6 ? d / len : float3(0.0, 0.0, 1.0);
    float4 cur = loadSlot(2, id.x);
    storeSlot(2, id.x, float4(d, cur.w));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSMath(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    int slot = int(PopParams.d.y);
    storeSlot(slot, id.x,
              loadSlot(slot, id.x) * PopParams.a + PopParams.b);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSRelax(uint3 id: SV_DispatchThreadID)
{
    uint count = pointTotal();
    if (id.x >= count) return;
    int slot = int(PopParams.d.y);
    uint a = id.x == 0u ? 0u : id.x - 1u;
    uint b = id.x + 1u < count ? id.x + 1u : id.x;
    float4 mid = (loadSlot(slot, a) + loadSlot(slot, b)) * 0.5;
    float4 v = loadSlot(slot, id.x);
    float4 r = v + (mid - v) * PopParams.a.x;
    LaneScratch s;
    s.x = r.x; s.y = r.y; s.z = r.z; s.w = r.w;
    g_Scratch[id.x] = s;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSSet(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    storeSlot(int(PopParams.d.y), id.x, PopParams.a);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSAtlas(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    int cols = max(int(PopParams.a.x), 1);
    int rows = max(int(PopParams.a.y), 1);
    uint seed = uint(PopParams.a.z);
    float du = 1.0 / float(cols);
    float dv = 1.0 / float(rows);
    int cells = max(cols * rows, 1);
    int cell = min(int(hash1(id.x * 13u + seed) * float(cells)),
                   cells - 1);
    storeSlot(5, id.x,
              float4(float(cell % cols) * du, float(cell / cols) * dv,
                     du, dv));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSCopyBack(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    LaneScratch s = g_Scratch[id.x];
    storeSlot(int(PopParams.d.y), id.x, float4(s.x, s.y, s.z, s.w));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSPopPack(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    float4 pv = loadSlot(0, id.x);
    float4 dv = loadSlot(2, id.x);
    float3 p = float3(pv.x, pv.y, pv.z);
    float3 zi = float3(dv.x, dv.y, dv.z);
    float zl = length(zi);
    zi = zl > 1e-6 ? zi / zl : float3(0.0, 0.0, 1.0);
    float s = loadSlot(3, id.x).x;
    float3 xi = cross(float3(0.0, 1.0, 0.0), zi);
    float xl = length(xi);
    // Degenerate (zi ≈ ±Y): shape::detail::basisFor's fallback axis.
    xi = xl > 1e-4 ? xi / xl
                   : normalize(cross(float3(1.0, 0.0, 0.0), zi));
    float3 yi = cross(zi, xi);
    Instance inst;
    inst.r0 = float4(xi.x * s, yi.x * s, zi.x * s, p.x);
    inst.r1 = float4(xi.y * s, yi.y * s, zi.y * s, p.y);
    inst.r2 = float4(xi.z * s, yi.z * s, zi.z * s, p.z);
    inst.tint = loadSlot(4, id.x);
    inst.tex = loadSlot(5, id.x);
    g_Instances[id.x] = inst;
}
