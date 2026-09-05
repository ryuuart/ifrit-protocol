#pragma once

/** @file
 * WHAT STANDS BEHIND AND AROUND: the ground a whole canvas is dressed
 * with, and the shell a screen is set into.
 */

#include <include/core/SkColor.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
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
  std::optional<compose::Fill> ground;
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
 *      ctx.composer.render(box().absolute().inset(0)
 *          .child(sketch::kit::backdrop({.over = ctx.size, .vignette = 0.45f}))
 *          .child(subject()));
 *
 *  It places itself over the canvas and paints nothing else, so it is the
 *  first child of whatever a sketch renders. */
[[nodiscard]] compose::Element backdrop(const Backdrop& ground);

/** A DEVICE'S CHROME: an outer shell, a screen inset into it, and the
 *  plate the maker's word is engraved on.
 *
 *  It is the shape every reconstruction of a console, a handset, a rack
 *  unit or a cabinet builds by hand out of three nested boxes, with the
 *  screen's inset arrived at by subtracting the bezel from the shell in
 *  four places. */
struct Frame {
  compose::Dim width;
  compose::Dim height;
  /** The body; unset is the theme's cell ground. */
  std::optional<compose::Fill> shell;
  float corners = 6;
  /** How much shell stands around the screen on every side. */
  float bezel = 10;
  /** The screen's own ground; unset is the theme's page ground, which is
   *  the darker of the theme's two. */
  std::optional<compose::Fill> screen;
  float screenCorners = 2;
  /** A keyline around the screen's opening; unset is the theme's rule. */
  std::optional<compose::Fill> keyline;
  /** Engraved under the screen in the theme's eyebrow register; empty
   *  leaves the shell blank and spends no room on a plate. */
  std::u8string plate;
};

/** THE CHROME, with @p screen inside its opening.
 *
 *      sketch::kit::frame({.width = Dim(275), .height = Dim(116),
 *                          .bezel = 6, .plate = toU8("MAIN WINDOW")},
 *                         readout({.rows = tape}))
 *
 *  The opening is a flex column, so what goes into it lays out normally;
 *  hand it `positioned()` where the contents carry their own rects. */
[[nodiscard]] compose::Element frame(const Frame& chrome,
                                     compose::Element screen);
[[nodiscard]] compose::Element frame(const Frame& chrome);

}  // namespace sigil::sketch::kit
