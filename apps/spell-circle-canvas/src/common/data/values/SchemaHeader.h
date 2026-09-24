/** @file
 * THE SCHEMA AS ONE HEADER'S WORTH OF VALUE TYPES: the walk that reads
 * the parser and writes the whole document, and the walks it is made
 * of, each defined beside the subject it writes.
 */

#pragma once

#include <flatbuffers/idl.h>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "SchemaNames.h"

namespace sigil::data::schema {

/** THE SCHEMA AS ONE HEADER'S WORTH OF VALUE TYPES. Every walk below
 *  reads the parser and writes into the stream it is handed; `why()`
 *  holds the first thing the schema asked for that a value cannot
 *  say.
 *
 *  A SCHEMA MAY SPAN NAMESPACES, and each definition's value type goes
 *  into the value namespace beside its own: `feed_sky::values`, or
 *  `sigil::protocol::clock::values` beside `sigil::protocol::values`.
 *  The header opens one of those at a time, in the order the
 *  definitions are written, and a name is spelled bare inside its own
 *  and qualified from the global namespace everywhere else. */
class Header {
 public:
  Header(const flatbuffers::Parser& parser, std::string stem)
      : m_parser(parser), m_stem(std::move(stem)) {}

  bool write(std::ostream& out);
  const std::string& why() const { return m_why; }

 private:
  /** The namespace a definition is declared in, as C++ spells it. */
  static std::string spaceOf(const Definition& def);
  /** The value namespace beside @p space: `feed_sky::values`, or
   *  `values` for a schema that declares no namespace. */
  static std::string valueSpaceOf(const std::string& space);
  /** A generated type, qualified from the global namespace. */
  static std::string wireOf(const Definition& def);
  /** A value type as the namespace being written spells it: bare where
   *  it is declared there, qualified from the global namespace
   *  otherwise. */
  std::string valueRef(const Definition& def) const;
  /** A reading or a writing named for @p def — `readSky`, `writeSky`,
   *  `typeOfMessage` — spelled by the same rule as a value type. */
  std::string callRef(const std::string& verb, const Definition& def) const;
  /** Opens the value namespace of @p space, closing the one open. */
  void enter(std::ostream& out, const std::string& space);
  /** Closes the value namespace open, where one is. */
  void leave(std::ostream& out);

  /** The value type a field reads as. */
  std::string valueTypeOf(const FieldDef& field);
  /** The value type one entry of a vector reads as. */
  std::string entryTypeOf(const Type& type);
  /** What a table's field starts at: its schema's default, spelled for
   *  the braces after the member; empty for the type's own zero. */
  std::string defaultOf(const FieldDef& field) const;

  void writeStructValue(std::ostream& out, const StructDef& def);
  void writeTableValue(std::ostream& out, const StructDef& def);
  void writeUnionAlias(std::ostream& out, const EnumDef& def);
  void writeComparison(std::ostream& out, const StructDef& def);

  void writeStructReadAndWrite(std::ostream& out, const StructDef& def);
  void writeTableRead(std::ostream& out, const StructDef& def);
  void writeTableWrite(std::ostream& out, const StructDef& def);
  void writeUnionReadAndWrite(std::ostream& out, const EnumDef& def);
  void writeJson(std::ostream& out, const StructDef& root);
  void writeDeclarations(std::ostream& out,
                         const std::vector<const StructDef*>& ordered);
  void writeReadTraits(std::ostream& out,
                       const std::vector<const StructDef*>& ordered);
  void writeJsonForms(std::ostream& out,
                      const std::vector<const StructDef*>& ordered);

  /** Every value type that must be complete before @p def's own is. */
  std::vector<const StructDef*> needs(const StructDef& def) const;
  /** The value types in an order where each stands on ones already
   *  defined; empty where they stand on one another in a ring. */
  std::vector<const StructDef*> inOrder();
  /** The schema's unions, in the order it declares them. */
  std::vector<const EnumDef*> unions() const;

  void refuseOnce(const std::string& why) {
    if (m_why.empty()) m_why = why;
  }

  const flatbuffers::Parser& m_parser;
  std::string m_stem;
  std::string m_why;
  /** The value namespace the header has open; empty between them. */
  std::string m_open;
};

}  // namespace sigil::data::schema
