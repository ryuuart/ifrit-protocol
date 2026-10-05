struct OverTerms {
  float3 normal;
  float roughness;
  float metal;
  float occlusion;
  float clearcoat;
  float clearcoatRoughness;
  float3 clearcoatNormal;
  float3 emission;
};

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

/** The two operands' per-pixel terms through the coverage that mixes
 *  their colours, so a top wearing a normal map bumps the surface only
 *  where the mask lets the top show. */
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
}
