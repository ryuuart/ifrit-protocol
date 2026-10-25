/** @file
 * `sigil.geometry.mesh.pop`: the point-operator chain an artist builds —
 * `on(...)` enters points on a closed loop, on a spline by its own
 * closure, on a formed model's faces or on another chain's result, each
 * verb appends one operation, and `instances(stamp, attributes)` or
 * `stamps(stamp)` cooks the chain on the CPU reference into what a body
 * wears. Attributes are named: an
 * `Attribute` for the conventional lanes, or any string for a lane the
 * chain invents.
 */

#include <pybind11/stl.h>
#include <sigilgeometry/mesh/Instances.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Builder.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/geometry/Registration.h>

#include <string>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace pop = geometry::mesh::pop;

namespace {

/** The lane @p value names: an `Attribute` or any string. */
pop::AttributeReference laneOf(py::handle value) {
  if (py::isinstance<pop::Attribute>(value))
    return pop::AttributeReference(value.cast<pop::Attribute>());
  return pop::AttributeReference(value.cast<std::string>());
}

}  // namespace

void bindGeometryPointBuilder(py::module_& module) {
  auto pops = submodule(module, "geometry.mesh.pop");
  submodule(module, "geometry.mesh.pop.profile");
  const auto fluent = py::return_value_policy::reference_internal;
  using pop::Builder;

  py::enum_<pop::Attribute>(pops, "Attribute")
      .value("P", pop::Attribute::P)
      .value("T", pop::Attribute::T)
      .value("Dir", pop::Attribute::Dir)
      .value("Scale", pop::Attribute::Scale)
      .value("Color", pop::Attribute::Color);

  py::class_<pop::Select> select(pops, "Select");
  py::enum_<pop::Select::Shape>(select, "Shape")
      .value("Sphere", pop::Select::Shape::Sphere)
      .value("Box", pop::Select::Shape::Box);
  py::enum_<pop::Select::Combine>(select, "Combine")
      .value("Replace", pop::Select::Combine::Replace)
      .value("Union", pop::Select::Combine::Union)
      .value("Intersect", pop::Select::Combine::Intersect)
      .value("Subtract", pop::Select::Combine::Subtract);

  py::class_<Builder> builder(pops, "Builder");
  copyProtocol(builder);
  builder.def("copy", [](const Builder& self) { return self; })
      .def("count", &Builder::count, py::arg("count"), fluent)
      .def("window", &Builder::window, py::arg("head"), py::arg("span"), fluent)
      .def("spread", &Builder::spread, py::arg("radius"), fluent)
      .def("seed", &Builder::seed, py::arg("seed"), fluent)
      .def(
          "jitter",
          [](Builder& self, float amplitude, py::handle lane) -> Builder& {
            return self.jitter(amplitude, laneOf(lane));
          },
          py::arg("amplitude"), py::arg("attribute") = pop::Attribute::P,
          fluent)
      .def(
          "noise",
          [](Builder& self, float amplitude, float frequency,
             py::handle lane) -> Builder& {
            return self.noise(amplitude, frequency, laneOf(lane));
          },
          py::arg("amplitude"), py::arg("frequency") = 0.01f,
          py::arg("attribute") = pop::Attribute::P, fluent)
      .def(
          "vary",
          [](Builder& self, float spread, float base, py::handle lane)
              -> Builder& { return self.vary(spread, base, laneOf(lane)); },
          py::arg("spread"), py::arg("base") = 1.0f,
          py::arg("attribute") = pop::Attribute::Scale, fluent)
      .def("fade", &Builder::fade, py::arg("start"), py::arg("end"), fluent)
      .def("tint", &Builder::tint, py::arg("color"), fluent)
      .def("lookAt", &Builder::lookAt, py::arg("target"), fluent)
      .def("move", &Builder::move, py::arg("offset"), fluent)
      .def(
          "fill",
          [](Builder& self, py::handle lane, glm::vec4 value) -> Builder& {
            return self.fill(laneOf(lane), value);
          },
          py::arg("attribute"), py::arg("value"), fluent)
      .def("atlas", &Builder::atlas, py::arg("columns"), py::arg("rows"),
           fluent)
      .def(
          "rampBy",
          [](Builder& self, py::handle from, int component,
             std::vector<glm::vec4> stops, float low, float high,
             py::handle to) -> Builder& {
            return self.rampBy(laneOf(from), component, std::move(stops), low,
                               high, laneOf(to));
          },
          py::arg("source"), py::arg("component"), py::arg("stops"),
          py::arg("low") = 0.0f, py::arg("high") = 1.0f,
          py::arg("to") = pop::Attribute::Color, fluent)
      .def("order", &Builder::order, py::arg("axis") = glm::vec3{0, 0, 1},
           py::arg("descending") = false, fluent)
      .def(
          "orderBy",
          [](Builder& self, py::handle by, int component,
             bool descending) -> Builder& {
            return self.orderBy(laneOf(by), component, descending);
          },
          py::arg("by"), py::arg("component") = 0,
          py::arg("descending") = false, fluent)
      .def(
          "promote",
          [](Builder& self, py::handle from, std::string to) -> Builder& {
            return self.promote(laneOf(from), std::move(to));
          },
          py::arg("source"), py::arg("to") = std::string{}, fluent)
      .def("smooth", &Builder::smooth, py::arg("strength") = 0.5f,
           py::arg("iterations") = 2, fluent)
      .def("relax", &Builder::relax, py::arg("radius"),
           py::arg("iterations") = 4, py::arg("strength") = 0.5f, fluent)
      .def("cluster", &Builder::cluster, py::arg("count"),
           py::arg("to") = std::string("cluster"), fluent)
      .def("select",
           py::overload_cast<std::string, pop::Select::Shape, glm::vec3,
                             glm::vec3, float, pop::Select::Combine, bool>(
               &Builder::select),
           py::arg("to"), py::arg("shape"), py::arg("center"), py::arg("size"),
           py::arg("feather") = 0.0f,
           py::arg("combine") = pop::Select::Combine::Replace,
           py::arg("invert") = false, fluent)
      .def("select",
           py::overload_cast<std::string, glm::vec3, float, float>(
               &Builder::select),
           py::arg("to"), py::arg("center"), py::arg("radius"),
           py::arg("feather") = 0.0f, fluent)
      .def("masked", &Builder::masked, py::arg("lane"), fluent)
      .def(
          "peak",
          [](Builder& self, float distance, py::handle along) -> Builder& {
            return self.peak(distance, laneOf(along));
          },
          py::arg("distance"), py::arg("along") = pop::Attribute::Dir, fluent)
      .def(
          "twist",
          [](Builder& self, AngleInDegrees degrees, glm::vec3 axis, float low,
             float high, glm::vec3 origin) -> Builder& {
            return self.twist(degrees.value, axis, low, high, origin);
          },
          py::arg("degrees"),
          py::arg("axis") = glm::vec3{0, 1, 0}, py::arg("low") = 0.0f,
          py::arg("high") = 100.0f, py::arg("origin") = glm::vec3{0, 0, 0},
          fluent)
      .def("taper", &Builder::taper, py::arg("scale"),
           py::arg("axis") = glm::vec3{0, 1, 0}, py::arg("low") = 0.0f,
           py::arg("high") = 100.0f, py::arg("origin") = glm::vec3{0, 0, 0},
           fluent)
      .def(
          "bend",
          [](Builder& self, AngleInDegrees degrees, glm::vec3 axis,
             glm::vec3 direction, float low, float high,
             glm::vec3 origin) -> Builder& {
            return self.bend(degrees.value, axis, direction, low, high, origin);
          },
          py::arg("degrees"),
          py::arg("axis") = glm::vec3{0, 1, 0},
          py::arg("direction") = glm::vec3{1, 0, 0}, py::arg("low") = 0.0f,
          py::arg("high") = 100.0f, py::arg("origin") = glm::vec3{0, 0, 0},
          fluent)
      .def(
          "mix",
          [](Builder& self, py::handle first, py::handle second, py::handle to,
             float factor) -> Builder& {
            return self.mix(laneOf(first), laneOf(second), laneOf(to), factor);
          },
          py::arg("first"), py::arg("second"), py::arg("to"),
          py::arg("factor") = 0.5f, fluent)
      .def(
          "mixBy",
          [](Builder& self, py::handle first, py::handle second, py::handle to,
             std::string factorLane) -> Builder& {
            return self.mixBy(laneOf(first), laneOf(second), laneOf(to),
                              std::move(factorLane));
          },
          py::arg("first"), py::arg("second"), py::arg("to"),
          py::arg("factorLane"), fluent)
      .def(
          "copy",
          [](Builder& self, py::handle from, py::handle to) -> Builder& {
            return self.copy(laneOf(from), laneOf(to));
          },
          py::arg("source"), py::arg("to"), fluent)
      .def("drop", &Builder::drop, py::arg("mask"), py::arg("threshold") = 0.5f,
           fluent)
      .def("keep", &Builder::keep, py::arg("mask"), py::arg("threshold") = 0.5f,
           fluent)
      .def(
          "normal",
          [](Builder& self, float sense, glm::vec3 center, py::handle lane)
              -> Builder& { return self.normal(sense, center, laneOf(lane)); },
          py::arg("sense") = 0.0f, py::arg("center") = glm::vec3{0, 0, 0},
          py::arg("attribute") = pop::Attribute::Dir, fluent)
      .def(
          "stamps",
          [](const Builder& self, const geometry::mesh::Mesh& stamp) {
            return self.stamps(stamp);
          },
          py::arg("stamp"))
      .def(
          "instances",
          [](const Builder& self, const geometry::mesh::Mesh& stamp,
             std::vector<std::string> attributes) {
            return self.instances(stamp, std::move(attributes));
          },
          py::arg("stamp"), py::arg("attributes") = std::vector<std::string>{});

  pops.def(
      "on",
      [](std::vector<glm::vec3> loop) { return pop::on(std::move(loop)); },
      py::arg("loop"));
  // An open spine is ridden along its own length only; a window at its
  // end never crosses the chord back to its start, as a loop's would.
  pops.def(
      "on",
      [](geometry::mesh::curve::Spline3 spine) {
        return pop::on(std::move(spine));
      },
      py::arg("spine"));
  pops.def(
      "on",
      [](geometry::mesh::Mesh surface, int count) {
        return pop::on(std::move(surface), count);
      },
      py::arg("surface"), py::arg("count") = 10000);
  pops.def(
      "on", [](const Builder& upstream) { return pop::on(upstream.chain()); },
      py::arg("upstream"));
}

}  // namespace sigil::python
