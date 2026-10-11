#include <include/core/SkTypeface.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/weave/Registration.h>
#include <sigilweave/advanced/Skia.h>
#include <sigilweave/style/Face.h>

#include <string>

namespace sigil::python {

namespace py = pybind11;

void bindWeaveFonts(py::module_& module) {
  auto weaveModule = module.attr("weave").cast<py::module_>();
  // One axis a face varies along: its tag and the coordinates it takes.
  py::class_<weave::VariationAxis>(
      weaveModule, "VariationAxis",
      "One variation axis a face carries: its four-character tag, the "
      "least, default and greatest coordinates it takes, and whether the "
      "font asks for it to stay out of a user's choices.")
      .def_property_readonly("tag",
                             [](const weave::VariationAxis& axis) {
                               return std::string(axis.tag, 4);
                             })
      .def_readonly("minimum", &weave::VariationAxis::minimum)
      .def_readonly("defaultValue", &weave::VariationAxis::defaultValue)
      .def_readonly("maximum", &weave::VariationAxis::maximum)
      .def_readonly("hidden", &weave::VariationAxis::hidden)
      .def(py::self == py::self);
  // The typeface class is the face's own, registered with Skia's values;
  // its axes are asked of it here.
  py::reinterpret_borrow<py::class_<SkTypeface, sk_sp<SkTypeface>>>(
      py::type::of<SkTypeface>())
      .def(
          "variationAxes",
          [](const sk_sp<SkTypeface>& face) {
            return weave::Face(face).variationAxes();
          },
          "The variation axes the face carries, in the font's order; empty "
          "for a face that varies along none.");
}

}  // namespace sigil::python
