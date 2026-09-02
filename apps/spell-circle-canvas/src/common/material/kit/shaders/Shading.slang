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
// is spelled differently in every language a renderer speaks while
// arithmetic is not: a caller fetches the radiance and hands it in. That
// is what lets one text be the terms for a device shader, for a 2D paint
// and — transcribed once, in Terms.h — for a host tier that has no
// shading language at all.
//
// ONE TEXT, TWO LANGUAGES. This module is what a Slang renderer imports;
// the same text with the module line and the `public` qualifiers removed
// is what a SkSL paint prepends, which is why nothing below uses a
// construct the two languages spell differently. The transcendentals are
// written out as polynomials for the same reason a portable subset
// exists at all: a library `atan2` is two pieces of code on two targets,
// and an equirect lookup that disagreed between them would put a seam
// down the middle of a reflection.

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

/** Where a direction reads on an equirect panorama: u = 0.5 turns to
 *  face -z, u wraps once around, and v runs 0 at the zenith to 1 at the
 *  nadir. The one convention every consumer of an environment map
 *  shares, on every tier. */
public float2 equirectUv(float3 d) {
  float len = sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
  float3 u = len > 0.0 ? d / len : float3(0.0, 1.0, 0.0);
  return float2(0.5 + atan2P(u.x, -u.z) * 0.15915494,
                acosP(u.y) * 0.31830989);
}

/** The unit direction a panorama coordinate stands for — the inverse of
 *  `equirectUv`. */
public float3 equirectDirection(float2 uv) {
  float theta = uv.y * 3.14159274;
  float phi = (uv.x - 0.5) * 6.28318548;
  float s = sin(theta);
  return float3(s * sin(phi), cos(theta), -s * cos(phi));
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

/** THE SPLIT-SUM ENVIRONMENT BRDF: the scale and bias that turn a
 *  prefiltered radiance into a specular reflection, as the analytic fit
 *  of the integral rather than a lookup table — one texture fewer to
 *  bind, and the same two numbers. x scales the surface's own
 *  reflectance, y is added to it. */
public float2 environmentBrdf(float roughness, float nDotV) {
  float r = min(max(roughness, 0.0), 1.0);
  float v = min(max(nDotV, 0.0), 1.0);
  float4 c0 = float4(-1.0, -0.0275, -0.572, 0.022);
  float4 c1 = float4(1.0, 0.0425, 1.04, -0.04);
  float4 t = r * c0 + c1;
  float a004 = min(t.x * t.x, exp2(-9.28 * v)) * t.x + t.y;
  return float2(-1.04, 1.04) * a004 + t.zw;
}

/** ENVIRONMENT SPECULAR, split-sum: the radiance the surface mirrors,
 *  weighted so a metal keeps its own colour, a dielectric reflects a few
 *  per cent head on and much more at the rim, and a rough surface takes
 *  less than a smooth one. */
public float3 environmentSpecular(float3 radiance, float3 f0,
                                  float roughness, float nDotV) {
  float2 ab = environmentBrdf(roughness, nDotV);
  return radiance * (f0 * ab.x + float3(ab.y, ab.y, ab.y));
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

/** ABSORPTION, Beer-Lambert: what is left of a radiance after it has
 *  travelled @p thickness through a medium that takes @p absorption out
 *  of it per unit. This is what makes thick glass green at its edge and
 *  clear across its face. */
public float3 absorption(float3 radiance, float3 absorb, float thickness) {
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
