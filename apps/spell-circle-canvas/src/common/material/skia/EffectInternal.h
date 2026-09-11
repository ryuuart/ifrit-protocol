#pragma once

/** @file
 * WHAT AN EFFECT'S OWN TRANSLATION UNITS SHARE: the levels a parametric
 * blur holds, the four filter graphs an effect kind is built as, and the
 * one report a name the effect will not take gets.
 *
 * Private to the effect: a consumer states a kind through the effect's
 * own factories, which is why each graph is opaque in its header.
 */

#include <include/core/SkImageFilter.h>
#include <include/core/SkShader.h>
#include <include/core/SkSize.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Effect.h>

#include <memory>
#include <string>
#include <vector>

#include "PaintDetail.h"

namespace sigil::material::skia {

/** The two blurred levels of the pyramid at a declared range: the layer
 *  at half the range and at the range. Level 0 is the layer itself. */
struct Effect::BlurLevels {
  float maxSigma = 0;
  sk_sp<SkImageFilter> half, full;
};

/** The programs a bloom is made of: the bright pass on its own, the
 *  gather, and the one tap of each that lays the gather's answer back
 *  over the sharp source. */
sk_sp<SkRuntimeEffect> bloomProgram(const char* door, const char* file);

/** The bloom's filter DAG: reduce, gather, enlarge, composite. */
sk_sp<SkImageFilter> makePhosphorBloom(SkRuntimeShaderBuilder& haloBuilder,
                                       const sk_sp<SkRuntimeEffect>& composite,
                                       float radius);

/** directionalBlur's filter: Skia's separable Gaussian on an axis, in a
 *  rotate/unrotate sandwich when the axis is not the box's. */
sk_sp<SkImageFilter> makeDirectionalBlur(float sigma, float angleDeg,
                                         float across);

/** The two blurred levels of blur()'s pyramid at a declared range, or
 *  nothing when no range was declared. */
std::shared_ptr<const Effect::BlurLevels> makeBlurLevels(float maxSigma);

/** blur()'s filter DAG over held levels: the layer, its two blurs, and
 *  the map that mixes between them, cropped to the reach the range
 *  declares. */
sk_sp<SkImageFilter> makeParamBlur(const Effect::BlurLevels* levels,
                                   float sigma, sk_sp<SkShader> sigmaMap,
                                   SkSize box);

/** A uniform name this effect will not take, said once per name.
 *
 *  Once, because a description is rebuilt every frame in a live-coding host:
 *  a per-call warning would bury the console under one typo. The ledger is
 *  capped so a program that mints names cannot grow it without bound. */
inline void warnUndeclaredEffectUniform(const char* door,
                                        const std::string& name) {
  static std::vector<std::string> seen;
  for (const std::string& s : seen)
    if (s == name) return;
  // Past the cap the name is still reported — it is the report that a
  // caller acts on — and only the ledger stops growing, which means a
  // program that mints names is told about each of them once per call
  // rather than once ever.
  if (seen.size() < 16) seen.push_back(name);
  SkDebugf(
      "[material] skia::Effect::%s(\"%s\"): the effect declares no uniform by "
      "that name at this value's size — ignored (warned once; an array "
      "must supply the declared total float count exactly)\n",
      door, name.c_str());
}

}  // namespace sigil::material::skia
