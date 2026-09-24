#include <sigilprotocol/definition/Definition.h>
#include <sigilprotocol/protocol_bfbs_generated.h>

namespace sigil::protocol {

// flatc embeds the reflected definition under the name of the schema's
// root table, which is the error every domain shares.
std::span<const std::byte> definition() {
  return {reinterpret_cast<const std::byte*>(ErrorBinarySchema::data()),
          ErrorBinarySchema::size()};
}

}  // namespace sigil::protocol
