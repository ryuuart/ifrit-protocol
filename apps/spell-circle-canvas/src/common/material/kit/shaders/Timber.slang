float4 surface(float2 uv) {
  float2 xy = along > 0.5 ? float2(uv.y, uv.x) : uv;
  float v = clamp(xy.y / max(span, 1.0), 0.0, 1.0);
  v = flip > 0.5 ? 1.0 - v : v;
  float lit = 1.0 - smoothstep(0.0, 0.17, v);
  float shade = smoothstep(0.78, 1.0, v);
  float s = seed * 0.41;
  float x = xy.x * grain + s * 17.3;
  float g = sin(x) * 0.5 + sin(x * 2.31 + s * 3.1) * 0.3 +
            sin(x * 5.77 + s * 7.7) * 0.2;
  g = g * 0.5 + 0.5;
  float fig = smoothstep(0.48, 0.96, g);
  float drift = sin(xy.x * 0.011 + s * 5.0) * 0.5 + 0.5;
  float3 c = lerp(base.rgb, light.rgb, lit * 0.92);
  c = lerp(c, dark.rgb, shade * 0.85);
  c = lerp(c, dark.rgb, fig * figure);
  c = lerp(c, light.rgb, drift * 0.07);
  float k = max(stretch, 0.01);
  float t = fbm3G((xy + s * 53.0) * float2(toothScale / k, toothScale * k));
  c = grainedG(c, t, tooth);
  return float4(clamp(c, 0.0, 1.0), 1.0);
}
