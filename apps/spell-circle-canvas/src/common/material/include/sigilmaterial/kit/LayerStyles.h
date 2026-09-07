#pragma once

/** @file
 * The colour TABLES behind the gel and chrome looks an image editor
 * builds from ramps and blurs — the aqua body, halo and glow colours and
 * the two chrome palettes, each a list of stops a renderer turns into its
 * own gradient — and the contour table a satin band remaps blurred
 * coverage through.
 *
 * Data and nothing else. What a renderer's bundle EXPOSES — which
 * highlight, how deep a bevel, how hard a recess — is the renderer's own
 * value, because those are knobs on its decorations rather than colours.
 */

#include <sigilmaterial/color/Color.h>

#include <array>
#include <cstdint>
#include <vector>

namespace sigil::material::kit {

/** The gel body ramp over the box's height: deep at the top, saturated in
 *  the middle, light below. */
std::vector<RampStop> aquaBodyRamp(Color tint);
/** The bottom glow's ramp, from clear at mid-height to the tint lightened
 *  at @p strength. */
std::vector<RampStop> aquaGlowRamp(Color tint, float strength);
/** The halo's colour: a lightened, half-transparent cast of the tint. */
Color aquaHalo(Color tint);
/** The recessed band's colour beneath the gel's top edge. */
Color aquaTopBand(Color tint);
/** The gel's hairline keyline colour. */
Color aquaHairline(Color tint);
/** The default gel tint. */
inline Color aquaTint() { return rgb(0x1E8FFF); }

/** Which chrome the bundle wears. */
enum class ChromePalette : uint8_t {
  Steel,  ///< the dark ramp — heavy contrast, for plates and wordmarks
  Silver  ///< the light ramp — window and control chrome
};

/** Where the chrome ramp's hard stop sits, as a fraction of the box's
 *  height. Position hand-added glints against this so they stay on the
 *  horizon at any size. */
inline constexpr float kChromeHorizonFrac = 0.50f;

/** The chrome ramp over the box's height, with its hard stop at the
 *  horizon. */
std::vector<RampStop> chromeRamp(ChromePalette palette);

/** The dark inner band the Steel palette wears beneath its top edge. */
inline Color chromeSteelTopBand() { return rgb(0x001020, 0.30f); }

// ---------------------------------------------------------------------------
// Contour tables — a remap of BLURRED COVERAGE, which is what makes a
// satin band follow the shape's own distance field rather than a screen
// axis: on a blob it curves with the blob.

/** A RING table: 256 entries peaking where blurred coverage crosses
 *  @p center — 0 is the rim, 1 the deep interior — over a band @p width
 *  wide, so a renderer that maps coverage through it gets one bright ring
 *  inside the silhouette. The peak is eased, not linear, or the ring's
 *  edges read as two hard lines rather than as light. */
std::array<uint8_t, 256> contourRing(float center = 0.55f, float width = 0.35f);

}  // namespace sigil::material::kit
