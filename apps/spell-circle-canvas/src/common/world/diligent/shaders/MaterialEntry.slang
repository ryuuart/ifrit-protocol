[shader("fragment")]
float4 fsMaterial(VSOut input) : SV_Target {
  return shade(surface(input.uv), input);
}
