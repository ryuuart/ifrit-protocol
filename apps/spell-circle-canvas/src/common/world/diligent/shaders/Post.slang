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

/** x: 1/width, y: 1/height, z: the blur's sigma, w: unused. */
uniform float4 uTexel;
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

/** A BLOOM'S FIRST STEP: what of each pixel is brighter than the
 *  threshold, by luminance, so a colour keeps its hue as it spreads and a
 *  picture nothing in which passes the threshold spreads nothing. */
[shader("fragment")]
float4 fsThreshold(VSOut input) : SV_Target {
  let src = uSource.Sample(input.uv);
  let l = luminance(src.rgb);
  let over = max(l - uBloom.x, 0.0);
  let kept = l > 0.0 ? over / l : 0.0;
  return float4(src.rgb * kept, 0.0);
}

/** A masked pass's second draw: the graded picture, reaching the one
 *  beneath it only where the coverage stands. */
[shader("fragment")]
float4 fsMasked(VSOut input) : SV_Target {
  return uSource.Sample(input.uv) * uCoverage.Sample(input.uv).a * uGrade.z;
}
