#pragma once

/** @file
 * @ingroup sketch-kit
 *
 * WHAT STANDS BEHIND AND AROUND: the ground a whole canvas is dressed
 * with, the titled region a page divides itself into, and the shell a
 * screen is set into.
 */

#include <include/core/SkColor.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
#include <string>

namespace sigil::sketch::kit {

/** THE GROUND OF A WHOLE SURFACE, dressed.
 *
 *  A sketch that is one picture rather than a page still wants the page's
 *  ground under it, and usually wants it shaded toward the corners and
 *  grained so a flat fill does not read as a flat fill. */
struct Backdrop {
  /** The surface the vignette is measured against — the canvas, which is
   *  `ctx.size`. A vignette is a fact about an EXTENT, so it is the one
   *  thing here a theme cannot carry. */
  SkSize over{0, 0};
  /** Unset is the theme's ground. */
  std::optional<compose::SurfacePaint> ground;
  /** How dark the corners go, 0 to 1. 0 shades nothing. */
  float vignette = 0;
  /** The colour the corners are shaded TOWARD; unset is black, because a
   *  vignette is a shadow. Its own alpha is ignored — `vignette` is how
   *  far it reaches. A warm sheet usually wants its own darkest ink here
   *  rather than a neutral black. */
  std::optional<SkColor4f> edge;
  /** How far the grain reaches, 0 to 1. 0 grains nothing. */
  float grain = 0;
  /** Features per px of the grain: about 0.8 is film, about 0.05 is
   *  paper. */
  float grainScale = 0.8f;
  /** The key the ground is named under; empty keys nothing. */
  std::string key;
};

/** THE DRESSED GROUND, over the whole surface.
 *
 *      ctx.composer.render(box().absolute().inset(0).children(
 *          {sketch::kit::backdrop({.over = ctx.size, .vignette = 0.45f}),
 *           subject()}));
 *
 *  It places itself over the canvas and paints nothing else, so it is the
 *  first child of whatever a sketch renders. */
[[nodiscard]] compose::Element backdrop(const Backdrop& ground);

/** A TITLED REGION OF A PAGE, in the theme's voice: an eyebrow over a
 *  title, a note ranged at the far edge of the head, and the content
 *  standing on the plate under it.
 *
 *  What it adds over `compose::kit::panel` is the theme's distances and
 *  colours: the head's two gaps, the ground, the padding, the radius and
 *  the keyline a PLATE takes — which are not a specimen well's, because a
 *  panel is a piece of furniture and a well is a picture's surface. */
struct Panel {
  compose::Utf8 eyebrow;
  compose::Utf8 title;
  compose::Utf8 note;
  /** A hairline under the head, in the theme's rule colour. */
  bool ruled = false;
  /** THE PLATE THE REGION STANDS ON. Unset padding is the theme's panel
   *  padding, an unset radius its panel corner and an unset keyline its
   *  rule; `Fill::none()` draws no keyline, and a recess or a relief
   *  under it reads the plate as sunk or raised. */
  Well body;
};

/** THE REGION, with @p content standing under its head.
 *
 *      sketch::kit::panel({.eyebrow = "LOADOUT", .note = "3 / 8",
 *                          .ruled = true,
 *                          .body = {.width = compose::Dimension(260)}},
 *                         slots())
 *
 *  The three lines are set in the classes `eyebrow`, `title` and
 *  `captionNote`, so a panel under a page — or under any root that states
 *  the theme's sheet — is in the theme's voice. */
[[nodiscard]] compose::Element panel(const Panel& region,
                                     compose::Element content);

/** A DEVICE'S CHROME: an outer shell, a screen inset into it, and the
 *  plate the maker's word is engraved on.
 *
 *  It is the shape every reconstruction of a console, a handset, a rack
 *  unit or a cabinet builds by hand out of three nested boxes, with the
 *  screen's inset arrived at by subtracting the bezel from the shell in
 *  four places. */
struct Frame {
  compose::Dimension width;
  compose::Dimension height;
  /** The body; unset is the theme's cell ground. */
  std::optional<compose::SurfacePaint> shell;
  /** Unset is the theme's panel radius. */
  std::optional<float> corners;
  /** How much shell stands around the screen on every side; unset is the
   *  theme's bezel. */
  std::optional<float> bezel;
  /** The screen's own ground; unset is the theme's page ground, which is
   *  the darker of the theme's two. */
  std::optional<compose::SurfacePaint> screen;
  /** Unset is the theme's screen radius. */
  std::optional<float> screenCorners;
  /** A keyline around the screen's opening; unset is the theme's rule.
   *  `Fill::none()` draws none — the same spelling `Well::ground` takes,
   *  for the shell whose only rule runs round its OUTER edge. */
  std::optional<compose::Fill> keyline;
  /** Engraved under the screen in the theme's eyebrow register; empty
   *  leaves the shell blank and spends no room on a plate. */
  compose::Utf8 plate;
};

/** THE CHROME, with @p screen inside its opening.
 *
 *      sketch::kit::frame({.width = Dimension(275), .height = Dimension(116),
 *                          .bezel = 6, .plate = "MAIN WINDOW"},
 *                         readout({.rows = tape}))
 *
 *  The opening is a flex column, so what goes into it lays out normally;
 *  hand it `positioned()` where the contents carry their own rects. */
[[nodiscard]] compose::Element frame(const Frame& chrome,
                                     compose::Element screen);
[[nodiscard]] compose::Element frame(const Frame& chrome);

}  // namespace sigil::sketch::kit
