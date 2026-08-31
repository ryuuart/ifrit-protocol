/** @file
 * Uniform declarations per target, emitted from a schema.
 */

#include "sigilmaterial/core/Params.h"

namespace sigil::material {

namespace {

/** The type name a kind is declared with. SkSL and Slang spell these
 *  vector and matrix types identically. */
const char* typeName(Kind kind) {
  switch (kind) {
    case Kind::Float:
    case Kind::FloatArray:
      return "float";
    case Kind::Vec2:
      return "float2";
    case Kind::Vec4:
    case Kind::Color:
      return "float4";
    case Kind::Mat3:
      return "float3x3";
  }
  return "float";
}

}  // namespace

std::string declare(const Field& field, Target) {
  std::string out = "uniform ";
  out += typeName(field.kind);
  out += ' ';
  out += field.name;
  if (field.kind == Kind::FloatArray) {
    out += '[';
    out += std::to_string(field.floats);
    out += ']';
  }
  out += ";\n";
  return out;
}

std::string declare(const Schema& schema, Target target) {
  std::string out;
  for (const Field& f : schema.fields) out += declare(f, target);
  return out;
}

}  // namespace sigil::material
