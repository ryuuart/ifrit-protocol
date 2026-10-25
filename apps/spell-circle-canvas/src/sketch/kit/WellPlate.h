#pragma once

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilsketch/kit/Cells.h>

#include <optional>

namespace sigil::sketch::kit::detail {

/** THE PLATE A WELL WRITES, with the padding and the corners it takes
 *  where @p specification leaves them unset: a specimen well pads by the
 *  sheet's well padding and stays square, a panel pads and rounds by the
 *  sheet's panel distances. Both are lengths, so either may be a property
 *  the sheet in force resolves. */
compose::Element plate(const Well& specification, compose::Element surface,
                       compose::Dimension padding,
                       std::optional<compose::Dimension> corners);

}  // namespace sigil::sketch::kit::detail
