// THE SHADING TERMS: the pieces a surface is composed OF, one function
// each, none of them a whole shading model.
//
// A surface in this kit is a composition — a diffuse term, a highlight,
// an environment reflection, a transmission — the way a shader graph in
// an authoring tool is a composition of nodes. Each term here is a small
// piece of arithmetic with a closed form, so what a surface does can be
// read off the list of terms it calls, and a surface that wants a term
// this kit does not ship writes that one and keeps the rest.
//
// EVERY TERM IS PURE. Nothing here samples a texture, because sampling
// is spelled differently in every language while arithmetic is not: a
// caller fetches the radiance and hands it in.
//
// ONE TEXT, TWO LANGUAGES. This module is what a Slang renderer imports;
// the same text with the module line and the `public` qualifiers removed
// is SkSL, which is how the terms are evaluated against their closed
// forms on the host. So nothing below uses a construct the two languages
// spell differently — no structure, no `out` parameter, no sampling —
// and the transcendentals the panorama needs are written out as
// polynomials: a library `atan2` is two pieces of code on two targets,
// and an equirectangular lookup that disagreed between them would put a
// seam down the middle of a reflection.

module Shading;

// ---- the panorama's geometry ----------------------------------------

/** Arctangent of y/x over all four quadrants, as arithmetic: the ratio
 *  of the smaller magnitude to the larger is folded into one octant, a
 *  degree-7 odd polynomial evaluates it there, and the quadrant is put
 *  back. */
public float atan2P(float y, float x) {
  float ax = abs(x);
  float ay = abs(y);
  float mx = max(ax, ay);
  float mn = min(ax, ay);
  float a = mx > 0.0 ? mn / mx : 0.0;
  float s = a * a;
  float r = ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a;
  if (ay > ax) r = 1.57079637 - r;
  if (x < 0.0) r = 3.14159274 - r;
  return y < 0.0 ? -r : r;
}

/** Arccosine over [-1, 1], as arithmetic: a square-root shoulder times a
 *  cubic, reflected for a negative argument. */
public float acosP(float x) {
  float c = min(max(x, -1.0), 1.0);
  float ax = abs(c);
  float r = sqrt(1.0 - ax) *
            (1.5707288 +
             ax * (-0.2121144 + ax * (0.0742610 + ax * (-0.0187293))));
  return c < 0.0 ? 3.14159274 - r : r;
}

/** Where a direction reads on an equirectangular panorama: u = 0.5 turns to
 *  face -z, u wraps once around, and v runs 0 at the zenith to 1 at the
 *  nadir. The one convention every consumer of an environment map
 *  shares, on every tier. */
public float2 equirectangularUv(float3 d) {
  float len = sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
  float3 u = len > 0.0 ? d / len : float3(0.0, 1.0, 0.0);
  return float2(0.5 + atan2P(u.x, -u.z) * 0.15915494,
                acosP(u.y) * 0.31830989);
}

/** Which prefiltered level a roughness reads, over a chain of @p levels.
 *  Linear in roughness because that is how the chain was filtered. */
public float roughnessLevel(float roughness, float levels) {
  return min(max(roughness, 0.0), 1.0) * (levels - 1.0);
}

// ---- the terms ------------------------------------------------------

/** LAMBERT: how much of a light reaches a surface facing @p n from the
 *  direction @p l (pointing from the surface toward the light). */
public float lambert(float3 n, float3 l) {
  return max(n.x * l.x + n.y * l.y + n.z * l.z, 0.0);
}

/** BLINN: the highlight, from the half vector between the light and the
 *  eye. @p gloss is the exponent — a large one is a narrow highlight. */
public float blinn(float3 n, float3 l, float3 v, float gloss) {
  float3 h = l + v;
  float len = sqrt(h.x * h.x + h.y * h.y + h.z * h.z);
  if (len <= 0.0) return 0.0;
  h = h / len;
  float nh = max(n.x * h.x + n.y * h.y + n.z * h.z, 0.0);
  return pow(nh, gloss);
}

/** The reflectance a surface shows head on: 4% for a dielectric, the
 *  surface's own colour for a metal, and the mix between for the values
 *  in the middle that a metallic map produces at an edge. */
public float3 specularColor(float3 baseColor, float metal) {
  float m = min(max(metal, 0.0), 1.0);
  return float3(0.04, 0.04, 0.04) * (1.0 - m) + baseColor * m;
}

/** FRESNEL, Schlick's form: reflectance rises to one as the surface
 *  turns away from the eye. This is the term a model without it is
 *  missing at the rim, where every real dielectric picks up the sky. */
public float3 fresnel(float3 f0, float cosTheta) {
  float c = 1.0 - min(max(cosTheta, 0.0), 1.0);
  float c2 = c * c;
  float f = c2 * c2 * c;
  return f0 + (float3(1.0, 1.0, 1.0) - f0) * f;
}

/** Fresnel that stops short of white on a rough surface, so a rough
 *  metal's rim does not read brighter than a mirror's. */
public float3 fresnelRough(float3 f0, float cosTheta, float roughness) {
  float c = 1.0 - min(max(cosTheta, 0.0), 1.0);
  float c2 = c * c;
  float f = c2 * c2 * c;
  float3 ceiling = max(float3(1.0 - roughness, 1.0 - roughness,
                              1.0 - roughness), f0);
  return f0 + (ceiling - f0) * f;
}

// ---- the microfacet model --------------------------------------------
// A surface is a field of tiny mirrors whose facing spreads with its
// roughness. What it reflects toward the eye from one light is the share
// of mirrors turned to bounce that light there (the distribution), the
// share of those neither hidden nor shadowed by their neighbours (the
// visibility), and how much each mirror reflects at that angle (Fresnel).
// What it does not reflect enters the surface and leaves it diffusely,
// coloured, unless the surface is a metal, which has no diffuse at all.
// ROUGHNESS is the perceptual one an author states; each term squares it
// into the width the distribution is written in.

/** The width a perceptual @p roughness is spread over, kept off zero: a
 *  perfect mirror is a delta no pixel could sample, and the narrowest
 *  lobe this answers stands for it. */
public float microfacetWidth(float roughness) {
  float r = min(max(roughness, 0.0), 1.0);
  return max(r * r, 1e-3);
}

/** THE GGX DISTRIBUTION: how densely the microfacets face the half
 *  vector, at @p nDotH its cosine from the normal. It integrates to one
 *  over the projected hemisphere, and a smooth surface concentrates it
 *  into a tall narrow peak. */
public float distributionGgx(float nDotH, float roughness) {
  float a = microfacetWidth(roughness);
  float a2 = a * a;
  float c = min(max(nDotH, 0.0), 1.0);
  float f = c * c * (a2 - 1.0) + 1.0;
  return a2 / max(3.14159274 * f * f, 1e-9);
}

/** THE SMITH VISIBILITY, height-correlated: the share of microfacets
 *  neither masked from the eye nor shadowed from the light, over the four
 *  cosines a microfacet reflection is divided by. Correlated because a
 *  facet standing high enough to see the light is likelier to see the eye
 *  as well. */
public float visibilityGgx(float nDotL, float nDotV, float roughness) {
  float a = microfacetWidth(roughness);
  float a2 = a * a;
  float l = max(nDotL, 0.0);
  float v = max(nDotV, 0.0);
  float lambdaV = l * sqrt(max(v * v * (1.0 - a2) + a2, 1e-7));
  float lambdaL = v * sqrt(max(l * l * (1.0 - a2) + a2, 1e-7));
  return 0.5 / max(lambdaV + lambdaL, 1e-7);
}

/** FRESNEL between two reflectances: @p f0 head on, rising to @p f90 at
 *  the grazing angle. */
public float3 fresnelSchlick(float3 f0, float3 f90, float cosTheta) {
  float c = 1.0 - min(max(cosTheta, 0.0), 1.0);
  float c2 = c * c;
  float f = c2 * c2 * c;
  return f0 + (f90 - f0) * f;
}

/** The reflectance a surface reaches at the grazing angle: white for any
 *  real material, falling to black only for a head-on reflectance under
 *  two per cent, which no real material has — the one way an author can
 *  state a surface that reflects nothing at all. */
public float3 reflectance90(float3 f0) {
  float r = min(max(max(f0.x, max(f0.y, f0.z)) * 50.0, 0.0), 1.0);
  return float3(r, r, r);
}

/** The colour the diffuse side reflects: the base colour less the four
 *  per cent a dielectric reflects specularly head on, and none of it for
 *  a metal. */
public float3 diffuseColor(float3 baseColor, float metal) {
  float m = min(max(metal, 0.0), 1.0);
  return baseColor * (0.96 * (1.0 - m));
}

/** THE DIFFUSE SIDE OF ONE LIGHT: Lambert's lobe — the diffuse colour over
 *  pi — less what Fresnel reflected at the surface, times the cosine the
 *  light arrives at. @p l points from the surface toward the light; all
 *  three vectors are unit length. Multiply by the light's radiance at the
 *  surface. */
public float3 directDiffuse(float3 n, float3 l, float3 v, float3 diffuse,
                            float3 f0, float3 f90) {
  float nl = max(n.x * l.x + n.y * l.y + n.z * l.z, 0.0);
  if (nl <= 0.0) return float3(0.0, 0.0, 0.0);
  float3 h = l + v;
  float len = sqrt(h.x * h.x + h.y * h.y + h.z * h.z);
  h = len > 0.0 ? h / len : n;
  float vh = v.x * h.x + v.y * h.y + v.z * h.z;
  float3 f = fresnelSchlick(f0, f90, vh);
  return (float3(1.0, 1.0, 1.0) - f) * diffuse * (nl / 3.14159274);
}

/** THE SPECULAR SIDE OF ONE LIGHT: distribution times visibility times
 *  Fresnel, times the cosine the light arrives at. Same vectors and same
 *  multiplication as `directDiffuse`. */
public float3 directSpecular(float3 n, float3 l, float3 v, float3 f0,
                             float3 f90, float roughness) {
  float nl = max(n.x * l.x + n.y * l.y + n.z * l.z, 0.0);
  if (nl <= 0.0) return float3(0.0, 0.0, 0.0);
  float nv = max(n.x * v.x + n.y * v.y + n.z * v.z, 0.0);
  float3 h = l + v;
  float len = sqrt(h.x * h.x + h.y * h.y + h.z * h.z);
  h = len > 0.0 ? h / len : n;
  float nh = n.x * h.x + n.y * h.y + n.z * h.z;
  float vh = v.x * h.x + v.y * h.y + v.z * h.z;
  float d = distributionGgx(nh, roughness);
  float vis = visibilityGgx(nl, nv, roughness);
  return fresnelSchlick(f0, f90, vh) * (d * vis * nl);
}

/** HOW A CLEARCOAT LETS THE SURFACE BENEATH IT THROUGH: a coat of
 *  @p weight reflects its own Fresnel share at the angle the eye meets it
 *  and passes the rest. A coat is a dielectric of index 1.5, four per cent
 *  head on; the angle is held off the grazing one, where a coat would
 *  otherwise hide the whole surface behind a white rim. */
public float clearcoatTransmittance(float weight, float nDotV) {
  float c = 1.0 - max(nDotV, 0.1);
  float c2 = c * c;
  float f = 0.04 + 0.96 * c2 * c2 * c;
  return 1.0 - min(max(weight, 0.0), 1.0) * f;
}

// ---- the optional lobes ------------------------------------------------
// Four further ways a surface answers light, each one a surface states or
// does not: a soft retro-reflective SHEEN over the base, as cloth and dust
// show; a highlight stretched along one direction of the surface, which is
// ANISOTROPY, as brushed metal shows; a thin film whose interference tints
// the reflectance by angle and thickness, which is IRIDESCENCE, as soap and
// oil show; and a dielectric's own SPECULAR strength and colour. Each is a
// pure term like the rest; a surface that states none of them is shaded
// exactly as if they were not here.

/** THE SHEEN DISTRIBUTION: microfacets standing mostly sideways, as the
 *  fibres of a cloth do, so the lobe is brightest where the half vector
 *  lies along the surface — at the rim, toward a light behind the cloth.
 *  @p sheenRoughness widens it; zero is held off, where the lobe would be a
 *  line no pixel could sample. */
public float distributionSheen(float nDotH, float sheenRoughness) {
  float r = max(min(sheenRoughness, 1.0), 1e-3);
  float a = r * r;
  float inverse = 1.0 / a;
  float c = min(max(nDotH, 0.0), 1.0);
  float s2 = max(1.0 - c * c, 0.0078125);
  return (2.0 + inverse) * pow(s2, inverse * 0.5) / 6.28318531;
}

/** The shadowing a sheen's fibres cast on one another along one direction
 *  at the cosine @p cosTheta, for the squared roughness @p alpha, as the
 *  exponent of a fitted curve: two fits, one for each roughness extreme,
 *  blended by how smooth the sheen is. Above a cosine of one half the
 *  curve is mirrored about that point, which is where the fit holds. */
public float sheenLambdaExponent(float x, float alpha) {
  float smooth = (1.0 - alpha) * (1.0 - alpha);
  float a = 21.5473 + (25.32450 - 21.5473) * smooth;
  float b = 3.82987 + (3.32435 - 3.82987) * smooth;
  float c = 0.19823 + (0.16801 - 0.19823) * smooth;
  float d = -1.97760 + (-1.27393 + 1.97760) * smooth;
  float e = -4.32054 + (-4.85967 + 4.32054) * smooth;
  return a / (1.0 + b * pow(max(x, 0.0), c)) + d * x + e;
}

public float sheenLambda(float cosTheta, float alpha) {
  float c = abs(cosTheta);
  if (c < 0.5) return exp(sheenLambdaExponent(c, alpha));
  return exp(2.0 * sheenLambdaExponent(0.5, alpha) -
             sheenLambdaExponent(1.0 - c, alpha));
}

/** THE SHEEN'S VISIBILITY over the four cosines its reflection is divided
 *  by, held to one: the lobe never shows more than the fibres facing the
 *  light can send. */
public float visibilitySheen(float nDotL, float nDotV, float sheenRoughness) {
  float r = max(min(sheenRoughness, 1.0), 1e-3);
  float alpha = r * r;
  float l = max(nDotL, 0.0);
  float v = max(nDotV, 0.0);
  float denominator = (1.0 + sheenLambda(v, alpha) + sheenLambda(l, alpha)) *
                      max(4.0 * v * l, 5e-8);
  return min(max(1.0 / denominator, 0.0), 1.0);
}

/** THE SHEEN OF ONE LIGHT: its colour times distribution times visibility,
 *  times the cosine the light arrives at. A sheen has no Fresnel: its
 *  colour is how much it reflects at every angle. Multiply by the light's
 *  radiance at the surface. */
public float3 directSheen(float3 n, float3 l, float3 v, float3 sheenColor,
                          float sheenRoughness) {
  float nl = max(n.x * l.x + n.y * l.y + n.z * l.z, 0.0);
  if (nl <= 0.0) return float3(0.0, 0.0, 0.0);
  float nv = max(n.x * v.x + n.y * v.y + n.z * v.z, 0.0);
  float3 h = l + v;
  float len = sqrt(h.x * h.x + h.y * h.y + h.z * h.z);
  h = len > 0.0 ? h / len : n;
  float nh = n.x * h.x + n.y * h.y + n.z * h.z;
  return sheenColor * (distributionSheen(nh, sheenRoughness) *
                       visibilitySheen(nl, nv, sheenRoughness) * nl);
}

/** WHAT THE SHEEN LEAVES THE SURFACE BENEATH IT: the base layer dimmed by
 *  the share the sheen reflected, seen from the eye at @p albedoView and
 *  from the light at @p albedoLight — each the sheen's directional albedo
 *  at that cosine, read from the table — and the larger of the two losses
 *  taken. Its brightest channel stands for the colour, so a coloured sheen
 *  does not tint what lies under it. */
public float sheenScaling(float3 sheenColor, float albedoView,
                          float albedoLight) {
  float strongest = max(sheenColor.x, max(sheenColor.y, sheenColor.z));
  return min(max(min(1.0 - strongest * albedoView,
                     1.0 - strongest * albedoLight),
                 0.0),
             1.0);
}

/** THE SHEEN OF LIGHT FROM EVERYWHERE: the radiance arriving along the
 *  normal times the sheen's colour and its directional albedo at the eye's
 *  cosine. The broad lobe reads the panorama where a rough surface would,
 *  so no chain filtered for it is needed. */
public float3 environmentSheen(float3 radiance, float3 sheenColor,
                               float albedo) {
  return radiance * sheenColor * albedo;
}

/** THE ANISOTROPIC GGX DISTRIBUTION: the isotropic one with a width of its
 *  own along the tangent @p alphaT and the bitangent @p alphaB, at the
 *  half vector's cosines to the normal, the tangent and the bitangent.
 *  With the two widths equal it is `distributionGgx` exactly. */
public float distributionGgxAnisotropic(float nDotH, float tDotH, float bDotH,
                                        float alphaT, float alphaB) {
  float a2 = alphaT * alphaB;
  float3 w = float3(alphaB * tDotH, alphaT * bDotH, a2 * nDotH);
  float w2 = w.x * w.x + w.y * w.y + w.z * w.z;
  float s = a2 / max(w2, 1e-6);
  return a2 * s * s * 0.31830989;
}

/** THE ANISOTROPIC SMITH VISIBILITY, height-correlated: `visibilityGgx`
 *  with each direction's spread measured along the tangent and the
 *  bitangent at their own widths. */
public float visibilityGgxAnisotropic(float nDotL, float nDotV, float tDotL,
                                      float tDotV, float bDotL, float bDotV,
                                      float alphaT, float alphaB) {
  float l = max(nDotL, 0.0);
  float v = max(nDotV, 0.0);
  float3 sv = float3(alphaT * tDotV, alphaB * bDotV, v);
  float3 sl = float3(alphaT * tDotL, alphaB * bDotL, l);
  float lambdaV = l * max(sqrt(sv.x * sv.x + sv.y * sv.y + sv.z * sv.z), 1e-3);
  float lambdaL = v * max(sqrt(sl.x * sl.x + sl.y * sl.y + sl.z * sl.z), 1e-3);
  return 0.5 / max(lambdaV + lambdaL, 1e-7);
}

/** The width along the brushing for a @p roughness and an @p anisotropy
 *  strength: the roughness's own at zero, rising to one at full strength.
 *  Across the brushing the width stays the roughness's own. */
public float anisotropicWidth(float roughness, float anisotropy) {
  float a = microfacetWidth(roughness);
  float s = min(max(anisotropy, 0.0), 1.0);
  return a + (1.0 - a) * s * s;
}

/** THE SPECULAR SIDE OF ONE LIGHT ON A BRUSHED SURFACE: `directSpecular`
 *  with the highlight stretched along the unit tangent @p t, square to the
 *  normal, by @p anisotropy. At zero strength it is `directSpecular`. */
public float3 directSpecularAnisotropic(float3 n, float3 t, float3 l,
                                        float3 v, float3 f0, float3 f90,
                                        float roughness, float anisotropy) {
  float nl = max(n.x * l.x + n.y * l.y + n.z * l.z, 0.0);
  if (nl <= 0.0) return float3(0.0, 0.0, 0.0);
  float nv = max(n.x * v.x + n.y * v.y + n.z * v.z, 0.0);
  float3 b = float3(n.y * t.z - n.z * t.y, n.z * t.x - n.x * t.z,
                    n.x * t.y - n.y * t.x);
  float3 h = l + v;
  float len = sqrt(h.x * h.x + h.y * h.y + h.z * h.z);
  h = len > 0.0 ? h / len : n;
  float nh = n.x * h.x + n.y * h.y + n.z * h.z;
  float vh = v.x * h.x + v.y * h.y + v.z * h.z;
  float alphaT = anisotropicWidth(roughness, anisotropy);
  float alphaB = microfacetWidth(roughness);
  float d = distributionGgxAnisotropic(nh, t.x * h.x + t.y * h.y + t.z * h.z,
                                       b.x * h.x + b.y * h.y + b.z * h.z,
                                       alphaT, alphaB);
  float vis = visibilityGgxAnisotropic(
      nl, nv, t.x * l.x + t.y * l.y + t.z * l.z,
      t.x * v.x + t.y * v.y + t.z * v.z, b.x * l.x + b.y * l.y + b.z * l.z,
      b.x * v.x + b.y * v.y + b.z * v.z, alphaT, alphaB);
  return fresnelSchlick(f0, f90, vh) * (d * vis * nl);
}

/** WHERE A BRUSHED SURFACE MIRRORS LIGHT FROM EVERYWHERE: a normal bent
 *  away from @p n toward the one that faces the eye across the brushing,
 *  by how strong the brushing is and how smooth the surface — the
 *  reflected ray read along it smears the panorama along the brushing.
 *  @p t is the unit tangent the highlight stretches along. At zero
 *  strength it is @p n. */
public float3 anisotropicBentNormal(float3 n, float3 t, float3 v,
                                    float roughness, float anisotropy) {
  float3 b = float3(n.y * t.z - n.z * t.y, n.z * t.x - n.x * t.z,
                    n.x * t.y - n.y * t.x);
  float3 across = float3(b.y * v.z - b.z * v.y, b.z * v.x - b.x * v.z,
                         b.x * v.y - b.y * v.x);
  float3 facing = float3(across.y * b.z - across.z * b.y,
                         across.z * b.x - across.x * b.z,
                         across.x * b.y - across.y * b.x);
  float s = min(max(anisotropy, 0.0), 1.0);
  float r = min(max(roughness, 0.0), 1.0);
  float bend = 1.0 - s * (1.0 - r);
  float bend4 = bend * bend * bend * bend;
  float3 bent = facing + (n - facing) * bend4;
  float len = sqrt(bent.x * bent.x + bent.y * bent.y + bent.z * bent.z);
  return len > 0.0 ? bent / len : n;
}

/** THE TANGENT A BRUSHING RUNS ALONG: the direction @p direction — x
 *  along @p t, y along @p b, the surface's own frame — turned by
 *  @p rotationDegrees counter-clockwise from t, and made square to the
 *  normal @p n. A frame that has collapsed answers @p fallback, which
 *  must already be square to @p n and unit length. */
public float3 anisotropyTangent(float3 n, float3 t, float3 b,
                                float2 direction, float rotationDegrees,
                                float3 fallback) {
  float angle = rotationDegrees * 0.01745329;
  float c = cos(angle);
  float s = sin(angle);
  float2 d = float2(direction.x * c - direction.y * s,
                    direction.x * s + direction.y * c);
  float3 along = t * d.x + b * d.y;
  float nd = n.x * along.x + n.y * along.y + n.z * along.z;
  along = along - n * nd;
  float len = sqrt(along.x * along.x + along.y * along.y + along.z * along.z);
  return len > 1e-6 ? along / len : fallback;
}

/** Where a Schlick reflectance that rises to @p f90 must start head on to
 *  reach @p f at the cosine @p cosTheta: Schlick run backward. */
public float3 fresnelToF0(float cosTheta, float3 f, float3 f90) {
  float x = 1.0 - min(max(cosTheta, 0.0), 1.0);
  float x2 = x * x;
  float x5 = min(max(x2 * x2 * x, 0.0), 0.9999);
  return (f - f90 * x5) / (1.0 - x5);
}

/** The head-on reflectance between two media of these indices. */
public float reflectanceFromIor(float transmitted, float incident) {
  float r = (transmitted - incident) / (transmitted + incident);
  return r * r;
}

/** THE COLOUR A PATH DIFFERENCE MAKES: two paths @p opd nanometres apart,
 *  at the phase shift @p shift each channel picked up along them, mapped
 *  through the eye's colour sensitivity — Gaussian fits of the matching
 *  functions over wavelength — into the linear display primaries. */
public float3 interferenceColor(float opd, float3 shift) {
  float phase = 6.28318531 * opd * 1.0e-9;
  float3 value = float3(5.4856e-13, 4.4201e-13, 5.2481e-13);
  float3 position = float3(1.6810e+06, 1.7953e+06, 2.2084e+06);
  float3 spread = float3(4.3278e+09, 9.3046e+09, 6.6121e+09);
  float p2 = phase * phase;
  float3 xyz = float3(
      value.x * sqrt(6.28318531 * spread.x) * cos(position.x * phase + shift.x) *
          exp(-p2 * spread.x),
      value.y * sqrt(6.28318531 * spread.y) * cos(position.y * phase + shift.y) *
          exp(-p2 * spread.y),
      value.z * sqrt(6.28318531 * spread.z) * cos(position.z * phase + shift.z) *
          exp(-p2 * spread.z));
  xyz.x = xyz.x + 9.7470e-14 * sqrt(6.28318531 * 4.5282e+09) *
                      cos(2.2399e+06 * phase + shift.x) *
                      exp(-4.5282e+09 * p2);
  xyz = xyz / 1.0685e-7;
  return float3(3.2404542 * xyz.x - 1.5371385 * xyz.y - 0.4985314 * xyz.z,
                -0.9692660 * xyz.x + 1.8760108 * xyz.y + 0.0415560 * xyz.z,
                0.0556434 * xyz.x - 0.2040259 * xyz.y + 1.0572252 * xyz.z);
}

/** THE REFLECTANCE OF A SURFACE UNDER A THIN FILM: light reflected off the
 *  film's top and off the surface beneath interferes, so what comes back
 *  is tinted by the path between them — by the film's index @p filmIor,
 *  its @p thicknessNanometres and the cosine @p cosTheta the eye meets it
 *  at — over the head-on reflectance @p baseF0 of what lies under it. The
 *  outside is air. Past total internal reflection in the film the answer
 *  is white. */
public float3 iridescenceFresnel(float filmIor, float cosTheta,
                                 float thicknessNanometres, float3 baseF0) {
  float outside = 1.0;
  float t = min(max(thicknessNanometres / 0.03, 0.0), 1.0);
  float eased = t * t * (3.0 - 2.0 * t);
  float film = outside + (filmIor - outside) * eased;
  float c1 = min(max(cosTheta, 0.0), 1.0);
  float ratio = outside / film;
  float sin2 = ratio * ratio * (1.0 - c1 * c1);
  float cos2Squared = 1.0 - sin2;
  if (cos2Squared < 0.0) return float3(1.0, 1.0, 1.0);
  float c2 = sqrt(cos2Squared);

  // The film's top, from outside: one reflectance for every channel.
  float r0 = reflectanceFromIor(film, outside);
  float x1 = 1.0 - c1;
  float x1Squared = x1 * x1;
  float r12 = r0 + (1.0 - r0) * x1Squared * x1Squared * x1;
  float t121 = 1.0 - r12;
  float phi12 = film < outside ? 3.14159274 : 0.0;
  float phi21 = 3.14159274 - phi12;

  // The surface beneath, from inside the film: the index its own
  // reflectance stands for, per channel.
  float3 f0 = min(max(baseF0, float3(0.0, 0.0, 0.0)),
                  float3(0.9999, 0.9999, 0.9999));
  float3 root = float3(sqrt(f0.x), sqrt(f0.y), sqrt(f0.z));
  float3 baseIor = (float3(1.0, 1.0, 1.0) + root) /
                   (float3(1.0, 1.0, 1.0) - root);
  float3 r1 = float3(reflectanceFromIor(baseIor.x, film),
                     reflectanceFromIor(baseIor.y, film),
                     reflectanceFromIor(baseIor.z, film));
  float x2 = 1.0 - c2;
  float x2Squared = x2 * x2;
  float3 r23 = r1 + (float3(1.0, 1.0, 1.0) - r1) * (x2Squared * x2Squared * x2);
  float3 phi23 = float3(baseIor.x < film ? 3.14159274 : 0.0,
                        baseIor.y < film ? 3.14159274 : 0.0,
                        baseIor.z < film ? 3.14159274 : 0.0);

  float opd = 2.0 * film * thicknessNanometres * c2;
  float3 phi = float3(phi21, phi21, phi21) + phi23;
  float3 r123 = min(max(r23 * r12, float3(1e-5, 1e-5, 1e-5)),
                    float3(0.9999, 0.9999, 0.9999));
  float3 rootR123 = float3(sqrt(r123.x), sqrt(r123.y), sqrt(r123.z));
  float3 rs = r23 * (t121 * t121) / (float3(1.0, 1.0, 1.0) - r123);

  // The reflectance with no interference, then the first two orders of it.
  float3 result = rs + float3(r12, r12, r12);
  float3 cm = rs - float3(t121, t121, t121);
  cm = cm * rootR123;
  result = result + cm * (interferenceColor(opd, phi) * 2.0);
  cm = cm * rootR123;
  result = result + cm * (interferenceColor(2.0 * opd, phi * 2.0) * 2.0);
  return max(result, float3(0.0, 0.0, 0.0));
}

/** THE HEAD-ON REFLECTANCE A FILM OF @p weight LEAVES: @p f0 moved toward
 *  the one whose Schlick curve reaches the film's reflectance at the eye's
 *  cosine, by the weight. A film of no thickness or no weight is no film:
 *  the answer is @p f0. Every light and the light from everywhere read the
 *  moved reflectance, so the film colours both. */
public float3 iridescentF0(float3 f0, float nDotV, float weight, float filmIor,
                           float thicknessNanometres) {
  float w = thicknessNanometres > 0.0 ? min(max(weight, 0.0), 1.0) : 0.0;
  if (w <= 0.0) return f0;
  float3 film = iridescenceFresnel(filmIor, nDotV, thicknessNanometres, f0);
  float3 filmF0 = fresnelToF0(nDotV, film, float3(1.0, 1.0, 1.0));
  return f0 + (filmF0 - f0) * w;
}

/** A DIELECTRIC'S HEAD-ON REFLECTANCE AT A STRENGTH AND A COLOUR: four per
 *  cent tinted by the linear @p color, held under one, scaled by
 *  @p strength. At strength one and white it is the four per cent every
 *  dielectric here reflects; at zero it reflects nothing. */
public float3 dielectricReflectance(float3 color, float strength) {
  float s = max(strength, 0.0);
  return min(color * 0.04, float3(1.0, 1.0, 1.0)) * s;
}

/** THE HEAD-ON REFLECTANCE of a surface whose dielectric side reflects
 *  @p dielectric: that, mixed toward the base colour as the surface turns
 *  metal. */
public float3 specularColorFrom(float3 baseColor, float metal,
                                float3 dielectric) {
  float m = min(max(metal, 0.0), 1.0);
  return dielectric * (1.0 - m) + baseColor * m;
}

/** …the grazing reflectance it rises to: the dielectric's own @p strength,
 *  mixed toward white as the surface turns metal. */
public float3 reflectance90From(float metal, float strength) {
  float m = min(max(metal, 0.0), 1.0);
  float r = min(max(strength, 0.0), 1.0) * (1.0 - m) + m;
  return float3(r, r, r);
}

/** …and the colour its diffuse side reflects: the base colour less what
 *  the dielectric reflects head on at its strongest channel, and none of
 *  it for a metal. With the four per cent every dielectric here reflects it
 *  is `diffuseColor`. */
public float3 diffuseColorFrom(float3 baseColor, float metal,
                               float3 dielectric) {
  float m = min(max(metal, 0.0), 1.0);
  float strongest = max(dielectric.x, max(dielectric.y, dielectric.z));
  return baseColor * ((1.0 - min(strongest, 1.0)) * (1.0 - m));
}

// ---- how a light reaches a point ---------------------------------------

/** HOW A POSITIONED LIGHT FALLS OFF over @p distance: the inverse square,
 *  which is what light spreading over a sphere does, times a smooth window
 *  that brings it to exactly zero at @p range — one minus the fourth power
 *  of the distance over the range, squared, so the cut has no edge. A range
 *  of zero or less is no window: the light reaches everywhere. The distance
 *  is held off zero, where the square would answer infinity. */
public float distanceFalloff(float distance, float range) {
  float d = max(distance, 1e-2);
  float falloff = 1.0 / (d * d);
  if (range > 0.0) {
    float q = d / range;
    float q2 = q * q;
    float w = min(max(1.0 - q2 * q2, 0.0), 1.0);
    falloff = falloff * w * w;
  }
  return falloff;
}

/** HOW A SPOT LIGHT'S CONE FALLS OFF: full strength where the cosine from
 *  its aim is at or above @p cosInner, nothing at or below @p cosOuter, and
 *  between them the square of where the cosine stands — a soft edge that
 *  meets both ends without a step. */
public float coneFalloff(float cosAngle, float cosOuter, float cosInner) {
  float span = max(cosInner - cosOuter, 1e-4);
  float t = min(max((cosAngle - cosOuter) / span, 0.0), 1.0);
  return t * t;
}

/** WHAT A HEMISPHERE OF TWO COLOURS PUTS ON A SURFACE: a sky of radiance
 *  @p sky over the plane square to @p up and a ground of @p ground below
 *  it, cosine-weighted toward @p n. A surface facing straight up sees
 *  only sky, one facing down only ground, one facing the horizon half of
 *  each — and that blend is exact for two half-spaces, not a fit. Read in
 *  a reflected direction, the same blend is what a mirror sees there. */
public float3 hemisphereIrradiance(float3 n, float3 up, float3 sky,
                                   float3 ground) {
  float t = 0.5 + 0.5 * (n.x * up.x + n.y * up.y + n.z * up.z);
  t = min(max(t, 0.0), 1.0);
  return ground + (sky - ground) * t;
}

/** WHERE THE SPLIT-SUM TABLE IS READ for a @p roughness and a viewing
 *  cosine @p nDotV, over a table of @p size texels a side whose columns run
 *  in the cosine and whose rows run in the roughness, both from 0 to 1 at
 *  the texel centres. Reading at the centres and between them, never past
 *  them, keeps the edges exact whatever the sampler wraps. */
public float2 environmentBrdfUv(float roughness, float nDotV, float size) {
  float r = min(max(roughness, 0.0), 1.0);
  float v = min(max(nDotV, 0.0), 1.0);
  return float2((0.5 + v * (size - 1.0)) / size,
                (0.5 + r * (size - 1.0)) / size);
}

/** ENVIRONMENT SPECULAR: the radiance the surface mirrors, weighted so a
 *  metal keeps its own colour, a dielectric reflects a few per cent head
 *  on and much more at the rim, and a rough surface takes less than a
 *  smooth one. @p brdf is the split sum's scale and bias read from the
 *  table for this roughness and angle — the GGX lobe with height-
 *  correlated visibility integrated once over the hemisphere — and the
 *  reflectance it scales is the rough Fresnel, which is the single-bounce
 *  half of the multiple-scattering composition `environmentDiffuse`
 *  completes. */
public float3 environmentSpecular(float3 radiance, float3 f0,
                                  float roughness, float nDotV,
                                  float2 brdf) {
  float3 k = fresnelRough(f0, nDotV, roughness);
  return radiance * (k * brdf.x + float3(brdf.y, brdf.y, brdf.y));
}

/** ENVIRONMENT DIFFUSE, with the light a rough surface scatters more than
 *  once put back. A single-bounce microfacet lobe loses energy as the
 *  surface roughens — the light that leaves after a second bounce is not
 *  in it — so a rough metal reads darker than the metal it is. What the
 *  single bounce reflects is `fresnelRough * brdf.x + brdf.y`; what it
 *  misses is one minus their unweighted sum; the missed share comes back
 *  tinted by the surface's average Fresnel, and the diffuse takes what
 *  neither specular share took. @p irradiance is the cosine-weighted mean
 *  radiance arriving at the surface — what a white Lambertian surface
 *  would show — and @p diffuse the surface's diffuse colour. Under a
 *  uniform environment a white metal returns exactly the radiance it was
 *  lit by. */
public float3 environmentDiffuse(float3 irradiance, float3 diffuse,
                                 float3 f0, float roughness, float nDotV,
                                 float2 brdf) {
  float3 k = fresnelRough(f0, nDotV, roughness);
  float3 one = float3(1.0, 1.0, 1.0);
  float3 singleBounce = k * brdf.x + float3(brdf.y, brdf.y, brdf.y);
  float missed = 1.0 - (brdf.x + brdf.y);
  float3 average = f0 + (one - f0) / 21.0;
  float3 returned = singleBounce * average / (one - average * missed);
  float3 remaining = one - (singleBounce + returned * missed);
  return (returned * missed + diffuse * remaining) * irradiance;
}

/** ENVIRONMENT REFLECTION, additive: the radiance at one weight, with no
 *  Fresnel and no energy accounting. Wrong at a grazing angle and right
 *  wherever an author wants to say how much sky is on a surface rather
 *  than be told. */
public float3 environmentReflection(float3 radiance, float weight) {
  return radiance * max(weight, 0.0);
}

/** The direction light takes entering a surface: @p i travels toward the
 *  surface, @p n faces the eye, @p eta is the ratio of the index it
 *  comes from to the one it enters. Zero where the angle is past total
 *  internal reflection and there is no refracted ray to answer with. */
public float3 refraction(float3 i, float3 n, float eta) {
  float ni = n.x * i.x + n.y * i.y + n.z * i.z;
  float k = 1.0 - eta * eta * (1.0 - ni * ni);
  if (k < 0.0) return float3(0.0, 0.0, 0.0);
  return i * eta - n * (eta * ni + sqrt(k));
}

/** ATTENUATION, Beer-Lambert: what is left of a radiance after it has
 *  travelled @p thickness through a medium that takes @p absorb out of
 *  it per unit. This is what makes thick glass green at its edge and
 *  clear across its face.
 *
 *  It is not called `absorption`, because a surface's own absorption is
 *  a uniform of that name and a term compiled beside one would be an
 *  ambiguous reference. A term's name has to survive being read in the
 *  same scope as every material parameter there is. */
public float3 attenuate(float3 radiance, float3 absorb, float thickness) {
  float t = max(thickness, 0.0);
  return radiance * float3(exp(-absorb.x * t), exp(-absorb.y * t),
                           exp(-absorb.z * t));
}

/** EMISSION: a surface's own light, which no shading scales. */
public float3 emission(float3 color, float strength, float3 map) {
  return color * map * max(strength, 0.0);
}

/** OCCLUSION: how much of the light that arrives from everywhere is
 *  blocked by the surface's own creases. @p strength is how far a map's
 *  value is believed. */
public float occlusion(float ao, float strength) {
  float s = min(max(strength, 0.0), 1.0);
  return 1.0 + (min(max(ao, 0.0), 1.0) - 1.0) * s;
}

// ---- the display transform ------------------------------------------

/** HOW BRIGHT A COLOUR READS, at the weights the three primaries carry
 *  into a display's luminance. The same three numbers the engine's own
 *  tone mapper uses, so a picture curved here and one curved there agree
 *  about which of two colours is the brighter. */
public float luminance(float3 color) {
  return color.x * 0.212671 + color.y * 0.715160 + color.z * 0.072169;
}

/** TONE MAPPING, Reinhard's operator on luminance: the radiance a
 *  surface or a sky answers with, turned into a colour a display can
 *  hold.
 *
 *  A panorama carries values far above one — that is what makes a sun a
 *  sun rather than a white disc the same brightness as the sky beside
 *  it — and a lit sum carries them through to the shaded colour. Cutting
 *  that sum off at one flattens every highlight to the same white and
 *  loses exactly the range the map was kept in floating point to hold.
 *  This divides it by one plus its own luminance instead: zero stays
 *  zero, a dim surface is barely touched, and a value a hundred times
 *  over white lands just under it with its shape still readable.
 *
 *  @p exposure multiplies the radiance BEFORE the curve, which is where
 *  an exposure belongs — it decides which part of the range the
 *  shoulder falls on, and doubling it is one stop. It is AUTHORED and
 *  never measured: no average luminance and no adaptation, because a
 *  diagram that dimmed itself because its content grew brighter would be
 *  a different picture every frame.
 *
 *  The ratio is taken on LUMINANCE rather than per channel, so hue and
 *  saturation survive the compression; dividing each channel on its own
 *  drags a bright colour toward white. A fully saturated channel can
 *  still land above one after this, and what holds it there is the
 *  target's own range — nothing clips it here, since clipping is what
 *  this term exists to avoid. */
public float3 toneMap(float3 radiance, float exposure) {
  float3 scaled = max(radiance * exposure, float3(0.0, 0.0, 0.0));
  return scaled / (1.0 + luminance(scaled));
}

/** THE NEUTRAL TONE CURVE: below a shoulder near 0.8 it changes nothing
 *  but a small offset — four hundredths off every channel, eased to
 *  nothing toward black so a black stays black — so a surface lit to show
 *  its own colour shows very nearly that colour and a page painted in the
 *  unit range reads as painted; above the shoulder a compression brings
 *  any brightness under one, desaturating toward white only as much as a
 *  highlight that bright needs. Exposure multiplies first, as for
 *  `toneMap`. */
public float3 toneMapNeutral(float3 radiance, float exposure) {
  float3 c = max(radiance * exposure, float3(0.0, 0.0, 0.0));
  float lowest = min(c.x, min(c.y, c.z));
  float offset = lowest < 0.08 ? lowest - 6.25 * lowest * lowest : 0.04;
  c = c - float3(offset, offset, offset);
  float peak = max(c.x, max(c.y, c.z));
  float start = 0.76;
  if (peak < start) return c;
  float d = 1.0 - start;
  float newPeak = 1.0 - d * d / (peak + d - start);
  c = c * (newPeak / peak);
  float g = 1.0 - 1.0 / (0.15 * (peak - newPeak) + 1.0);
  return c + (float3(newPeak, newPeak, newPeak) - c) * g;
}

/** NO CURVE: the radiance at the exposure, cut off at one by the target
 *  and nowhere else. What a picture whose every value already sits in
 *  the unit range wants, and the reference the other two are measured
 *  against. */
public float3 toneMapLinear(float3 radiance, float exposure) {
  return max(radiance * exposure, float3(0.0, 0.0, 0.0));
}

/** THE CURVE A CAMERA NAMES, by its number: 0 Reinhard on luminance
 *  (`toneMap`), 1 the neutral curve, 2 none. */
public float3 toneCurve(float3 radiance, float exposure, float curve) {
  if (curve < 0.5) return toneMap(radiance, exposure);
  if (curve < 1.5) return toneMapNeutral(radiance, exposure);
  return toneMapLinear(radiance, exposure);
}

/** THE CURVES RUN BACKWARD: the radiance that @p display — a linear
 *  colour in the unit range, the curve's output before it is encoded —
 *  is what the curve @p curve makes of at @p exposure. Each curve here
 *  rises without turning back, so each has exactly one answer; a display
 *  value at white, which Reinhard and the neutral curve only approach, is
 *  answered with the brightest radiance a half float holds short of it.
 *  This is how a surface drawn as authored passes through the camera's
 *  reading unchanged: it is written as the light the reading turns back
 *  into the colour it was authored in. */
public float3 toneMapInverse(float3 display, float exposure) {
  float3 y = max(display, float3(0.0, 0.0, 0.0));
  float l = min(luminance(y), 0.9999);
  return y / ((1.0 - l) * max(exposure, 1e-6));
}

/** `toneMapNeutral` backward: the compression above the shoulder undone
 *  from the peak it left — the brightest channel, which the
 *  desaturation never moves — and then the offset near black undone from
 *  the lowest channel, which the offset alone decides. */
public float3 toneMapNeutralInverse(float3 display, float exposure) {
  float3 c = max(display, float3(0.0, 0.0, 0.0));
  float newPeak = min(max(c.x, max(c.y, c.z)), 0.9999);
  float start = 0.76;
  if (newPeak >= start) {
    float d = 1.0 - start;
    float peak = d * d / (1.0 - newPeak) - d + start;
    float g = 1.0 - 1.0 / (0.15 * (peak - newPeak) + 1.0);
    c = (min(c, float3(newPeak, newPeak, newPeak)) -
         float3(newPeak, newPeak, newPeak) * g) /
        max(1.0 - g, 1e-6);
    c = c * (peak / max(newPeak, 1e-6));
  }
  float lowest = min(c.x, min(c.y, c.z));
  float offset = 0.04;
  if (lowest < 0.04) {
    float before = sqrt(max(lowest, 0.0) / 6.25);
    offset = before - max(lowest, 0.0);
  }
  return (c + float3(offset, offset, offset)) / max(exposure, 1e-6);
}

/** `toneMapLinear` backward: the exposure divided out. */
public float3 toneMapLinearInverse(float3 display, float exposure) {
  return max(display, float3(0.0, 0.0, 0.0)) / max(exposure, 1e-6);
}

/** THE CURVE A CAMERA NAMES, backward, by its number. */
public float3 toneCurveInverse(float3 display, float exposure, float curve) {
  if (curve < 0.5) return toneMapInverse(display, exposure);
  if (curve < 1.5) return toneMapNeutralInverse(display, exposure);
  return toneMapLinearInverse(display, exposure);
}

// ---- the colour encoding --------------------------------------------
// Light adds and multiplies in LINEAR values; a display shows, and an
// eight-bit image stores, ENCODED ones, spaced so equal steps look equal.
// Every colour an author types and every colour texture is encoded, so it
// is decoded before any light touches it, and what the lighting answers is
// encoded once more on its way to the target. Data — a normal, a
// roughness, a metalness, an occlusion — is a number and not a colour, and
// is read as stored.

/** One encoded channel as the linear value it stands for, along the
 *  standard display curve: a short straight segment near black and a
 *  2.4 power above it. Above one it carries on along the same power, so a
 *  colour typed brighter than white stays brighter in light. */
public float linearFromSrgbChannel(float c) {
  float x = max(c, 0.0);
  return x <= 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

/** A colour, decoded. */
public float3 linearFromSrgb(float3 c) {
  return float3(linearFromSrgbChannel(c.x), linearFromSrgbChannel(c.y),
                linearFromSrgbChannel(c.z));
}

/** One linear channel, encoded for a display: the inverse of
 *  `linearFromSrgbChannel` over the unit range, which is all a display
 *  target holds. */
public float srgbFromLinearChannel(float c) {
  float x = min(max(c, 0.0), 1.0);
  return x <= 0.0031308 ? x * 12.92 : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}

/** A colour, encoded. */
public float3 srgbFromLinear(float3 c) {
  return float3(srgbFromLinearChannel(c.x), srgbFromLinearChannel(c.y),
                srgbFromLinearChannel(c.z));
}
