/** @file
 * THE ORDER THE VALUE TYPES ARE WRITTEN IN: which definitions a value
 * holds by value and must therefore stand after, the order that
 * satisfies all of them at once, and the ring of definitions holding
 * one another that no such order exists for.
 */

#include <flatbuffers/idl.h>

#include <set>
#include <string>
#include <vector>

#include "SchemaHeader.h"
#include "SchemaNames.h"

namespace sigil::data::schema {

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

}  // namespace sigil::data::schema
