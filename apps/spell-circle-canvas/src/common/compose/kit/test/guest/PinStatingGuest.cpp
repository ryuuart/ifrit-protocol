/** @file
 * A GUEST IMAGE THAT STATES A FACT OF A LIBRARY TYPE, built beside the
 * kit's test binary and compiled hidden, as the live host compiles a
 * sketch: the fact is written with this image's own identity for the
 * type, and the pin operator that reads it runs in the test binary. The
 * library resolves out of the test binary as a sketch resolves it out of
 * Grimoire.
 */

#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Pin.h>

#include <typeinfo>

/** States on @p card a request to hang a red box, 40 by 20, ten units to
 *  its right and level with it, under the lane `label`. */
extern "C" __attribute__((visibility("default"))) void sigilGuestStatePin(
    sigil::compose::Element* card) {
  using namespace sigil::compose;
  card->attribute(
      "label",
      pin::Request{
          .element = box().fill(Fill::color({1, 0, 0, 1})),
          .size = {40, 20},
          .where = {.on = {1, 0.5f}, .at = {0, 0.5f}, .offset = {10, 0}}});
}

/** The identity this image holds for the request's type. */
extern "C" __attribute__((visibility("default"))) const std::type_info*
sigilGuestRequestType() {
  return &typeid(sigil::compose::pin::Request);
}
