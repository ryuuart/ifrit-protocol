#pragma once

/** @file
 * Layer-style presets: the colour tables and option sets behind the
 * gel and chrome looks an image editor builds from ramps and blurs —
 * the aqua body, lens and glow ramps and the two chrome palettes, each
 * as stops a renderer turns into its own gradient — and the contour
 * table a satin band remaps blurred coverage through. The decorations
 * that paint them belong to the renderer; what is preset here is the
 * look.
 */

#include <sigilmaterial/color/Color.h>

#include <array>
#include <cstdint>
#include <vector>

namespace sigil::material::kit {

/** Knobs the gel bundle exposes; the defaults dress a pill. */
struct AquaGelOptions {
  float lensAlphaTop = 0.72f;    ///< lens ramp: white at the top, clear below
  float lensBottomFrac = 0.52f;  ///< lens ends this far down the box
  float lensInsetXFrac = 0.05f;  ///< lens inset each side; ~0.16 on spheres
  /** Where down the lens its ramp has reached its bottom value, as a
   *  fraction of the lens's own height. Below 1 the lens's lower arc is
   *  painted at that value and its outline never shows, so the highlight
   *  ends in a fade; at 1 the ramp runs to the arc itself and the lens
   *  reads as a cut-out shape laid on the surface. */
  float lensFadeEnd = 0.82f;
  float bottomGlow = 0.85f;  ///< strength of the light from below
  /** How hard the recessed band under the top edge cuts, as a weight on
   *  `aquaTopBand`'s own alpha. At 1 the recess is a dark cap that ends in
   *  a visible line across the shape, and the lens above it reads as a
   *  second object; the default keeps the recess as shading on one
   *  surface. */
  float topBand = 0.55f;
  bool halo = true;  ///< luminous tint drop beneath the shape
  /** The tallest the gel will be: the halo's reach beyond the box is a
   *  fraction of it, and a renderer's cull reserve reads this before the
   *  box has a size. Under-declaring it truncates the halo. */
  float expectedHeight = 64.0f;
  bool operator==(const AquaGelOptions&) const = default;
};

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

/** The bundle's knobs. */
struct ChromeOptions {
  using Palette = ChromePalette;
  Palette palette = Palette::Steel;
  bool horizonSliver = true;  ///< white specular sliver straddling 50%
  float keylineWidth = 2.0f;
  Color keyline = rgb(0x10141A);
  float bevelDepth = 3.0f, bevelSize = 5.0f;
  bool operator==(const ChromeOptions&) const = default;
};

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
