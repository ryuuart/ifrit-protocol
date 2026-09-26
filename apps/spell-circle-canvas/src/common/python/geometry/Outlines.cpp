#include <include/core/SkPath.h>
#include <pybind11/functional.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilgeometry/path/Offset.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Points.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Radial.h>
#include <sigilgeometry/path/Skia.h>
#include <sigilgeometry/path/Through.h>
#include <sigilgeometry/path/Transform.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/geometry/Registration.h>

#include <vector>

namespace sigil::python {

namespace {

namespace path = geometry::path;

/** A width law from Python: a number is the constant width, a list of
 *  `(along, width)` pairs a law through stops. */
path::Profile widthOf(pybind11::handle value) {
  namespace py = pybind11;
  if (py::isinstance<py::float_>(value) || py::isinstance<py::int_>(value))
    return path::Profile(value.cast<float>());
  std::vector<path::Stop> stops;
  for (py::handle stop : value.cast<py::sequence>()) {
    const auto pair = stop.cast<std::pair<float, float>>();
    stops.push_back({pair.first, pair.second});
  }
  return path::Profile(std::move(stops));
}

}  // namespace

void bindGeometryOutlines(pybind11::module_& module) {
  namespace py = pybind11;
  py::module_ paths = submodule(module, "geometry.path");

  py::enum_<path::FillRule>(paths, "FillRule")
      .value("NonZero", path::FillRule::NonZero)
      .value("EvenOdd", path::FillRule::EvenOdd);
  py::enum_<path::Winding>(paths, "Winding")
      .value("OutersClockwise", path::Winding::OutersClockwise)
      .value("OutersCounterClockwise", path::Winding::OutersCounterClockwise);
  py::enum_<path::Wrap>(paths, "Wrap")
      .value("Clamp", path::Wrap::Clamp)
      .value("Around", path::Wrap::Around);

  auto rect = bindRecord<path::Rect>(paths, "Rect", "Unknown rectangle field: ");
  rect.def_readwrite("min", &path::Rect::min)
      .def_readwrite("max", &path::Rect::max)
      .def_static("of", &path::Rect::of, py::arg("origin"), py::arg("size"))
      .def_static("centredOn", &path::Rect::centredOn, py::arg("centre"),
                  py::arg("size"))
      .def("width", &path::Rect::width)
      .def("height", &path::Rect::height)
      .def("size", &path::Rect::size)
      .def("centre", &path::Rect::centre)
      .def("empty", &path::Rect::empty)
      .def(py::self == py::self);

  auto pose = bindRecord<path::Pose>(paths, "Pose", "Unknown pose field: ");
  pose.def_readwrite("position", &path::Pose::position)
      .def_readwrite("tangent", &path::Pose::tangent)
      .def_readwrite("normal", &path::Pose::normal)
      .def_readwrite("distance", &path::Pose::distance)
      .def(py::self == py::self);
  auto nearest =
      bindRecord<path::Nearest>(paths, "Nearest", "Unknown nearest field: ");
  nearest.def_readwrite("distance", &path::Nearest::distance)
      .def_readwrite("position", &path::Nearest::position)
      .def_readwrite("gap", &path::Nearest::gap)
      .def(py::self == py::self);

  py::class_<path::Transform> transform(paths, "Transform");
  transform.def(py::init<>())
      .def_static("translate", &path::Transform::translate, py::arg("offset"))
      .def_static("rotate", &path::Transform::rotate, py::arg("degrees"),
                  py::arg("about") = glm::vec2{0, 0})
      .def_static("scale", &path::Transform::scale, py::arg("factors"),
                  py::arg("about") = glm::vec2{0, 0})
      .def_static("skew", &path::Transform::skew, py::arg("degrees"))
      .def_static("fit", &path::Transform::fit, py::arg("source"),
                  py::arg("target"), py::arg("preserveAspect") = false)
      .def("__call__", &path::Transform::operator(), py::arg("point"))
      .def("__mul__", &path::Transform::operator*, py::arg("first"))
      .def("inverse", &path::Transform::inverse)
      .def(py::self == py::self);
  copyProtocol(transform);

  py::class_<path::Outline> outline(paths, "Outline");
  outline.def(py::init<>())
      .def_static("svg", &path::Outline::svg, py::arg("data"))
      .def_static("rectangle", &path::Outline::rectangle, py::arg("rect"))
      .def("empty", &path::Outline::empty)
      .def("fillRule", &path::Outline::fillRule)
      .def("withFillRule", &path::Outline::withFillRule, py::arg("rule"))
      .def("bounds", &path::Outline::bounds)
      .def("length", &path::Outline::length)
      .def("pointAt", &path::Outline::pointAt, py::arg("distance"),
           py::arg("wrap") = path::Wrap::Clamp)
      .def("tangentAt", &path::Outline::tangentAt, py::arg("distance"),
           py::arg("wrap") = path::Wrap::Clamp)
      .def("normalAt", &path::Outline::normalAt, py::arg("distance"),
           py::arg("wrap") = path::Wrap::Clamp)
      .def("poseAt", &path::Outline::poseAt, py::arg("distance"),
           py::arg("wrap") = path::Wrap::Clamp)
      .def("segment", &path::Outline::segment, py::arg("start"),
           py::arg("end"))
      .def("split", &path::Outline::split, py::arg("distance"))
      .def("nearest", &path::Outline::nearest, py::arg("point"))
      .def("contains", &path::Outline::contains, py::arg("point"))
      .def("area", &path::Outline::area)
      .def("winding", &path::Outline::winding)
      .def("united", &path::Outline::united, py::arg("other"))
      .def("subtracted", &path::Outline::subtracted, py::arg("other"))
      .def("intersected", &path::Outline::intersected, py::arg("other"))
      .def("excluded", &path::Outline::excluded, py::arg("other"))
      .def("simplified", &path::Outline::simplified)
      .def("reversed", &path::Outline::reversed)
      .def("joined", &path::Outline::joined, py::arg("other"))
      .def("transformed", &path::Outline::transformed, py::arg("transform"))
      .def(
          "resampled",
          [](const path::Outline& self, int count, float spacing,
             float tolerance) {
            std::vector<std::vector<glm::vec2>> contours;
            for (const path::Polyline& line : self.resampled(
                     {.count = count, .spacing = spacing, .tolerance = tolerance}))
              contours.push_back(line.points);
            return contours;
          },
          py::arg("count") = 0, py::arg("spacing") = 0.0f,
          py::arg("tolerance") = 0.25f)
      .def(py::self == py::self);
  copyProtocol(outline);

  // The crossing to the Skia path a drawing takes, and back.
  paths
      .def(
          "toSk", [](const path::Outline& value) { return path::toSk(value); },
          py::arg("outline"))
      .def(
          "fromSk", [](const SkPath& value) { return path::fromSk(value); },
          py::arg("path"));

  py::enum_<path::Smooth>(paths, "Smooth")
      .value("None_", path::Smooth::None)
      .value("CatmullRom", path::Smooth::CatmullRom)
      .value("Midpoint", path::Smooth::Midpoint)
      .value("Fit", path::Smooth::Fit);
  paths
      .def(
          "through",
          [](const std::vector<glm::vec2>& points, path::Smooth smooth,
             float tolerance, bool closed) {
            return path::through(points, {.smooth = smooth,
                                          .tolerance = tolerance,
                                          .closed = closed});
          },
          py::arg("points"), py::arg("smooth") = path::Smooth::None,
          py::arg("tolerance") = 1.0f, py::arg("closed") = false)
      .def(
          "curveThrough",
          [](const std::vector<glm::vec2>& points, bool closed) {
            return path::curveThrough(points, closed);
          },
          py::arg("points"), py::arg("closed") = false);

  paths
      .def(
          "offset",
          [](const path::Outline& source, py::handle width, bool region,
             path::Join join, float miterLimit) {
            return path::offset(source, widthOf(width),
                                {.join = join,
                                 .miterLimit = miterLimit,
                                 .region = region});
          },
          py::arg("outline"), py::arg("width"), py::arg("region") = false,
          py::arg("join") = path::Join::Round, py::arg("miterLimit") = 4.0f)
      .def(
          "band",
          [](const path::Outline& spine, py::handle width,
             path::Formation side) {
            return path::band(spine, widthOf(width), {.side = side});
          },
          py::arg("spine"), py::arg("width"),
          py::arg("side") = path::Formation::Center);

  py::enum_<path::Connect>(paths, "Connect")
      .value("Loop", path::Connect::Loop)
      .value("Each", path::Connect::Each)
      .value("None_", path::Connect::None);
  py::enum_<path::Growth>(paths, "Growth")
      .value("None_", path::Growth::None)
      .value("Linear", path::Growth::Linear)
      .value("SquareRoot", path::Growth::SquareRoot);
  py::class_<path::Mark> mark(paths, "Mark");
  mark.def(py::init<>())
      .def_static("line", &path::Mark::line, py::arg("inner") = 0.92f,
                  py::arg("outer") = 1.0f)
      .def_static("segment", &path::Mark::segment, py::arg("spanDegrees"),
                  py::arg("inner") = 0.88f, py::arg("outer") = 1.0f)
      .def_static("bar", &path::Mark::bar, py::arg("width"),
                  py::arg("inner") = 0.92f, py::arg("outer") = 1.0f)
      .def_static("shape", &path::Mark::shape, py::arg("figure"))
      .def_readwrite("inner", &path::Mark::inner)
      .def_readwrite("outer", &path::Mark::outer)
      .def_readwrite("spanDegrees", &path::Mark::spanDegrees)
      .def_readwrite("width", &path::Mark::width)
      .def(py::self == py::self);
  copyProtocol(mark);

  auto radial = bindRecord<path::RadialOptions>(paths, "RadialOptions",
                                                "Unknown radial option: ");
  radial.def_readwrite("radii", &path::RadialOptions::radii)
      .def_readwrite("fromDegrees", &path::RadialOptions::fromDegrees)
      .def_readwrite("sweepDegrees", &path::RadialOptions::sweepDegrees)
      .def_readwrite("stepDegrees", &path::RadialOptions::stepDegrees)
      .def_readwrite("closed", &path::RadialOptions::closed)
      .def_readwrite("skip", &path::RadialOptions::skip)
      .def_readwrite("connect", &path::RadialOptions::connect)
      .def_readwrite("marks", &path::RadialOptions::marks)
      .def_readwrite("growth", &path::RadialOptions::growth)
      .def_readwrite("inset", &path::RadialOptions::inset)
      .def_readwrite("waist", &path::RadialOptions::waist)
      .def_readwrite("uniform", &path::RadialOptions::uniform)
      .def(py::self == py::self);

  py::class_<path::Pattern> pattern(paths, "Pattern");
  pattern.def(py::self == py::self);
  copyProtocol(pattern);
  paths
      .def("random", &path::random, py::arg("count"), py::arg("seed") = 1)
      .def("poisson", &path::poisson, py::arg("radius"), py::arg("seed") = 1)
      .def(
          "grid",
          [](float spacing, float jitter, uint64_t seed) {
            return path::grid(spacing, {.jitter = jitter, .seed = seed});
          },
          py::arg("spacing"), py::arg("jitter") = 0.0f, py::arg("seed") = 1)
      .def("radial", &path::radial, py::arg("count"),
           py::arg("options") = path::RadialOptions{})
      .def(
          "along",
          [](float spacing, float from, float to) {
            return path::along(spacing, {.from = from, .to = to});
          },
          py::arg("spacing"), py::arg("start") = 0.0f, py::arg("end") = -1.0f)
      .def(
          "points",
          [](const path::Outline& where, const path::Pattern& pattern) {
            return path::points(where, pattern);
          },
          py::arg("where"), py::arg("pattern"))
      .def(
          "points",
          [](const path::Rect& where, const path::Pattern& pattern) {
            return path::points(where, pattern);
          },
          py::arg("where"), py::arg("pattern"))
      .def("heading", &path::heading, py::arg("vector"));
}

}  // namespace sigil::python
