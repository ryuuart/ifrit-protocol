// A FLOW MAP: the source read through a second texture of directions,
// moving along them with the clock. Two reads half a cycle apart each
// slide along the flow and jump back when their cycle ends; the mix gives
// each one no weight at the moment it jumps, so the source streams without
// a seam. The flow's red and green are the direction along u and v,
// encoded around one half.

float4 surface(float2 uv) {
  let along = (flow.Sample(uv).rg * 2.0 - float2(1.0, 1.0)) * strength;
  let cycle = uTime * speed;
  let first = cycle - floor(cycle);
  let shifted = cycle + 0.5;
  let second = shifted - floor(shifted);
  let a = source.Sample(uv - along * first);
  let b = source.Sample(uv - along * second);
  return a + (b - a) * abs(1.0 - 2.0 * first);
}
