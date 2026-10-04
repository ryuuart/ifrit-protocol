#pragma once

// THE BEAM: the picture a tube draws, raster lines, RGB spread, jitter and
// grain over a flat surface, with no glass and no light — a program the
// study's filter runs over its whole frame. The body adapts
// cool-retro-term and is GPL-3.0-or-later, as CRT-NOTICE.md beside this
// header says.

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>

#include <cmath>
#include <glm/vec4.hpp>
#include <initializer_list>
#include <string>
#include <string_view>

namespace lain_navi {

/** What the tube draws, in the coordinates it draws it in: the picture
 * in the `content` slot read line by line with the guns converging
 * `uRgbShift` apart, rastered at `uScanPitch`, swept by `uJitter` and
 * `uSync`, and carrying the supply's `uFlicker` and the signal's
 * `uNoise` at `uBrightness`. No curvature and no light — those are the
 * glass's and the bloom's. What it draws is opaque and clamped to what
 * a display can carry; outside the bounds it draws nothing at all. */
struct CrtBeamParameters {
  glm::vec4 uBounds{0, 0, 1, 1};  ///< left, top, width, height
  float uRgbShift = 0;
  float uScanPitch = 3;
  float uRaster = 0;
  float uNoise = 0;
  float uBrightness = 1;
  float uJitter = 0;
  float uFlicker = 0;
  float uSync = 0;
  float uTime = 0;
};

inline constexpr std::string_view kCrtBeamSkSL = R"SHADER(
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) Filippo Scognamiglio and contributors

// THE BEAM: the picture a tube draws, in the coordinates it draws it in.
// Nothing here knows there is glass over it — no curvature and no light
// — so raster lines over a flat surface are this body alone, and a
// glass over it evaluates this at the coordinate it bent to.

float crtHash(float2 p) {
  return fract(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

// The signal where the picture is, and nothing outside it: a sweep the
// clock displaced past the edge of the tube carries no signal back.
half3 crtPicture(float2 p) {
  float2 uv = (p - uBounds.xy) / max(uBounds.zw, float2(1));
  if (min(uv.x, uv.y) < 0 || max(uv.x, uv.y) >= 1) return half3(0);
  return content.eval(p).rgb;
}

half4 crtBeam(float2 p) {
  float2 size = max(uBounds.zw, float2(1));
  float2 uv = (p - uBounds.xy) / size;
  if (min(uv.x, uv.y) < 0 || max(uv.x, uv.y) >= 1) return half4(0);
  float frame = floor(uTime * 60);
  // WHERE THE LINE WAS READ FROM, which is where the sweep was when it
  // drew it: an unsteady clock jitters each line of the frame by its own
  // amount, and a sync that is off bends the whole column slowly.
  float2 q = p;
  q.x += (crtHash(float2(floor(p.y), frame)) - 0.5) * uJitter;
  q.x += sin(uv.y * 24 + uTime * 4) * uSync;
  half3 texel = crtPicture(q);
  // Three guns converge a little apart, so each channel is read where
  // its own gun put it and an edge carries colour along it. It is read
  // HERE, of the picture, rather than of the finished line: a reading
  // of the line would be the whole sweep taken three times over.
  if (uRgbShift != 0) {
    half3 left = crtPicture(q - float2(uRgbShift, 0));
    half3 right = crtPicture(q + float2(uRgbShift, 0));
    texel = left * half3(0.10, 0.20, 0.30) +
            right * half3(0.30, 0.20, 0.10) + texel * 0.60;
  }
  // The raster: a bright line and a dark one per pitch, the bright one
  // lifted and the dark one held down by what the phosphor already
  // carries, so a lit grey gains lines and an unlit black does not.
  half3 high = ((1.0 + 0.30) - 0.2 * texel) * texel;
  half3 low = ((1.0 - 0.30) + 0.1 * texel) * texel;
  float mask = 1 - abs(fract((q.y - uBounds.y) / max(uScanPitch, 1)) * 2 - 1);
  texel = mix(texel, mix(low, high, mask), clamp(uRaster, 0, 1));
  // The supply and the signal: every frame stands a little brighter or
  // dimmer than the one before it, and the line carries its own grain.
  texel *= uBrightness * (1 + (crtHash(float2(frame, 7)) - 0.5) * uFlicker);
  texel +=
      half3((crtHash(floor(p) + float2(frame, frame * 0.37)) - 0.5) * uNoise);
  return half4(clamp(texel, 0, 1), 1);
}
)SHADER";

/** A body out of @p parts — each module before the ones that read it —
 *  entered at @p call, the one line saying which of them is the whole
 *  picture. */
inline std::string crtBodyOf(std::initializer_list<std::string_view> parts,
                             std::string_view call) {
  std::string source;
  for (std::string_view part : parts) {
    source += part;
    source += '\n';
  }
  source += "half4 main(float2 p) { return ";
  source += call;
  source += "; }\n";
  return source;
}

inline sigil::material::Material crtBeam(const CrtBeamParameters& parameters) {
  return sigil::material::shader(
      crtBodyOf({kCrtBeamSkSL}, "crtBeam(p)"), parameters,
      {.key = "lain_navi.crt.beam", .textures = {{"content", {}}}});
}

/** How far the beam reads away from the pixel it paints: the sideways
 *  sweep and the spread across the guns. */
inline float crtBeamSampleRadius(const CrtBeamParameters& p) {
  return std::abs(p.uRgbShift) + std::abs(p.uJitter) + std::abs(p.uSync);
}

}  // namespace lain_navi
