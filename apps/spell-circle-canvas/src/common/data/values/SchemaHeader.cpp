/** @file
 * THE WRITTEN HEADER AS ONE DOCUMENT: its preamble and includes, the
 * value namespaces it opens and how a name is spelled from inside one,
 * the names declared before any of them is written so one type may hold
 * another, the root's JSON form both ways, and the two traits each
 * table's reading out of bytes and its JSON form are named through.
 */

#include <flatbuffers/idl.h>

#include <algorithm>
#include <ostream>
#include <string>
#include <vector>

#include "SchemaHeader.h"
#include "SchemaLines.h"
#include "SchemaNames.h"

namespace sigil::data::schema {

std::string Header::spaceOf(const Definition& def) {
  return namespaceOf(def.defined_namespace);
}

std::string Header::valueSpaceOf(const std::string& space) {
  return space.empty() ? std::string("values") : space + "::values";
}

std::string Header::wireOf(const Definition& def) {
  return wireName(spaceOf(def), def.name);
}

std::string Header::valueRef(const Definition& def) const {
  const std::string home = valueSpaceOf(spaceOf(def));
  return home == m_open ? def.name : valueName(home, def.name);
}

std::string Header::callRef(const std::string& verb,
                            const Definition& def) const {
  const std::string home = valueSpaceOf(spaceOf(def));
  return home == m_open ? verb + def.name : valueName(home, verb + def.name);
}

void Header::enter(std::ostream& out, const std::string& space) {
  const std::string wanted = valueSpaceOf(space);
  if (wanted == m_open) return;
  if (!m_open.empty()) out << "}  // namespace " << m_open << "\n\n";
  out << "namespace " << wanted << " {\n\n";
  m_open = wanted;
}

void Header::leave(std::ostream& out) {
  if (m_open.empty()) return;
  out << "}  // namespace " << m_open << "\n";
  m_open.clear();
}

void Header::writeJson(std::ostream& out, const StructDef& root) {
  const std::string wire = wireOf(root);

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
                             const std::vector<const StructDef*>& ordered) {
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
    const std::string value = valueRef(*def);
    out << "\ntemplate <>\nstruct ::sigil::data::values::Read<" << value
        << "> {\n";
    out << "  static std::optional<" << value << "> from(\n";
    out << "      std::span<const std::byte> bytes) {\n";
    out << "    return " << callRef("read", *def) << "(bytes);\n";
    out << "  }\n};\n";
  }
}

/** EACH TABLE'S JSON FORM, for a reader that names the value: one
 *  specialization of the JSON trait per table, reading and writing
 *  through the schema the generated header embeds, read once at that
 *  table and kept. Written only for a schema that declares a root,
 *  since the root is what makes flatc embed the binary schema. */
void Header::writeJsonForms(std::ostream& out,
                            const std::vector<const StructDef*>& ordered) {
  bool any = false;
  for (const StructDef* def : ordered) {
    if (def->fixed) continue;
    if (!any) {
      out << "\n// EACH TABLE'S JSON FORM, for a reader that names the value:\n"
             "// the schema the generated header embeds, read once at that\n"
             "// table and kept, converts the text to a buffer and back, and\n"
             "// the table's own reading and writing cross to the value.\n";
      any = true;
    }
    const std::string value = valueRef(*def);
    const std::string qualified =
        def->defined_namespace
            ? def->defined_namespace->GetFullyQualifiedName(def->name)
            : def->name;
    out << "\ntemplate <>\nstruct ::sigil::data::values::JsonForm<" << value
        << "> {\n";
    out << "  static const ::sigil::data::Schema& schema() {\n";
    out << "    static const ::sigil::data::Schema table =\n";
    out << "        ::sigil::data::schema<" << wireOf(*def) << ">().rootedAt(\n";
    out << "            \"" << qualified << "\");\n";
    out << "    return table;\n";
    out << "  }\n";
    out << "  static std::optional<" << value << "> from(\n";
    out << "      std::string_view json, std::string* why) {\n";
    out << "    const std::optional<std::vector<std::byte>> buffer =\n";
    out << "        schema().binary(json, why);\n";
    out << "    if (!buffer) return std::nullopt;\n";
    out << "    return " << callRef("read", *def) << "(*buffer);\n";
    out << "  }\n";
    out << "  static std::optional<std::string> to(\n";
    out << "      const " << value << "& value, std::string* why) {\n";
    out << "    return schema().text(" << callRef("write", *def)
        << "(value), why);\n";
    out << "  }\n};\n";
  }
}

void Header::writeDeclarations(std::ostream& out,
                               const std::vector<const StructDef*>& ordered) {
  bool named = false;
  const auto nameOnce = [&] {
    if (named) return;
    out << "// The readings and the writings, named before any of them is\n"
           "// written, so a type that holds another reads and writes it.\n";
    named = true;
  };
  for (const StructDef* def : ordered) {
    enter(out, spaceOf(*def));
    nameOnce();
    const std::string wire = wireOf(*def);
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
    enter(out, spaceOf(*def));
    nameOnce();
    const std::string tag = wireOf(*def);
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
  const bool embedded = m_parser.root_struct_def_ != nullptr;

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
  if (embedded) out << "#include <sigildata/decode/Schema.h>\n";
  out << "\n";
  out << "#include <cstddef>\n";
  out << "#include <cstdint>\n";
  out << "#include <limits>\n";
  out << "#include <optional>\n";
  out << "#include <span>\n";
  out << "#include <string>\n";
  if (embedded) out << "#include <string_view>\n";
  out << "#include <utility>\n";
  out << "#include <variant>\n";
  out << "#include <vector>\n\n";
  out << "#include \"" << m_stem << "_generated.h\"\n\n";

  // The value types, named first so one may hold another: each value
  // namespace's names together, in the order the namespaces first
  // appear, which for a schema of one namespace is one block.
  std::vector<std::string> spaces;
  for (const StructDef* def : ordered) {
    const std::string space = spaceOf(*def);
    if (std::find(spaces.begin(), spaces.end(), space) == spaces.end())
      spaces.push_back(space);
  }
  for (const std::string& space : spaces) {
    enter(out, space);
    out << "// The value types, named first so one may hold another.\n";
    for (const StructDef* def : ordered)
      if (spaceOf(*def) == space) out << "struct " << def->name << ";\n";
    out << "\n";
  }

  for (const EnumDef* def : unions()) {
    for (const EnumVal* value : def->Vals()) {
      if (value->union_type.base_type == flatbuffers::BASE_TYPE_NONE) continue;
      if (!value->union_type.struct_def)
        refuseOnce("the union " + def->name + " carries " + value->name +
                   ", which is no table and has no value form here");
    }
    enter(out, spaceOf(*def));
    writeUnionAlias(out, *def);
  }
  if (!m_why.empty()) return false;

  for (const StructDef* def : ordered) {
    enter(out, spaceOf(*def));
    if (def->fixed)
      writeStructValue(out, *def);
    else
      writeTableValue(out, *def);
  }
  if (!m_why.empty()) return false;

  writeDeclarations(out, ordered);

  for (const StructDef* def : ordered) {
    enter(out, spaceOf(*def));
    if (def->fixed) {
      writeStructReadAndWrite(out, *def);
      continue;
    }
    writeTableRead(out, *def);
    writeTableWrite(out, *def);
  }
  for (const EnumDef* def : unions()) {
    enter(out, spaceOf(*def));
    writeUnionReadAndWrite(out, *def);
  }
  if (!m_why.empty()) return false;

  if (embedded) {
    enter(out, spaceOf(*m_parser.root_struct_def_));
    writeJson(out, *m_parser.root_struct_def_);
  }

  leave(out);

  writeReadTraits(out, ordered);
  if (embedded) writeJsonForms(out, ordered);
  return true;
}

}  // namespace sigil::data::schema
