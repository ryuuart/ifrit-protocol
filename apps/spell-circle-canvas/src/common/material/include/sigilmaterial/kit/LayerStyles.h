#pragma once

/** @file
 * Layer-style presets: the colour tables and option sets behind the
 * gel and chrome looks an image editor builds from ramps and blurs —
 * the aqua body, lens and glow ramps and the two chrome palettes, each
 * as stops a renderer turns into its own gradient. The decorations that
 * paint them belong to the renderer; what is preset here is the look.
 */

#include <sigilmaterial/color/Color.h>

#include <cstdint>
#include <vector>

namespace sigil::material::kit {

/** One stop of a ramp: a position in [0, 1] and its colour. */
struct RampStop {
  float pos = 0.0f;
  Color color;
  bool operator==(const RampStop&) const = default;
};

/** @p c scaled by @p k in every channel, at alpha @p a. */
Color scaled(Color c, float k, float a);
/** @p c moved a fraction @p t toward @p target, at alpha @p a. */
Color toward(Color c, Color target, float t, float a);

/** Knobs the gel bundle exposes; the defaults dress a pill. */
struct AquaGelOptions {
  float lensAlphaTop = 0.72f;    ///< lens ramp: white at the top, clear below
  float lensBottomFrac = 0.52f;  ///< lens ends this far down the box
  float lensInsetXFrac = 0.05f;  ///< lens inset each side; ~0.16 on spheres
  float bottomGlow = 0.85f;      ///< strength of the light from below
  bool halo = true;              ///< luminous tint drop beneath the shape
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

}  // namespace sigil::material::kit
