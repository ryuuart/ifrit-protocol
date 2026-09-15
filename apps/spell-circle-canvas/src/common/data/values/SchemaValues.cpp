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
 * run, and reading them is how the header below is written.
 */

#include <flatbuffers/idl.h>
#include <flatbuffers/util.h>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using flatbuffers::BaseType;
using flatbuffers::EnumDef;
using flatbuffers::EnumVal;
using flatbuffers::FieldDef;
using flatbuffers::StructDef;
using flatbuffers::Type;

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

/** The stem of a path: `a/b/schema_scene.fbs` is `schema_scene`. */
std::string stemOf(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  const size_t dot = name.find_last_of('.');
  return dot == std::string::npos ? name : name.substr(0, dot);
}

/** A schema's namespace as C++ spells it: `schema_scene`, or empty for
 *  a schema that declares none. */
std::string namespaceOf(const flatbuffers::Namespace* space) {
  std::string out;
  if (!space) return out;
  for (const std::string& part : space->components) {
    if (!out.empty()) out += "::";
    out += part;
  }
  return out;
}

/** A generated type as the generated header declares it, qualified from
 *  the global namespace so the value namespace under it cannot shadow
 *  anything: `::schema_scene::Sky`. */
std::string wireName(const std::string& space, const std::string& name) {
  return space.empty() ? "::" + name : "::" + space + "::" + name;
}

/** One enumerator of a union's tag enum: `::schema_scene::Message_Sky`. */
std::string wireEnumerator(const std::string& space, const EnumDef& tag,
                           const std::string& value) {
  return wireName(space, tag.name + "_" + value);
}

/** A field's name with its first letter raised, which is how the local
 *  holding what the field points at is named. */
std::string raised(const std::string& name) {
  std::string out = name;
  if (!out.empty())
    out[0] =
        static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
  return out;
}

/** The C++ spelling of a scalar the schema declares; empty for a base
 *  type that is no scalar. */
std::string scalarName(BaseType type) {
  switch (type) {
    case flatbuffers::BASE_TYPE_BOOL:
      return "bool";
    case flatbuffers::BASE_TYPE_CHAR:
      return "int8_t";
    case flatbuffers::BASE_TYPE_UCHAR:
      return "uint8_t";
    case flatbuffers::BASE_TYPE_SHORT:
      return "int16_t";
    case flatbuffers::BASE_TYPE_USHORT:
      return "uint16_t";
    case flatbuffers::BASE_TYPE_INT:
      return "int32_t";
    case flatbuffers::BASE_TYPE_UINT:
      return "uint32_t";
    case flatbuffers::BASE_TYPE_LONG:
      return "int64_t";
    case flatbuffers::BASE_TYPE_ULONG:
      return "uint64_t";
    case flatbuffers::BASE_TYPE_FLOAT:
      return "float";
    case flatbuffers::BASE_TYPE_DOUBLE:
      return "double";
    default:
      return std::string();
  }
}

/** The field a union's tag travels in. It is a field of the table on
 *  the wire and no member of the value: the variant's alternative is
 *  the type, so a second place to say it could only disagree. */
bool isUnionTag(const Type& type) {
  return type.enum_def != nullptr && type.enum_def->is_union &&
         flatbuffers::IsInteger(type.base_type);
}

/** The doc comment a schema wrote over a definition, carried onto what
 *  is generated for it. */
void writeDocComment(std::ostream& out, const std::vector<std::string>& lines,
                     const std::string& indent) {
  if (lines.empty()) return;
  if (lines.size() == 1) {
    out << indent << "/**" << lines[0] << " */\n";
    return;
  }
  out << indent << "/**" << lines[0] << "\n";
  for (size_t i = 1; i < lines.size(); ++i)
    out << indent << " *" << lines[i] << "\n";
  out << indent << " */\n";
}

/** A CALL ON ONE LINE, or broken after its opening bracket when the
 *  line would run past the width this tree's formatter keeps. @p head
 *  ends with that bracket and @p tail is the rest of the call. */
void writeCall(std::ostream& out, const std::string& head,
               const std::string& tail, const std::string& indent) {
  if (head.size() + tail.size() <= 80) {
    out << head << tail << "\n";
    return;
  }
  out << head << "\n" << indent << tail << "\n";
}

/** AN ASSIGNMENT ON ONE LINE, or with the value on the next when the
 *  line would run past that width. @p head ends with the equals sign. */
void writeAssignment(std::ostream& out, const std::string& head,
                     const std::string& tail, const std::string& indent) {
  if (head.size() + 1 + tail.size() <= 80) {
    out << head << " " << tail << "\n";
    return;
  }
  out << head << "\n" << indent << tail << "\n";
}

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

std::vector<const EnumDef*> Header::unions() const {
  std::vector<const EnumDef*> out;
  for (const EnumDef* def : m_parser.enums_.vec)
    if (def->is_union) out.push_back(def);
  return out;
}

std::vector<const StructDef*> Header::needs(const StructDef& def) const {
  std::vector<const StructDef*> out;
  for (const FieldDef* field : def.fields.vec) {
    if (field->deprecated) continue;
    const Type& type = field->value.type;
    if (isUnionTag(type)) continue;
    if (type.base_type == flatbuffers::BASE_TYPE_UNION) {
      for (const EnumVal* value : type.enum_def->Vals())
        if (value->union_type.struct_def)
          out.push_back(value->union_type.struct_def);
      continue;
    }
    if (type.base_type == flatbuffers::BASE_TYPE_STRUCT) {
      out.push_back(type.struct_def);
      continue;
    }
    if (flatbuffers::IsVector(type) &&
        type.VectorType().base_type == flatbuffers::BASE_TYPE_STRUCT)
      out.push_back(type.struct_def);
  }
  return out;
}

std::vector<const StructDef*> Header::inOrder() {
  std::vector<const StructDef*> left(m_parser.structs_.vec.begin(),
                                     m_parser.structs_.vec.end());
  std::vector<const StructDef*> out;
  std::set<const StructDef*> done;
  while (!left.empty()) {
    std::vector<const StructDef*> still;
    for (const StructDef* def : left) {
      bool ready = true;
      for (const StructDef* on : needs(*def))
        if (on != def && done.find(on) == done.end()) ready = false;
      if (!ready) {
        still.push_back(def);
        continue;
      }
      out.push_back(def);
      done.insert(def);
    }
    // Nothing moved: what is left holds itself, and a value holding its
    // neighbour by value cannot be written for a ring.
    if (still.size() == left.size()) {
      std::string ring;
      for (const StructDef* def : still) {
        if (!ring.empty()) ring += ", ";
        ring += def->name;
      }
      refuseOnce("these hold one another and have no value form here: " + ring);
      return {};
    }
    left = still;
  }
  return out;
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

void Header::writeStructReadAndWrite(std::ostream& out, const StructDef& def) {
  const std::string wire = wireName(m_space, def.name);
  out << "inline " << def.name << " read" << def.name << "(const " << wire
      << "& from) {\n";
  out << "  " << def.name << " value;\n";
  for (const FieldDef* field : def.fields.vec) {
    const Type& type = field->value.type;
    if (type.base_type == flatbuffers::BASE_TYPE_STRUCT)
      out << "  value." << field->name << " = read" << type.struct_def->name
          << "(from." << field->name << "());\n";
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
      out << "write" << type.struct_def->name << "(value." << field->name
          << ")";
    else
      out << "value." << field->name;
  }
  out << ");\n}\n\n";
}

void Header::writeTableRead(std::ostream& out, const StructDef& def) {
  const std::string wire = wireName(m_space, def.name);
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
      out << "    const std::optional<" << type.enum_def->name
          << "> read = read" << type.enum_def->name << "(\n";
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
          entry.struct_def ? entry.struct_def->name : std::string();
      if (field->IsRequired())
        out << "  if (!" << at << ") return std::nullopt;\n";
      if (entry.base_type == flatbuffers::BASE_TYPE_STRING) {
        writeAssignment(out, "  " + here + " =",
                        "::sigil::data::values::readStrings(" + at + ");",
                        "      ");
      } else if (entry.base_type == flatbuffers::BASE_TYPE_STRUCT &&
                 entry.struct_def->fixed) {
        out << "  " << here << " = ::sigil::data::values::readEach(\n";
        out << "      " << at << ", [](const " << wireName(m_space, entryName)
            << "* each) {\n";
        out << "        return read" << entryName << "(*each);\n";
        out << "      });\n";
      } else if (entry.base_type == flatbuffers::BASE_TYPE_STRUCT) {
        out << "  {\n";
        out << "    const std::optional<std::vector<" << entryName
            << ">> read =\n";
        out << "        ::sigil::data::values::readEachOrNone(\n";
        out << "            " << at << ", [](const "
            << wireName(m_space, entryName) << "* each) {\n";
        out << "              return read" << entryName << "(each);\n";
        out << "            });\n";
        out << "    if (!read) return std::nullopt;\n";
        out << "    " << here << " = *read;\n";
        out << "  }\n";
      } else if (entry.enum_def) {
        // The wire holds the enum's underlying integer; the value holds
        // the enumerated type, which is what the reading crosses.
        writeAssignment(out, "  " + here + " =",
                        "::sigil::data::values::readEnums<" +
                            wireName(m_space, entry.enum_def->name) + ">(" +
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
        out << "  if (" << at << ") " << here << " = read"
            << type.struct_def->name << "(*" << at << ");\n";
        continue;
      }
      if (field->IsRequired())
        out << "  if (!" << at << ") return std::nullopt;\n";
      out << "  if (" << at << ") {\n";
      out << "    const std::optional<" << type.struct_def->name
          << "> read = read" << type.struct_def->name << "(" << at << ");\n";
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
  const std::string wire = wireName(m_space, def.name);
  out << "inline ::flatbuffers::Offset<" << wire << "> write" << def.name
      << "(\n    ::flatbuffers::FlatBufferBuilder& into, const " << def.name
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
      out << "      write" << type.enum_def->name << "(into, " << here
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
          entry.struct_def ? entry.struct_def->name : std::string();
      if (entry.base_type == flatbuffers::BASE_TYPE_STRING) {
        writeAssignment(
            out, "  const auto " + name + " =",
            "::sigil::data::values::writeStrings(into, " + here + ");",
            "      ");
      } else if (entry.base_type == flatbuffers::BASE_TYPE_STRUCT &&
                 entry.struct_def->fixed) {
        out << "  const auto " << name << " =\n";
        out << "      ::sigil::data::values::writeStructs<"
            << wireName(m_space, entryName) << ">(\n";
        out << "          into, " << here << ",\n";
        out << "          [](const " << entryName << "& each) { return write"
            << entryName << "(each); });\n";
      } else if (entry.base_type == flatbuffers::BASE_TYPE_STRUCT) {
        out << "  const auto " << name
            << " = ::sigil::data::values::writeEach(\n";
        out << "      into, " << here << ",\n";
        out << "      [](::flatbuffers::FlatBufferBuilder& each,\n";
        out << "         const " << entryName << "& one) { return write"
            << entryName << "(each, one); });\n";
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
      const std::string nested = wireName(m_space, type.struct_def->name);
      if (type.struct_def->fixed) {
        out << "  const " << nested << " " << name << " = write"
            << type.struct_def->name << "(" << here << ");\n";
      } else if (field->IsRequired()) {
        out << "  const auto " << name << " = write" << type.struct_def->name
            << "(into, " << here << ");\n";
      } else {
        out << "  const ::flatbuffers::Offset<" << nested << "> " << name
            << " =\n";
        out << "      " << here << " ? write" << type.struct_def->name
            << "(into, *" << here << ")\n";
        out << "          : ::flatbuffers::Offset<" << nested << ">();\n";
      }
      laid[field->name] = name;
      continue;
    }
  }

  out << "  " << wireName(m_space, def.name + "Builder") << " builder(into);\n";
  for (const FieldDef* field : def.fields.vec) {
    if (field->deprecated) continue;
    const Type& type = field->value.type;
    if (isUnionTag(type)) continue;
    const std::string here = "value." + field->name;
    const auto found = laid.find(field->name);
    if (type.base_type == flatbuffers::BASE_TYPE_UNION) {
      out << "  builder.add_" << field->name << "_type(typeOf"
          << type.enum_def->name << "(" << here << "));\n";
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
  const std::string tag = wireName(m_space, def.name);

  out << "inline std::optional<" << def.name << "> read" << def.name
      << "(\n    const void* from, " << tag << " which) {\n";
  out << "  switch (which) {\n";
  size_t index = 0;
  for (const EnumVal* value : def.Vals()) {
    if (!value->union_type.struct_def) continue;
    ++index;
    const StructDef& alternative = *value->union_type.struct_def;
    out << "    case " << wireEnumerator(m_space, def, value->name) << ": {\n";
    out << "      const std::optional<" << alternative.name << "> read =\n";
    out << "          read" << alternative.name << "(static_cast<const "
        << wireName(m_space, alternative.name) << "*>(from));\n";
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
    out << "      return " << wireEnumerator(m_space, def, value->name)
        << ";\n";
  }
  out << "    default:\n";
  out << "      return " << wireEnumerator(m_space, def, "NONE") << ";\n";
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
    out << "      return write" << value->union_type.struct_def->name
        << "(into, std::get<" << index << ">(value)).Union();\n";
  }
  out << "    default:\n";
  out << "      return ::flatbuffers::Offset<void>();\n";
  out << "  }\n}\n\n";
}

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
  return true;
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
    const std::string here = namespaceOf(def->defined_namespace);
    if (found && here != *space) return false;
    *space = here;
    found = true;
  }
  for (const EnumDef* def : parser.enums_.vec) {
    const std::string here = namespaceOf(def->defined_namespace);
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

  const std::string stem = stemOf(ask.schema);
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
