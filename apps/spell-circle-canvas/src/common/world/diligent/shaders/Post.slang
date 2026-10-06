// A POST PASS ON THE DEVICE: one triangle covering the target, and one
// fragment stage per thing a post pass does.
//
// These entry points depend on nothing a material supplies, so they are
// compiled once when this library is built rather than when it runs.
//
// EVERY IMAGE HERE HOLDS PREMULTIPLIED LINEAR RADIANCE, because that is
// what the targets hold: light, before the camera's exposure and tone
// curve read it. A blur, a composite and a bloom are linear in it and run
// on it as it stands, which is where spreading and adding light is
// right. A grade is stated as a display reads a colour, so it reads the
// picture through the camera's curve, grades it there and writes it back
// through the curve run backward. The final stage is the last of them:
// the curve, once, and the encoding for a display.

import Portable;
import Shading;

uniform Sampler2D uSource;
/** The coverage a masked pass reaches its picture through; sampled only
 *  by the masked stage. */
uniform Sampler2D uCoverage;

/** A second and a third picture, for a stage that reads more than one:
 *  a composite's second layer, a bloom's two spreads. */
uniform Sampler2D uSecond;
uniform Sampler2D uThird;

/** x: 1/width, y: 1/height of what `uSource` holds; z: the blur's sigma,
 *  w: unused. */
uniform float4 uTexel;
/** x, y: 1/width and 1/height of what `uSecond` holds; z, w: the same for
 *  `uThird`. */
uniform float4 uTexels;
/** The blur's direction in texels — (1,0) across, (0,1) down. */
uniform float4 uDirection;
/** A grade: x gain, y lift, z opacity, w unused. */
uniform float4 uGrade;
/** The colour a grade multiplies through; for a fill, the colour filled
 *  with, as authored. */
uniform float4 uTint;
/** THE CAMERA'S READING. x: the exposure; y: the curve — 0 Reinhard on
 *  luminance, 1 the neutral curve, 2 none. */
uniform float4 uTone;
/** A bloom's dials. x: the threshold a pixel's luminance must pass before
 *  it spreads; y: how much of what passes is added back. */
uniform float4 uBloom;
/** A composite's: x the opacity its second layer arrives at, y the mode
 *  — 0 plus-lighter, 1 source-over. */
uniform float4 uLayer;

/** A premultiplied radiance as a display shows it, premultiplied. Alpha
 *  past one — light added over what was already opaque — counts as one. */
float4 displayed(float4 radiance) {
  let a = saturateP(radiance.a);
  let straight = a > 0.0001 ? radiance.rgb / a : radiance.rgb;
  let rgb = srgbFromLinear(toneCurve(straight, uTone.x, uTone.y));
  return float4(rgb * (a > 0.0001 ? a : 1.0), a);
}

/** A straight display colour as the premultiplied radiance the final
 *  stage turns back into it. */
float4 radianceOf(float3 display, float a) {
  let light = toneCurveInverse(linearFromSrgb(saturateP(display)), uTone.x,
                               uTone.y);
  return float4(light * a, a);
}

struct VSOut {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

/** One triangle large enough to cover the target, so no vertex buffer
 *  and no index buffer are bound for a post pass at all.
 *
 *  A TEXTURE'S ORIGIN IS ITS TOP LEFT and clip space counts y upward, so
 *  the vertical coordinate is turned over between the two. A stage that
 *  sampled without turning it over would copy an image upside down —
 *  and, because a second such stage would turn it back, a chain of them
 *  would be right only when its length was even. */
[shader("vertex")]
VSOut vsFullscreen(uint index: SV_VertexID) {
  VSOut out;
  let uv = float2(float((index << 1) & 2), float(index & 2));
  out.uv = uv;
  out.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
  return out;
}

[shader("fragment")]
float4 fsCopy(VSOut input) : SV_Target {
  return uSource.Sample(input.uv) * uGrade.z;
}

/** One direction of a separable Gaussian. The radius is three sigma
 *  rounded up, which is where the weight left outside is under a
 *  thousandth of the total, and the weights are normalised over exactly
 *  the taps taken so a flat field stays flat. */
[shader("fragment")]
float4 fsBlur(VSOut input) : SV_Target {
  let sigma = max(uTexel.z, 0.0001);
  let radius = min(int(ceil(sigma * 3.0)), 64);
  let step = uDirection.xy * uTexel.xy;
  let denom = 2.0 * sigma * sigma;
  float4 sum = uSource.Sample(input.uv);
  float weightSum = 1.0;
  for (int i = 1; i <= 64; ++i) {
    if (i > radius) break;
    let offset = float(i);
    let weight = exp(-(offset * offset) / denom);
    sum = sum + (uSource.Sample(input.uv + step * offset) +
                 uSource.Sample(input.uv - step * offset)) *
                    weight;
    weightSum = weightSum + 2.0 * weight;
  }
  return sum / weightSum;
}

[shader("fragment")]
float4 fsLevels(VSOut input) : SV_Target {
  let shown = displayed(uSource.Sample(input.uv));
  let a = shown.a;
  let straight = a > 0.0 ? shown.rgb / a : shown.rgb;
  let graded = saturateP(straight * uGrade.x * uTint.rgb + uGrade.y * uTint.rgb);
  return radianceOf(graded, a) * uGrade.z;
}

/** THE FINAL STAGE: the picture as the camera reads it, encoded for a
 *  display. */
[shader("fragment")]
float4 fsFinal(VSOut input) : SV_Target {
  return displayed(uSource.Sample(input.uv));
}

/** A whole target filled with one authored colour — what a pass clears
 *  to — written as the light that leaves the final stage as that colour. */
[shader("fragment")]
float4 fsFill(VSOut input) : SV_Target {
  return radianceOf(uTint.rgb, saturateP(uTint.a));
}

/** What of a pixel is brighter than the bloom's threshold, by luminance,
 *  so a colour keeps its hue as it spreads and a picture nothing in which
 *  passes the threshold spreads nothing. */
float4 overThreshold(float4 src) {
  let l = luminance(src.rgb);
  let over = max(l - uBloom.x, 0.0);
  let kept = l > 0.0 ? over / l : 0.0;
  return float4(src.rgb * kept, 0.0);
}

/** THE FOUR TAPS A LEVEL IS MADE FROM: one texel of the level below,
 *  each tap reading between four of the larger picture's texels, so a
 *  level is the larger one averaged over four by four of its texels — a
 *  filtered halving, which keeps a thin bright line from flickering in
 *  and out of a smaller level as it moves. */
float4 tapsDown(float2 uv, bool threshold) {
  let d = uTexel.xy;
  let a = uSource.Sample(uv + float2(-d.x, -d.y));
  let b = uSource.Sample(uv + float2(d.x, -d.y));
  let c = uSource.Sample(uv + float2(-d.x, d.y));
  let e = uSource.Sample(uv + float2(d.x, d.y));
  if (threshold)
    return (overThreshold(a) + overThreshold(b) + overThreshold(c) +
            overThreshold(e)) * 0.25;
  return (a + b + c + e) * 0.25;
}

/** A LEVEL HALF THE SIZE of what it reads. */
[shader("fragment")]
float4 fsDown(VSOut input) : SV_Target {
  return tapsDown(input.uv, false);
}

/** A BLOOM'S FIRST STEP, taken while halving: what passes the threshold,
 *  at half the size. Each of the larger picture's texels is held against
 *  the threshold before it is averaged, so a single bright texel spreads
 *  as it would have at full size. */
[shader("fragment")]
float4 fsDownThreshold(VSOut input) : SV_Target {
  return tapsDown(input.uv, true);
}

/** A SMALLER PICTURE READ AT ANY POINT through a cubic B-spline over the
 *  four by four texels around it, in four filtered taps: smooth in the
 *  value and in both its slopes, so a blur that was taken small and is
 *  read large shows no seam between its texels. @p texel is one texel of
 *  the picture read. */
float4 smoothAt(Sampler2D picture, float2 uv, float2 texel) {
  let size = 1.0 / texel;
  let st = uv * size - 0.5;
  let i = floor(st);
  let f = st - i;
  let f2 = f * f;
  let f3 = f2 * f;
  let w0 = (1.0 - 3.0 * f + 3.0 * f2 - f3) / 6.0;
  let w1 = (4.0 - 6.0 * f2 + 3.0 * f3) / 6.0;
  let w2 = (1.0 + 3.0 * f + 3.0 * f2 - 3.0 * f3) / 6.0;
  let w3 = f3 / 6.0;
  let g0 = w0 + w1;
  let g1 = w2 + w3;
  // Where each pair of texels is read by one filtered tap, in texels.
  let p0 = (i - 1.0 + w1 / g0 + 0.5) * texel;
  let p1 = (i + 1.0 + w3 / g1 + 0.5) * texel;
  return (picture.Sample(float2(p0.x, p0.y)) * g0.x +
          picture.Sample(float2(p1.x, p0.y)) * g1.x) * g0.y +
         (picture.Sample(float2(p0.x, p1.y)) * g0.x +
          picture.Sample(float2(p1.x, p1.y)) * g1.x) * g1.y;
}

/** A BLUR TAKEN SMALL, read back at the target's size. */
[shader("fragment")]
float4 fsUp(VSOut input) : SV_Target {
  return smoothAt(uSource, input.uv, uTexel.xy) * uGrade.z;
}

/** A BLOOM'S LAST STEP: the picture, and both spreads read back at its
 *  size and added at the intensity. */
[shader("fragment")]
float4 fsBloom(VSOut input) : SV_Target {
  let spread = smoothAt(uSecond, input.uv, uTexels.xy) +
               smoothAt(uThird, input.uv, uTexels.zw);
  let picture = uSource.Sample(input.uv);
  return picture + float4(spread.rgb * uGrade.z, 0.0);
}

/** TWO LAYERS LAID AT ONCE: the first as it is, the second over it at
 *  the opacity, plus-lighter or source-over. */
[shader("fragment")]
float4 fsComposite(VSOut input) : SV_Target {
  let under = uSource.Sample(input.uv);
  let over = uSecond.Sample(input.uv) * uLayer.x;
  if (uLayer.y > 0.5) return over + under * (1.0 - over.a);
  return under + over;
}

/** A MASKED GRADE IN ONE DRAW: the picture everywhere, and the grade of
 *  it laid over the picture where the coverage stands. */
[shader("fragment")]
float4 fsMaskedLevels(VSOut input) : SV_Target {
  let picture = uSource.Sample(input.uv);
  let shown = displayed(picture);
  let a = shown.a;
  let straight = a > 0.0 ? shown.rgb / a : shown.rgb;
  let graded = saturateP(straight * uGrade.x * uTint.rgb + uGrade.y * uTint.rgb);
  let lifted = radianceOf(graded, a) * uCoverage.Sample(input.uv).a;
  return lifted + picture * (1.0 - lifted.a);
}

/** A masked pass's second draw: the graded picture, reaching the one
 *  beneath it only where the coverage stands. */
[shader("fragment")]
float4 fsMasked(VSOut input) : SV_Target {
  return uSource.Sample(input.uv) * uCoverage.Sample(input.uv).a * uGrade.z;
}
