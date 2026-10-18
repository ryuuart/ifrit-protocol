// LUMINANCE GRAIN: a seeded value noise summed over octaves and collapsed
// to one opaque grey, contrast scaled about one half, as the page's body
// sums it. The seed enters the lattice hash, so two grains that differ
// only by seed are different noise rather than the same noise slid
// sideways. The octave count is a constant written into the body, one
// program per count. The point is the texture coordinate, and a body's
// region is one unit across, so the frequency a page reads per pixel a
// body reads per region; the body asks for no size, which would make the
// page's grain depend on the box it fills when its own body reads none.

float seededValueNoise(float2 q) {
  float2 c = floor(q);
  float2 f = frac(q);
  float2 w = f * f * (3.0 - 2.0 * f);
  float2 k0 = frac((c + float2(0.0, 0.0)) * float2(123.34, 456.21) + uSeed);
  k0 += dot(k0, k0 + 45.32);
  float2 k1 = frac((c + float2(1.0, 0.0)) * float2(123.34, 456.21) + uSeed);
  k1 += dot(k1, k1 + 45.32);
  float2 k2 = frac((c + float2(0.0, 1.0)) * float2(123.34, 456.21) + uSeed);
  k2 += dot(k2, k2 + 45.32);
  float2 k3 = frac((c + float2(1.0, 1.0)) * float2(123.34, 456.21) + uSeed);
  k3 += dot(k3, k3 + 45.32);
  return lerp(lerp(frac(k0.x * k0.y), frac(k1.x * k1.y), w.x),
              lerp(frac(k2.x * k2.y), frac(k3.x * k3.y), w.x), w.y);
}

float4 surface(float2 uv) {
  const int kOctaves = 1;
  float2 q = uv * uFreq;
  float sum = 0.0;
  float total = 0.0;
  float amplitude = 0.5;
  for (int octave = 0; octave < kOctaves; ++octave) {
    sum += amplitude * seededValueNoise(q);
    total += amplitude;
    q *= 2.0;
    amplitude *= 0.5;
  }
  float v = total > 0.0 ? sum / total : 0.5;
  v = clamp(0.5 + (v - 0.5) * uContrast, 0.0, 1.0);
  return float4(v, v, v, 1.0);
}
