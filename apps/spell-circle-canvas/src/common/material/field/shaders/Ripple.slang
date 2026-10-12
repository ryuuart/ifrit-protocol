// A RIPPLE: the content read through a sine displacement — down by a sine
// of the column, or across by a sine of the row when vertical. The point
// is the texture coordinate across the region at the region's own size,
// so the amplitude and the wavelength are in the pixels a page measures
// them in.

float4 surface(float2 uv) {
  let size = max(uResolution, float2(1.0, 1.0));
  let p = uv * size;
  float2 q = p;
  if (uVertical > 0.5)
    q.x += sin(p.y * uFreq + uPhase) * uAmp;
  else
    q.y += sin(p.x * uFreq + uPhase) * uAmp;
  return content.Sample(q / size);
}
