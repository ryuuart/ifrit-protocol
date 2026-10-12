float4 surface(float2 uv) {
  compositingS();
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
  gSurfaceReflection = REFLECTION_WEIGHT;

  // clearcoat {
  float4 cm = clearcoatMap.Sample(uv);
  float4 crm = clearcoatRoughnessMap.Sample(uv);
  gSurfaceClearcoat = clamp(
      clearcoat * (dressedS(cm) ? chanS(cm, clearcoatChannel) : 1.0), 0.0, 1.0);
  gSurfaceClearcoatRoughness = clamp(
      clearcoatRoughness *
          (dressedS(crm) ? chanS(crm, clearcoatRoughnessChannel) : 1.0),
      0.0, 1.0);
  // } clearcoat

  // THE OPTIONAL LOBES, each between its two marks: a program that does
  // not carry a lobe has the text between them struck out, so it neither
  // samples the lobe's maps nor says anything the renderer would shade.
  // sheen {
  float4 sm = sheenColorMap.Sample(uv);
  float4 srm = sheenRoughnessMap.Sample(uv);
  gSurfaceSheenColor = linearFromSrgb(sheenColor.rgb) * linearFromSrgb(sm.rgb);
  gSurfaceSheenRoughness = clamp(
      sheenRoughness * (dressedS(srm) ? chanS(srm, sheenRoughnessChannel) : 1.0),
      0.0, 1.0);
  // } sheen
  // anisotropy {
  float4 am = anisotropyMap.Sample(uv);
  float2 direction = float2(1.0, 0.0);
  float strength = 1.0;
  if (dressedS(am)) {
    direction = am.rg * 2.0 - 1.0;
    strength = am.b;
  }
  gSurfaceAnisotropy = clamp(anisotropy * strength, 0.0, 1.0);
  gSurfaceAnisotropyDirection = direction;
  gSurfaceAnisotropyRotation = anisotropyRotation;
  // } anisotropy
  // iridescence {
  float4 im = iridescenceMap.Sample(uv);
  float4 itm = iridescenceThicknessMap.Sample(uv);
  gSurfaceIridescence = clamp(
      iridescence * (dressedS(im) ? chanS(im, iridescenceChannel) : 1.0), 0.0,
      1.0);
  gSurfaceIridescenceIor = max(iridescenceIor, 1.0);
  gSurfaceIridescenceThickness =
      dressedS(itm)
          ? mixS(iridescenceThicknessMinimum, iridescenceThickness,
                 chanS(itm, iridescenceThicknessChannel))
          : iridescenceThickness;
  // } iridescence
  // specular {
  float4 spm = specularMap.Sample(uv);
  float4 scm = specularColorMap.Sample(uv);
  gSurfaceSpecular = max(
      specular * (dressedS(spm) ? chanS(spm, specularChannel) : 1.0), 0.0);
  gSurfaceSpecularColor = linearFromSrgb(specularColorFactor.rgb) *
                          linearFromSrgb(scm.rgb);
  // } specular
  // transmission {
  float4 tm = transmissionMap.Sample(uv);
  float4 thm = thicknessMap.Sample(uv);
  gSurfaceTransmission =
      clamp(transmission * (dressedS(tm) ? tm.r : 1.0), 0.0, 1.0);
  gSurfaceIor = max(ior, 1.0);
  float copyThickness = thicknessAttribute != 0.0 &&
                                instanceCarries(thicknessAttribute)
                            ? instanceAttribute(thicknessAttribute)
                            : 1.0;
  gSurfaceThickness =
      max(thickness * (dressedS(thm) ? thm.r : 1.0) * copyThickness, 0.0);
  gSurfaceFlatBack = flatBack;
  // The attenuation colour is a colour as it was typed, decoded like the
  // base colour before any light passes through it, and its map's colour
  // with it.
  float4 atm = attenuationMap.Sample(uv);
  gSurfaceAttenuationColor =
      linearFromSrgb(attenuationColor.rgb) * linearFromSrgb(atm.rgb);
  gSurfaceAttenuationDistance = max(attenuationDistance, 0.0);
  gSurfaceDispersion = max(dispersion, 0.0);
  // A VOLUME MAP states the volume point by point: the index in red as
  // 1 - 1 / index, the dispersion in green and the attenuation distance in
  // blue each as value / (value + 1).
  float4 vm = volumeMap.Sample(uv);
  if (dressedS(vm)) {
    float3 held = min(vm.rgb, float3(0.999, 0.999, 0.999));
    gSurfaceIor = max(1.0 / (1.0 - held.r), 1.0);
    gSurfaceDispersion = held.g / (1.0 - held.g);
    gSurfaceAttenuationDistance = held.b / (1.0 - held.b);
  }
  // } transmission
  // distortion {
  // The map is DATA: half-grey is still, and each channel runs one unit
  // either way of it.
  float4 dm = distortionMap.Sample(uv);
  gSurfaceDistortion = (dm.rg * 2.0 - 1.0) * distortionStrength;
  // } distortion

  float4 nm = normalMap.Sample(uv);
  if (dressedS(nm)) {
    float3 tn = nm.rgb * 2.0 - 1.0;
    float ty = normalDirectX > 0.5 ? -tn.y : tn.y;
    gSurfaceNormal = float3(tn.x * normalScale, ty * normalScale, tn.z);
  }
  // clearcoat {
  float4 cn = clearcoatNormalMap.Sample(uv);
  if (dressedS(cn)) {
    float3 tn = cn.rgb * 2.0 - 1.0;
    float ty = normalDirectX > 0.5 ? -tn.y : tn.y;
    gSurfaceClearcoatNormal = float3(tn.x, ty, tn.z);
  }
  // } clearcoat
  return float4(min(c.rgb, float3(a, a, a)), a);
}
