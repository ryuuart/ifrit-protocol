/** @file
 * Element's decoration slots — the backgrounds, overlays, foregrounds and
 * strokes that dress the outline, a layer style's bundle of them, the
 * misprint echo, and the local labels and derive borrows a mark
 * declares as it is appended.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

/** Register whatever a decoration says it borrows (BorrowingDecoration) so
 *  the derive pass resolves it. EVERY slot that accepts a Decoration must
 *  route through here: a borrow honoured on some slots and not others fails
 *  silently — the decoration draws with nothing borrowed and says why on no
 *  channel — and the difference between the slots is invisible to the
 *  author. */
void Element::claimBorrows(const Decoration& d) {
  if (d.borrows().empty()) return;
  detail::DeriveData& derive = m_node->deriveData.ensure();
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
void Element::labelMark(int slot, size_t index, std::string name) {
  if (name.empty()) return;
  m_node->fxData.ensure().markNames.push_back(detail::MarkLabel{
      (detail::MarkSlot)slot, (uint32_t)index, std::move(name)});
}

Element& Element::overlay(Decoration d, std::string name) {
  claimBorrows(d);
  const size_t index = m_node->fxData.ensure().overlays.size();
  m_node->fxData->overlays.push_back(std::move(d));
  labelMark((int)detail::MarkSlot::Overlay, index, std::move(name));
  return *this;
}

Element& Element::background(Decoration d, std::string name) {
  claimBorrows(d);
  const size_t index = m_node->backgrounds.size();
  m_node->backgrounds.push_back(std::move(d));
  labelMark((int)detail::MarkSlot::Background, index, std::move(name));
  return *this;
}

Element& Element::foreground(Decoration d, std::string name) {
  claimBorrows(d);
  const size_t index = m_node->foregrounds.size();
  m_node->foregrounds.push_back(std::move(d));
  labelMark((int)detail::MarkSlot::Foreground, index, std::move(name));
  return *this;
}

Element& Element::stroke(Decoration brush, std::string name) {
  return foreground(std::move(brush), std::move(name));
}

Element& Element::echo(SkVector offset, SkColor4f color) {
  m_node->fxData.ensure().echoes.push_back(Echo{offset, color});
  return *this;
}

Element& Element::style(LayerStyle s) {
  for (Decoration& d : s.under) {
    claimBorrows(d);
    m_node->backgrounds.push_back(std::move(d));
  }
  for (Decoration& d : s.over) {
    claimBorrows(d);
    m_node->foregrounds.push_back(std::move(d));
  }
  return *this;
}

}  // namespace sigil::compose
