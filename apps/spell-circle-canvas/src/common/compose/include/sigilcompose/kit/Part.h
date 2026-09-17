#pragma once
/** @file
 * A part: one of a component's own lines, as a function of what the
 * component knows about it, called with only the parameters it names.
 */
#include <sigilcompose/core/Element.h>
#include <sigilcore/callable/Callable.h>

namespace sigil::compose::kit {

/** A PART OF A COMPONENT — one of the lines a component writes itself,
 *  as a function of what the component offers about it: for a caption's
 *  label, the text and then the caption. A part is handed ANY callable
 *  whose parameters are a prefix of that offer and is called with the
 *  ones it names, so `[](const Utf8& text) {…}`, `[](const Utf8& text,
 *  const Caption& voice) {…}` and `[] {…}` are all parts of a caption,
 *  as a range's children take a function of the item or of the item and
 *  its index.
 *
 *  A component's stock text parts carry document roles, or a data class
 *  for measured values, so the sheet where they land styles them. A caller
 *  can supply its own leaf to change one line without changing the sheet
 *  for the rest of the component. An empty part selects the component's
 *  own default leaf. */
template <class... Offered>
using Part = core::Callable<Element(const Offered&...)>;

}  // namespace sigil::compose::kit
