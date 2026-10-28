#pragma once

/** @file
 * Internal to the cascade's translation units — what the pass, the lengths
 * measured against the font, the rules in force and a text leaf's share of
 * the pass read in common.
 */

#include <sigilweave/style/Type.h>

#include <vector>

#include "SelectorMatch.h"
#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

/** The size in PIXELS a resolved font carries; the initial size where a
 *  font has none yet. A resolved size may still be absolute in another
 *  unit — an overlay leaves a size in points in points, since points are
 *  a fixed count of pixels and nothing is owed to resolve them — so the
 *  conversion is asked for rather than assumed. An `em` against a size
 *  stated in points is otherwise a multiple of the point COUNT. */
inline float fontSizePx(const sigil::weave::Type& font) {
  return font.size ? font.size->absolutePx() : sigil::weave::kInitialTypeSizePx;
}

/** Whether two fonts agree in every field but the colour. */
inline bool sameFontButColour(sigil::weave::Type a,
                              const sigil::weave::Type& b) {
  a.ink = b.ink;
  return a == b;
}

/** THE RULES IN FORCE at a node, as the cascade resolved them this pass:
 *  the selector sheets that speak about it, the rules of those sheets
 *  that match it weakest first, and whether the layers the instance
 *  builds from them moved. */
struct RulesInForce {
  /** The sheets in force here: the parent's chain, or the chain extended
   *  by the sheets this node applies. */
  const detail::SheetChain* sheets = nullptr;
  /** The rules that speak about this node, weakest first. */
  std::vector<detail::MatchedRule> matched;
  /** Whether these are the rules matched last pass, which keeps every
   *  layer built from them. */
  bool sameRules = false;
  /** Whether the layer of carried properties the rules state moved… */
  bool layerMoved = false;
  /** …and whether the paint behind its fill did, which the computed style
   *  does not hold. */
  bool fillMaterialMoved = false;
};

/** RESOLVES THE RULES IN FORCE at @p inst and leaves on it what they
 *  state — the transition, the layer of carried properties, the marks
 *  laid under its own — where they moved. @p extended holds the chain
 *  when this node applies sheets of its own, so it must outlive the
 *  answer; @p first says the node is resolving for the first time. */
RulesInForce resolveRulesInForce(Composer::Impl& impl, detail::Instance& inst,
                                 const detail::SheetChain& parentSheets,
                                 detail::SheetChain& extended, bool first);

/** A TEXT LEAF'S SHARE OF THE PASS, once its @p font, @p block and ink are
 *  final: the text properties the rules state, the named styles the sheets
 *  in force resolve, and the paragraph reshaped, laid out again or
 *  repainted as far as what it inherits moved. */
void resolveTextLeafCascade(Composer::Impl& impl, detail::Instance& inst,
                            const sigil::weave::Type& font,
                            const sigil::weave::ParagraphBlock& block,
                            bool statesOwnInk, const RulesInForce& rules,
                            bool first, bool indentPercentMoved);

}  // namespace sigil::compose
