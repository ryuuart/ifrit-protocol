/** @file
 * The decoration slots — the backgrounds, overlays, foregrounds and
 * strokes that dress the outline, a layer style's bundle of them with
 * its misprint echoes, what outline they all dress, and the local labels
 * and derive borrows a mark declares as it is appended.
 */

#include <algorithm>

#include "ComposeInternal.h"

namespace sigil::compose {

/** Register whatever a decoration says it borrows (BorrowingDecoration) so
 *  the derive pass resolves it. EVERY slot that accepts a Decoration must
 *  route through here: a borrow honoured on some slots and not others fails
 *  silently — the decoration draws with nothing borrowed and says why on no
 *  channel — and the difference between the slots is invisible to the
 *  author. */
template <class Derived>
void DecorationVerbs<Derived>::claimBorrows(const Decoration& d) {
  if (d.borrows().empty()) return;
  detail::DeriveData& derive = declarations()->deriveData.ensure();
  for (const std::string& key : d.borrows()) {
    derive.borrowedPathKeys.push_back(key);
    // A borrowed strand is the target's own outline, not its box.
    derive.reads.push_back({key, sigil::core::Facet::Outline});
  }
}

/** Bind a LOCAL label to the mark at (slot, index), so `parts::named()`
 *  can address it. `slot` is a detail::MarkSlot as an int, for the same
 *  reason addSpanPass takes its half that way. An empty name costs
 *  nothing — the vector stays absent on the overwhelming majority of
 *  nodes, which is why the labels are a side list and not a field beside
 *  every Decoration. */
template <class Derived>
void DecorationVerbs<Derived>::labelMark(int slot, size_t index,
                                         std::string name) {
  if (name.empty()) return;
  declarations()->fxData.ensure().markNames.push_back(detail::MarkLabel{
      (detail::MarkSlot)slot, (uint32_t)index, std::move(name)});
}

template <class Derived>
Derived& DecorationVerbs<Derived>::overlay(Decoration d, std::string name) {
  claimBorrows(d);
  detail::ElementNode* node = declarations();
  const size_t index = node->fxData.ensure().overlays.size();
  node->fxData->overlays.push_back(std::move(d));
  labelMark((int)detail::MarkSlot::Overlay, index, std::move(name));
  return self();
}

template <class Derived>
Derived& DecorationVerbs<Derived>::background(Decoration d, std::string name) {
  claimBorrows(d);
  detail::ElementNode* node = declarations();
  const size_t index = node->backgrounds.size();
  node->backgrounds.push_back(std::move(d));
  labelMark((int)detail::MarkSlot::Background, index, std::move(name));
  return self();
}

template <class Derived>
Derived& DecorationVerbs<Derived>::foreground(Decoration d, std::string name) {
  claimBorrows(d);
  detail::ElementNode* node = declarations();
  const size_t index = node->foregrounds.size();
  node->foregrounds.push_back(std::move(d));
  labelMark((int)detail::MarkSlot::Foreground, index, std::move(name));
  return self();
}

template <class Derived>
Derived& DecorationVerbs<Derived>::stroke(Decoration brush, std::string name) {
  return foreground(std::move(brush), std::move(name));
}

template <class Derived>
Derived& DecorationVerbs<Derived>::decorationOutline(Boundary source,
                                                     float coverage) {
  detail::ElementNode* node = declare(Property::DecorationOutline);
  node->boundary = source;
  node->coverageThreshold = std::clamp(coverage, 0.0f, 1.0f);
  return self();
}

template <class Derived>
Derived& DecorationVerbs<Derived>::layerStyle(LayerStyle s) {
  for (Decoration& d : s.under) {
    claimBorrows(d);
    declarations()->backgrounds.push_back(std::move(d));
  }
  for (Decoration& d : s.over) {
    claimBorrows(d);
    declarations()->foregrounds.push_back(std::move(d));
  }
  if (!s.echoes.empty()) {
    std::vector<Echo>& echoes = declarations()->fxData.ensure().echoes;
    echoes.insert(echoes.end(), s.echoes.begin(), s.echoes.end());
  }
  return self();
}

template class DecorationVerbs<Element>;
template class DecorationVerbs<Text>;
template class DecorationVerbs<Image>;
template class DecorationVerbs<Band>;

}  // namespace sigil::compose
