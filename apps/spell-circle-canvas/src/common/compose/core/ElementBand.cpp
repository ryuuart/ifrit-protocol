/** @file
 * The band leaf's own verb — which side of its spine the band
 * occupies.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& BandVerbs<Derived>::centered() {
  declarations()->deriveData.ensure().bandFormation =
      geometry::path::Formation::Center;
  return self();
}

template <class Derived>
Derived& BandVerbs<Derived>::outward() {
  declarations()->deriveData.ensure().bandFormation =
      geometry::path::Formation::Outer;
  return self();
}

template <class Derived>
Derived& BandVerbs<Derived>::inward() {
  declarations()->deriveData.ensure().bandFormation =
      geometry::path::Formation::Inner;
  return self();
}

template class BandVerbs<Element>;

}  // namespace sigil::compose
