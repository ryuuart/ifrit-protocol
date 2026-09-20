#pragma once

/** @file
 * Binding geometry, with the casters that let a Python sequence
 * stand for a vector and a matrix.
 */

#include <pybind11/pybind11.h>

#include <glm/glm.hpp>

#include <type_traits>

namespace pybind11::detail {

/** A sequence of exactly N numbers stands for a glm vector of N
 *  components, and a vector is answered as a tuple. The components are
 *  floats, or the signed and unsigned whole numbers an index pair and a
 *  kernel's dispatch size are counted in; a component of any other type
 *  keeps whatever reading pybind11 gives it. */
template <glm::length_t N, typename T, glm::qualifier Q>
struct type_caster<
    glm::vec<N, T, Q>,
    std::enable_if_t<std::is_same_v<T, float> || std::is_same_v<T, int> ||
                     std::is_same_v<T, unsigned int>>> {
  using Value = glm::vec<N, T, Q>;
  PYBIND11_TYPE_CASTER(Value, const_name("tuple"));
  bool load(handle input, bool) {
    if (!isinstance<sequence>(input)) return false;
    const auto seq = reinterpret_borrow<sequence>(input);
    if (seq.size() != N) return false;
    for (glm::length_t i = 0; i < N; ++i)
      value[i] = pybind11::cast<T>(seq[i]);
    return true;
  }
  static handle cast(const Value& input, return_value_policy, handle) {
    tuple result(N);
    for (glm::length_t i = 0; i < N; ++i)
      result[i] = pybind11::cast(input[i]);
    return result.release();
  }
};

/** A ROTATION IS COUNTED BY COLUMNS, as glm counts one and as the
 *  camera's matrix is: three sequences of three numbers, each where one
 *  of the axes points, and a tuple of three tuples on the way out. Only
 *  the three-by-three matrix has a caster, because the four-by-four one
 *  is a bound class with verbs of its own. */
template <glm::qualifier Q>
struct type_caster<glm::mat<3, 3, float, Q>> {
  using Value = glm::mat<3, 3, float, Q>;
  using Column = glm::vec<3, float, Q>;
  static constexpr glm::length_t kColumns = 3;
  PYBIND11_TYPE_CASTER(Value, const_name("tuple"));
  bool load(handle input, bool convert) {
    if (!isinstance<sequence>(input)) return false;
    const auto columns = reinterpret_borrow<sequence>(input);
    if (columns.size() != kColumns) return false;
    for (glm::length_t index = 0; index < kColumns; ++index) {
      const object item = columns[index];
      make_caster<Column> column;
      if (!column.load(item, convert)) return false;
      value[index] = cast_op<Column&>(column);
    }
    return true;
  }
  static handle cast(const Value& input, return_value_policy, handle) {
    tuple result(kColumns);
    for (glm::length_t index = 0; index < kColumns; ++index)
      result[index] = pybind11::cast(input[index]);
    return result.release();
  }
};

}  // namespace pybind11::detail
