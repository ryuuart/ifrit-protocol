// A BAKED RESPONSE applied to the content: each channel of the content's
// straight colour read through its own channel of the one row, and
// premultiplied again by the content's alpha.
float4 surface(float2 uv) {
  let c = content.Sample(uv);
  let a = max(c.a, 0.0001);
  let rgb = clamp(c.rgb / a, 0.0, 1.0);
  let p = (rgb * (lutSize - 1.0) + 0.5) / lutSize;
  let mapped = float3(lut.Sample(float2(p.r, 0.5)).r,
                      lut.Sample(float2(p.g, 0.5)).g,
                      lut.Sample(float2(p.b, 0.5)).b);
  return float4(mapped * a, c.a);
}
