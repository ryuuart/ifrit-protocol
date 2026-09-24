/** @file
 * HOW EACH DEFINITION CROSSES BETWEEN THE WIRE AND ITS VALUE: the
 * reading that copies a generated struct, table or union out into plain
 * C++, and the writing that lays the same value back down through the
 * generated builders.
 */

#include <flatbuffers/idl.h>

#include <map>
#include <ostream>
#include <string>

#include "SchemaHeader.h"
#include "SchemaLines.h"
#include "SchemaNames.h"

namespace sigil::data::schema {

void Header::writeStructReadAndWrite(std::ostream& out, const StructDef& def) {
  const std::string wire = wireOf(def);
  out << "inline " << def.name << " read" << def.name << "(const " << wire
      << "& from) {\n";
  out << "  " << def.name << " value;\n";
  for (const FieldDef* field : def.fields.vec) {
    const Type& type = field->value.type;
    if (type.base_type == flatbuffers::BASE_TYPE_STRUCT)
      out << "  value." << field->name << " = "
          << callRef("read", *type.struct_def) << "(from." << field->name
          << "());\n";
    else
      out << "  value." << field->name << " = from." << field->name << "();\n";
  }
  out << "  return value;\n}\n\n";

  out << "inline " << wire << " write" << def.name << "(const " << def.name
      << "& value) {\n";
  out << "  return " << wire << "(";
  bool first = true;
  for (const FieldDef* field : def.fields.vec) {
    if (!first) out << ", ";
    first = false;
    const Type& type = field->value.type;
    if (type.base_type == flatbuffers::BASE_TYPE_STRUCT)
      out << callRef("write", *type.struct_def) << "(value." << field->name
          << ")";
    else
      out << "value." << field->name;
  }
  out << ");\n}\n\n";
}

void Header::writeTableRead(std::ostream& out, const StructDef& def) {
  const std::string wire = wireOf(def);
  writeCall(out, "inline std::optional<" + def.name + "> read" + def.name + "(",
            "const " + wire + "* from) {", "    ");
  out << "  if (!from) return std::nullopt;\n";
  out << "  " << def.name << " value;\n";
  for (const FieldDef* field : def.fields.vec) {
    if (field->deprecated) continue;
    const Type& type = field->value.type;
    if (isUnionTag(type)) continue;
    const std::string here = "value." + field->name;
    const std::string at = "from->" + field->name + "()";

    if (type.base_type == flatbuffers::BASE_TYPE_UNION) {
      out << "  {\n";
      out << "    const std::optional<" << valueRef(*type.enum_def)
          << "> read = " << callRef("read", *type.enum_def) << "(\n";
      out << "        " << at << ", from->" << field->name << "_type());\n";
      out << "    if (!read) return std::nullopt;\n";
      out << "    " << here << " = *read;\n";
      out << "  }\n";
      continue;
    }
    if (type.base_type == flatbuffers::BASE_TYPE_STRING) {
      if (field->IsRequired())
        out << "  if (!" << at << ") return std::nullopt;\n";
      writeAssignment(out, "  " + here + " =",
                      "::sigil::data::values::readString(" + at + ");",
                      "      ");
      continue;
    }
    if (flatbuffers::IsVector(type)) {
      const Type entry = type.VectorType();
      const std::string entryName =
          entry.struct_def ? valueRef(*entry.struct_def) : std::string();
      const std::string entryWire =
          entry.struct_def ? wireOf(*entry.struct_def) : std::string();
      const std::string entryRead =
          entry.struct_def ? callRef("read", *entry.struct_def) : std::string();
      if (field->IsRequired())
        out << "  if (!" << at << ") return std::nullopt;\n";
      if (entry.base_type == flatbuffers::BASE_TYPE_STRING) {
        writeAssignment(out, "  " + here + " =",
                        "::sigil::data::values::readStrings(" + at + ");",
                        "      ");
      } else if (entry.base_type == flatbuffers::BASE_TYPE_STRUCT &&
                 entry.struct_def->fixed) {
        out << "  " << here << " = ::sigil::data::values::readEach(\n";
        out << "      " << at << ", [](const " << entryWire
            << "* each) {\n";
        out << "        return " << entryRead << "(*each);\n";
        out << "      });\n";
      } else if (entry.base_type == flatbuffers::BASE_TYPE_STRUCT) {
        out << "  {\n";
        out << "    const std::optional<std::vector<" << entryName
            << ">> read =\n";
        out << "        ::sigil::data::values::readEachOrNone(\n";
        out << "            " << at << ", [](const " << entryWire
            << "* each) {\n";
        out << "              return " << entryRead << "(each);\n";
        out << "            });\n";
        out << "    if (!read) return std::nullopt;\n";
        out << "    " << here << " = *read;\n";
        out << "  }\n";
      } else if (entry.enum_def) {
        // The wire holds the enum's underlying integer; the value holds
        // the enumerated type, which is what the reading crosses.
        writeAssignment(out, "  " + here + " =",
                        "::sigil::data::values::readEnums<" +
                            wireOf(*entry.enum_def) + ">(" +
                            at + ");",
                        "      ");
      } else if (entry.base_type == flatbuffers::BASE_TYPE_BOOL) {
        writeAssignment(out, "  " + here + " =",
                        "::sigil::data::values::readBools(" + at + ");",
                        "      ");
      } else {
        writeAssignment(out, "  " + here + " =",
                        "::sigil::data::values::readScalars(" + at + ");",
                        "      ");
      }
      continue;
    }
    if (type.base_type == flatbuffers::BASE_TYPE_STRUCT) {
      if (type.struct_def->fixed) {
        out << "  if (" << at << ") " << here << " = "
            << callRef("read", *type.struct_def) << "(*" << at << ");\n";
        continue;
      }
      if (field->IsRequired())
        out << "  if (!" << at << ") return std::nullopt;\n";
      out << "  if (" << at << ") {\n";
      out << "    const std::optional<" << valueRef(*type.struct_def)
          << "> read = " << callRef("read", *type.struct_def) << "(" << at
          << ");\n";
      out << "    if (!read) return std::nullopt;\n";
      out << "    " << here << " = *read;\n";
      out << "  }\n";
      continue;
    }
    if (field->IsOptional()) {
      out << "  if (const auto read = " << at << ") " << here << " = *read;\n";
      continue;
    }
    out << "  " << here << " = " << at << ";\n";
  }
  out << "  return value;\n}\n\n";

  writeCall(out, "inline std::optional<" + def.name + "> read" + def.name + "(",
            "std::span<const std::byte> bytes) {", "    ");
  writeCall(out, "  return read" + def.name + "(",
            "::sigil::data::values::rootOf<" + wire + ">(bytes));", "      ");
  out << "}\n\n";
}

void Header::writeTableWrite(std::ostream& out, const StructDef& def) {
  const std::string wire = wireOf(def);
  // A table with nothing to write — one a protocol says "nothing" with —
  // never reads its value, so the parameter says so rather than warn.
  bool holds = false;
  for (const FieldDef* field : def.fields.vec)
    holds = holds || !field->deprecated;
  out << "inline ::flatbuffers::Offset<" << wire << "> write" << def.name
      << "(\n    ::flatbuffers::FlatBufferBuilder& into, "
      << (holds ? "" : "[[maybe_unused]] ") << "const " << def.name
      << "& value) {\n";

  // EVERY OFFSET AND EVERY INLINE STRUCT IS LAID DOWN FIRST. A builder
  // may not start a table and then write a string, a vector or another
  // table into the same buffer, so what each field points at stands
  // here before the table opens.
  //
  // A string and a vector are written whatever they hold, empty
  // included: reading either back answers the same value, and a field
  // the schema declared REQUIRED is satisfied by construction rather
  // than by a rule somewhere else.
  std::map<std::string, std::string> laid;  // field name -> its local
  for (const FieldDef* field : def.fields.vec) {
    if (field->deprecated) continue;
    const Type& type = field->value.type;
    if (isUnionTag(type)) continue;
    const std::string name = "written" + raised(field->name);
    const std::string here = "value." + field->name;

    if (type.base_type == flatbuffers::BASE_TYPE_UNION) {
      out << "  const ::flatbuffers::Offset<void> " << name << " =\n";
      out << "      " << callRef("write", *type.enum_def) << "(into, " << here
          << ");\n";
      laid[field->name] = name;
      continue;
    }
    if (type.base_type == flatbuffers::BASE_TYPE_STRING) {
      writeAssignment(out, "  const auto " + name + " =",
                      "::sigil::data::values::writeString(into, " + here + ");",
                      "      ");
      laid[field->name] = name;
      continue;
    }
    if (flatbuffers::IsVector(type)) {
      const Type entry = type.VectorType();
      const std::string entryName =
          entry.struct_def ? valueRef(*entry.struct_def) : std::string();
      const std::string entryWrite =
          entry.struct_def ? callRef("write", *entry.struct_def)
                           : std::string();
      if (entry.base_type == flatbuffers::BASE_TYPE_STRING) {
        writeAssignment(
            out, "  const auto " + name + " =",
            "::sigil::data::values::writeStrings(into, " + here + ");",
            "      ");
      } else if (entry.base_type == flatbuffers::BASE_TYPE_STRUCT &&
                 entry.struct_def->fixed) {
        out << "  const auto " << name << " =\n";
        out << "      ::sigil::data::values::writeStructs<"
            << wireOf(*entry.struct_def) << ">(\n";
        out << "          into, " << here << ",\n";
        out << "          [](const " << entryName << "& each) { return "
            << entryWrite << "(each); });\n";
      } else if (entry.base_type == flatbuffers::BASE_TYPE_STRUCT) {
        out << "  const auto " << name
            << " = ::sigil::data::values::writeEach(\n";
        out << "      into, " << here << ",\n";
        out << "      [](::flatbuffers::FlatBufferBuilder& each,\n";
        out << "         const " << entryName << "& one) { return "
            << entryWrite << "(each, one); });\n";
      } else if (entry.enum_def) {
        writeAssignment(
            out, "  const auto " + name + " =",
            "::sigil::data::values::writeEnums<" +
                scalarName(entry.enum_def->underlying_type.base_type) +
                ">(into, " + here + ");",
            "      ");
      } else if (entry.base_type == flatbuffers::BASE_TYPE_BOOL) {
        writeAssignment(
            out, "  const auto " + name + " =",
            "::sigil::data::values::writeBools(into, " + here + ");", "      ");
      } else {
        writeAssignment(
            out, "  const auto " + name + " =",
            "::sigil::data::values::writeScalars(into, " + here + ");",
            "      ");
      }
      laid[field->name] = name;
      continue;
    }
    if (type.base_type == flatbuffers::BASE_TYPE_STRUCT) {
      const std::string nested = wireOf(*type.struct_def);
      const std::string nestedWrite = callRef("write", *type.struct_def);
      if (type.struct_def->fixed) {
        out << "  const " << nested << " " << name << " = " << nestedWrite
            << "(" << here << ");\n";
      } else if (field->IsRequired()) {
        out << "  const auto " << name << " = " << nestedWrite << "(into, "
            << here << ");\n";
      } else {
        out << "  const ::flatbuffers::Offset<" << nested << "> " << name
            << " =\n";
        out << "      " << here << " ? " << nestedWrite << "(into, *" << here
            << ")\n";
        out << "          : ::flatbuffers::Offset<" << nested << ">();\n";
      }
      laid[field->name] = name;
      continue;
    }
  }

  out << "  " << wireName(spaceOf(def), def.name + "Builder")
      << " builder(into);\n";
  for (const FieldDef* field : def.fields.vec) {
    if (field->deprecated) continue;
    const Type& type = field->value.type;
    if (isUnionTag(type)) continue;
    const std::string here = "value." + field->name;
    const auto found = laid.find(field->name);
    if (type.base_type == flatbuffers::BASE_TYPE_UNION) {
      out << "  builder.add_" << field->name << "_type("
          << callRef("typeOf", *type.enum_def) << "(" << here << "));\n";
      out << "  builder.add_" << field->name << "(" << found->second << ");\n";
      continue;
    }
    if (found != laid.end()) {
      const bool inlineStruct =
          type.base_type == flatbuffers::BASE_TYPE_STRUCT &&
          type.struct_def->fixed;
      out << "  builder.add_" << field->name << "(" << (inlineStruct ? "&" : "")
          << found->second << ");\n";
      continue;
    }
    if (field->IsOptional()) {
      out << "  if (" << here << ") builder.add_" << field->name << "(*" << here
          << ");\n";
      continue;
    }
    out << "  builder.add_" << field->name << "(" << here << ");\n";
  }
  out << "  return builder.Finish();\n}\n\n";

  out << "inline std::vector<std::byte> write" << def.name << "(const "
      << def.name << "& value) {\n";
  out << "  ::flatbuffers::FlatBufferBuilder into;\n";
  out << "  into.Finish(write" << def.name << "(into, value));\n";
  out << "  return ::sigil::data::values::bytesOf(into);\n}\n\n";
}

void Header::writeUnionReadAndWrite(std::ostream& out, const EnumDef& def) {
  const std::string tag = wireOf(def);
  const std::string space = spaceOf(def);

  out << "inline std::optional<" << def.name << "> read" << def.name
      << "(\n    const void* from, " << tag << " which) {\n";
  out << "  switch (which) {\n";
  size_t index = 0;
  for (const EnumVal* value : def.Vals()) {
    if (!value->union_type.struct_def) continue;
    ++index;
    const StructDef& alternative = *value->union_type.struct_def;
    out << "    case " << wireEnumerator(space, def, value->name) << ": {\n";
    out << "      const std::optional<" << valueRef(alternative)
        << "> read =\n";
    out << "          " << callRef("read", alternative) << "(static_cast<const "
        << wireOf(alternative) << "*>(from));\n";
    out << "      if (!read) return std::nullopt;\n";
    out << "      return " << def.name << "(std::in_place_index<" << index
        << ">, *read);\n";
    out << "    }\n";
  }
  out << "    default:\n";
  out << "      return " << def.name << "();\n";
  out << "  }\n}\n\n";

  out << "inline " << tag << " typeOf" << def.name << "(const " << def.name
      << "& value) {\n";
  out << "  switch (value.index()) {\n";
  index = 0;
  for (const EnumVal* value : def.Vals()) {
    if (!value->union_type.struct_def) continue;
    ++index;
    out << "    case " << index << ":\n";
    out << "      return " << wireEnumerator(space, def, value->name)
        << ";\n";
  }
  out << "    default:\n";
  out << "      return " << wireEnumerator(space, def, "NONE") << ";\n";
  out << "  }\n}\n\n";

  out << "inline ::flatbuffers::Offset<void> write" << def.name
      << "(\n    ::flatbuffers::FlatBufferBuilder& into, const " << def.name
      << "& value) {\n";
  out << "  switch (value.index()) {\n";
  index = 0;
  for (const EnumVal* value : def.Vals()) {
    if (!value->union_type.struct_def) continue;
    ++index;
    out << "    case " << index << ":\n";
    out << "      return " << callRef("write", *value->union_type.struct_def)
        << "(into, std::get<" << index << ">(value)).Union();\n";
  }
  out << "    default:\n";
  out << "      return ::flatbuffers::Offset<void>();\n";
  out << "  }\n}\n\n";
}

}  // namespace sigil::data::schema
