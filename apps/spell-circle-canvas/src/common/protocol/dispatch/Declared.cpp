#include "Declared.h"

#include <flatbuffers/reflection.h>
#include <sigilprotocol/definition/Definition.h>

#include <span>
#include <string>

namespace sigil::protocol {
namespace {

/** The suffix an events service carries after its domain's name. */
constexpr std::string_view kEventsSuffix = "Events";

/** The definition's methods out of its reflected schema. A service is
 *  named fully, `sigil.protocol.clock.Clock`, and its domain is the
 *  namespace's last word — the generator refuses a definition where
 *  that and the service's name disagree, so the word is all that is
 *  read here. */
Declared read() {
  Declared out;
  const std::span<const std::byte> bytes = definition();
  const reflection::Schema* schema = reflection::GetSchema(bytes.data());
  out.schema = schema;
  if (!schema || !schema->services()) return out;
  for (const reflection::Service* service : *schema->services()) {
    const std::string_view name = service->name()->string_view();
    const size_t last = name.rfind('.');
    if (last == std::string_view::npos || last == 0) continue;
    const size_t before = name.rfind('.', last - 1);
    const std::string_view domain =
        name.substr(before == std::string_view::npos ? 0 : before + 1,
                    last - (before == std::string_view::npos ? 0 : before + 1));
    if (name.ends_with(kEventsSuffix)) {
      out.eventful.emplace(domain);
      continue;
    }
    if (!service->calls()) continue;
    for (const reflection::RPCCall* call : *service->calls()) {
      std::string method = std::string(domain) + "." + call->name()->str();
      out.parameters[method] = call->request();
      out.commands.insert(std::move(method));
    }
  }
  return out;
}

}  // namespace

const Declared& declared() {
  static const Declared once = read();
  return once;
}

std::string_view domainOf(std::string_view method) {
  return method.substr(0, method.find('.'));
}

}  // namespace sigil::protocol
