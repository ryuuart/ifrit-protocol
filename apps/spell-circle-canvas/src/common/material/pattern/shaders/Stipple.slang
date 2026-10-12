// One ink through a 1-bit mask on the pixel lattice. The mask is up to
// sixty-four cells, four words of sixteen bits held as whole floats, and
// cell (x, y) is bit y * size + x. The point is the texture coordinate
// across the region at the region's own size, so a cell is the pixels a
// page measures it in.
float4 surface(float2 uv) {
  let p = uv * max(uResolution, float2(1.0, 1.0));
  let column = floor(p.x / cell);
  let row = floor(p.y / cell);
  let x = column - size * floor(column / size);
  let y = row - size * floor(row / size);
  let index = uint(y * size + x);
  let word = uint(index < 16u   ? words.x
                  : index < 32u ? words.y
                  : index < 48u ? words.z
                                : words.w);
  if (((word >> (index & 15u)) & 1u) == 0u) return float4(0.0, 0.0, 0.0, 0.0);
  return float4(ink.rgb * ink.a, ink.a);
}
