/** @file
 * The layout of a field list assembled at run time, and the uniform
 * declarations per target emitted from a schema.
 */

#include "sigilmaterial/core/Parameters.h"

#include <utility>

#include "sigilmaterial/core/Program.h"  // reportOnce

namespace sigil::material {

namespace {

/** How many floats a kind spans, and zero for the array that states its
 *  own count. */
size_t floatsOf(ParameterType kind) {
  switch (kind) {
    case ParameterType::Float:
      return 1;
    case ParameterType::Vec2:
      return 2;
    case ParameterType::Vec4:
    case ParameterType::Color:
      return 4;
    case ParameterType::Mat3:
      return 9;
    case ParameterType::FloatArray:
      return 0;
  }
  return 0;
}

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

Schema packedSchema(std::vector<Field> fields) {
  Schema out;
  out.fields.reserve(fields.size());
  for (Field& field : fields) {
    if (field.kind != ParameterType::FloatArray)
      field.floats = floatsOf(field.kind);
    const std::string key = "packedSchema:" + field.name;
    if (field.floats == 0) {
      reportOnce(key, "parameter field \"" + field.name +
                          "\" is an array of no floats; the field is left "
                          "out of the layout");
      continue;
    }
    if (out.find(field.name)) {
      reportOnce(key, "parameter field \"" + field.name +
                          "\" is declared twice; the second is left out of "
                          "the layout");
      continue;
    }
    field.offset = out.byteSize;
    out.byteSize += field.floats * sizeof(float);
    out.fields.push_back(std::move(field));
  }
  return out;
}

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
