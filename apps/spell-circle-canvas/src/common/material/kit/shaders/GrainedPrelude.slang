float hashG(float2 p) {
  return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float vnoiseG(float2 p) {
  float2 i = floor(p);
  float2 f = frac(p);
  float2 u = f * f * (3.0 - 2.0 * f);
  float a = hashG(i);
  float b = hashG(i + float2(1.0, 0.0));
  float c = hashG(i + float2(0.0, 1.0));
  float d = hashG(i + float2(1.0, 1.0));
  return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float fbm3G(float2 p) {
  return vnoiseG(p) * 0.55 + vnoiseG(p * 2.13) * 0.3 +
         vnoiseG(p * 4.41) * 0.15;
}

float3 grainedG(float3 c, float g, float k) {
  return c * (1.0 + (g - 0.5) * 2.0 * k);
}

float2 fleckG(float2 q, float cell, float density, float s) {
  float2 c = floor(q / cell);
  float h = hashG(c + s);
  if (h >= density) return float2(0.0, 0.0);
  float r = cell * (0.12 + 0.18 * hashG(c + s + 7.0));
  float2 jitter = float2(hashG(c + s + 17.0), hashG(c + s + 31.0)) - 0.5;
  float2 centre = (c + 0.5) * cell + jitter * (cell - 2.0 * r);
  float d = length(q - centre);
  float cover = 1.0 - smoothstep(r - 0.75, r + 0.75, d);
  return float2(cover, step(0.5, hashG(c + s + 3.0)));
}
