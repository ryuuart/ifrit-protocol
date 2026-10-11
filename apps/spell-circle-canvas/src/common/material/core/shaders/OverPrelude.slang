struct OverTerms {
  float3 normal;
  float roughness;
  float metal;
  float occlusion;
  float clearcoat;
  float clearcoatRoughness;
  float3 clearcoatNormal;
  float3 emission;
  // sheen {
  float3 sheenColor;
  float sheenRoughness;
  // } sheen
  // anisotropy {
  float anisotropy;
  float2 anisotropyDirection;
  float anisotropyRotation;
  // } anisotropy
  // iridescence {
  float iridescence;
  float iridescenceIor;
  float iridescenceThickness;
  // } iridescence
  // specular {
  float specular;
  float3 specularColor;
  // } specular
  // transmission {
  float transmission;
  float ior;
  float thickness;
  float3 attenuationColor;
  float attenuationDistance;
  float dispersion;
  // } transmission
  // distortion {
  float2 distortion;
  // } distortion
};

// THE OPTIONAL LOBES each stand between two marks, as a surface body's
// do: a stack none of whose operands states a lobe has the text between
// them struck out, so it carries no lobe its operands do not.

/** The per-pixel variables back at what "said nothing" means, so each
 *  operand is asked with a clean slate and the stack answers for all
 *  three of them rather than for whichever ran last. */
void overClear() {
  gSurfaceNormal = float3(0.0, 0.0, 1.0);
  gSurfaceRoughness = -1.0;
  gSurfaceMetal = 0.0;
  gSurfaceOcclusion = 1.0;
  gSurfaceClearcoat = 0.0;
  gSurfaceClearcoatRoughness = 0.0;
  gSurfaceClearcoatNormal = float3(0.0, 0.0, 1.0);
  gSurfaceEmission = float3(0.0, 0.0, 0.0);
  // sheen {
  gSurfaceSheenColor = float3(0.0, 0.0, 0.0);
  gSurfaceSheenRoughness = 0.0;
  // } sheen
  // anisotropy {
  gSurfaceAnisotropy = 0.0;
  gSurfaceAnisotropyDirection = float2(1.0, 0.0);
  gSurfaceAnisotropyRotation = 0.0;
  // } anisotropy
  // iridescence {
  gSurfaceIridescence = 0.0;
  gSurfaceIridescenceIor = 1.3;
  gSurfaceIridescenceThickness = 0.0;
  // } iridescence
  // specular {
  gSurfaceSpecular = 1.0;
  gSurfaceSpecularColor = float3(1.0, 1.0, 1.0);
  // } specular
  // transmission {
  gSurfaceTransmission = 0.0;
  gSurfaceIor = 1.5;
  gSurfaceThickness = 0.0;
  gSurfaceAttenuationColor = float3(1.0, 1.0, 1.0);
  gSurfaceAttenuationDistance = 0.0;
  gSurfaceDispersion = 0.0;
  // } transmission
  // distortion {
  gSurfaceDistortion = float2(0.0, 0.0);
  // } distortion
}

OverTerms overTaken() {
  OverTerms out;
  out.normal = gSurfaceNormal;
  out.roughness = gSurfaceRoughness;
  out.metal = gSurfaceMetal;
  out.occlusion = gSurfaceOcclusion;
  out.clearcoat = gSurfaceClearcoat;
  out.clearcoatRoughness = gSurfaceClearcoatRoughness;
  out.clearcoatNormal = gSurfaceClearcoatNormal;
  out.emission = gSurfaceEmission;
  // sheen {
  out.sheenColor = gSurfaceSheenColor;
  out.sheenRoughness = gSurfaceSheenRoughness;
  // } sheen
  // anisotropy {
  out.anisotropy = gSurfaceAnisotropy;
  out.anisotropyDirection = gSurfaceAnisotropyDirection;
  out.anisotropyRotation = gSurfaceAnisotropyRotation;
  // } anisotropy
  // iridescence {
  out.iridescence = gSurfaceIridescence;
  out.iridescenceIor = gSurfaceIridescenceIor;
  out.iridescenceThickness = gSurfaceIridescenceThickness;
  // } iridescence
  // specular {
  out.specular = gSurfaceSpecular;
  out.specularColor = gSurfaceSpecularColor;
  // } specular
  // transmission {
  out.transmission = gSurfaceTransmission;
  out.ior = gSurfaceIor;
  out.thickness = gSurfaceThickness;
  out.attenuationColor = gSurfaceAttenuationColor;
  out.attenuationDistance = gSurfaceAttenuationDistance;
  out.dispersion = gSurfaceDispersion;
  // } transmission
  // distortion {
  out.distortion = gSurfaceDistortion;
  // } distortion
  return out;
}

// A roughness below zero is "the operand did not say", which is not a
// number to interpolate toward: where one operand named a roughness and
// the other did not, the one that named it stands.
float overRoughness(float a, float b, float t) {
  if (a < 0.0) return b;
  if (b < 0.0) return a;
  return a + (b - a) * t;
}

// Written out rather than taken from the language's library, for the
// reason the portable subset exists: an intrinsic whose two targets are
// two different pieces of code is where one source stops producing one
// answer.
float4 overMixed(float4 a, float4 b, float t) { return a + (b - a) * t; }
float overBetween(float a, float b, float t) { return a + (b - a) * t; }
float2 overBetween(float2 a, float2 b, float t) { return a + (b - a) * t; }
float3 overBetween(float3 a, float3 b, float t) { return a + (b - a) * t; }

/** The two operands' per-pixel terms through the coverage that mixes
 *  their colours, so a top wearing a normal map bumps the surface only
 *  where the mask lets the top show, and a top with a sheen shines only
 *  as far as it shows. */
void overSay(OverTerms a, OverTerms b, float t) {
  // What each operand gives off of its own mixes by the same coverage; a
  // composition that combines colours otherwise says so after this.
  gSurfaceEmission = a.emission + (b.emission - a.emission) * t;
  gSurfaceNormal = normalizeP(a.normal + (b.normal - a.normal) * t);
  gSurfaceRoughness = overRoughness(a.roughness, b.roughness, t);
  gSurfaceMetal = a.metal + (b.metal - a.metal) * t;
  gSurfaceOcclusion = a.occlusion + (b.occlusion - a.occlusion) * t;
  gSurfaceClearcoat = a.clearcoat + (b.clearcoat - a.clearcoat) * t;
  gSurfaceClearcoatRoughness =
      a.clearcoatRoughness + (b.clearcoatRoughness - a.clearcoatRoughness) * t;
  gSurfaceClearcoatNormal = normalizeP(
      a.clearcoatNormal + (b.clearcoatNormal - a.clearcoatNormal) * t);
  // sheen {
  gSurfaceSheenColor = overBetween(a.sheenColor, b.sheenColor, t);
  gSurfaceSheenRoughness = overBetween(a.sheenRoughness, b.sheenRoughness, t);
  // } sheen
  // anisotropy {
  gSurfaceAnisotropy = overBetween(a.anisotropy, b.anisotropy, t);
  gSurfaceAnisotropyDirection =
      overBetween(a.anisotropyDirection, b.anisotropyDirection, t);
  gSurfaceAnisotropyRotation =
      overBetween(a.anisotropyRotation, b.anisotropyRotation, t);
  // } anisotropy
  // iridescence {
  gSurfaceIridescence = overBetween(a.iridescence, b.iridescence, t);
  gSurfaceIridescenceIor = overBetween(a.iridescenceIor, b.iridescenceIor, t);
  gSurfaceIridescenceThickness =
      overBetween(a.iridescenceThickness, b.iridescenceThickness, t);
  // } iridescence
  // specular {
  gSurfaceSpecular = overBetween(a.specular, b.specular, t);
  gSurfaceSpecularColor = overBetween(a.specularColor, b.specularColor, t);
  // } specular
  // transmission {
  gSurfaceTransmission = overBetween(a.transmission, b.transmission, t);
  gSurfaceIor = overBetween(a.ior, b.ior, t);
  gSurfaceThickness = overBetween(a.thickness, b.thickness, t);
  gSurfaceAttenuationColor =
      overBetween(a.attenuationColor, b.attenuationColor, t);
  gSurfaceAttenuationDistance =
      overBetween(a.attenuationDistance, b.attenuationDistance, t);
  gSurfaceDispersion = overBetween(a.dispersion, b.dispersion, t);
  // } transmission
  // distortion {
  gSurfaceDistortion = overBetween(a.distortion, b.distortion, t);
  // } distortion
}
