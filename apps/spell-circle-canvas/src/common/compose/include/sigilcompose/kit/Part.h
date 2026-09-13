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
 *  Every component's parts default to a leaf in the register's class,
 *  so the sheet in force sets the line; a sketch hands in its own to set
 *  that one line otherwise — the register's leaf with a font over it, or
 *  a leaf of its own — and nothing else under the component changes,
 *  because no sheet moved. An empty part is the default, so a component
 *  falls back to its own leaf where a caller left one unset. */
template <class... Offered>
using Part = core::Callable<Element(const Offered&...)>;

}  // namespace sigil::compose::kit
