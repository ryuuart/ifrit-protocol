struct OverTerms {
  float3 normal;
  float gloss;
  float metal;
  bool perPixel;
};

/** The per-pixel variables back at what "said nothing" means, so each
 *  operand is asked with a clean slate and the stack answers for all
 *  three of them rather than for whichever ran last. */
void overClear() {
  gSurfaceNormal = float3(0.0, 0.0, 1.0);
  gSurfaceGloss = -1.0;
  gSurfaceMetal = 0.0;
  gSurfacePerPixel = false;
}

OverTerms overTaken() {
  OverTerms out;
  out.normal = gSurfaceNormal;
  out.gloss = gSurfaceGloss;
  out.metal = gSurfaceMetal;
  out.perPixel = gSurfacePerPixel;
  return out;
}

// A Blinn exponent below zero is "take the scaffold's own", which is not
// a number to interpolate toward: where one operand named an exponent
// and the other did not, the one that named it stands.
float overGloss(float a, float b, float t) {
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
  if (!a.perPixel && !b.perPixel) return;
  gSurfaceNormal = normalizeP(a.normal + (b.normal - a.normal) * t);
  gSurfaceGloss = overGloss(a.gloss, b.gloss, t);
  gSurfaceMetal = a.metal + (b.metal - a.metal) * t;
  gSurfacePerPixel = true;
}
