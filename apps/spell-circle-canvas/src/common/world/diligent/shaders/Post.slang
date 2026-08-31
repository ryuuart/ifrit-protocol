// A POST PASS ON THE DEVICE: one triangle covering the target, and one
// fragment stage per thing a post pass does.
//
// These entry points depend on nothing a material supplies, so they are
// compiled once when this library is built rather than when it runs.
//
// EVERY IMAGE HERE HOLDS PREMULTIPLIED PIXELS, because that is what the
// targets hold. A grade is defined on straight colour, so it
// unpremultiplies, grades and premultiplies again; a blur and a
// composite are linear in premultiplied colour and need neither.

import Portable;

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
/** The colour a grade multiplies through. */
uniform float4 uTint;

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
  let src = uSource.Sample(input.uv);
  let a = src.a;
  let straight = a > 0.0 ? src.rgb / a : src.rgb;
  let graded = saturateP(straight * uGrade.x * uTint.rgb + uGrade.y * uTint.rgb);
  return float4(graded * a, a) * uGrade.z;
}

/** A masked pass's second draw: the graded picture, reaching the one
 *  beneath it only where the coverage stands. */
[shader("fragment")]
float4 fsMasked(VSOut input) : SV_Target {
  return uSource.Sample(input.uv) * uCoverage.Sample(input.uv).a * uGrade.z;
}
