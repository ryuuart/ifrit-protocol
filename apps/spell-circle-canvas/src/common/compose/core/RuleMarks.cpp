/** @file
 * The marks the matched rules state, laid onto the description a node is
 * painted from — CSS's border, outline and box-shadow stated by a class —
 * and the effects of the material the fill or the ink standing there was
 * stated with.
 */

#include "RuleMarks.h"

#include "MaterialEffects.h"

#include <include/core/SkTypes.h>  // SkDebugf
#include <sigilcompose/core/StyleSheet.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace sigil::compose::detail {

namespace {

/** The once-per-process diagnostic behind a rule's mark that borrows a
 *  keyed node's outline: the derive pass resolves a borrow for the node
 *  that DECLARES it, which a rule is not. */
void warnRuleMarkBorrows() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "[compose] a rule states a mark that borrows another node's outline "
      "(strand::from), which only the element carrying the mark can "
      "declare — the mark is left out of the elements the rule matches. "
      "State it on the element. (warned once)\n");
}

/** Whether @p stated says anything a painted node would wear. */
bool statesMarks(const ElementNode& stated) {
  return !stated.backgrounds.empty() || !stated.foregrounds.empty() ||
         (stated.fxData && !stated.fxData->overlays.empty());
}

/** THE MARKS OF ONE SLOT, gathered across the declarations that state
 *  them, with the local labels each declaration bound moved to where its
 *  marks now stand. */
class SlotGathering {
 public:
  explicit SlotGathering(MarkSlot slot) : m_slot(slot) {}

  /** Appends @p marks, as @p from states them in this slot, after what is
   *  already gathered. A mark that borrows is dropped where @p borrowsAllowed
   *  is false — its label goes with it. */
  void append(const std::vector<Decoration>& marks, const ElementNode& from,
              bool borrowsAllowed, std::vector<MarkLabel>& labels) {
    std::vector<int64_t> moved(marks.size(), -1);
    for (size_t index = 0; index < marks.size(); ++index) {
      if (!borrowsAllowed && !marks[index].borrows().empty()) {
        warnRuleMarkBorrows();
        continue;
      }
      moved[index] = (int64_t)m_marks.size();
      m_marks.push_back(marks[index]);
    }
    if (!from.fxData) return;
    for (const MarkLabel& label : from.fxData->markNames) {
      if (label.slot != m_slot || label.index >= moved.size() ||
          moved[label.index] < 0)
        continue;
      labels.push_back({m_slot, (uint32_t)moved[label.index], label.name});
    }
  }

  /** Appends marks no declaration labelled. */
  void append(const std::vector<Decoration>& marks) {
    m_marks.insert(m_marks.end(), marks.begin(), marks.end());
  }

  [[nodiscard]] std::vector<Decoration> take() { return std::move(m_marks); }

 private:
  MarkSlot m_slot;
  std::vector<Decoration> m_marks;
};

const std::vector<Decoration>& overlaysOf(const ElementNode& node) {
  static const std::vector<Decoration> kNone;
  return node.fxData ? node.fxData->overlays : kNone;
}

/** THE EFFECTS OF THE FILL AND THE INK STANDING AT A NODE, where a rule
 *  stated them: each belongs to its lane, so the strongest statement of
 *  the lane — a value, or a keyword, which states no material — decides
 *  whether its effects dress the node, and the node's own statement of
 *  the lane leaves none of a rule's. */
struct LaneEffects {
  std::optional<material::Filter> fill;
  std::optional<material::Filter> ink;
};

bool writesKeyword(const ElementNode& stated, Property property) {
  return stated.fields.keywords() &&
         stated.fields.keywords()->find(property).has_value();
}

LaneEffects laneEffectsOf(const ElementNode& node,
                          std::span<const Rule* const> matched) {
  LaneEffects effects;
  // Weakest first, so the strongest rule stating a lane is the one left.
  for (const Rule* rule : matched) {
    const ElementNode& stated = *rule->node();
    if (stated.fields.declared().has(Property::Fill))
      effects.fill = stated.fxData && !writesKeyword(stated, Property::Fill)
                         ? stated.fxData->fillEffects
                         : std::nullopt;
    if (stated.fields.declared().has(Property::Ink))
      effects.ink =
          stated.cascadeData && !writesKeyword(stated, Property::Ink)
              ? stated.cascadeData->inkEffects
              : std::nullopt;
  }
  if (node.fields.declared().has(Property::Fill)) effects.fill.reset();
  if (node.fields.declared().has(Property::Ink)) effects.ink.reset();
  return effects;
}

}  // namespace

std::shared_ptr<const ElementNode> dressedWithRules(
    const ElementNode& node, std::span<const Rule* const> matched) {
  const LaneEffects lanes = laneEffectsOf(node, matched);
  bool any = lanes.fill.has_value() || lanes.ink.has_value();
  for (const Rule* rule : matched) any |= statesMarks(*rule->node());
  if (!any) return nullptr;

  SlotGathering backgrounds(MarkSlot::Background);
  SlotGathering overlays(MarkSlot::Overlay);
  SlotGathering foregrounds(MarkSlot::Foreground);
  std::vector<MarkLabel> labels;
  const auto gather = [&](const ElementNode& from, bool borrowsAllowed) {
    backgrounds.append(from.backgrounds, from, borrowsAllowed, labels);
    overlays.append(overlaysOf(from), from, borrowsAllowed, labels);
    foregrounds.append(from.foregrounds, from, borrowsAllowed, labels);
  };
  // Weakest first, and the node's own last: a class's keyline stands under
  // the node's own stroke exactly as an earlier call on the node would.
  for (const Rule* rule : matched) gather(*rule->node(), false);

  // The effects of the rule's fill and ink, after the rules' marks and
  // before the node's own — where an element's own `fill(material)` would
  // have spliced them had it been written first. The fill's precede the
  // ink's, and their pixel passes run in that order too.
  std::vector<Echo> echoes;
  std::optional<material::Filter> pixels;
  for (const std::optional<material::Filter>* lane : {&lanes.fill, &lanes.ink}) {
    if (!*lane) continue;
    EffectMarks marks = effectMarksOf(**lane);
    backgrounds.append(marks.beneath);
    foregrounds.append(marks.over);
    echoes.insert(echoes.end(), marks.echoes.begin(), marks.echoes.end());
    if (marks.pixels)
      pixels = pixels ? pixels->then(*marks.pixels) : std::move(marks.pixels);
  }
  gather(node, true);

  auto dressed = std::make_shared<ElementNode>(node);
  dressed->backgrounds = backgrounds.take();
  dressed->foregrounds = foregrounds.take();
  std::vector<Decoration> gatheredOverlays = overlays.take();
  if (!gatheredOverlays.empty() || !labels.empty() || !echoes.empty() ||
      pixels || dressed->fxData) {
    FxData& fx = dressed->fxData.ensure();
    fx.overlays = std::move(gatheredOverlays);
    fx.markNames = std::move(labels);
    // The rules' echoes stand under the node's own, bottom first.
    echoes.insert(echoes.end(), fx.echoes.begin(), fx.echoes.end());
    fx.echoes = std::move(echoes);
    // The rules' pixel passes run first, and the node's own filter over
    // their result.
    if (pixels)
      fx.layerEffect =
          fx.layerEffect ? pixels->then(*fx.layerEffect) : std::move(pixels);
  }
  return dressed;
}

}  // namespace sigil::compose::detail
