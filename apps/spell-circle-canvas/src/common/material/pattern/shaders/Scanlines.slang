// Hard rows: a pixel whose centre lies in the first `on` pixels of a
// period takes the ink, and every other pixel is left clear. The point is
// the texture coordinate across the region at the region's own size, so
// the period is in the pixels a page measures it in.
float4 surface(float2 uv) {
  let p = uv * max(uResolution, float2(1.0, 1.0));
  let span = max(period, 0.0001);
  let shifted = p.y - phase;
  let row = shifted - span * floor(shifted / span);
  if (row >= on) return float4(0.0, 0.0, 0.0, 0.0);
  return float4(ink.rgb * ink.a, ink.a);
}
