#pragma once

/** @file
 * The two pieces a specimen on a sheet is made of: the well it is shown
 * in, and the caption that names it — both in the theme's voice.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
#include <string>

namespace sigil::sketch::kit {

/** THE FIXED SURFACE A SPECIMEN IS SHOWN IN, sized by the caller and
 *  grounded by the theme. */
struct Well {
  compose::Dim width;
  compose::Dim height;
  /** Unset is the theme's cell ground. Set it to `Fill::none()` for a
   *  well that paints nothing. */
  std::optional<compose::Fill> ground;
  /** Unset is the theme's well padding. */
  std::optional<float> padding;
  bool clip = true;
};

/** @p surface, sized, grounded, padded and clipped as @p spec and the
 *  theme say.
 *
 *      sketch::kit::well({.width = kCell, .height = kPicture})
 *          .child(subject())
 *
 *  The second argument is the surface itself rather than a child wrapped
 *  in a new box: hand it `custom(key, draw)` where the drawing wants the
 *  well's resolved size, and `box().child(body)` where the well holds a
 *  laid-out body. Omitted, it is an empty box ready for children. */
[[nodiscard]] compose::Element well(const Well& spec,
                                    compose::Element surface);
[[nodiscard]] compose::Element well(const Well& spec);

/** ONE CAPTIONED SPECIMEN: @p body with @p label over it and @p note
 *  under it, set in the theme's caption registers and spaced by its
 *  caption gaps.
 *
 *      sketch::kit::caption(
 *          kCell, toU8("Border::Mode::Bracket"),
 *          toU8("only within 18 px of each corner"),
 *          well({.width = kCell, .height = kPicture}).child(plaque()))
 *
 *  @p measure is the width the remark wraps at — the cell's own width.
 *  It is the one distance a caption cannot inherit, because it is a fact
 *  about the specimen rather than about the look; 0 lets the remark take
 *  whatever width the cell resolves to. An empty label or an empty note
 *  is simply absent, and spends no gap. */
[[nodiscard]] compose::Element caption(float measure, std::u8string label,
                                       std::u8string note,
                                       compose::Element body);

}  // namespace sigil::sketch::kit
