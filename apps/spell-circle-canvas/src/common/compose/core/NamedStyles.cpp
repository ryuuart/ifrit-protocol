/** @file
 * What the sheets in force say about the names a text leaf's rich runs
 * and paragraph styles were written with.
 */

#include <sigilmaterial/color/Color.h>

#include <algorithm>
#include <string>
#include <variant>

#include "ComposeRuntime.h"
#include "FaceChoice.h"
#include "LonghandFold.h"
#include "SelectorMatch.h"

namespace sigil::compose {

using namespace detail;

void Composer::Impl::namedStylesOf(
    const Instance& leaf, const SheetChain& chain,
    sigil::weave::TypeSheet& runs,
    std::vector<NamedParagraphStyle>& blocks) const {
  runs = {};
  blocks.clear();
  if (chain.empty()) return;
  const TextData& text = *leaf.description->textData;
  const TextOptions& options = leaf.textOptions;
  const bool inherits = text.inherits;
  const std::string* familyInForce =
      inherits && !leaf.family.empty() ? &leaf.family : nullptr;
  const bool italicInForce = inherits && leaf.italic;
  // A RUN NAMED SO IS A VIRTUAL CHILD OF THE LEAF: the rules' partials
  // folded weakest first, and a family, an italic or a weight they state
  // choosing its face over the one the leaf is set in, as they would a
  // child's.
  const auto fontFor = [&](std::string_view name) {
    std::optional<sigil::weave::Type> partial;
    LonghandFold longhands(familyInForce, italicInForce, std::nullopt);
    for (const MatchedRule& one :
         matchRulesForName(chain, leaf, name, hasNames)) {
      if (!partial) partial.emplace();
      sigil::weave::merge(*partial, one.rule->type());
      const ElementNode& stated = *one.rule->node();
      if (stated.cascadeData)
        longhands.state(
            stated.cascadeData->font ? &*stated.cascadeData->font : nullptr,
            nullptr, &*stated.cascadeData);
      if (const std::optional<VarRef>& reference = one.rule->inkVar()) {
        const VarValue* value =
            leaf.vars ? leaf.vars->find(*reference) : nullptr;
        if (const material::Color* colour =
                value ? std::get_if<material::Color>(value) : nullptr)
          partial->color = *colour;
        else
          warnNoSuchVar(*reference, true);
      }
    }
    if (partial)
      chooseRangeFace(fonts, *partial, leafStyle(leaf), familyInForce,
                      italicInForce, longhands.familyNamed(),
                      longhands.italic());
    return partial;
  };
  if (!text.rich.hasStyles())
    for (const sigil::weave::RichText::Run& run : text.rich.runs())
      if (!run.styleName.empty() && !runs.contains(run.styleName))
        if (std::optional<sigil::weave::Type> partial = fontFor(run.styleName))
          runs.set(run.styleName, std::move(*partial));
  if (!(options.set & TextOptions::kBlockClasses)) return;
  // A PARAGRAPH NAMED SO is laid out in the rules' block partials, folded
  // weakest first; an indent they give in another unit is resolved at the
  // leaf, and a percentage waits for the measure as the leaf's own does.
  for (const std::string& name : options.blockClassNames) {
    const bool seen =
        std::any_of(blocks.begin(), blocks.end(),
                    [&](const auto& entry) { return entry.name == name; });
    if (seen) continue;
    const std::vector<MatchedRule> matched =
        matchRulesForName(chain, leaf, name, hasNames);
    if (matched.empty()) continue;
    NamedParagraphStyle style{name, {}, std::nullopt};
    LonghandFold longhands(nullptr, false, std::nullopt);
    for (const MatchedRule& one : matched) {
      sigil::weave::merge(style.block, one.rule->paragraph());
      const ElementNode& stated = *one.rule->node();
      longhands.state(nullptr, &one.rule->paragraph(),
                      stated.cascadeData ? &*stated.cascadeData : nullptr);
    }
    style.indentPercent = longhands.indentPercent();
    if (longhands.indent())
      resolveTextIndent(leaf, *longhands.indent(), style.block,
                        style.indentPercent);
    blocks.push_back(std::move(style));
  }
}

}  // namespace sigil::compose
