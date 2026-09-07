// The PORTABLE SUBSET: the operations whose results are the same on
// every target one source is compiled for.
//
// Arithmetic and the operations IEEE 754 pins exactly agree as they are
// written — floor, frac, fmod, step, clamp, min, max, abs. Every other
// library intrinsic is two different pieces of code on two targets and
// their answers differ: writing the expression out here is what makes
// one source produce one answer.
//
//   sqrtP        correctly rounded, where a target's own sqrt is an
//                approximation
//   dotP, lengthP a fused multiply-add inside one target's library and
//                not the other's is a disagreement; written out, both
//                sides evaluate the same expression
//   mixP, smoothstepP, saturateP  written out for the same reason
//   sinP, cosP   polynomial, because library transcendentals disagree by
//                up to a few parts in a million and a discontinuity
//                downstream — a frac, a step, a clamp edge — amplifies
//                whatever disagreement reaches it
//
// Nothing here decorates its arithmetic against contraction, because
// Slang emits no such decoration in SPIR-V: a driver is free to fuse a
// multiply-add, so a polynomial written by Horner is the one shape that
// can still differ. Where that matters, evaluate without relying on the
// unfused answer.

module Portable;

/** Correctly rounded square root. A target whose own `sqrt` is an
 *  approximation is asked for its precise form instead. */
public float sqrtP(float x) {
  __target_switch {
  case metal:
    __intrinsic_asm "precise::sqrt";
  default:
    return sqrt(x);
  }
}

public float dotP(float2 a, float2 b) { return a.x * b.x + a.y * b.y; }
public float dotP(float3 a, float3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
public float dotP(float4 a, float4 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

public float lengthP(float2 v) { return sqrtP(dotP(v, v)); }
public float lengthP(float3 v) { return sqrtP(dotP(v, v)); }

/** Unit length, or the zero vector where there is no direction to
 *  answer with. */
public float3 normalizeP(float3 v) {
  let len = lengthP(v);
  return len > 0.0 ? v / len : float3(0.0, 0.0, 0.0);
}

public float mixP(float a, float b, float t) { return a + (b - a) * t; }
public float3 mixP(float3 a, float3 b, float t) { return a + (b - a) * t; }
public float4 mixP(float4 a, float4 b, float t) { return a + (b - a) * t; }

public float saturateP(float x) { return min(max(x, 0.0), 1.0); }
public float3 saturateP(float3 v) {
  return float3(saturateP(v.x), saturateP(v.y), saturateP(v.z));
}

public float smoothstepP(float edge0, float edge1, float x) {
  let t = saturateP((x - edge0) / (edge1 - edge0));
  return t * t * (3.0 - 2.0 * t);
}

/** @p x raised to a whole power, by repeated multiplication — the one
 *  form of `pow` whose two sides evaluate the same expression. A
 *  negative or fractional exponent is not this function's question. */
public float powiP(float x, int n) {
  float acc = 1.0;
  float base = x;
  int e = n;
  [ForceUnroll]
  for (int i = 0; i < 32; ++i) {
    if (e <= 0) break;
    if ((e & 1) != 0) acc = acc * base;
    base = base * base;
    e = e >> 1;
  }
  return acc;
}

/** Sine over the whole line, as a polynomial: the argument is folded
 *  into a quarter period and a degree-9 minimax series evaluates it, so
 *  the answer is arithmetic rather than a library. */
public float sinP(float x) {
  const float kTwoPi = 6.283185307179586;
  const float kPi = 3.141592653589793;
  const float kHalfPi = 1.5707963267948966;
  // Fold to [-pi, pi], then reflect the two outer quarters into the
  // inner half, where the series is accurate.
  float t = x - kTwoPi * floor(x / kTwoPi + 0.5);
  if (t > kHalfPi) t = kPi - t;
  if (t < -kHalfPi) t = -kPi - t;
  let s = t * t;
  return t * (1.0 +
              s * (-0.16666667163372039794921875 +
                   s * (0.008333329111337661743164062 +
                        s * (-0.00019840874092653393745422363 +
                             s * 0.000002752209926244640722870826))));
}

public float cosP(float x) { return sinP(x + 1.5707963267948966); }
