#pragma once

/** @file
 * A live picture beside its readings, with a shared page and measured preview.
 */

#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilsketch/kit/Page.h>

namespace sigil::sketch::kit {

struct Instrument {
  Page page;
  /** The picture's authored coordinates. Its layout keeps this extent. */
  SkSize pictureSize{1280, 720};
  /** The displayed width; the height keeps the picture's aspect ratio. */
  float pictureWidth = 832;
  compose::Utf8 pictureLabel = "OUTPUT";
  compose::Utf8 readingsLabel = "CONNECTION";
  compose::Utf8 note;
};

/** A picture scaled from its authored coordinates, beside an ordinary flow
 *  of readings. The page and both column headings inherit the active theme. */
[[nodiscard]] compose::Element instrument(const Instrument& specification,
                                          compose::Element picture,
                                          compose::Element readings);

}  // namespace sigil::sketch::kit
