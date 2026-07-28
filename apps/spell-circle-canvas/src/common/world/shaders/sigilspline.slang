// The shared curve module — written once, imported by every kernel.
module sigilspline;

public struct LoopPoint { public float px, py, pz, pad; }

public float3 pointAt(StructuredBuffer<LoopPoint> points, int n, int i)
{
    int k = ((i % n) + n) % n;
    LoopPoint p = points[(k)];
    return float3(p.px, p.py, p.pz);
}

public float3 splineAt(StructuredBuffer<LoopPoint> points, int n, float t)
{
    float f = frac(t) * float(n);
    int i = min(int(f), n - 1);
    float u = f - float(i);
    float3 p0 = pointAt(points, n, i - 1);
    float3 p1 = pointAt(points, n, i);
    float3 p2 = pointAt(points, n, i + 1);
    float3 p3 = pointAt(points, n, i + 2);
    return ((p1 * 2.0) + (p2 - p0) * u +
            (p0 * 2.0 - p1 * 5.0 + p2 * 4.0 - p3) * (u * u) +
            (p1 * 3.0 - p0 - p2 * 3.0 + p3) * (u * u * u)) * 0.5;
}

public float hash1(uint x)
{
    x = x * 747796405u + 2891336453u;
    x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    x = (x >> 22u) ^ x;
    return float(x & 0x00FFFFFFu) / 16777216.0;
}

public float3 drift(float3 p, float freq, float seed)
{
    float sx = sin(p.y * freq * 6.1 + seed) +
               0.5 * sin(p.z * freq * 11.3 + seed * 1.7);
    float sy = sin(p.z * freq * 5.3 + seed * 2.1) +
               0.5 * sin(p.x * freq * 9.7 + seed);
    float sz = sin(p.x * freq * 7.9 + seed * 1.3) +
               0.5 * sin(p.y * freq * 8.3 + seed * 2.6);
    return float3(sx, sy, sz) * 0.6667;
}
