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
 *  say. */
class Header {
 public:
  Header(const flatbuffers::Parser& parser, std::string space, std::string stem)
      : m_parser(parser), m_space(std::move(space)), m_stem(std::move(stem)) {}

  bool write(std::ostream& out);
  const std::string& why() const { return m_why; }

 private:
  /** The value type a field reads as, bare inside the value namespace. */
  std::string valueTypeOf(const FieldDef& field);
  /** The value type one entry of a vector reads as. */
  std::string entryTypeOf(const Type& type);

  void writeStructValue(std::ostream& out, const StructDef& def);
  void writeTableValue(std::ostream& out, const StructDef& def);
  void writeUnionAlias(std::ostream& out, const EnumDef& def);

  void writeStructReadAndWrite(std::ostream& out, const StructDef& def);
  void writeTableRead(std::ostream& out, const StructDef& def);
  void writeTableWrite(std::ostream& out, const StructDef& def);
  void writeUnionReadAndWrite(std::ostream& out, const EnumDef& def);
  void writeJson(std::ostream& out, const StructDef& root);
  void writeDeclarations(std::ostream& out,
                         const std::vector<const StructDef*>& ordered);
  void writeReadTraits(std::ostream& out,
                       const std::vector<const StructDef*>& ordered,
                       const std::string& valueSpace);

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
  std::string m_space;
  std::string m_stem;
  std::string m_why;
};

}  // namespace sigil::data::schema
