/** @file
 * WHAT A VALUE OF EACH DEFINITION IS: the C++ type a field reads as,
 * the entry type of a vector, and the struct or variant alias the
 * written header declares for a schema's struct, table or union.
 */

#include <flatbuffers/idl.h>

#include <ostream>
#include <string>

#include "SchemaHeader.h"
#include "SchemaLines.h"
#include "SchemaNames.h"

namespace sigil::data::schema {

std::string Header::entryTypeOf(const Type& type) {
  const Type entry = type.VectorType();
  if (entry.base_type == flatbuffers::BASE_TYPE_STRING) return "std::string";
  if (entry.base_type == flatbuffers::BASE_TYPE_STRUCT)
    return entry.struct_def->name;
  // A vector of unions is two fields on the wire, the tags beside the
  // values; both land here and both are refused.
  if (entry.base_type == flatbuffers::BASE_TYPE_UNION ||
      (entry.enum_def && entry.enum_def->is_union)) {
    refuseOnce("a vector of unions has no value form here");
    return "void";
  }
  if (entry.enum_def) return wireName(m_space, entry.enum_def->name);
  const std::string scalar = scalarName(entry.base_type);
  if (scalar.empty())
    refuseOnce(std::string("a vector of ") +
               flatbuffers::TypeName(entry.base_type) +
               " has no value form here");
  return scalar;
}

std::string Header::valueTypeOf(const FieldDef& field) {
  const Type& type = field.value.type;
  if (flatbuffers::IsArray(type)) {
    refuseOnce("the fixed-size array field " + field.name +
               " has no value form here");
    return "void";
  }
  if (type.base_type == flatbuffers::BASE_TYPE_UNION)
    return type.enum_def->name;
  if (type.base_type == flatbuffers::BASE_TYPE_STRING) return "std::string";
  if (flatbuffers::IsVector(type))
    return "std::vector<" + entryTypeOf(type) + ">";
  if (type.base_type == flatbuffers::BASE_TYPE_STRUCT) {
    // A struct is inline and reads as the value its fields default to
    // where the wire left it out; a table is an offset that may be
    // absent, and only a REQUIRED one is promised to be there.
    if (type.struct_def->fixed || field.IsRequired())
      return type.struct_def->name;
    return "std::optional<" + type.struct_def->name + ">";
  }
  const std::string scalar = type.enum_def
                                 ? wireName(m_space, type.enum_def->name)
                                 : scalarName(type.base_type);
  if (scalar.empty()) {
    refuseOnce("the field " + field.name + " has no value form here");
    return "void";
  }
  // A scalar the schema declared with no default is absent or present,
  // which is a difference the wire carries and the value keeps.
  if (field.IsOptional()) return "std::optional<" + scalar + ">";
  return scalar;
}

void Header::writeStructValue(std::ostream& out, const StructDef& def) {
  writeDocComment(out, def.doc_comment, "");
  out << "struct " << def.name << " {\n";
  for (const FieldDef* field : def.fields.vec) {
    writeDocComment(out, field->doc_comment, "  ");
    out << "  " << valueTypeOf(*field) << " " << field->name << "{};\n";
  }
  out << "};\n\n";
}

void Header::writeTableValue(std::ostream& out, const StructDef& def) {
  writeDocComment(out, def.doc_comment, "");
  out << "struct " << def.name << " {\n";
  for (const FieldDef* field : def.fields.vec) {
    if (field->deprecated) continue;
    if (isUnionTag(field->value.type)) continue;
    writeDocComment(out, field->doc_comment, "  ");
    out << "  " << valueTypeOf(*field) << " " << field->name << "{};\n";
  }
  out << "};\n\n";
}

void Header::writeUnionAlias(std::ostream& out, const EnumDef& def) {
  writeDocComment(out, def.doc_comment, "");
  out << "using " << def.name << " = std::variant<std::monostate";
  for (const EnumVal* value : def.Vals()) {
    if (!value->union_type.struct_def) continue;
    out << ", " << value->union_type.struct_def->name;
  }
  out << ">;\n\n";
}

}  // namespace sigil::data::schema
