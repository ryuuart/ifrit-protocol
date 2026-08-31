#pragma once

/** @file
 * Reflection over a params struct — the ABI between a material's author
 * and the shader that reads it. A plain aggregate of uniform-typed fields
 * is walked by name and offset with no macro and no registration, and
 * its uniform declarations are emitted per target from the same walk.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Target.h>

#include <array>
#include <boost/pfr.hpp>
#include <cstddef>
#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::material {

/** The uniform types a params field may have. Every one is some count of
 *  floats with float alignment, so a struct of them has no padding and its
 *  bytes ARE the upload. */
enum class Kind : uint8_t {
  Float,       ///< one float
  Vec2,        ///< two floats: an offset, a size, a direction
  Vec4,        ///< four floats that are not a colour: a rect, a quaternion
  FloatArray,  ///< N floats, declared as `float name[N]`
  Color,       ///< four straight sRGB floats, declared as float4
  Mat3,        ///< nine floats, column-major, declared as float3x3
};

/** One field of a params struct as the upload sees it. */
struct Field {
  std::string name;
  Kind kind;
  size_t floats;  ///< how many floats the field spans
  size_t offset;  ///< byte offset from the start of the struct

  bool operator==(const Field&) const = default;
};

/** The upload layout of a params struct, in declaration order. */
struct Schema {
  std::vector<Field> fields;
  size_t byteSize = 0;

  bool operator==(const Schema&) const = default;
  /** The field named @p name, or null. */
  const Field* find(std::string_view name) const {
    for (const Field& f : fields)
      if (f.name == name) return &f;
    return nullptr;
  }
};

/** Maps a C++ type to its uniform kind and float count. A field type with
 *  no specialisation is not a uniform, and `schema<P>()` refuses the
 *  struct at compile time. */
template <class T>
struct UniformTraits;
template <>
struct UniformTraits<float> {
  static constexpr Kind kind = Kind::Float;
  static constexpr size_t floats = 1;
};
template <>
struct UniformTraits<glm::vec2> {
  static constexpr Kind kind = Kind::Vec2;
  static constexpr size_t floats = 2;
};
template <>
struct UniformTraits<glm::vec4> {
  static constexpr Kind kind = Kind::Vec4;
  static constexpr size_t floats = 4;
};
template <size_t N>
struct UniformTraits<std::array<float, N>> {
  static constexpr Kind kind = Kind::FloatArray;
  static constexpr size_t floats = N;
};
template <>
struct UniformTraits<Color> {
  static constexpr Kind kind = Kind::Color;
  static constexpr size_t floats = 4;
};

/** A type `UniformTraits` knows. */
template <class T>
concept Uniform = requires {
  { UniformTraits<T>::kind } -> std::convertible_to<Kind>;
  { UniformTraits<T>::floats } -> std::convertible_to<size_t>;
};

/** How many fields @p P declares. */
template <class P>
constexpr size_t fieldCount() {
  return boost::pfr::tuple_size_v<P>;
}

/** The name of field @p I of @p P, read off the type at compile time. */
template <size_t I, class P>
constexpr std::string_view fieldName() {
  return boost::pfr::get_name<I, P>();
}

namespace detail {
template <class P, class F, size_t... I>
void forEachFieldImpl(const P& params, F&& f, std::index_sequence<I...>) {
  (f(fieldName<I, P>(), boost::pfr::get<I>(params)), ...);
}
template <class P, class F, size_t... I>
void forEachFieldImpl(F&& f, std::index_sequence<I...>) {
  (f(fieldName<I, P>(), UniformTraits<boost::pfr::tuple_element_t<I, P>>::kind,
     UniformTraits<boost::pfr::tuple_element_t<I, P>>::floats),
   ...);
}
template <class P, size_t... I>
constexpr bool allUniform(std::index_sequence<I...>) {
  return (Uniform<boost::pfr::tuple_element_t<I, P>> && ...);
}
template <class P, size_t... I>
constexpr size_t floatBytes(std::index_sequence<I...>) {
  return ((sizeof(float) *
           UniformTraits<boost::pfr::tuple_element_t<I, P>>::floats) +
          ... + 0);
}
}  // namespace detail

/** Calls `f(name, value)` for each field of @p params in order. */
template <class P, class F>
void forEachField(const P& params, F&& f) {
  detail::forEachFieldImpl(params, std::forward<F>(f),
                           std::make_index_sequence<fieldCount<P>()>{});
}

/** Calls `f(name, kind, floats)` for each field of @p P in order, with no
 *  instance. */
template <class P, class F>
void forEachField(F&& f) {
  detail::forEachFieldImpl<P>(std::forward<F>(f),
                              std::make_index_sequence<fieldCount<P>()>{});
}

/** The upload layout of @p P. Refuses a struct with a field that is not a
 *  uniform type, and one whose size is not the sum of its fields — padding
 *  between fields would put bytes in the upload the shader does not
 *  declare. A struct with NO fields is a recipe with no ABI of its own —
 *  a body that reads only child slots and frame inputs — and lays out to
 *  nothing; the size rule cannot ask anything of it, because an empty
 *  aggregate occupies a byte the upload never carries. */
template <class P>
const Schema& schema() {
  static_assert(std::is_aggregate_v<P>,
                "a params struct is a plain aggregate of uniform fields");
  static_assert(
      detail::allUniform<P>(std::make_index_sequence<fieldCount<P>()>{}),
      "every params field is float, glm::vec2, glm::vec4, "
      "std::array<float, N> or Color");
  static_assert(
      fieldCount<P>() == 0 ||
          sizeof(P) == detail::floatBytes<P>(
                           std::make_index_sequence<fieldCount<P>()>{}),
      "a params struct is packed floats with no padding");
  static const Schema s = [] {
    Schema out;
    size_t offset = 0;
    forEachField<P>([&](std::string_view name, Kind kind, size_t floats) {
      out.fields.push_back({std::string(name), kind, floats, offset});
      offset += floats * sizeof(float);
    });
    out.byteSize = offset;
    return out;
  }();
  return s;
}

/** The uniform declaration of one field in @p target's syntax, with its
 *  trailing newline. */
std::string declare(const Field& field, Target target);

/** The uniform declarations of every field of @p schema, one per line. */
std::string declare(const Schema& schema, Target target);

/** The uniform declarations of @p P in @p target's syntax. */
template <class P>
std::string declare(Target target) {
  return declare(schema<P>(), target);
}

}  // namespace sigil::material
