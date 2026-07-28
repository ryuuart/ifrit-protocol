// The ribbon sweep kernel — the towed-banner rig over the shared
// spline module. Formula-identical to shape's CPU queries (the dart
// rides the same curve) and to the retired HLSL original.
import sigilspline;

#include "WorldShaderParams.h"
ConstantBuffer<SweepParamsData> SweepParams;

StructuredBuffer<LoopPoint> g_Points;
struct SweepVertex { float px, py, pz, nx, ny, nz, u, v, r, g, b, a; }
RWStructuredBuffer<SweepVertex> g_Vertices;

[shader("compute")]
[numthreads(64, 1, 1)]
void CSSweep(uint3 id: SV_DispatchThreadID)
{
    uint sections = uint(SweepParams.window.w);
    if (id.x >= sections)
        return;
    int n = int(SweepParams.loop.x);
    float eps = SweepParams.loop.y;
    float f = float(id.x) / float(sections - 1u);
    float t = SweepParams.window.x - SweepParams.window.y +
              SweepParams.window.y * f;
    float3 p = splineAt(g_Points, n, t);
    float3 tangent = splineAt(g_Points, n, t + eps) -
                     splineAt(g_Points, n, t - eps);
    float len = length(tangent);
    tangent = len > 1e-6 ? tangent / len : float3(1.0, 0.0, 0.0);
    float3 down = float3(0.0, -1.0, 0.0) -
                  tangent * dot(tangent, float3(0.0, -1.0, 0.0));
    float dl = length(down);
    float3 hang = dl > 0.15
                      ? down / dl
                      : normalize(cross(tangent, float3(0.0, 0.0, 1.0)));
    float3 nrm = cross(hang, tangent);
    float3 a = p - hang * (SweepParams.window.z * 0.5);
    float3 b = p + hang * (SweepParams.window.z * 0.5);
    SweepVertex v0;
    v0.px = a.x; v0.py = a.y; v0.pz = a.z;
    v0.nx = nrm.x; v0.ny = nrm.y; v0.nz = nrm.z;
    v0.u = 0.0; v0.v = f;
    v0.r = 1.0; v0.g = 1.0; v0.b = 1.0; v0.a = 1.0;
    SweepVertex v1;
    v1.px = b.x; v1.py = b.y; v1.pz = b.z;
    v1.nx = nrm.x; v1.ny = nrm.y; v1.nz = nrm.z;
    v1.u = 1.0; v1.v = f;
    v1.r = 1.0; v1.g = 1.0; v1.b = 1.0; v1.a = 1.0;
    g_Vertices[id.x * 2u] = v0;
    g_Vertices[id.x * 2u + 1u] = v1;
}
