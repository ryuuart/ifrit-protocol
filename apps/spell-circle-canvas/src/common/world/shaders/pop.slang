// The pop kernel, authored in Slang over the sigilspline module. All
// attributes — builtins AND custom names — live in ONE lane arena:
// g_Lanes holds slotCount lanes of count float4s each, addressed
// slot * count + i. Builtin slots are fixed (P=0, T=1, Dir=2,
// Scale=3, Color=4, Tex=5); the CPU side assigns custom names the
// slots above. This mirrors geometry::pop' named Attrs store exactly
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
// Every Lookup op's stop table, concatenated in chain order; each op
// carries its own (offset, count) in PopParams. Its own element struct
// — SPIRV-Cross names the MSL struct after the first variable, so two
// buffers may not share one.
struct Stop4 { float x, y, z, w; }
StructuredBuffer<Stop4> g_Table;

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

// The mask every filter shares — a lane's .x clamped to [0, 1], 1 when
// no lane is named (m.x < 0). geometry::pop' storeMasked is this pair,
// expression for expression: m == 1 stores the new value outright, so an
// unmasked write stays bit-identical to a direct store.
float maskAt(uint i)
{
    int slot = int(PopParams.m.x);
    if (slot < 0) return 1.0;
    return clamp(loadSlot(slot, i).x, 0.0, 1.0);
}

void storeMasked(int slot, uint i, float4 v)
{
    float mk = maskAt(i);
    if (mk >= 1.0) { storeSlot(slot, i, v); return; }
    float4 old = loadSlot(slot, i);
    storeSlot(slot, i, old + (v - old) * mk);
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
    storeMasked(slot, id.x, v);
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
    storeMasked(slot, id.x, v);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSRamp(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    float t = loadSlot(1, id.x).x;
    storeMasked(int(PopParams.d.y), id.x,
                PopParams.a + (PopParams.b - PopParams.a) * t);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSVary(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    uint seed = uint(PopParams.a.z);
    float v = PopParams.a.x *
              (1.0 + PopParams.a.y * (hash1(id.x * 11u + seed) * 2.0 - 1.0));
    storeMasked(int(PopParams.d.y), id.x, float4(v, v, v, v));
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
    storeMasked(2, id.x, float4(d, cur.w));
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSMath(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    int slot = int(PopParams.d.y);
    storeMasked(slot, id.x,
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
    float4 relaxed = v + (mid - v) * PopParams.a.x;
    // Masked before the scratch write, so neighbours read the old value
    // this pass — the CPU twin blends at the same point.
    float mk = maskAt(id.x);
    float4 r = mk >= 1.0 ? relaxed : v + (relaxed - v) * mk;
    LaneScratch s;
    s.x = r.x; s.y = r.y; s.z = r.z; s.w = r.w;
    g_Scratch[id.x] = s;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSFill(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    storeMasked(int(PopParams.d.y), id.x, PopParams.a);
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
    storeMasked(5, id.x,
                float4(float(cell % cols) * du, float(cell / cols) * dv,
                       du, dv));
}

// Drive one attribute from another through a stop table. The twin of
// geometry::pop' Lookup branch, formula for formula: same left-to-right
// key sum, same clamp, same floor, same a + (b - a) * f.
//   a = weights, b = (fromSlot, tableOffset, stopCount, -),
//   c = (low, high, -, -), d.y = destination slot
[shader("compute")]
[numthreads(64, 1, 1)]
void CSLookup(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    int n = int(PopParams.b.z);
    if (n <= 0) return; // an empty table is a no-op on both executors
    int base = int(PopParams.b.y);
    float4 s = loadSlot(int(PopParams.b.x), id.x);
    float key = s.x * PopParams.a.x + s.y * PopParams.a.y +
                s.z * PopParams.a.z + s.w * PopParams.a.w;
    float denom = PopParams.c.y - PopParams.c.x;
    float u = denom != 0.0 ? (key - PopParams.c.x) / denom : 0.0;
    u = clamp(u, 0.0, 1.0);
    float x = u * float(n - 1);
    int i0 = clamp(int(floor(x)), 0, n - 1);
    int i1 = min(i0 + 1, n - 1);
    float f = x - float(i0);
    Stop4 a = g_Table[uint(base + i0)];
    Stop4 b = g_Table[uint(base + i1)];
    float4 v0 = float4(a.x, a.y, a.z, a.w);
    float4 v1 = float4(b.x, b.y, b.z, b.w);
    storeMasked(int(PopParams.d.y), id.x, v0 + (v1 - v0) * f);
}

// The selector. geometry::pop' Group branch, formula for formula: the
// per-axis ratio, the sphere/box distance, the spelled-out smoothstep
// over the feather band, the flip, the combine.
//   a = center (xyz), b = size (xyz), c = (feather, shape, invert,
//   combine), d.y = destination slot, m.y = source slot
[shader("compute")]
[numthreads(64, 1, 1)]
void CSSelect(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    float4 s = loadSlot(int(PopParams.m.y), id.x);
    float3 size = float3(PopParams.b.x != 0.0 ? PopParams.b.x : 1e-6,
                         PopParams.b.y != 0.0 ? PopParams.b.y : 1e-6,
                         PopParams.b.z != 0.0 ? PopParams.b.z : 1e-6);
    float3 q = float3((s.x - PopParams.a.x) / size.x,
                      (s.y - PopParams.a.y) / size.y,
                      (s.z - PopParams.a.z) / size.z);
    int shape = int(PopParams.c.y);
    float d = shape == 0 ? sqrt(q.x * q.x + q.y * q.y + q.z * q.z)
                         : max(abs(q.x), max(abs(q.y), abs(q.z)));
    float f = clamp(PopParams.c.x, 0.0, 1.0);
    float inside;
    if (f <= 0.0) {
        inside = d <= 1.0 ? 1.0 : 0.0;
    } else {
        float t = clamp((d - (1.0 - f)) / f, 0.0, 1.0);
        inside = 1.0 - t * t * (3.0 - 2.0 * t);
    }
    if (PopParams.c.z > 0.5) inside = 1.0 - inside;
    int slot = int(PopParams.d.y);
    float old = loadSlot(slot, id.x).x;
    int combine = int(PopParams.c.w);
    float v = inside;
    if (combine == 1) v = max(old, inside);
    else if (combine == 2) v = min(old, inside);
    else if (combine == 3) v = old * (1.0 - inside);
    storeSlot(slot, id.x, float4(v, v, v, v));
}

// lane = M * lane. Columns ride e, f, g, h — the same column-major
// memory glm holds, poured straight in — and the product is written as
// the four column-scaled sums so the association matches glm's
// operator*. a.x > 0.5 selects the direction form (w = 0, renormalized).
[shader("compute")]
[numthreads(64, 1, 1)]
void CSAffine(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    int slot = int(PopParams.d.y);
    float4 v = loadSlot(slot, id.x);
    float4 r;
    if (PopParams.a.x > 0.5) {
        float4 t = PopParams.e * v.x + PopParams.f * v.y + PopParams.g * v.z;
        float len = sqrt(t.x * t.x + t.y * t.y + t.z * t.z);
        r = len > 1e-6 ? float4(t.x / len, t.y / len, t.z / len, v.w)
                       : float4(v.x, v.y, v.z, v.w);
    } else {
        float4 t = PopParams.e * v.x + PopParams.f * v.y + PopParams.g * v.z +
                   PopParams.h;
        r = float4(t.x, t.y, t.z, v.w);
    }
    storeMasked(slot, id.x, r);
}

// lane.xyz += normalize(along.xyz) * distance.
//   a.x = distance, d.y = lane slot, m.y = along slot
[shader("compute")]
[numthreads(64, 1, 1)]
void CSPeak(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    int slot = int(PopParams.d.y);
    float4 a = loadSlot(int(PopParams.m.y), id.x);
    float len = sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    float4 v = loadSlot(slot, id.x);
    if (len > 1e-6) {
        float k = PopParams.a.x / len;
        v.x += a.x * k; v.y += a.y * k; v.z += a.z * k;
    }
    storeMasked(slot, id.x, v);
}

// Twist / Taper / Bend about an axis. The frame (unit axis, direction
// made perpendicular, side = axis x direction) is prepared on the CPU
// and uploaded, so both executors deform in the identical frame:
//   a = (kind, amount, low, high), b = (radians, span, -, -),
//   e = axis, f = origin, g = direction, h = side, d.y = lane slot
[shader("compute")]
[numthreads(64, 1, 1)]
void CSDeform(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    int slot = int(PopParams.d.y);
    float4 v = loadSlot(slot, id.x);
    float3 axis = PopParams.e.xyz;
    float3 origin = PopParams.f.xyz;
    float3 dir = PopParams.g.xyz;
    float3 side = PopParams.h.xyz;
    int kind = int(PopParams.a.x);
    float amount = PopParams.a.y;
    float low = PopParams.a.z;
    float high = PopParams.a.w;
    float rad = PopParams.b.x;
    float span = PopParams.b.y;
    float3 p = float3(v.x, v.y, v.z) - origin;
    float h = dot(p, axis);
    float3 perp = p - axis * h;
    float u = span != 0.0 ? (h - low) / span : (h >= low ? 1.0 : 0.0);
    u = clamp(u, 0.0, 1.0);
    float3 outp;
    if (kind == 0) {
        float ang = rad * u;
        float c = cos(ang), sn = sin(ang);
        outp = axis * h + perp * c + cross(axis, perp) * sn;
    } else if (kind == 1) {
        outp = axis * h + perp * (1.0 + (amount - 1.0) * u);
    } else {
        float x = dot(perp, dir);
        float y = dot(perp, side);
        if (rad == 0.0 || span == 0.0) {
            outp = p;
        } else {
            float R = span / rad;
            float hb = clamp(h, low, high);
            float theta = (hb - low) / R;
            float c = cos(theta), sn = sin(theta);
            float extra = h - hb;
            float hOut = low + (R - x) * sn + extra * c;
            float xOut = R - (R - x) * c + extra * sn;
            outp = axis * hOut + dir * xOut + side * y;
        }
    }
    outp += origin;
    storeMasked(slot, id.x, float4(outp, v.w));
}

// to = a + (b - a) * factor, factor a constant or a lane's .x.
//   a.x = factor, b = (aSlot, bSlot, factorSlot or -1, -), d.y = to slot
[shader("compute")]
[numthreads(64, 1, 1)]
void CSMix(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= pointTotal()) return;
    float4 a = loadSlot(int(PopParams.b.x), id.x);
    float4 b = loadSlot(int(PopParams.b.y), id.x);
    int fs = int(PopParams.b.z);
    float f = fs < 0 ? PopParams.a.x : loadSlot(fs, id.x).x;
    storeMasked(int(PopParams.d.y), id.x, a + (b - a) * f);
}

// The point-set generator has no arithmetic: its lanes are uploaded as
// the arena's initial contents (geometry::pop::seedAttrs decides the
// layout). The kernel exists so the generator has a row like every
// other operator and the dispatch loop stays uniform.
[shader("compute")]
[numthreads(64, 1, 1)]
void CSPointSet(uint3 id: SV_DispatchThreadID)
{
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
    // Degenerate (zi ≈ ±Y): geometry::basisFor's fallback axis.
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
