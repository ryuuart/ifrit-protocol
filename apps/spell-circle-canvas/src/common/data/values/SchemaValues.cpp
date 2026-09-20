/** @file
 * THE FRIENDLY HEADER OVER A GENERATED ONE: a schema read with
 * FlatBuffers' own parser and written back out as value types.
 *
 *   sigil_schema_values <schema.fbs> -o <directory> [-I <include dir>]...
 *
 * writes `<stem>_values.h` beside whatever `flatc --cpp` wrote for the
 * same schema. That header stands ON the generated one and never
 * replaces it: every reading goes through the generated accessors and
 * every writing through the generated builders, so the wire format is
 * the one flatc decided and nothing here re-implements it.
 *
 * WHAT A VALUE IS. A FlatBuffer is read in place — a string is a
 * pointer that may be null, a vector is a pointer that may be null, a
 * table is a pointer into bytes that have to outlive it. A value is the
 * same message as plain C++: `std::string`, `std::vector`, a struct of
 * its own fields, `std::optional` for a table that may be absent and
 * `std::variant` for a union. It copies, it outlives the bytes, it
 * compares and it is edited, which is what a consumer holding a scene
 * in a field wants.
 *
 * THE SCHEMA'S OWN NAMES, UNCHANGED. Fields keep their order and their
 * spelling; a deprecated field is skipped, because the schema says it
 * may not be read or written. The value types go in
 * `<namespace>::values` rather than in the schema's own namespace,
 * since the generated header already declares `Sky` there and two `Sky`
 * in one namespace is no program.
 *
 * AND EACH TABLE'S READING IS NAMED ONCE MORE, as a specialization of
 * the trait the values header declares, so a reader that names the
 * VALUE type reaches the reading without naming it. Those stand outside
 * the schema's namespace: a specialization of a template another
 * namespace declares is written at namespace scope.
 *
 * WHAT THIS REFUSES rather than half-answering: a fixed-size array
 * field, a vector of unions, a union alternative that is not a table, a
 * schema whose definitions do not share one namespace, and tables that
 * hold one another in a ring, which no value holding its neighbour by
 * value can be. Each names itself on the way out, so a schema that
 * grows one of them stops the build instead of writing a header that
 * does not compile.
 *
 * Of flatbuffers this opens the parser, which is the whole point: the
 * schema's types, fields and unions are what it holds once Parse has
 * run, and reading them is how the header beside this is written. This
 * file is the tool itself: what it was asked to do, and the one walk
 * from a schema on disk to a header on disk.
 */

#include <flatbuffers/idl.h>
#include <flatbuffers/util.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "SchemaHeader.h"
#include "SchemaNames.h"

namespace {

using sigil::data::schema::EnumDef;
using sigil::data::schema::Header;
using sigil::data::schema::StructDef;

/** What the generator was asked to do. */
struct Ask {
  std::string schema;
  std::string directory;
  std::vector<std::string> includes;
};

/** What stopped the generator, named on the way out so a schema that
 *  grows something this cannot express breaks the build. */
void refuse(const std::string& why) {
  std::fprintf(stderr, "sigil_schema_values: %s\n", why.c_str());
}

/** The arguments, or nothing where they are not a whole ask. */
bool readAsk(int count, char** words, Ask* ask) {
  for (int i = 1; i < count; ++i) {
    const std::string word = words[i];
    if (word == "-o") {
      if (++i == count) return false;
      ask->directory = words[i];
    } else if (word == "-I") {
      if (++i == count) return false;
      ask->includes.push_back(words[i]);
    } else if (!word.empty() && word[0] == '-') {
      return false;
    } else if (ask->schema.empty()) {
      ask->schema = word;
    } else {
      return false;
    }
  }
  return !ask->schema.empty() && !ask->directory.empty();
}

/** The one namespace every definition of the schema shares, or nothing
 *  where they do not share one. */
bool oneNamespace(const flatbuffers::Parser& parser, std::string* space) {
  bool found = false;
  for (const StructDef* def : parser.structs_.vec) {
    const std::string here =
        sigil::data::schema::namespaceOf(def->defined_namespace);
    if (found && here != *space) return false;
    *space = here;
    found = true;
  }
  for (const EnumDef* def : parser.enums_.vec) {
    const std::string here =
        sigil::data::schema::namespaceOf(def->defined_namespace);
    if (found && here != *space) return false;
    *space = here;
    found = true;
  }
  return true;
}

}  // namespace

int main(int count, char** words) {
  Ask ask;
  if (!readAsk(count, words, &ask)) {
    refuse(
        "usage: sigil_schema_values <schema.fbs> -o <directory>"
        " [-I <include dir>]...");
    return 1;
  }

  std::string source;
  if (!flatbuffers::LoadFile(ask.schema.c_str(), false, &source)) {
    refuse("cannot read " + ask.schema);
    return 1;
  }

  std::vector<const char*> includes;
  for (const std::string& each : ask.includes) includes.push_back(each.c_str());
  includes.push_back(nullptr);

  flatbuffers::Parser parser;
  if (!parser.Parse(source.c_str(), includes.data(), ask.schema.c_str())) {
    refuse(parser.error_);
    return 1;
  }

  std::string space;
  if (!oneNamespace(parser, &space)) {
    refuse(
        "the schema's definitions do not share one namespace, and one"
        " header of value types stands in one namespace");
    return 1;
  }

  const std::string stem = sigil::data::schema::stemOf(ask.schema);
  std::ostringstream text;
  Header header(parser, space, stem);
  if (!header.write(text)) {
    refuse(header.why());
    return 1;
  }

  const std::string path = ask.directory + "/" + stem + "_values.h";
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text.str();
  file.close();
  if (!file) {
    refuse("cannot write " + path);
    return 1;
  }
  return 0;
}
