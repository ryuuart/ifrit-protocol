// CELLULAR NOISE: the distance from a point to the nearest of a set of
// seeds, one seed in each square cell of the lattice, and to the second
// nearest. Distances are in cells, so the nearest is zero at a seed and
// about a half between two. Red is the nearest, green the second, and blue
// the second less the nearest, which is zero along the borders between
// cells: the network a crackle draws. The point is the texture coordinate
// across the region at the region's own size, so a body reads the field at
// the frequency a page does.

// Where in its cell a seed stands, from the cell's lattice coordinates and
// the seed of the field: two fractions, so a jitter of one may put a seed
// anywhere in its cell and a jitter of zero at its centre.
float2 seedWithin(float2 cell) {
  float2 k = frac(cell * float2(123.34, 456.21) + uSeed);
  k += dot(k, k + 45.32);
  return frac(float2(k.x * k.y, (k.x + k.y) * k.y));
}

float4 surface(float2 uv) {
  float2 q = uv * max(uResolution, float2(1.0, 1.0)) * uFrequency;
  float2 cell = floor(q);
  float nearest = 8.0;
  float second = 8.0;
  for (int row = -1; row <= 1; ++row) {
    for (int column = -1; column <= 1; ++column) {
      float2 neighbour = cell + float2(float(column), float(row));
      float2 site = neighbour + 0.5 + (seedWithin(neighbour) - 0.5) * uJitter;
      float apart = length(site - q);
      if (apart < nearest) {
        second = nearest;
        nearest = apart;
      } else if (apart < second) {
        second = apart;
      }
    }
  }
  return float4(clamp(nearest, 0.0, 1.0), clamp(second, 0.0, 1.0),
                clamp(second - nearest, 0.0, 1.0), 1.0);
}
