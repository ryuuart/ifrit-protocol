// The flock kernel — the fused comet generator over the shared spline
// module: stratified window param, stable radial offset, sin-field
// drift, tangent basis, tail-to-head tint — packed straight into the
// instanced stream. Formula-identical to the retired HLSL original.
import sigilspline;

#include "WorldShaderParams.h"
ConstantBuffer<FlockParamsData> FlockParams;

StructuredBuffer<LoopPoint> g_Points;
struct Instance { float4 r0, r1, r2, tint, tex; }
RWStructuredBuffer<Instance> g_Instances;

[shader("compute")]
[numthreads(64, 1, 1)]
void CSFlock(uint3 id: SV_DispatchThreadID)
{
    uint count = uint(FlockParams.windowA.w);
    if (id.x >= count)
        return;
    int n = int(FlockParams.loop.x);
    float eps = FlockParams.loop.y;
    float u0 = (float(id.x) + 0.5) / float(count);
    float t = FlockParams.windowA.x - FlockParams.windowA.y +
              FlockParams.windowA.y * u0 +
              (hash1(id.x * 3u + 1u) - 0.5) *
                  (FlockParams.windowA.y / float(count)) * 4.0;
    float3 p = splineAt(g_Points, n, t);
    float3 tangent = splineAt(g_Points, n, t + eps) -
                     splineAt(g_Points, n, t - eps);
    float len = length(tangent);
    tangent = len > 1e-6 ? tangent / len : float3(1.0, 0.0, 0.0);

    float3 n0 = abs(tangent.y) < 0.9
                    ? cross(tangent, float3(0.0, 1.0, 0.0))
                    : cross(tangent, float3(1.0, 0.0, 0.0));
    n0 = normalize(n0);
    float3 b0 = cross(tangent, n0);
    float ang = hash1(id.x * 7u + 2u) * 6.2831853;
    float rad = sqrt(hash1(id.x * 5u + 3u)) * FlockParams.windowA.z;
    p += (n0 * cos(ang) + b0 * sin(ang)) * rad;
    if (FlockParams.windowB.y > 0.0)
        p += drift(p, FlockParams.windowB.z,
                   FlockParams.windowB.w + FlockParams.windowA.x * 3.0) *
             FlockParams.windowB.y;

    float s = FlockParams.windowB.x * (0.5 + hash1(id.x * 11u + 4u));
    float3 zi = tangent;
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
    inst.tint = lerp(FlockParams.tintTail, FlockParams.tintHead, u0);
    inst.tex = float4(0.0, 0.0, 1.0, 1.0);
    g_Instances[id.x] = inst;
}
