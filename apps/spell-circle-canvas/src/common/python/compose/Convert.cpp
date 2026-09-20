/** @file
 * The readings the compose bindings share: a dimension, a fill, a paint,
 * an alignment, a justification, a shape and a list of children, each
 * taken from the forms Python spells it in.
 */

#include <pybind11/stl.h>
#include <sigilcompose/core/Factories.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

compose::Dimension dimension(py::handle value) {
  if (py::isinstance<compose::Dimension>(value))
    return value.cast<compose::Dimension>();
  if (py::isinstance<weave::Length>(value)) return value.cast<weave::Length>();
  if (py::isinstance<compose::VarRef>(value))
    return value.cast<compose::VarRef>();
  if (!py::isinstance<py::str>(value))
    return compose::Dimension{value.cast<float>()};
  const auto text = value.cast<std::string>();
  if (text == "auto") return compose::autoDimension();
  if (text.size() > 1 && text.back() == '%') {
    std::size_t end = 0;
    const float amount = std::stof(text, &end);
    if (end == text.size() - 1 && std::isfinite(amount))
      return compose::pct(amount);
  }
  throw py::value_error(
      "A dimension is a native Dimension or Length, number, percentage string, "
      "or 'auto'.");
}

compose::Fill fill(py::handle value) {
  if (value.is_none()) return compose::Fill::none();
  if (py::isinstance<compose::Fill>(value)) return value.cast<compose::Fill>();
  if (py::isinstance<compose::VarRef>(value))
    return compose::Fill::var(value.cast<compose::VarRef>());
  if (py::isinstance<material::skia::Paint>(value)) {
    // A flat mark holds one comparable Fill and is measured with no
    // frame in hand, so a static paint collapses onto it while a live or
    // geometry-dependent one has no single colour to give.
    const auto paint = value.cast<material::skia::Paint>();
    if (paint.isAnimated() || paint.geometryDependent())
      throw py::type_error(
          "A live or geometry-dependent paint is not a flat fill. Give it to "
          "a verb that resolves against the frame it paints at, such as "
          "Element.fill or Element.textFill.");
    return compose::toFill(paint);
  }
  if (py::isinstance<material::Material>(value))
    throw py::type_error(
        "A material is not a flat fill. Give it to a verb that takes a "
        "surface paint, such as Element.fill.");
  return compose::Fill::color(color(value));
}

compose::SurfacePaint surfacePaint(py::handle value) {
  if (py::isinstance<compose::SurfacePaint>(value))
    return value.cast<compose::SurfacePaint>();
  if (py::isinstance<material::skia::Paint>(value))
    return value.cast<material::skia::Paint>();
  if (py::isinstance<material::Material>(value))
    return value.cast<material::Material>();
  return compose::SurfacePaint{motionFill(value)};
}

compose::Align alignment(py::handle value) {
  if (py::isinstance<compose::Align>(value))
    return value.cast<compose::Align>();
  const auto name = value.cast<std::string>();
  if (name == "auto") return compose::Align::Auto;
  if (name == "start") return compose::Align::Start;
  if (name == "center") return compose::Align::Center;
  if (name == "end") return compose::Align::End;
  if (name == "stretch") return compose::Align::Stretch;
  if (name == "baseline") return compose::Align::Baseline;
  throw py::value_error("Unknown alignment: " + name);
}

compose::Justify justification(py::handle value) {
  if (py::isinstance<compose::Justify>(value))
    return value.cast<compose::Justify>();
  const auto name = value.cast<std::string>();
  if (name == "start") return compose::Justify::Start;
  if (name == "center") return compose::Justify::Center;
  if (name == "end") return compose::Justify::End;
  if (name == "space_between") return compose::Justify::SpaceBetween;
  if (name == "space_around") return compose::Justify::SpaceAround;
  if (name == "space_evenly") return compose::Justify::SpaceEvenly;
  throw py::value_error("Unknown justification: " + name);
}

compose::Shape shape(py::handle value) {
  if (py::isinstance<compose::Shape>(value))
    return value.cast<compose::Shape>();
  if (PyCallable_Check(value.ptr())) {
    auto callback = retainCallback(py::reinterpret_borrow<py::function>(value));
    return compose::Shape{[callback](SkSize size) {
      const py::gil_scoped_acquire lock;
      const CallbackBoundary boundary;
      try {
        return callback->get()(size.width(), size.height()).cast<SkPath>();
      } catch (const py::error_already_set& error) {
        throw std::runtime_error(error.what());
      }
    }};
  }
  return compose::heldPath(value.cast<SkPath>());
}

std::vector<compose::Element> elements(py::args values) {
  try {
    if (values.size() == 1 && (py::isinstance<py::str>(values[0]) ||
                               py::isinstance<py::bytes>(values[0])))
      throw py::cast_error();
    const auto children =
        values.size() == 1 && !py::isinstance<compose::Element>(values[0])
            ? py::tuple(values[0])
            : py::tuple(values);
    return children.cast<std::vector<compose::Element>>();
  } catch (const py::cast_error&) {
    throw py::type_error(
        "children expects Elements or one iterable of Elements");
  }
}
}  // namespace sigil::python
