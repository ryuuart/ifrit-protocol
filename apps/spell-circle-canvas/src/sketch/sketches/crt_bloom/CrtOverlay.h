#pragma once

// THE TUBE, as something laid OVER a picture: hard scanlines at a stated
// pitch — the first half of every pitch darkened flat — and a corner
// falloff squeezed along the short axis; the beam, the beat and the grain
// are off until a field names them. It reads the node's resolution.

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <memory>
#include <string>
#include <string_view>

namespace crt_bloom {

/** The tube overlay's ABI.
 *
 *  THE DARKENING IS A SUM: a hard line, a beam, a beat under it and a
 *  grain, each of which is absent at no strength. A monitor seen from
 *  across a room is the hard line alone; a plate shot close enough that
 *  the beam's own profile spans several pixels wants the beam, and close
 *  enough to read the composite signal's own period wants the beat as
 *  well. */
struct CrtOverlayParameters {
  float uScanPitch = 4.0f;       ///< px between scanline centres
  float uScanStrength = 0.052f;  ///< how dark the dark half of a pitch goes
  float uVigInner = 1.45f;       ///< normalised radius the falloff starts at
  float uVigOuter = 2.15f;       ///< where it reaches full strength
  float uVigStrength = 0.34f;
  float uSqueeze = 0.70f;  ///< < 1 pulls the falloff in along the short axis
  /** THE BEAM: px between beam centres, how fast its weight falls away
   *  from one, and how dark the gap between two goes. */
  float uBeamPitch = 4.0f;
  float uBeamFalloff = 1.0f;
  float uBeamStrength = 0.0f;
  /** THE BEAT the composite carries under the line — a second period of
   *  the same shape. */
  float uBeatPitch = 8.0f;
  float uBeatFalloff = 1.0f;
  float uBeatStrength = 0.0f;
  /** How far a per-pixel speckle moves the DARKENING either way. A tube's
   *  noise is in how much light a cell gives up, not in its colour. */
  float uGrain = 0.0f;
};

inline constexpr std::string_view kCrtOverlaySkSL = R"SHADER(
half4 main(float2 xy) {
  // THE HARD LINE: the first half of every pitch, darkened flat — what a
  // tube reads as once the beam's own shape is under a pixel.
  float line = mod(xy.y, uScanPitch) < uScanPitch * 0.5
                   ? uScanStrength
                   : 0.0;
  // THE BEAM, and THE BEAT UNDER IT: a weight falling off from each
  // centre, which is the profile a gun draws, and a second period for the
  // beat a composite signal carries beneath the line. Either at no
  // strength is a tube without it.
  float f = fract(xy.y / max(uBeamPitch, 1e-6)) - 0.5;
  line += (1.0 - exp2(-uBeamFalloff * f * f * 4.0)) * uBeamStrength;
  float g = fract(xy.y / max(uBeatPitch, 1e-6)) - 0.5;
  line += (1.0 - exp2(-uBeatFalloff * g * g * 4.0)) * uBeatStrength;
  // The grain is in the DARKENING, not in the colour: what varies cell to
  // cell is how much light the cell gives up.
  float gr = fract(sin(dot(floor(xy), float2(12.9898, 78.233)))
             * 43758.5453) - 0.5;
  float dark = clamp(line + gr * uGrain, 0.0, 1.0);
  float2 p = (xy / max(uResolution, float2(1.0)) - 0.5) * 2.0;
  float r = length(p / uSqueeze);
  float vig = smoothstep(uVigInner, uVigOuter, r) * uVigStrength;
  return half4(0.0, 0.0, 0.0, half(clamp(dark + vig, 0.0, 1.0)));
}
)SHADER";

inline const std::shared_ptr<const sigil::material::Recipe>& crtOverlayRecipe() {
  using sigil::material::FrameInput;
  using sigil::material::Recipe;
  using sigil::material::Target;
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<CrtOverlayParameters>("crt_bloom.crtOverlay")
          .frame(FrameInput::Resolution)
          .body(Target::SkSL, std::string(kCrtOverlaySkSL)));
  return recipe;
}

inline sigil::material::Material crtOverlay(
    const CrtOverlayParameters& parameters = {}) {
  return sigil::material::Material(crtOverlayRecipe(), parameters);
}

}  // namespace crt_bloom
