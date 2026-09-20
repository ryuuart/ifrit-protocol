/** @file
 * The mask verbs — the appearance-gating family, and the reads a span
 * gate declares for the derive pass.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& MaskVerbs<Derived>::mask(Gate with) {
  return mask(parts::all(), std::move(with));
}

template <class Derived>
Derived& MaskVerbs<Derived>::mask(Parts what, Gate with) {
  detail::ElementNode* node = declarations();
  // A fit() term inside a span GATE borrows another element's resolved box
  // exactly as one inside a span PASS does, so the keys ride into
  // DeriveData where the ONE derive-registration walk finds them.
  if (with.kind == Gate::Kind::Spans)
    for (const Spans::Term& t : with.where.terms)
      if (t.rule == Spans::Rule::Fit && !t.key.empty()) {
        detail::DeriveData& derive = node->deriveData.ensure();
        derive.spanFitKeys.push_back(t.key);
        // A gap sized from where a node LANDED is a read of its box.
        derive.reads.push_back({t.key, sigil::core::Facet::Bounds});
      }
  node->fxData.ensure().masks.push_back(Mask{std::move(what), std::move(with)});
  return self();
}

template class MaskVerbs<Element>;

}  // namespace sigil::compose
