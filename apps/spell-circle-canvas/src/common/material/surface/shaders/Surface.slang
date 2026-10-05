float4 surface(float2 uv) {
  // The base colour comes back ENCODED, as it was typed and as its map
  // stores it: the renderer decodes what a body returns once, whatever
  // body it is.
  float4 c = baseColor * baseColorMap.Sample(uv);
  float a = cutS(c.a * chanS(opacityMap.Sample(uv), opacityChannel));

  // Emission is a colour too, and it is handed to the renderer as LIGHT:
  // decoded here, then scaled, so a strength above one is brighter than
  // white rather than clipped to it.
  gSurfaceEmission = linearFromSrgb(emissive.rgb) *
                     linearFromSrgb(emissiveMap.Sample(uv).rgb) *
                     max(emissiveStrength, 0.0);
  gSurfaceOcclusion = occlusion(
      chanS(occlusionMap.Sample(uv), occlusionChannel), occlusionStrength);

  float4 rm = roughnessMap.Sample(uv);
  float4 mm = metallicMap.Sample(uv);
  gSurfaceRoughness = clamp(
      roughness * (dressedS(rm) ? chanS(rm, roughnessChannel) : 1.0), 0.0, 1.0);
  gSurfaceMetal = clamp(
      metallic * (dressedS(mm) ? chanS(mm, metallicChannel) : 1.0), 0.0, 1.0);
  gSurfaceTransmission = clamp(transmission, 0.0, 1.0);
  gSurfaceIor = max(ior, 1.0);
  gSurfaceThickness = max(thickness, 0.0);
  gSurfaceAbsorption = absorption.rgb;
  gSurfaceReflection = REFLECTION_WEIGHT;

  float4 cm = clearcoatMap.Sample(uv);
  float4 crm = clearcoatRoughnessMap.Sample(uv);
  gSurfaceClearcoat = clamp(
      clearcoat * (dressedS(cm) ? chanS(cm, clearcoatChannel) : 1.0), 0.0, 1.0);
  gSurfaceClearcoatRoughness = clamp(
      clearcoatRoughness *
          (dressedS(crm) ? chanS(crm, clearcoatRoughnessChannel) : 1.0),
      0.0, 1.0);

  float4 nm = normalMap.Sample(uv);
  if (dressedS(nm)) {
    float3 tn = nm.rgb * 2.0 - 1.0;
    float ty = normalDirectX > 0.5 ? -tn.y : tn.y;
    gSurfaceNormal = float3(tn.x * normalScale, ty * normalScale, tn.z);
  }
  float4 cn = clearcoatNormalMap.Sample(uv);
  if (dressedS(cn)) {
    float3 tn = cn.rgb * 2.0 - 1.0;
    float ty = normalDirectX > 0.5 ? -tn.y : tn.y;
    gSurfaceClearcoatNormal = float3(tn.x, ty, tn.z);
  }
  return float4(min(c.rgb, float3(a, a, a)), a);
}
