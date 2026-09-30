#pragma once

// THE TUBE'S FACE: what lies between the phosphor and the viewer, as two
// programs painted once and held. The shadow mask MULTIPLIES what the
// beam lit — the three stripes of each triad, the dark between scan
// lines, and the falloff toward the corners where the face curves away —
// and the glass is painted OVER it: the bezel's plastic outside the
// face, and on the face only what a dark room gives back, a lamp's sheen
// and the shape of whoever sits in front of it.

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>

#include <glm/vec4.hpp>
#include <string>
#include <string_view>

namespace lain_navi {

/** The face of the tube: its rectangle, the radius its corners turn on,
 *  and how far its middle bulges toward the viewer. */
struct TubeParameters {
  glm::vec4 uScreen{0, 0, 1, 1};  ///< left, top, width, height
  float uCorner = 36;
  float uBulge = 0.06f;
  float uScanPitch = 3;
  float uTriadWidth = 3;
};

inline constexpr std::string_view kTubeShapeSkSL = R"SHADER(
// Signed distance to the face's rounded rectangle, negative inside.
float faceDistance(float2 p) {
  float2 halfSize = uScreen.zw * 0.5;
  float2 q = abs(p - uScreen.xy - halfSize) - halfSize + uCorner;
  return length(max(q, float2(0))) + min(max(q.x, q.y), 0) - uCorner;
}

// The face in [-1, 1] across both axes, from its centre.
float2 faceUnit(float2 p) {
  return (p - uScreen.xy - uScreen.zw * 0.5) / (uScreen.zw * 0.5);
}
)SHADER";

inline constexpr std::string_view kShadowMaskSkSL = R"SHADER(
half4 main(float2 p) {
  float2 unit = faceUnit(p);
  // The face bulges, so a row the beam draws straight is seen bowed: the
  // mask is read where the curved glass puts the point.
  float2 bent = unit * (1 + uBulge * dot(unit, unit));
  float2 seen = uScreen.xy + (bent + 1) * uScreen.zw * 0.5;
  // Each scan line is brightest along its middle and dark between.
  float across = abs(fract((seen.y - uScreen.y) / uScanPitch) * 2 - 1);
  float line = mix(1.0, 0.70, smoothstep(0.35, 1.0, across));
  // The aperture grille: red, green and blue stripes side by side, each
  // passing its own colour and holding back the other two.
  float stripe = fract(seen.x / uTriadWidth);
  half3 grille = half3(0.84) + half3(0.16) * half3(
      cos(6.2832 * (stripe - 0.1667)), cos(6.2832 * (stripe - 0.5)),
      cos(6.2832 * (stripe - 0.8333)));
  // The face falls away from the viewer toward its corners.
  float falloff = 1 - 0.55 * smoothstep(0.45, 1.35, length(unit * float2(0.92, 1.0)));
  return half4(grille * line * falloff, 1);
}
)SHADER";

inline constexpr std::string_view kGlassSkSL = R"SHADER(
half4 main(float2 p) {
  float edge = faceDistance(p);
  if (edge > 0) {
    // The bezel: dark plastic, its inner lip catching what light there is
    // and the moulding darkening away from it.
    float lip = exp(-edge / 2.5) * 0.16 + exp(-edge / 14) * 0.03;
    float lit = 1 - smoothstep(0, uScreen.w + uScreen.y * 2, p.y);
    half3 plastic = half3(0.050, 0.052, 0.062) + half3(lit * 0.025 + lip);
    return half4(plastic, 1);
  }
  float2 unit = faceUnit(p);
  // Where the face meets the bezel it turns away and nothing reaches the
  // viewer from it.
  float rim = smoothstep(-30, 0, edge) * 0.8;
  // The viewer, back-lit by nothing: a head and shoulders where the face
  // gives back less of the room.
  float2 head = (p - float2(uScreen.x + uScreen.z * 0.53, uScreen.y + uScreen.w * 0.70)) / float2(108, 128);
  float2 shoulders = (p - float2(uScreen.x + uScreen.z * 0.53, uScreen.y + uScreen.w * 1.10)) / float2(330, 190);
  float figure = max(1 - smoothstep(0.80, 1.15, length(head)),
                     1 - smoothstep(0.80, 1.10, length(shoulders)));
  float dark = max(rim, figure * 0.20);
  // The one lamp behind the viewer, a long soft sheen across the upper
  // left following the curve of the glass.
  float2 sheenAxis = unit - float2(-0.55, -0.75);
  float sheen = exp(-pow(length(sheenAxis * float2(0.9, 2.4)), 2) * 2.2) * 0.07;
  // A window's blinds, far off in the room, over the upper right.
  float2 blinds = (p - uScreen.xy) / uScreen.zw;
  float slats = step(0.5, fract(blinds.y * 46)) *
                smoothstep(0.70, 0.74, blinds.x) * (1 - smoothstep(0.88, 0.92, blinds.x)) *
                smoothstep(0.05, 0.10, blinds.y) * (1 - smoothstep(0.38, 0.46, blinds.y)) * 0.022;
  float light = (sheen + slats) * (1 - figure * 0.8) * (1 - rim);
  half3 tint = half3(0.78, 0.86, 1.0);
  // Premultiplied: the light laid over the darkening.
  return half4(tint * light, light) + half4(0, 0, 0, dark) * (1 - light);
}
)SHADER";

/** The shadow mask, painted multiplied over everything the beam lit. */
inline sigil::material::Material shadowMask(const TubeParameters& parameters) {
  std::string body{kTubeShapeSkSL};
  body += kShadowMaskSkSL;
  return sigil::material::shader(body, parameters, {.key = "lain_navi.tube.mask"});
}

/** The glass and the bezel around it, painted over the mask. */
inline sigil::material::Material glass(const TubeParameters& parameters) {
  std::string body{kTubeShapeSkSL};
  body += kGlassSkSL;
  return sigil::material::shader(body, parameters, {.key = "lain_navi.tube.glass"});
}

}  // namespace lain_navi
