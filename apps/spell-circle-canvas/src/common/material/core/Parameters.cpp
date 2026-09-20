/** @file
 * Uniform declarations per target, emitted from a schema.
 */

#include "sigilmaterial/core/Parameters.h"

namespace sigil::material {

namespace {

/** The type name a kind is declared with. SkSL and Slang spell these
 *  vector and matrix types identically. */
const char* typeName(ParameterType kind) {
  switch (kind) {
    case ParameterType::Float:
    case ParameterType::FloatArray:
      return "float";
    case ParameterType::Vec2:
      return "float2";
    case ParameterType::Vec4:
    case ParameterType::Color:
      return "float4";
    case ParameterType::Mat3:
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
  if (field.kind == ParameterType::FloatArray) {
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
