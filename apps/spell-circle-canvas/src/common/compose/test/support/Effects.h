#pragma once
// SkSL effects the tests drive materials with: a one-uniform probe and a
// deliberately per-pixel-expensive shader for the cache promoter.

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Paint.h>

#include "Host.h"

namespace {

sk_sp<SkRuntimeEffect> ukEffect() {
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uK;"
               "half4 main(float2 p) { return half4(uK, 0, 0, 1); }"));
  return effect;
}

/** Expensive per PIXEL, which is the only kind of expensive that matters
 *  here: automatic promotion thresholds on how long a node takes to paint,
 *  so cost has to come from fragment work over an area. */
sk_sp<SkRuntimeEffect> heavyEffect(bool withTime) {
  // The static variant must not so much as DECLARE uTime: Material::sksl
  // reads liveness off the declaration, not off whether anything drives it.
  SkString src;
  if (withTime) src.append("uniform float uTime;");
  src.append("half4 main(float2 p) {");
  src.append(withTime ? "  float t = uTime;" : "  float t = 0.0;");
  src.append(
      "  float v = 0.0;"
      "  for (int i = 0; i < 40; ++i) {"
      "    float f = float(i) + 1.0;"
      "    v += sin(p.x * 0.031 * f + t) *"
      "         cos(p.y * 0.027 * f - t) / f;"
      "  }"
      "  float g = clamp(v * 0.5 + 0.5, 0.0, 1.0);"
      "  return half4(half(g), half(g * 0.5), half(1.0 - g), 1.0);"
      "}");
  auto [effect, err] = SkRuntimeEffect::MakeForShader(src);
  if (!effect) ADD_FAILURE() << err.c_str();
  return effect;
}

/** ONE effect for the whole process, and this is not tidiness.
 *
 *  `heavyEffect()` mints a fresh SkRuntimeEffect on every call, and a fresh
 *  effect pointer makes the material recipe compare unequal — so a fixture
 *  that re-describes each frame dirties the node's own paint each frame and
 *  no bake of any kind can hold. Tests that describe once and then only
 *  `frame()` never notice; a test whose whole point is that a child moves
 *  must re-describe, and then it does. Materials built from ordinary values
 *  compare fine; a raw SkSL effect pointer is the one that does not. */
sk_sp<SkRuntimeEffect> sharedHeavyEffect() {
  static sk_sp<SkRuntimeEffect> effect = heavyEffect(false);
  return effect;
}

}  // namespace
