// A normal derived from a sampled height, as the 2D program derives it:
// luminance read as height at one step either side along each axis, the
// central differences scaled by depth over the step. A step is a logical
// pixel of the region the material fills where the region states its
// size, and otherwise the span of the surface one pixel of the picture
// covers, so a body seen at any distance shows the relief a page at its
// own size does.
float heightValue(float4 colour) {
  float value = dot(colour.rgb, float3(0.2126, 0.7152, 0.0722));
  return value >= 0.0 ? min(value, 1.0) : 0.0;
}
float4 heightNormal(float left, float right, float above, float below) {
  float slope = (depth / sampleDistance) * 0.5;
  float3 n = float3((left - right) * slope, (below - above) * slope * green,
                    1.0);
  n /= max(max(abs(n.x), abs(n.y)), 1.0);
  n /= sqrt(dot(n, n));
  return float4(n * 0.5 + 0.5, 1.0);
}
float4 surface(float2 uv) {
  float2 across = abs(ddx(uv)) + abs(ddy(uv));
  float2 pixel = uResolution.x > 0.0 && uResolution.y > 0.0
                     ? 1.0 / uResolution
                     : max(across, float2(1e-6, 1e-6));
  float2 dx = float2(pixel.x * sampleDistance, 0.0);
  float2 dy = float2(0.0, pixel.y * sampleDistance);
  return heightNormal(heightValue(height.Sample(uv - dx)),
                      heightValue(height.Sample(uv + dx)),
                      heightValue(height.Sample(uv - dy)),
                      heightValue(height.Sample(uv + dy)));
}
