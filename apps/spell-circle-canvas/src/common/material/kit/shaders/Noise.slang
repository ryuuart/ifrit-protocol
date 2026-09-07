// THE VALUE NOISE EVERY GRAINED AND MARBLED BODY IN THE KIT READS,
// written once and crossed into SkSL by the core: a hash on a lattice,
// its smoothed interpolation, three octaves of it, the luminance fold a
// grain is applied with, and the lattice fleck a speckle is scattered
// with. One text, so a stone's grain and a chrome's marbling are the
// same noise on a device and on a raster surface.
//
// The hash is the sine-fract one, which is not a good hash and is the
// right one here: it is the same arithmetic on every device that can run
// this file, and a surface's grain has to be the same grain in a stored
// render and on screen.
float hash(float2 p) {
  return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float valueNoise(float2 p) {
  float2 i = floor(p);
  float2 f = frac(p);
  float2 u = f * f * (3.0 - 2.0 * f);
  float a = hash(i);
  float b = hash(i + float2(1.0, 0.0));
  float c = hash(i + float2(0.0, 1.0));
  float d = hash(i + float2(1.0, 1.0));
  return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float fbm(float2 p) {
  return valueNoise(p) * 0.55 + valueNoise(p * 2.13) * 0.3 +
         valueNoise(p * 4.41) * 0.15;
}

float3 grained(float3 c, float g, float k) {
  return c * (1.0 + (g - 0.5) * 2.0 * k);
}

float2 fleck(float2 q, float cell, float density, float s) {
  float2 c = floor(q / cell);
  float h = hash(c + s);
  if (h >= density) return float2(0.0, 0.0);
  float r = cell * (0.12 + 0.18 * hash(c + s + 7.0));
  float2 jitter = float2(hash(c + s + 17.0), hash(c + s + 31.0)) - 0.5;
  float2 centre = (c + 0.5) * cell + jitter * (cell - 2.0 * r);
  float d = length(q - centre);
  float cover = 1.0 - smoothstep(r - 0.75, r + 0.75, d);
  return float2(cover, step(0.5, hash(c + s + 3.0)));
}
