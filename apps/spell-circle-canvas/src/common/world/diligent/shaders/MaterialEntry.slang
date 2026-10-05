[shader("fragment")]
float4 fsMaterial(VSOut input) : SV_Target {
  enterInstance(input);
  return shade(surface(input.uv), input);
}

[shader("fragment")]
void fsMaterialShadow(VSOut input) {
  enterInstance(input);
  shadowCoverage(surface(input.uv), input);
}
