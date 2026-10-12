// A BAKED VOLUME applied to the content: the content's straight colour
// read through the lookup's slices — the slices of blue laid side by side
// along the image, red across each and green down it — between the two
// nearest slices, and premultiplied again by the content's alpha.
float4 surface(float2 uv) {
  let c = content.Sample(uv);
  let a = max(c.a, 0.0001);
  let rgb = clamp(c.rgb / a, 0.0, 1.0);
  let n = lutSize;
  let p = rgb * (n - 1.0);
  let lowSlice = floor(p.z);
  let highSlice = min(lowSlice + 1.0, n - 1.0);
  let t = p.z - lowSlice;
  let extent = float2(n * n, n);
  let low = float2(lowSlice * n + p.x + 0.5, p.y + 0.5) / extent;
  let high = float2(highSlice * n + p.x + 0.5, p.y + 0.5) / extent;
  let mapped = lerp(lut.Sample(low).rgb, lut.Sample(high).rgb, t);
  return float4(mapped * a, c.a);
}
