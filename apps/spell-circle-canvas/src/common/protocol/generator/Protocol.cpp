/** @file
 * THE PROTOCOL'S GENERATOR: the definition's reflected schema read, and
 * every C++ artefact that derives from it written.
 *
 *   sigil_protocol <protocol.bfbs> -o <include dir>
 *                  --domains <name>[,<name>...] --description <file.json>
 *
 * reads the `.bfbs` flatc wrote for protocol.fbs with its comments and
 * its attributes kept, and writes, for every domain, `<name>/<Service>
 * Agent.h` and `<name>/<Service>Client.h` under the include directory,
 * `Tables.h` beside them, and the whole definition as JSON text to the
 * description, which the Python generator reads for the Python client
 * and the reference pages.
 * `--domains` is the list the build expects, so a definition that grows
 * or loses a domain stops the build until the build names it.
 *
 * The value types every output spells are sigil_schema_values', written
 * from the same definition; nothing here re-reads the wire format, and
 * nothing here is hand-maintained beside the definition.
 */

#include <flatbuffers/util.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ProtocolModel.h"
#include "ProtocolOutputs.h"
#include "ProtocolWriting.h"

namespace {

using sigil::protocol::generator::Domain;
using sigil::protocol::generator::Model;

/** What the generator was asked to do. */
struct Ask {
  std::string schema;
  std::string directory;
  std::string description;
  std::vector<std::string> domains;
};

/** What stopped the generator, named on the way out so a definition
 *  that breaks a rule breaks the build. */
void refuse(const std::string& why) {
  std::fprintf(stderr, "sigil_protocol: %s\n", why.c_str());
}

std::vector<std::string> splitOnCommas(const std::string& text) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t comma = text.find(',', start);
    const size_t end = comma == std::string::npos ? text.size() : comma;
    if (end > start) out.push_back(text.substr(start, end - start));
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return out;
}

bool readAsk(int count, char** words, Ask* ask) {
  for (int i = 1; i < count; ++i) {
    const std::string word = words[i];
    const bool valued =
        word == "-o" || word == "--description" || word == "--domains";
    if (valued && ++i == count) return false;
    if (word == "-o") {
      ask->directory = words[i];
    } else if (word == "--description") {
      ask->description = words[i];
    } else if (word == "--domains") {
      ask->domains = splitOnCommas(words[i]);
    } else if (!word.empty() && word[0] == '-') {
      return false;
    } else if (ask->schema.empty()) {
      ask->schema = word;
    } else {
      return false;
    }
  }
  return !ask->schema.empty() && !ask->directory.empty() &&
         !ask->description.empty() && !ask->domains.empty();
}

/** Whether the definition's domains are the ones the build expects. */
bool domainsAre(const Model& model, const std::vector<std::string>& expected,
                std::string* why) {
  std::vector<std::string> found;
  for (const Domain& domain : model.domains) found.push_back(domain.name);
  std::vector<std::string> wanted = expected;
  std::sort(found.begin(), found.end());
  std::sort(wanted.begin(), wanted.end());
  if (found == wanted) return true;
  std::string list;
  for (const std::string& name : found) list += (list.empty() ? "" : ",") + name;
  *why = "the definition declares the domains " + list +
         ", and the build expects others: name them in --domains";
  return false;
}

}  // namespace

int main(int count, char** words) {
  Ask ask;
  if (!readAsk(count, words, &ask)) {
    refuse(
        "usage: sigil_protocol <protocol.bfbs> -o <include dir>"
        " --domains <name>[,<name>...] --description <file.json>");
    return 1;
  }

  std::string bytes;
  if (!flatbuffers::LoadFile(ask.schema.c_str(), true, &bytes)) {
    refuse("cannot read " + ask.schema);
    return 1;
  }

  std::string why;
  const std::optional<Model> model = sigil::protocol::generator::readModel(
      std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()),
                               bytes.size()),
      &why);
  if (!model || !domainsAre(*model, ask.domains, &why)) {
    refuse(why);
    return 1;
  }

  for (const Domain& domain : model->domains) {
    const std::string base = ask.directory + "/" + domain.name + "/" +
                             domain.service;
    if (!sigil::protocol::generator::writeFile(
            base + "Agent.h",
            sigil::protocol::generator::agentHeader(*model, domain), &why) ||
        !sigil::protocol::generator::writeFile(
            base + "Client.h",
            sigil::protocol::generator::clientHeader(*model, domain), &why)) {
      refuse(why);
      return 1;
    }
  }
  if (!sigil::protocol::generator::writeFile(
          ask.directory + "/Tables.h",
          sigil::protocol::generator::tablesHeader(*model), &why) ||
      !sigil::protocol::generator::writeFile(
          ask.description, sigil::protocol::generator::description(*model),
          &why)) {
    refuse(why);
    return 1;
  }
  return 0;
}
