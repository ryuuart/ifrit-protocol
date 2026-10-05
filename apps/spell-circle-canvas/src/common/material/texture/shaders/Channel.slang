// ONE CHANNEL OF THE SOURCE as a grey value: red, green, blue, alpha, or
// the luminance of the three colours. The grey stands in all three colour
// channels and the alpha is one, so it fills a slot that reads red, a
// mask that reads any channel, or a colour, alike.

float4 surface(float2 uv) {
  let read = source.Sample(uv);
  float value = read.a;
  if (which > 0.5) value = dot(read.rgb, float3(0.2126, 0.7152, 0.0722));
  if (which > 1.5) value = read.r;
  if (which > 2.5) value = read.g;
  if (which > 3.5) value = read.b;
  return float4(value, value, value, 1.0);
}
