// A HALFTONE RAMP: a staggered grid of dots, turned by the angle, whose
// radius swells from the smallest at the top of the band to the largest
// at its bottom. The point is the texture coordinate across the region at
// the region's own size, so the spacing and the radii are in the pixels a
// page measures them in. The ink is premultiplied by the dot's coverage.

float4 surface(float2 uv) {
  let size = max(uResolution, float2(1.0, 1.0));
  let xy = uv * size;
  let along = xy.y / size.y;
  let t = clamp((along - uRamp0) / max(uRamp1 - uRamp0, 0.001), 0.0, 1.0);
  float2 p = xy + float2(uDriftX, uDriftY);
  let turnCos = cos(uAngle);
  let turnSin = sin(uAngle);
  p = float2(p.x * turnCos - p.y * turnSin, p.x * turnSin + p.y * turnCos);
  let row = floor(p.y / uSpacing);
  p.x += (row - 2.0 * floor(row * 0.5)) * uSpacing * 0.5;
  let cell = p - uSpacing * (floor(p / uSpacing) + 0.5);
  let radius = lerp(uRMin, uRMax, t);
  let outside = length(cell) - radius;
  let coverage = 1.0 - smoothstep(-0.75, 0.75, outside);
  let alpha = uColor.a * coverage;
  return float4(uColor.rgb * alpha, alpha);
}
