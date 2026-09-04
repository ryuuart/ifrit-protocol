float chanM(float4 c, float which) {
  return which < 0.5 ? c.r : which < 1.5 ? c.g : which < 2.5 ? c.b : c.a;
}

float3 unitM(float3 v) {
  float len = lengthP(v);
  return len < 1e-5 ? float3(0.0, 1.0, 0.0) : v / len;
}

float4 surface(float2 uv) {
  float4 s = source.Sample(uv);
  float raw = reading < 0.5   ? chanM(s, channel)
              : reading < 1.5 ? dotP(s.rgb * 2.0 - 1.0, unitM(axis.xyz))
                              : dotP(s.rgb, axis.xyz);
  float v = shapeS(raw);
  return float4(v, v, v, 1.0);
}
