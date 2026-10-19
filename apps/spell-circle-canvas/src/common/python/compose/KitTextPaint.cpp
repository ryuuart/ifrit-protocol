/** @file
 * `sigil.compose.kit`'s text paints: `TextField`, the procedural fields a
 * run of type is inked with, `TextPaint`, the field, moment and box one
 * paint is made of, and `textPaint`, the material it describes.
 */

#include <pybind11/stl.h>
#include <sigilcompose/kit/TextPaint.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>

namespace sigil::python {
namespace py = pybind11;
namespace composeKit = compose::kit;

namespace {

/** The box @p value names: a geometry rectangle, or anything a Skia
 *  rectangle is read from. */
geometry::path::Rect boxOf(py::handle value) {
  if (py::isinstance<geometry::path::Rect>(value))
    return value.cast<geometry::path::Rect>();
  const SkRect bounds = rect(value);
  return {{bounds.left(), bounds.top()}, {bounds.right(), bounds.bottom()}};
}

}  // namespace

void bindComposeKitTextPaint(py::module_& root) {
  auto module = root.attr("compose").attr("kit").cast<py::module_>();
  using composeKit::TextField;
  py::enum_<TextField>(module, "TextField",
                       "The field a text paint fills its box with.")
      .value("Water", TextField::Water)
      .value("MeshGradient", TextField::MeshGradient)
      .value("Sparkle", TextField::Sparkle)
      .value("Stars", TextField::Stars)
      .value("Clouds", TextField::Clouds)
      .value("Tunnel", TextField::Tunnel);

  auto paint = bindRecord<composeKit::TextPaint>(module, "TextPaint",
                                                 "Unknown kit property: ");
  paint.doc() =
      "How a text paint is made: which field, at what moment in seconds, "
      "over what box. A time that is not a constant is bound, and the "
      "field drifts with it.";
  paint.def_readwrite("field", &composeKit::TextPaint::field)
      .def_property(
          "time",
          [](const composeKit::TextPaint& self) { return self.time; },
          [](composeKit::TextPaint& self, py::handle value) {
            self.time = motionAnimatable(value);
          })
      .def_property(
          "box", [](const composeKit::TextPaint& self) { return self.box; },
          [](composeKit::TextPaint& self, py::handle value) {
            self.box = boxOf(value);
          });

  module.def(
      "textPaint",
      [](TextField field, py::handle time, py::object box) {
        composeKit::TextPaint paint{.field = field,
                                    .time = motionAnimatable(time)};
        if (!box.is_none()) paint.box = boxOf(box);
        return composeKit::textPaint(paint);
      },
      py::kw_only(), py::arg("field") = TextField::Water,
      py::arg("time") = 0.0f, py::arg("box") = py::none(),
      "The text paint as a material: the field's program over the box — the "
      "unit square an ink stretches over its passage when none is named — "
      "at the moment, drifting with it where the time is bound.");
  module.def(
      "textPaint",
      [](const composeKit::TextPaint& paint) {
        return composeKit::textPaint(paint);
      },
      py::arg("paint"));
}

}  // namespace sigil::python
