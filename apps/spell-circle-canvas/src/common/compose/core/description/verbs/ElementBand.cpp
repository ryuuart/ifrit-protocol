/** @file
 * The band leaf's own verb — which side of its spine the band
 * occupies.
 */

#include "description/ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& BandVerbs<Derived>::bandAlignment(
    geometry::path::Formation formation) {
  declarations()->deriveData.ensure().bandFormation = formation;
  return self();
}

template class BandVerbs<Band>;

}  // namespace sigil::compose
