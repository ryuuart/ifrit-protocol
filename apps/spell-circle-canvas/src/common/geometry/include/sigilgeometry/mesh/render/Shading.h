#pragma once

/** @file
 * The shading terms a host tier evaluates, and the panorama it samples
 * them against.
 *
 * These are the SAME PIECES of arithmetic a device shader is composed
 * of — a Lambert factor, a Fresnel weight, a split-sum environment
 * reflection — written for a tier that has no shading language. Two
 * transcriptions of one arithmetic is what a host tier costs, and what
 * holds them together is that each is pinned to the same closed form by
 * its own test and that the two tiers are compared as pictures within a
 * stated per-channel ceiling. Where the two must not drift, the value is
 * written out — the transcendentals as polynomials — rather than taken
 * from a library that answers differently on either side.
 *
 * A host tier shades PER VERTEX. That is not a limitation of these
 * functions, which take a direction and answer a colour; it is where
 * they are called from, and it is why a coarse mesh under a bright sky
 * reads as facets where a device reads as a curve.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>

namespace sigil::geometry::mesh::render {

/** THE PANORAMA A LIT SURFACE SAMPLES, as this tier reads it: the
 *  prefiltered chain a reflection picks a level from, the cosine
 *  convolution a diffuse term reads, where the sky is turned to, and how
 *  far it is believed. Empty levels are no environment at all, and a
 *  surface then keeps the flat ambient it always had. */
struct Environment {
  /** Level 0 sharpest, each one after it blurrier; a roughness picks
   *  among them. */
  std::vector<sk_sp<SkImage>> levels;
  /** The diffuse side: what a surface facing a direction receives from
   *  everywhere, already convolved with a cosine lobe. */
  sk_sp<SkImage> irradiance;
  /** A SECOND PANORAMA and how far along the way to it every sample
   *  stands. Both are sampled and mixed rather than one being rebuilt,
   *  which is what lets a sky change while the frame is running. */
  std::vector<sk_sp<SkImage>> nextLevels;
  sk_sp<SkImage> nextIrradiance;
  float crossfade = 0;
  /** Takes a WORLD-space direction into the panorama's own frame, so
   *  turning the node that placed the sky turns the reflection. */
  glm::mat3 orientation{1.0f};
  glm::vec3 tint{1, 1, 1};
  float intensity = 1;
  float diffuse = 1;
  float specular = 1;
  float roughnessBias = 0;
  /** THE EXPOSURE the whole picture is read at: what every radiance is
   *  multiplied by before the tone curve compresses it. It stands even
   *  where no panorama does — a set with no sky still shades a sum and
   *  still ends at the curve — which is why it is a plain field and not
   *  something `valid()` gates. */
  float exposure = 1;
  /** THE SKY SHOWN behind the set, at this strength — zero draws none of
   *  it, so the dial is also the switch — and blurred by this much, in
   *  the same roughness units a reflection reads. */
  float backdrop = 0;
  float backdropBlur = 0;
  /** GROUND PROJECTION: past zero, the sky SHOWN is a sphere of this
   *  radius centred at `projectionCenter` rather than a panorama at
   *  infinity, so an eye moving through the set sees the horizon shift
   *  the way it would outdoors. Zero leaves the sky at infinity, which
   *  is what a panorama means on its own. It reaches the backdrop and
   *  nothing else: what a surface mirrors stays at infinity. */
  float groundRadius = 0;
  /** Where that sphere is centred, in world units. */
  glm::vec3 projectionCenter{0, 0, 0};

  bool valid() const { return !levels.empty() || irradiance != nullptr; }
};

/** THE DIRECTION A BACKDROP PIXEL READS, for an eye at @p eye (world
 *  space) looking along @p ray (world space, any length): with no
 *  ground projection the ray itself; with one, the direction from the
 *  sphere's centre to where the ray leaves it. An eye on or outside the
 *  sphere sees no inside to project onto and reads by direction again,
 *  and an eye at the centre reads by direction whatever the radius,
 *  which is what makes the two agree where they meet. */
inline glm::vec3 backdropRay(const Environment& environment, glm::vec3 eye,
                             glm::vec3 ray) {
  const float radius = environment.groundRadius;
  if (radius <= 0) return ray;
  const float len = std::sqrt(glm::dot(ray, ray));
  if (len <= 0) return ray;
  const glm::vec3 d = ray / len;
  const glm::vec3 o = eye - environment.projectionCenter;
  const float b = glm::dot(o, d);
  const float c = glm::dot(o, o) - radius * radius;
  if (c >= 0) return ray;
  // Inside the sphere the quadratic has one positive root — the exit —
  // and its discriminant is positive wherever c is negative.
  const float t = std::sqrt(b * b - c) - b;
  // The exit point stands on the sphere, so its distance from the centre
  // is the radius: dividing by it is the normalisation.
  return (o + d * t) / radius;
}

/** Arctangent of y/x over all four quadrants, as arithmetic. */
inline float atan2P(float y, float x) {
  const float ax = std::abs(x), ay = std::abs(y);
  const float mx = std::max(ax, ay), mn = std::min(ax, ay);
  const float a = mx > 0.0f ? mn / mx : 0.0f;
  const float s = a * a;
  float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
  if (ay > ax) r = 1.57079637f - r;
  if (x < 0.0f) r = 3.14159274f - r;
  return y < 0.0f ? -r : r;
}

/** Arccosine over [-1, 1], as arithmetic. */
inline float acosP(float x) {
  const float c = std::clamp(x, -1.0f, 1.0f);
  const float ax = std::abs(c);
  const float r =
      std::sqrt(1.0f - ax) *
      (1.5707288f + ax * (-0.2121144f + ax * (0.0742610f + ax * -0.0187293f)));
  return c < 0.0f ? 3.14159274f - r : r;
}

/** Where a direction reads on an equirect panorama: u = 0.5 turns to
 *  face -z, u wraps once around, and v runs 0 at the zenith to 1 at the
 *  nadir. */
inline glm::vec2 equirectUv(glm::vec3 d) {
  const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
  const glm::vec3 u = len > 0 ? d / len : glm::vec3(0, 1, 0);
  return {0.5f + atan2P(u.x, -u.z) * 0.15915494f, acosP(u.y) * 0.31830989f};
}

/** The reflectance a surface shows head on: four per cent for a
 *  dielectric, the surface's own colour for a metal. */
inline glm::vec3 specularColor(glm::vec3 baseColor, float metal) {
  const float m = std::clamp(metal, 0.0f, 1.0f);
  return glm::vec3(0.04f) * (1.0f - m) + baseColor * m;
}

/** Fresnel, Schlick's form, held short of white on a rough surface so a
 *  rough metal's rim does not read brighter than a mirror's. */
inline glm::vec3 fresnelRough(glm::vec3 f0, float cosTheta, float roughness) {
  const float c = 1.0f - std::clamp(cosTheta, 0.0f, 1.0f);
  const float c2 = c * c;
  const float f = c2 * c2 * c;
  const glm::vec3 ceiling = glm::max(glm::vec3(1.0f - roughness), f0);
  return f0 + (ceiling - f0) * f;
}

/** The split-sum environment BRDF as the analytic fit of the integral:
 *  x scales the surface's own reflectance, y is added to it. */
inline glm::vec2 environmentBrdf(float roughness, float nDotV) {
  const float r = std::clamp(roughness, 0.0f, 1.0f);
  const float v = std::clamp(nDotV, 0.0f, 1.0f);
  const glm::vec4 c0(-1.0f, -0.0275f, -0.572f, 0.022f);
  const glm::vec4 c1(1.0f, 0.0425f, 1.04f, -0.04f);
  const glm::vec4 t = r * c0 + c1;
  const float a004 = std::min(t.x * t.x, std::exp2(-9.28f * v)) * t.x + t.y;
  return glm::vec2(-1.04f, 1.04f) * a004 + glm::vec2(t.z, t.w);
}

/** The radiance a surface mirrors, weighted so a metal keeps its own
 *  colour and a dielectric picks the sky up at its rim. */
inline glm::vec3 environmentSpecular(glm::vec3 radiance, glm::vec3 f0,
                                     float roughness, float nDotV) {
  const glm::vec2 ab = environmentBrdf(roughness, nDotV);
  return radiance * (f0 * ab.x + glm::vec3(ab.y));
}

/** Beer-Lambert: what is left of a radiance after @p thickness of a
 *  medium that takes @p absorb out of it per unit. */
inline glm::vec3 attenuate(glm::vec3 radiance, glm::vec3 absorb,
                           float thickness) {
  const float t = std::max(thickness, 0.0f);
  return radiance * glm::vec3(std::exp(-absorb.x * t), std::exp(-absorb.y * t),
                              std::exp(-absorb.z * t));
}

/** How bright a colour reads, at the weights the three primaries carry
 *  into a display's luminance. */
inline float luminance(glm::vec3 color) {
  return color.x * 0.212671f + color.y * 0.715160f + color.z * 0.072169f;
}

/** TONE MAPPING, Reinhard's operator on luminance: the radiance a
 *  surface or a sky answers with, turned into a colour a display can
 *  hold.
 *
 *  A panorama carries values far above one — that is what makes a sun a
 *  sun rather than a white disc the same brightness as the sky beside
 *  it. Cutting the lit sum off at one flattens every highlight to the
 *  same white; dividing it by one plus its own luminance leaves zero at
 *  zero, barely touches a dim surface, and lands a value a hundred times
 *  over white just under it with its shape still readable.
 *
 *  @p exposure multiplies the radiance BEFORE the curve, which is where
 *  an exposure belongs: it decides which part of the range the shoulder
 *  falls on, and doubling it is one stop. The ratio is taken on
 *  LUMINANCE rather than per channel, so hue and saturation survive the
 *  compression. A fully saturated channel can still land above one, and
 *  what holds it there is the surface it is written into. */
inline glm::vec3 toneMap(glm::vec3 radiance, float exposure) {
  const glm::vec3 scaled = glm::max(radiance * exposure, glm::vec3(0.0f));
  return scaled / (1.0f + luminance(scaled));
}

/** The direction light takes entering a surface; the zero vector past
 *  total internal reflection, where there is no refracted ray. */
inline glm::vec3 refraction(glm::vec3 i, glm::vec3 n, float eta) {
  const float ni = glm::dot(n, i);
  const float k = 1.0f - eta * eta * (1.0f - ni * ni);
  if (k < 0) return {0, 0, 0};
  return i * eta - n * (eta * ni + std::sqrt(k));
}

/** ONE TEXEL OF A PANORAMA, bilinear, wrapping in azimuth and clamping
 *  at the poles. A host tier reads a handful of these per vertex, so the
 *  image is read through its own pixels rather than through a shader. */
glm::vec3 samplePanorama(const sk_sp<SkImage>& panorama, glm::vec2 uv);

/** The radiance @p direction (world space) mirrors off the environment
 *  at @p roughness, tint and strength applied. Black when the
 *  environment carries no chain. */
glm::vec3 environmentRadiance(const Environment& environment,
                              glm::vec3 direction, float roughness);

/** What falls on a surface facing @p normal (world space) from every
 *  direction — the cosine convolution, tint and strength applied. Black
 *  when the environment carries none. */
glm::vec3 environmentIrradiance(const Environment& environment,
                                glm::vec3 normal);

/** THE SKY ITSELF, painted over the whole of @p canvas before anything
 *  stands in front of it: every pixel reads the panorama along the ray
 *  the eye looks through it — remapped through `backdropRay` where the
 *  environment projects onto a ground sphere. @p projection is the
 *  camera's own, without the view in it, and @p viewMatrix is what
 *  carries a direction back out of the space the shading is written in;
 *  the eye's own world position is read off its inverse, so there is no
 *  second statement of where the camera stands to disagree with it.
 *
 *  It is a fill and not a body because a body would need a mesh, a
 *  placement and a depth, and a sky has none of the three: it is what is
 *  there when nothing else is. Does nothing where the environment is
 *  empty or its backdrop strength is zero. */
void drawBackdrop(SkCanvas& canvas, const Environment& environment,
                  const glm::mat4& projection, const glm::mat4& viewMatrix,
                  SkSize viewport);

}  // namespace sigil::geometry::mesh::render
