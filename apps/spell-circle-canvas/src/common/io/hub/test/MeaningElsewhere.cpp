/** @file
 * THE OTHER IMAGE: a translation unit whose `Shade` is its own type — it
 * stands in an anonymous namespace, so its identity is not the identity
 * of the `Shade` the cases declare — under the one meaning name both
 * declare. It is how a sketch compiled and loaded while its host runs
 * sees a type the host registered: same name, same layout, another
 * identity.
 */

#include "MeaningElsewhere.h"

#include <sigilio/advanced/Decoding.h>

#include <string>
#include <type_traits>
#include <typeindex>

namespace {

struct Shade {
  std::string text;
};

std::string_view meaningName(std::type_identity<Shade>) { return "test.Shade"; }

}  // namespace

namespace sigil::io::test {

std::optional<size_t> loadShadeElsewhere(Hub& hub, std::string_view uri) {
  const auto shade = hub.load<Shade>(uri);
  if (!shade) return std::nullopt;
  return shade->text.size();
}

std::type_index shadeIdentityElsewhere() { return typeid(Shade); }

void registerShadeElsewhere(Hub& hub) {
  registerDecoder<Shade>(hub, [](const Bytes& bytes) {
    return std::optional<Shade>(Shade{std::string(bytes.asText())});
  });
}

}  // namespace sigil::io::test
