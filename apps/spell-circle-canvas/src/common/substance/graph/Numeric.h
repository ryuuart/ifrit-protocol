#pragma once

/** @file
 * Numeric inputs visited by their concrete type, and their values
 * carried to and from plain float vectors — one component per element,
 * integers truncated from floats on the way in.
 */

#include <substance/framework/framework.h>

#include <type_traits>
#include <vector>

namespace sigil::substance {

/** Visit a numeric input instance with its concrete type. The callback
 *  receives (InputInstanceNumerical<T>&, componentCount); false when
 *  the input is not numeric, otherwise what the callback returns. */
template <class F>
bool withNumeric(SubstanceAir::InputInstanceBase& input, F&& f) {
  namespace air = SubstanceAir;
  switch (input.mDesc.mType) {
    case Substance_IOType_Float:
      return f(static_cast<air::InputInstanceFloat&>(input), 1);
    case Substance_IOType_Float2:
      return f(static_cast<air::InputInstanceFloat2&>(input), 2);
    case Substance_IOType_Float3:
      return f(static_cast<air::InputInstanceFloat3&>(input), 3);
    case Substance_IOType_Float4:
      return f(static_cast<air::InputInstanceFloat4&>(input), 4);
    case Substance_IOType_Integer:
      return f(static_cast<air::InputInstanceInt&>(input), 1);
    case Substance_IOType_Integer2:
      return f(static_cast<air::InputInstanceInt2&>(input), 2);
    case Substance_IOType_Integer3:
      return f(static_cast<air::InputInstanceInt3&>(input), 3);
    case Substance_IOType_Integer4:
      return f(static_cast<air::InputInstanceInt4&>(input), 4);
    default:
      return false;
  }
}

template <class T>
void toFloats(const T& v, int n, std::vector<float>& out) {
  out.resize((size_t)n);
  if constexpr (std::is_arithmetic_v<T>) {
    out[0] = (float)v;
  } else {
    for (int i = 0; i < n; ++i) out[(size_t)i] = (float)v[i];
  }
}

template <class T>
T fromFloats(const std::vector<float>& in, int n) {
  T v{};
  if constexpr (std::is_arithmetic_v<T>) {
    v = (T)in[0];
  } else {
    using E = std::decay_t<decltype(v[0])>;
    for (int i = 0; i < n; ++i) v[i] = (E)in[(size_t)i];
  }
  return v;
}

}  // namespace sigil::substance
