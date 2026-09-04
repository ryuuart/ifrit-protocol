float4 surface(float2 uv) {
  overClear();
  float4 b = overBase::surface(uv);
  OverTerms under = overTaken();
  overClear();
  float4 t = overTop::surface(uv);
  OverTerms upper = overTaken();
  overClear();
  float4 k = overMask::surface(uv);
  overClear();
  float cov = min(max(k.r * amount, 0.0), 1.0);
  overSay(under, upper, cov);
  return overMixed(b, b * t, cov);
}
