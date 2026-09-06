float4 surface(float2 uv) {
  float2 q = uv + seed * 61.0;
  float k = max(stretch, 0.01);
  float t = fbm(q * float2(toothScale / k, toothScale * k));
  float3 c = grained(paint.rgb, t, tooth);
  float w = fbm(q * wearScale + 11.0);
  c = grained(c, w, wear);
  return float4(clamp(c, 0.0, 1.0), 1.0);
}
