#pragma once

/** @file
 * What the SDK-facing sources of the Substance feature share and no
 * consumer sees: the decode behind an Archive, the framework's strings
 * copied out, numeric inputs visited by their concrete type, and an
 * engine result turned into an image. It names SDK types, and the SDK is
 * private to the feature.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <sigilmaterial/substance/Substance.h>
#include <sigilmaterial/substance/advanced/Archive.h>
#include <substance/framework/framework.h>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace sigil::material::sbsar {

/** The package the SDK parsed, and every graph described from it once. */
struct Archive::Decoded {
  std::unique_ptr<SubstanceAir::PackageDesc> package;
  std::vector<Description> graphs;
  std::vector<std::string> urls;
};

/** The framework's strings use their own allocator; copy out. */
inline std::string toString(const SubstanceAir::string& text) {
  return {text.data(), text.size()};
}

/** How an output's values are read, from the colour space the graph
 *  declared for its first channel, or the SDK's default for its usage
 *  when it declared none. */
Encoding encodingOf(const SubstanceAir::OutputDesc& output);

/** The usage word an output declares, or its identifier when none. */
std::string usageOf(const SubstanceAir::OutputDesc& output);

/** An engine result in host memory as an image tagged by @p encoding:
 *  8-bit, 16-bit, half and full float, grey or colour, either channel
 *  order. Null for a compressed or empty result. */
sk_sp<SkImage> imageOf(const SubstanceTexture& texture, Encoding encoding);

/** Visits a numeric input with its concrete type. The callback receives
 *  (InputInstanceNumerical<T>&, componentCount); false when the input is
 *  not numeric, otherwise what the callback answers. */
template <class Visit>
bool withNumeric(SubstanceAir::InputInstanceBase& input, Visit&& visit) {
  namespace air = SubstanceAir;
  switch (input.mDesc.mType) {
    case Substance_IOType_Float:
      return visit(static_cast<air::InputInstanceFloat&>(input), 1);
    case Substance_IOType_Float2:
      return visit(static_cast<air::InputInstanceFloat2&>(input), 2);
    case Substance_IOType_Float3:
      return visit(static_cast<air::InputInstanceFloat3&>(input), 3);
    case Substance_IOType_Float4:
      return visit(static_cast<air::InputInstanceFloat4&>(input), 4);
    case Substance_IOType_Integer:
      return visit(static_cast<air::InputInstanceInt&>(input), 1);
    case Substance_IOType_Integer2:
      return visit(static_cast<air::InputInstanceInt2&>(input), 2);
    case Substance_IOType_Integer3:
      return visit(static_cast<air::InputInstanceInt3&>(input), 3);
    case Substance_IOType_Integer4:
      return visit(static_cast<air::InputInstanceInt4&>(input), 4);
    default:
      return false;
  }
}

/** A numeric value as one float per component. */
template <class Value>
void toFloats(const Value& value, int count, std::vector<float>& out) {
  out.resize((size_t)count);
  if constexpr (std::is_arithmetic_v<Value>) {
    out[0] = (float)value;
  } else {
    for (int index = 0; index < count; ++index)
      out[(size_t)index] = (float)value[index];
  }
}

/** One float per component as a numeric value; integers truncated. */
template <class Value>
Value fromFloats(std::span<const float> floats, int count) {
  Value value{};
  if constexpr (std::is_arithmetic_v<Value>) {
    value = (Value)floats[0];
  } else {
    using Element = std::decay_t<decltype(value[0])>;
    for (int index = 0; index < count; ++index)
      value[index] = (Element)floats[(size_t)index];
  }
  return value;
}

}  // namespace sigil::material::sbsar
