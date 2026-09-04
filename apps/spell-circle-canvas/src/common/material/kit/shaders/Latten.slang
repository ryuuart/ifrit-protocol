float4 surface(float2 uv) {
  float2 run = to - from;
  float u = clamp(dot(uv - from, run) / max(dot(run, run), 1e-6), 0.0, 1.0);
  float along = clamp(level + (u - 0.5) * sheen, 0.0, 1.0);
  float3 c = along < 0.5 ? lerp(shadow.rgb, body.rgb, along * 2.0)
                         : lerp(body.rgb, light.rgb, (along - 0.5) * 2.0);
  float2 q = uv + seed * 37.0;
  float t = fbm3G(q * toothScale);
  c = grainedG(c, t, tooth);
  float2 f = fleckG(q, max(patinaCell, 2.0), patina, seed);
  c = lerp(c, patinaColor.rgb, f.x * patinaColor.a);
  return float4(clamp(c, 0.0, 1.0), 1.0);
}
