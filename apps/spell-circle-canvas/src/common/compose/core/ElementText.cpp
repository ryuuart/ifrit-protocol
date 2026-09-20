/** @file
 * The text leaf's own content — the frame it fills into, the balanced
 * run it opens, and the leaf as it stands at rest, which is a copy of
 * its description. The verbs that dress type are declared beside these
 * and defined by the typography tier.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the rest-of-non-text diagnostic

#include <algorithm>

#include "ComposeInternal.h"

namespace sigil::compose {

namespace {
void warnAtRestOfNonText() {
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "[compose] atRest() on an element that is not text() hands back a "
      "plain copy: a rest pose is what an fx() track's per-glyph deviation "
      "is measured against, and every other element already draws where "
      "its layout put it.\n");
}
}  // namespace

template <class Derived>
Element TextContentVerbs<Derived>::atRest() const {
  const std::shared_ptr<detail::ElementNode>& source =
      detail::NodeAccess::node(self());
  if (source->kind != detail::Kind::Text || !source->textData) {
    warnAtRestOfNonText();
    return Element{source};
  }
  // A COPY OF THE DESCRIPTION, not a re-description: the copy has to be
  // the same paragraph, laid out the same way, at the same width, or the
  // two disagree about where a letter belongs and a comparison against it
  // is worthless. Everything that could shift a glyph is therefore carried
  // over untouched, and only what moves or restyles one at paint time
  // goes.
  auto rest = std::make_shared<detail::ElementNode>(*source);
  detail::TextData& text = rest->textData.ensure();
  text.tracks.clear();
  text.spanRestyles.clear();
  // …AND NO CHILDREN, which is the same rule read twice. A text node's
  // children are its marks and its slot mounts, and both are already on
  // screen once: copying them would draw each of them twice under a
  // duplicate key, which the composer's key index cannot answer for. The
  // slot RUNS stay — they are content, and they reserve the same space in
  // the copy's paragraph, which is what keeps the two copies' letters in
  // the same places.
  rest->children.clear();
  text.marks.clear();
  if (!rest->key.empty()) rest->key += "-rest";
  return Element{std::move(rest)};
}

template <class Derived>
Derived& TextContentVerbs<Derived>::thread(std::string_view key) {
  detail::ElementNode* node = declarations();
  node->textData.ensure().threadTo = std::string(key);
  // A frame is cut where the frame before it stopped, so it reads the
  // FINEST answer that frame produces — its units, not its box. The link
  // itself is LAST-WINS, so the read is replaced rather than added to: a
  // frame threads into exactly one frame, and a chain that named another
  // one first no longer waits for it.
  detail::DeriveData& derive = node->deriveData.ensure();
  std::erase_if(derive.reads, [](const sigil::core::Read& read) {
    return read.facet == sigil::core::Facet::Units;
  });
  derive.reads.push_back({std::string(key), sigil::core::Facet::Units});
  return self();
}

template <class Derived>
Derived& TextContentVerbs<Derived>::balanceChain(uint32_t throughLine) {
  detail::TextData& text = declarations()->textData.ensure();
  text.balanceChain = true;
  text.balanceThroughLine = throughLine;
  return self();
}

template class TextContentVerbs<Element>;

}  // namespace sigil::compose
