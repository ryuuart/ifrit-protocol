/** @file
 * THE WRITTEN HEADER AS ONE DOCUMENT: its preamble and includes, the
 * names declared before any of them is written so one type may hold
 * another, the root's JSON form both ways, and the trait each table's
 * reading out of bytes is named through.
 */

#include <flatbuffers/idl.h>

#include <ostream>
#include <string>
#include <vector>

#include "SchemaHeader.h"
#include "SchemaLines.h"
#include "SchemaNames.h"

namespace sigil::data::schema {

void Header::writeJson(std::ostream& out, const StructDef& root) {
  const std::string wire = wireName(m_space, root.name);

  out << "/** THE ROOT READ OUT OF THE SCHEMA'S OWN JSON FORM, through\n"
         " *  the schema the generated header carries. Nothing where the\n"
         " *  text does not fit it, and @p why carries the reader's own\n"
         " *  message — the line, the column and the field — where it is\n"
         " *  asked for. The generated header has to be the one written\n"
         " *  with its binary schema embedded, which is what the\n"
         " *  constraint on Root says. */\n";
  out << "template <class Root = " << wire << ">\n";
  out << "  requires ::sigil::data::CarriesSchema<Root>\n";
  out << "std::optional<" << root.name << "> fromJson(\n";
  out << "    std::string_view json, std::string* why = nullptr) {\n";
  out << "  const std::optional<std::vector<std::byte>> buffer =\n";
  out << "      ::sigil::data::schema<Root>().binary(json, why);\n";
  out << "  if (!buffer) return std::nullopt;\n";
  out << "  return read" << root.name << "(*buffer);\n";
  out << "}\n\n";

  out << "/** THE ROOT WRITTEN AS THE SCHEMA'S OWN JSON FORM. Nothing\n"
         " *  where what the value made is no buffer of this schema. */\n";
  out << "template <class Root = " << wire << ">\n";
  out << "  requires ::sigil::data::CarriesSchema<Root>\n";
  out << "std::optional<std::string> toJson(\n";
  out << "    const " << root.name
      << "& value, std::string* why = nullptr) {\n";
  out << "  return ::sigil::data::schema<Root>().text(write" << root.name
      << "(value), why);\n";
  out << "}\n\n";
}

/** HOW EACH TABLE IS READ OUT OF BYTES, for a reader that names the
 *  value and not the reading: one specialization of the trait the
 *  values header declares, per table.
 *
 *  These stand OUTSIDE the schema's own namespace, because a
 *  specialization of a template another namespace declares has to be
 *  written at namespace scope, and after the readings because each one
 *  calls the reading for its table. A struct gets none: it travels
 *  inline inside a table, is never a root, and has no reading out of
 *  bytes to stand on. */
void Header::writeReadTraits(std::ostream& out,
                             const std::vector<const StructDef*>& ordered,
                             const std::string& valueSpace) {
  bool any = false;
  for (const StructDef* def : ordered) {
    if (def->fixed) continue;
    if (!any) {
      out << "\n// HOW EACH TABLE IS READ OUT OF BYTES, for a reader that\n"
             "// names the value and not the reading. A specialization of a\n"
             "// template another namespace declares stands at namespace\n"
             "// scope, so these are written outside the schema's own\n"
             "// namespace, after the readings each of them calls.\n";
      any = true;
    }
    const std::string value = valueName(valueSpace, def->name);
    out << "\ntemplate <>\nstruct ::sigil::data::values::Read<" << value
        << "> {\n";
    out << "  static std::optional<" << value << "> from(\n";
    out << "      std::span<const std::byte> bytes) {\n";
    out << "    return " << valueName(valueSpace, "read" + def->name)
        << "(bytes);\n";
    out << "  }\n};\n";
  }
}

void Header::writeDeclarations(std::ostream& out,
                               const std::vector<const StructDef*>& ordered) {
  out << "// The readings and the writings, named before any of them is\n"
         "// written, so a type that holds another reads and writes it.\n";
  for (const StructDef* def : ordered) {
    const std::string wire = wireName(m_space, def->name);
    if (def->fixed) {
      out << "inline " << def->name << " read" << def->name << "(const " << wire
          << "& from);\n";
      out << "inline " << wire << " write" << def->name << "(const "
          << def->name << "& value);\n";
      continue;
    }
    writeCall(out,
              "inline std::optional<" + def->name + "> read" + def->name + "(",
              "const " + wire + "* from);", "    ");
    writeCall(out,
              "inline std::optional<" + def->name + "> read" + def->name + "(",
              "std::span<const std::byte> bytes);", "    ");
    out << "inline ::flatbuffers::Offset<" << wire << "> write" << def->name
        << "(\n    ::flatbuffers::FlatBufferBuilder& into, const " << def->name
        << "& value);\n";
    out << "inline std::vector<std::byte> write" << def->name << "(const "
        << def->name << "& value);\n";
  }
  for (const EnumDef* def : unions()) {
    const std::string tag = wireName(m_space, def->name);
    out << "inline std::optional<" << def->name << "> read" << def->name
        << "(\n    const void* from, " << tag << " which);\n";
    out << "inline " << tag << " typeOf" << def->name << "(const " << def->name
        << "& value);\n";
    out << "inline ::flatbuffers::Offset<void> write" << def->name
        << "(\n    ::flatbuffers::FlatBufferBuilder& into, const " << def->name
        << "& value);\n";
  }
  out << "\n";
}

bool Header::write(std::ostream& out) {
  const std::vector<const StructDef*> ordered = inOrder();
  if (!m_why.empty()) return false;

  const std::string valueSpace =
      m_space.empty() ? std::string("values") : m_space + "::values";

  out << "// The friendly header over " << m_stem
      << "_generated.h, written from\n// " << m_stem
      << ".fbs by sigil_schema_values. Edit the schema; this\n"
         "// is a build artefact and never hand-edited.\n"
         "//\n"
         "// Every type here says what the generated one says, as plain\n"
         "// C++: a value that copies, outlives the bytes it was read from\n"
         "// and is edited. Reading goes through the generated accessors\n"
         "// and writing through the generated builders, so the wire\n"
         "// format is the generated header's alone.\n\n";
  out << "#pragma once\n\n";
  out << "#include <sigildata/values/Values.h>\n";
  if (m_parser.root_struct_def_)
    out << "#include <sigildata/decode/Schema.h>\n";
  out << "\n";
  out << "#include <cstddef>\n";
  out << "#include <cstdint>\n";
  out << "#include <optional>\n";
  out << "#include <span>\n";
  out << "#include <string>\n";
  if (m_parser.root_struct_def_) out << "#include <string_view>\n";
  out << "#include <utility>\n";
  out << "#include <variant>\n";
  out << "#include <vector>\n\n";
  out << "#include \"" << m_stem << "_generated.h\"\n\n";

  out << "namespace " << valueSpace << " {\n\n";

  out << "// The value types, named first so one may hold another.\n";
  for (const StructDef* def : ordered) out << "struct " << def->name << ";\n";
  out << "\n";

  for (const EnumDef* def : unions()) {
    for (const EnumVal* value : def->Vals()) {
      if (value->union_type.base_type == flatbuffers::BASE_TYPE_NONE) continue;
      if (!value->union_type.struct_def)
        refuseOnce("the union " + def->name + " carries " + value->name +
                   ", which is no table and has no value form here");
    }
    writeUnionAlias(out, *def);
  }
  if (!m_why.empty()) return false;

  for (const StructDef* def : ordered) {
    if (def->fixed)
      writeStructValue(out, *def);
    else
      writeTableValue(out, *def);
  }
  if (!m_why.empty()) return false;

  writeDeclarations(out, ordered);

  for (const StructDef* def : ordered) {
    if (def->fixed) {
      writeStructReadAndWrite(out, *def);
      continue;
    }
    writeTableRead(out, *def);
    writeTableWrite(out, *def);
  }
  for (const EnumDef* def : unions()) writeUnionReadAndWrite(out, *def);
  if (!m_why.empty()) return false;

  if (m_parser.root_struct_def_) writeJson(out, *m_parser.root_struct_def_);

  out << "}  // namespace " << valueSpace << "\n";

  writeReadTraits(out, ordered, valueSpace);
  return true;
}

}  // namespace sigil::data::schema
