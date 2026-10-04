#pragma once

// THE TUBE: a softly curved colour CRT with optical bloom, RGB spread,
// raster lines, vignette and fine grain — the beam, the light it throws
// and the glass over both, their bodies in one program with the glass
// reading the beam, so the tube is drawn flat and bent once. The bodies
// adapt cool-retro-term and are GPL-3.0-or-later, as CRT-NOTICE.md beside
// this header says. The MAGI studies of this family include it from here.

#include <include/core/SkRect.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/core/Material.h>

#include <glm/vec4.hpp>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>

namespace evangelion {

/** A screen in local coordinates. Distances are local pixels, strengths
 * are nonnegative, and zero strengths preserve the content inside the
 * bounds. Time is explicit: re-describe to advance noise, flicker and
 * sync. THE SUPPLY IS THE BEAM'S — brightness and flicker modulate the
 * picture the tube draws and nothing else, so the light over it holds
 * its own strength while the picture dims and stutters.
 * @trap The screen is OPAQUE: a transparent source pixel reads as black
 * glass. */
struct CrtParameters {
  glm::vec4 uBounds{0, 0, 1, 1};  ///< left, top, width, height
  float uCurvature = 0;
  float uRgbShift = 0;
  float uScanPitch = 3;
  float uRaster = 0;
  float uBloomRadius = 3;  ///< Gaussian sigma of the bloom, local pixels
  float uBloom = 0;
  float uNoise = 0;
  float uVignette = 0;
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
inline constexpr std::string_view kCrtBloomSkSL = R"SHADER(
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) Filippo Scognamiglio and contributors

// THE TUBE'S LIGHT: the layer already blurred, read once. The blur is an
// EXECUTOR's — a separable Gaussian at uBloomRadius over the whole layer
// — so the cost of the light follows its own radius and no tap count
// caps how far it may reach before it starts leaving copies of fine
// lettering. It is taken over the WHOLE layer, so something bright
// standing outside the tube lights the glass near that edge the way a
// lamp beside a monitor does; where the light LANDS is the tube's
// rectangle and nowhere else.
half4 crtLight(float2 p) {
  float2 uv = (p - uBounds.xy) / max(uBounds.zw, float2(1));
  if (min(uv.x, uv.y) < 0 || max(uv.x, uv.y) >= 1) return half4(0);
  return bloom.eval(p);
}
)SHADER";
inline constexpr std::string_view kCrtGlassSkSL = R"SHADER(
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) Filippo Scognamiglio and contributors

// THE GLASS: the one place a coordinate is bent, and the only place a
// screen pays for its curvature. The picture crtScreen answers with and
// the light crtLight carries are read at the bent coordinate, once
// each, however many passes stand under the glass.
half4 crtGlass(float2 p) {
  float2 size = max(uBounds.zw, float2(1));
  float2 uv = (p - uBounds.xy) / size;
  if (min(uv.x, uv.y) < 0 || max(uv.x, uv.y) >= 1) return half4(0);
  float2 cc = uv - 0.5;
  float dist = dot(cc, cc) * uCurvature;
  float2 warped = uv + cc * (1 + dist) * dist;
  // Past the corners the bend pushes off the picture there is tube and
  // no signal, which is black glass rather than nothing at all.
  if (min(warped.x, warped.y) < 0 || max(warped.x, warped.y) >= 1)
    return half4(0, 0, 0, 1);
  float2 q = uBounds.xy + warped * size;
  // ONE READING OF WHAT IS UNDER THE GLASS, at the bent coordinate: a
  // second reading is a second whole pass of whatever stands there.
  half3 texel = crtScreen(q).rgb;
  if (uBloom > 0) texel += clamp(crtLight(q).rgb * uBloom, 0, 0.5);
  texel *= 1 - clamp(uVignette * dot(cc, cc) * 2, 0, 1);
  return half4(clamp(texel, 0, 1), 1);
}
)SHADER";

/** In the composed screen the glass reads the beam's own picture. */
inline constexpr std::string_view kScreenIsTheBeam =
    "half4 crtScreen(float2 p) { return crtBeam(p); }";

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

inline const std::shared_ptr<const sigil::material::Recipe>& crtRecipe() {
  using sigil::material::LayerFilter;
  using sigil::material::Recipe;
  using sigil::material::Target;
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<CrtParameters>("evangelion.crt")
          .slot("content")
          .slot("bloom", LayerFilter::Blurred, "uBloomRadius")
          .body(Target::SkSL, crtBodyOf({kCrtBeamSkSL, kScreenIsTheBeam,
                                         kCrtBloomSkSL, kCrtGlassSkSL},
                                        "crtGlass(p)")));
  return recipe;
}

/** The tube over @p bounds (the content's coordinates) at one restrained
 *  set of numbers. It leaves two slots open: `content`, the picture or
 *  layer, and `bloom`, which a layer effect fills from that same layer
 *  blurred and a fill must be given itself whatever the bloom's strength.
 *  Explicit seconds animate the grain reproducibly; zero holds a still. */
inline sigil::material::Material crtTube(const SkRect& bounds,
                                         float seconds = 0) {
  return sigil::material::Material(
      crtRecipe(), CrtParameters{
                       .uBounds = {bounds.left(), bounds.top(), bounds.width(),
                                   bounds.height()},
                       .uCurvature = 0.055f,
                       .uRgbShift = 0.8f,
                       .uScanPitch = 3.0f,
                       .uRaster = 0.45f,
                       .uBloomRadius = 2.4f,
                       .uBloom = 0.38f,
                       .uNoise = 0.012f,
                       .uVignette = 0.22f,
                       .uTime = seconds,
                   });
}

}  // namespace evangelion
