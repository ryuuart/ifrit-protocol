// PERLIN FRACTAL NOISE, as the page's generator sums it: gradient noise on
// a square lattice at the frequency, each octave at twice the frequency
// and half the weight of the one before. Fractal noise is the sum moved
// from about minus one and one onto nought and one; turbulence is the sum
// of each octave's magnitude. Red, green, blue and alpha are four
// independent fields, and the colour is premultiplied by the alpha, as the
// page's generator answers it. The lattice's gradients are hashed from the
// seed rather than drawn from the page generator's seeded tables, so the
// field has the page's frequency, octaves and character but not its exact
// values. The point is the texture coordinate across the region at the
// region's own size, so the frequency is in features per pixel as a page
// reads it.

// The unit gradient at a lattice corner, for one channel's field.
float2 latticeGradient(float2 corner, float channel) {
  float3 k = frac(float3(corner, channel + uSeed * 0.618034) *
                  float3(0.1031, 0.1030, 0.0973));
  k += dot(k, k.yzx + 33.33);
  let turn = frac((k.x + k.y) * k.z) * 6.2831853;
  return float2(cos(turn), sin(turn));
}

// One octave of one channel's gradient noise, in about minus one to one.
float gradientNoise(float2 p, float channel) {
  let cell = floor(p);
  let inside = p - cell;
  let ease = inside * inside * (3.0 - 2.0 * inside);
  let a = dot(latticeGradient(cell, channel), inside);
  let b = dot(latticeGradient(cell + float2(1.0, 0.0), channel),
              inside - float2(1.0, 0.0));
  let c = dot(latticeGradient(cell + float2(0.0, 1.0), channel),
              inside - float2(0.0, 1.0));
  let d = dot(latticeGradient(cell + float2(1.0, 1.0), channel),
              inside - float2(1.0, 1.0));
  return lerp(lerp(a, b, ease.x), lerp(c, d, ease.x), ease.y);
}

float4 surface(float2 uv) {
  let place = uv * max(uResolution, float2(1.0, 1.0));
  float4 sum = float4(0.0, 0.0, 0.0, 0.0);
  float2 lattice = place * uFrequency;
  float weight = 1.0;
  for (int octave = 0; octave < 16; ++octave) {
    if (float(octave) >= uOctaves) break;
    float4 layer = float4(gradientNoise(lattice, 0.0), gradientNoise(lattice, 1.0),
                          gradientNoise(lattice, 2.0), gradientNoise(lattice, 3.0));
    if (uTurbulence > 0.5) layer = abs(layer);
    sum += layer * weight;
    lattice *= 2.0;
    weight *= 0.5;
  }
  if (uTurbulence < 0.5) sum = sum * 0.5 + 0.5;
  sum = clamp(sum, 0.0, 1.0);
  return float4(sum.rgb * sum.a, sum.a);
}
