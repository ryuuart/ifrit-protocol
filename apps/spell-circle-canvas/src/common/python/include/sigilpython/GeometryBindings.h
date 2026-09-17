#pragma once

#include <pybind11/pybind11.h>

#include <glm/glm.hpp>

namespace pybind11::detail {
template <glm::length_t N, glm::qualifier Q>
struct type_caster<glm::vec<N, float, Q>> {
  using Value = glm::vec<N, float, Q>;
  PYBIND11_TYPE_CASTER(Value, const_name("tuple"));
  bool load(handle input, bool) {
    if (!isinstance<sequence>(input)) return false;
    const auto seq = reinterpret_borrow<sequence>(input);
    if (seq.size() != N) return false;
    for (glm::length_t i = 0; i < N; ++i)
      value[i] = pybind11::cast<float>(seq[i]);
    return true;
  }
  static handle cast(const Value& input, return_value_policy, handle) {
    tuple result(N);
    for (glm::length_t i = 0; i < N; ++i)
      result[i] = pybind11::float_(input[i]);
    return result.release();
  }
};
}  // namespace pybind11::detail

namespace sigil::python {
void bindGeometry(pybind11::module_& module);
}  // namespace sigil::python
