float4 surface(float2 uv) {
  float2 q = uv + seed * 91.7;
  float a = bedAngle * 0.017453292;
  float2 d = float2(cos(a), sin(a));
  float t = frac(dot(q, d) / max(bedLength, 1.0));
  float bed = 1.0 - abs(2.0 * t - 1.0);
  float3 c = lerp(hi.rgb, lo.rgb, bed * bedDepth);
  float k = max(stretch, 0.01);
  float g = fbm3G(q * float2(grainScale / k, grainScale * k));
  c = grainedG(c, g, grainContrast);
  float2 f = fleckG(q, max(speckleCell, 2.0), speckle, seed);
  float3 tone = f.y > 0.5 ? min(hi.rgb * 1.45, float3(1.0, 1.0, 1.0))
                          : lo.rgb * 0.55;
  c = lerp(c, tone, f.x * speckleAlpha);
  return float4(clamp(c, 0.0, 1.0), 1.0);
}
