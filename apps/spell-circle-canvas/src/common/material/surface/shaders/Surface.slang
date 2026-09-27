float4 surface(float2 uv) {
  float4 c = baseColor * baseColorMap.Sample(uv);
  float occ = occlusion(chanS(occlusionMap.Sample(uv), occlusionChannel),
                        occlusionStrength);
  float3 rgb = c.rgb * occ + emission(emissive.rgb, emissiveStrength,
                                      emissiveMap.Sample(uv).rgb);
  float a = cutS(c.a * chanS(opacityMap.Sample(uv), opacityChannel));

  float4 rm = roughnessMap.Sample(uv);
  float4 mm = metallicMap.Sample(uv);
  float rough = clamp(
      roughness * (dressedS(rm) ? chanS(rm, roughnessChannel) : 1.0), 0.0, 1.0);
  gSurfaceRoughness = rough;
  gSurfaceMetal = clamp(
      metallic * (dressedS(mm) ? chanS(mm, metallicChannel) : 1.0), 0.0, 1.0);
  float smoothness = 1.0 - rough;
  gSurfaceGloss = 2.0 + 254.0 * smoothness * smoothness;
  gSurfaceTransmission = clamp(transmission, 0.0, 1.0);
  gSurfaceIor = max(ior, 1.0);
  gSurfaceThickness = max(thickness, 0.0);
  gSurfaceAbsorption = absorption.rgb;
  gSurfaceReflection = REFLECTION_WEIGHT;

  float4 nm = normalMap.Sample(uv);
  if (dressedS(nm) || dressedS(rm) || dressedS(mm)) {
    if (dressedS(nm)) {
      float3 tn = nm.rgb * 2.0 - 1.0;
      float ty = normalDirectX > 0.5 ? -tn.y : tn.y;
      gSurfaceNormal = float3(tn.x * normalScale, ty * normalScale, tn.z);
    }
    gSurfacePerPixel = true;
  }
  return float4(min(rgb, float3(a, a, a)), a);
}
