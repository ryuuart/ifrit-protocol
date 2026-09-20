#include "SchemaNames.h"

#include <flatbuffers/idl.h>

#include <cctype>
#include <string>

namespace sigil::data::schema {

std::string stemOf(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  const size_t dot = name.find_last_of('.');
  return dot == std::string::npos ? name : name.substr(0, dot);
}

std::string namespaceOf(const flatbuffers::Namespace* space) {
  std::string out;
  if (!space) return out;
  for (const std::string& part : space->components) {
    if (!out.empty()) out += "::";
    out += part;
  }
  return out;
}

std::string wireName(const std::string& space, const std::string& name) {
  return space.empty() ? "::" + name : "::" + space + "::" + name;
}

std::string valueName(const std::string& valueSpace, const std::string& name) {
  return "::" + valueSpace + "::" + name;
}

std::string wireEnumerator(const std::string& space, const EnumDef& tag,
                           const std::string& value) {
  return wireName(space, tag.name + "_" + value);
}

std::string raised(const std::string& name) {
  std::string out = name;
  if (!out.empty())
    out[0] =
        static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
  return out;
}

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

bool isUnionTag(const Type& type) {
  return type.enum_def != nullptr && type.enum_def->is_union &&
         flatbuffers::IsInteger(type.base_type);
}

}  // namespace sigil::data::schema
