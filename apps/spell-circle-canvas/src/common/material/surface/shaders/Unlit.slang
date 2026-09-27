float4 surface(float2 uv) {
  float4 c = baseColor * baseColorMap.Sample(uv);
  float a = cutS(c.a * chanS(opacityMap.Sample(uv), opacityChannel));
  return float4(min(c.rgb, float3(a, a, a)), a);
}
