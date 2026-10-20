/** @file
 * A text leaf under the cascade: the text properties the matched rules
 * state, and the paragraph shaped once, reshaped, laid out again or only
 * repainted as far as the font, the paragraph setting and the ink it
 * inherits moved — the last by the ink-only repaint of an inheriting leaf.
 */

#include <sigilweave/style/Type.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "CascadePass.h"
#include "SelectorMatch.h"
#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** What the rules of @p matched state about a text leaf's text
 *  properties, folded weakest first, or null where none states one. */
std::unique_ptr<const RuleTextLayer> ruleTextLayerOf(
    const std::vector<MatchedRule>& matched) {
  std::unique_ptr<RuleTextLayer> layer;
  for (const MatchedRule& one : matched) {
    const ElementNode& stated = *one.rule->node();
    if (!stated.textData) continue;
    const TextData& said = *stated.textData;
    if (said.options.set == 0) continue;
    if (!layer) layer = std::make_unique<RuleTextLayer>();
    layer->options.overlay(said.options);
    if (said.options.set & TextOptions::kTextStroke) {
      layer->statesStroke = true;
      layer->hasTextStroke = said.hasTextStroke;
      layer->textStrokeWidth = said.textStrokeWidth;
      layer->textStrokeFill = said.textStrokeFill;
    }
  }
  return layer;
}

bool sameRuleText(const RuleTextLayer* a, const RuleTextLayer* b) {
  if (a == nullptr || b == nullptr) return a == b;
  return *a == *b;
}

}  // namespace

// A text leaf: text reconcile left owed is shaped here, once, in the font
// and the paragraph setting it lands in. After that, a change of face, size
// or any other shaping field of an inheriting leaf, or of the writing mode
// or locale in force, is a new paragraph and a new layout; a change of any
// other block field is the same paragraph laid out again; a change of colour
// alone is set on the paragraph it already has.
void resolveTextLeafCascade(Composer::Impl& impl, Instance& inst,
                            const sigil::weave::Type& font,
                            const sigil::weave::ParagraphBlock& block,
                            bool statesOwnInk, const RulesInForce& rules,
                            bool first, bool indentPercentMoved) {
  const ElementNode& node = *inst.description;
  const SheetChain* const sheets = rules.sheets;
  // A LEAF SET IN A WHOLE STYLE takes no colour from above, but one rule
  // holds for either kind of ink stated on the leaf itself — its own verb
  // or a rule matching it: it paints the glyphs, a colour over the style's
  // own as a paint does.
  const bool wholeStyleLeaf =
      node.kind == Kind::Text && node.textData && !node.textData->inherits &&
      node.textData->rich.empty() && !node.textData->paragraphOverride;
  const bool inherits = node.textData->inherits;
  // A whole-style leaf's own colour ink is set in its paragraph, so one
  // that moved is a new paragraph of the same words.
  const std::optional<material::Color> wholeStyleInk =
      wholeStyleLeaf && statesOwnInk ? font.color : std::nullopt;
  const bool wholeStyleInkMoved = !(wholeStyleInk == inst.wholeStyleInk);
  inst.wholeStyleInk = wholeStyleInk;
  const bool reshapes = (inherits && !sameFontButColour(font, inst.textFont)) ||
                        wholeStyleInkMoved;
  // THE TEXT PROPERTIES IN FORCE: what the matched rules state, with the
  // leaf's own statements standing over it. A rule that moved one is a
  // new layout of the same words; one that moved the outline alone is a
  // repaint.
  bool ruleTextMoved = false;
  if (!rules.sameRules) {
    std::unique_ptr<const RuleTextLayer> ruleText =
        ruleTextLayerOf(rules.matched);
    ruleTextMoved = !sameRuleText(inst.ruleText.get(), ruleText.get());
    if (ruleTextMoved) inst.ruleText = std::move(ruleText);
  }
  TextOptions options = inst.ruleText ? inst.ruleText->options : TextOptions{};
  options.overlay(node.textData->options);
  const bool optionsMoved = !(options == inst.textOptions);
  if (optionsMoved) inst.textOptions = std::move(options);
  if (ruleTextMoved && !first) {
    inst.markPaintDirtyUp();
    impl.contentDirty = true;
  }
  // A named run resolves through the sheets in force, so a sheet that
  // changed what a name means under the leaf is a new paragraph as a new
  // face is.
  sigil::weave::TypeSheet runStyles;
  std::vector<NamedParagraphStyle> paragraphBlockStyles;
  impl.namedStylesOf(inst, *sheets, runStyles, paragraphBlockStyles);
  const bool namesMoved = !(runStyles == inst.runStyles) ||
                          !(paragraphBlockStyles == inst.paragraphBlockStyles);
  inst.runStyles = std::move(runStyles);
  inst.paragraphBlockStyles = std::move(paragraphBlockStyles);
  inst.sheetsInForce = !sheets->empty();
  // A run, a span or a reading may state its size in rems, which the
  // paragraph holds as pixels, so a root that moved remakes it.
  const bool remakes =
      block.writingMode != inst.textBlock.writingMode ||
      block.baseDirection != inst.textBlock.baseDirection ||
      block.lineBreakLocale != inst.textBlock.lineBreakLocale || namesMoved ||
      optionsMoved || impl.remMoved;
  if (inst.textDirty || !inst.paragraph) {
    inst.textDirty = false;
    impl.materializeText(inst);
    if (inst.yoga) YGNodeMarkDirty(inst.yoga);
    impl.needsLayout = true;
  } else if (reshapes || remakes) {
    inst.contentRev++;
    impl.materializeText(inst);
    if (inst.yoga) YGNodeMarkDirty(inst.yoga);
    impl.needsLayout = true;
  } else {
    if (!(block == inst.textBlock) || indentPercentMoved) {
      inst.textBlock = block;
      inst.contentRev++;
      if (inst.yoga) YGNodeMarkDirty(inst.yoga);
      impl.needsLayout = true;
    }
    if (inherits && !(font.color == inst.textFont.color))
      impl.refreshInheritedInk(inst);
  }
}

void Composer::Impl::refreshInheritedInk(Instance& inst) {
  if (!inst.paragraph || inst.inheritedInkRanges.empty()) {
    inst.textFont.color = inst.font.color;
    return;
  }
  const TextData& text = *inst.description->textData;
  const sigil::weave::TextStyle base = sigil::weave::toTextStyle(inst.font);
  for (const Instance::InkRange& range : inst.inheritedInkRanges) {
    const sigil::weave::TextStyle style =
        range.over ? sigil::weave::overlay(base, *range.over, remPx) : base;
    inst.paragraph->setPaint(range.range.start, range.range.end, style.paint);
  }
  // The restyles back over it, in the order and by the rule materialisation
  // applied them: a paint-only restyle paints its range; a whole style
  // paints its range and then every earlier paint-only restyle stands
  // again where the two overlap; a restyle folded into an axis track
  // painted nothing.
  if (inst.textState) {
    TextState& state = *inst.textState;
    const size_t count =
        std::min(text.spanRestyles.size(), state.restyles.size());
    for (size_t i = 0; i < count; ++i) {
      TextState::Restyle& retained = state.restyles[i];
      auto next = styleOfSpan(base, text.spanRestyles[i], inst);
      const auto& previous = retained.style.paint.foregroundMaterial;
      const auto& source = next.paint.foregroundMaterial;
      if (previous && source && *previous == *source) {
        next.paint.foregroundMaterial = previous;
      } else {
        retained.ink.reset();
        retained.surfaceMapInputs.reset();
      }
      retained.style = std::move(next);
      retained.inkDeclared = text.spanRestyles[i].inkMaterial.has_value();
      retained.inkBox = text.spanRestyles[i].inkBox;
    }
    for (size_t i = 0; i < count; ++i) {
      const TextState::Restyle& retained = state.restyles[i];
      if (retained.folded) continue;
      inst.paragraph->setPaint(retained.ranges, retained.style.paint);
      if (retained.paintOnly) continue;
      for (size_t j = 0; j < i; ++j) {
        const TextState::Restyle& earlier = state.restyles[j];
        if (!earlier.paintOnly || earlier.folded) continue;
        for (const sigil::weave::CharRange& a : retained.ranges)
          for (const sigil::weave::CharRange& b : earlier.ranges)
            if (a.start < b.end && b.start < a.end)
              inst.paragraph->setPaint(std::max(a.start, b.start),
                                       std::min(a.end, b.end),
                                       earlier.style.paint);
      }
    }
  }
  inst.textFont.color = inst.font.color;
  inst.markPaintDirtyUp();
  contentDirty = true;
}

}  // namespace sigil::compose
