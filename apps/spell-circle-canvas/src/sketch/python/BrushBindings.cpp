/** @file
 * Python access to native natural-media tools, sampled geometry and engines.
 */

#include "BrushBindings.h"

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigildraw/brush/Brush.h>
#include <sigildraw/brush/format/Load.h>
#include <sigildraw/brush/format/Photoshop.h>
#include <sigildraw/brush/format/Procreate.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "Bindings.h"
#include "ValueBindings.h"

namespace sigil::sketch::python {
namespace py = pybind11;
namespace brush = draw::brush;

namespace {

/** Keyword construction uses the same bound setters as later edits, so tuple,
 *  color and callable conversion has one owner. The temporary is a native
 *  value wrapper; copying it preserves the native description's ownership. */
template <class T>
py::class_<T> record(py::module_& module, const char* name) {
  return py::class_<T>(module, name)
      .def(py::init([](py::kwargs fields) {
        py::object value = py::cast(T{});
        for (auto item : fields) {
          const auto name = py::cast<std::string>(item.first);
          if (!py::hasattr(value, name.c_str()))
            throw py::type_error("Unknown brush property: " + name);
          py::setattr(value, name.c_str(), item.second);
        }
        return value.cast<T>();
      }))
      .def("copy", [](const T& value) { return value; });
}

std::function<float(float)> scalarCurve(py::handle value) {
  if (value.is_none()) return {};
  if (!PyCallable_Check(value.ptr()))
    throw py::type_error("A brush curve or field must be callable");
  auto held = retainCallback(py::reinterpret_borrow<py::function>(value));
  return [held](float x) {
    const py::gil_scoped_acquire lock;
    try {
      return held->get()(x).template cast<float>();
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

template <class T>
struct Optional : std::false_type {};
template <class T>
struct Optional<std::optional<T>> : std::true_type {};

template <class T, class M>
void field(py::class_<T>& type, const char* name, M T::* member) {
  if constexpr (Optional<M>::value) {
    // Replacing or resetting an optional destroys its payload. Return an owned
    // copy so Python references cannot outlive that payload's storage.
    type.def_property(
        name, [member](const T& value) -> M { return value.*member; },
        [member](T& value, M input) { value.*member = std::move(input); });
  } else {
    type.def_property(
        name, [member](T& value) -> M& { return value.*member; },
        [member](T& value, py::handle input) {
          if constexpr (std::is_same_v<M, SkPoint>)
            value.*member = point(input);
          else if constexpr (std::is_same_v<M, SkColor4f>)
            value.*member = color(input);
          else if constexpr (std::is_same_v<M, std::function<float(float)>>)
            value.*member = scalarCurve(input);
          else
            value.*member = py::cast<M>(input);
        },
        py::return_value_policy::reference_internal);
  }
}

std::vector<SkPoint> points(py::iterable values) {
  std::vector<SkPoint> result;
  for (auto value : values) result.push_back(point(value));
  return result;
}

brush::Stroke samples(py::iterable values) {
  brush::Stroke result;
  for (auto value : values) {
    if (py::isinstance<brush::Sample>(value)) {
      result.push_back(py::cast<brush::Sample>(value));
      continue;
    }
    const auto sample = py::reinterpret_borrow<py::sequence>(value);
    if (sample.size() == 3)
      result.push_back(
          {{py::cast<float>(sample[0]), py::cast<float>(sample[1])},
           py::cast<float>(sample[2])});
    else if (sample.size() == 2 && (py::isinstance<py::tuple>(sample[0]) ||
                                    py::isinstance<py::list>(sample[0]) ||
                                    py::isinstance<SkPoint>(sample[0])))
      result.push_back({point(sample[0]), py::cast<float>(sample[1])});
    else
      result.push_back({point(value), 1.0f});
  }
  return result;
}

struct Direction {
  brush::Direction value;
};

brush::Direction direction(py::handle value) {
  if (value.is_none()) return {};
  if (py::isinstance<Direction>(value)) return py::cast<Direction>(value).value;
  if (py::isinstance<brush::Curl>(value)) return py::cast<brush::Curl>(value);
  if (py::isinstance<brush::Vortex>(value))
    return py::cast<brush::Vortex>(value);
  if (py::isinstance<brush::Wave>(value)) return py::cast<brush::Wave>(value);
  if (!PyCallable_Check(value.ptr()))
    throw py::type_error("A brush curve or field must be callable");
  auto held = retainCallback(py::reinterpret_borrow<py::function>(value));
  return [held](SkPoint at, float seconds) {
    const py::gil_scoped_acquire lock;
    try {
      return held->get()(at, seconds).template cast<float>();
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

std::optional<brush::Tool> toolCopy(const brush::Tool* value) {
  return value ? std::optional(*value) : std::nullopt;
}

std::span<const std::byte> bytes(const std::string& value) {
  return std::as_bytes(std::span(value.data(), value.size()));
}

}  // namespace

void bindBrush(py::module_& root) {
  auto drawing = root.attr("draw").cast<py::module_>();
  auto module = drawing.def_submodule("brush");
  py::enum_<brush::Tip>(module, "Tip")
      .value("Dust", brush::Tip::Dust)
      .value("Fibres", brush::Tip::Fibres)
      .value("Nib", brush::Tip::Nib)
      .value("Scatter", brush::Tip::Scatter)
      .value("Image", brush::Tip::Image)
      .value("Custom", brush::Tip::Custom);
  py::enum_<brush::Rotation>(module, "Rotation")
      .value("Fixed", brush::Rotation::Fixed)
      .value("Natural", brush::Rotation::Natural)
      .value("Random", brush::Rotation::Random)
      .value("Tilt", brush::Rotation::Tilt);
  py::enum_<brush::Drive>(module, "Drive")
      .value("Pressure", brush::Drive::Pressure)
      .value("Velocity", brush::Drive::Velocity)
      .value("Tilt", brush::Drive::Tilt);
  py::enum_<brush::ImageMask>(module, "ImageMask")
      .value("InvertedLuminance", brush::ImageMask::InvertedLuminance)
      .value("Alpha", brush::ImageMask::Alpha);
  py::enum_<brush::GrainSpace>(module, "GrainSpace")
      .value("Stroke", brush::GrainSpace::Stroke)
      .value("Dab", brush::GrainSpace::Dab);
  py::enum_<brush::BleedDirection>(module, "BleedDirection")
      .value("Out", brush::BleedDirection::Out)
      .value("In", brush::BleedDirection::In);
  py::enum_<brush::PlotType>(module, "PlotType")
      .value("Curve", brush::PlotType::Curve)
      .value("Segments", brush::PlotType::Segments);
  auto gaussian = record<brush::Pressure::Gaussian>(module, "Gaussian");
  field(gaussian, "center", &brush::Pressure::Gaussian::center);
  field(gaussian, "width", &brush::Pressure::Gaussian::width);
  field(gaussian, "sharpness", &brush::Pressure::Gaussian::sharpness);
  field(gaussian, "minimum", &brush::Pressure::Gaussian::minimum);
  field(gaussian, "maximum", &brush::Pressure::Gaussian::maximum);
  field(gaussian, "centerJitter", &brush::Pressure::Gaussian::centerJitter);
  field(gaussian, "widthJitter", &brush::Pressure::Gaussian::widthJitter);
  auto variation = record<brush::Pressure::Variation>(module, "Variation");
  field(variation, "offset", &brush::Pressure::Variation::offset);
  field(variation, "scale", &brush::Pressure::Variation::scale);
  field(variation, "warp", &brush::Pressure::Variation::warp);
  field(variation, "tilt", &brush::Pressure::Variation::tilt);
  auto pressure = record<brush::Pressure>(module, "Pressure");
  field(pressure, "start", &brush::Pressure::start);
  field(pressure, "middle", &brush::Pressure::middle);
  field(pressure, "end", &brush::Pressure::end);
  field(pressure, "curve", &brush::Pressure::curve);
  field(pressure, "gaussian", &brush::Pressure::gaussian);
  field(pressure, "variation", &brush::Pressure::variation);
  auto curve = record<brush::Curve>(module, "Curve");
  field(curve, "minimum", &brush::Curve::minimum);
  field(curve, "maximum", &brush::Curve::maximum);
  field(curve, "bend", &brush::Curve::bend);
  field(curve, "curve", &brush::Curve::curve);
  auto response = record<brush::Response>(module, "Response");
  field(response, "drive", &brush::Response::drive);
  field(response, "curve", &brush::Response::curve);
  auto dynamics = record<brush::Dynamics>(module, "Dynamics");
  field(dynamics, "size", &brush::Dynamics::size);
  field(dynamics, "opacity", &brush::Dynamics::opacity);
  field(dynamics, "flow", &brush::Dynamics::flow);
  auto shape = record<brush::Shape>(module, "Shape");
  field(shape, "image", &brush::Shape::image);
  field(shape, "mask", &brush::Shape::mask);
  field(shape, "spacing", &brush::Shape::spacing);
  field(shape, "scatter", &brush::Shape::scatter);
  field(shape, "angleJitter", &brush::Shape::angleJitter);
  auto grain = record<brush::Grain>(module, "Grain");
  field(grain, "image", &brush::Grain::image);
  field(grain, "space", &brush::Grain::space);
  field(grain, "scale", &brush::Grain::scale);
  field(grain, "depth", &brush::Grain::depth);
  auto tool = record<brush::Tool>(module, "Tool");
  field(tool, "tip", &brush::Tool::tip);
  field(tool, "color", &brush::Tool::color);
  field(tool, "width", &brush::Tool::width);
  field(tool, "spacing", &brush::Tool::spacing);
  field(tool, "opacity", &brush::Tool::opacity);
  field(tool, "scatter", &brush::Tool::scatter);
  field(tool, "density", &brush::Tool::density);
  field(tool, "bristles", &brush::Tool::bristles);
  field(tool, "pressure", &brush::Tool::pressure);
  field(tool, "blend", &brush::Tool::blend);
  field(tool, "rotation", &brush::Tool::rotation);
  field(tool, "angle", &brush::Tool::angle);
  field(tool, "aspect", &brush::Tool::aspect);
  field(tool, "sizeJitter", &brush::Tool::sizeJitter);
  field(tool, "opacityJitter", &brush::Tool::opacityJitter);
  field(tool, "spacingJitter", &brush::Tool::spacingJitter);
  field(tool, "speedSize", &brush::Tool::speedSize);
  field(tool, "speedOpacity", &brush::Tool::speedOpacity);
  field(tool, "speedReference", &brush::Tool::speedReference);
  field(tool, "pressureSize", &brush::Tool::pressureSize);
  field(tool, "pressureOpacity", &brush::Tool::pressureOpacity);
  field(tool, "tiltSize", &brush::Tool::tiltSize);
  field(tool, "tiltOpacity", &brush::Tool::tiltOpacity);
  field(tool, "tiltAspect", &brush::Tool::tiltAspect);
  field(tool, "tiltOffset", &brush::Tool::tiltOffset);
  field(tool, "sharpness", &brush::Tool::sharpness);
  field(tool, "noise", &brush::Tool::noise);
  field(tool, "markerTip", &brush::Tool::markerTip);
  field(tool, "shape", &brush::Tool::shape);
  field(tool, "grain", &brush::Tool::grain);
  field(tool, "dynamics", &brush::Tool::dynamics);
  auto input = record<brush::Input>(module, "Input");
  field(input, "position", &brush::Input::position);
  field(input, "pressure", &brush::Input::pressure);
  field(input, "tilt", &brush::Input::tilt);
  field(input, "barrelRotation", &brush::Input::barrelRotation);
  field(input, "seconds", &brush::Input::seconds);
  field(input, "tiltDirection", &brush::Input::tiltDirection);
  auto dab = record<brush::Dab>(module, "Dab");
  field(dab, "position", &brush::Dab::position);
  field(dab, "pressure", &brush::Dab::pressure);
  field(dab, "tilt", &brush::Dab::tilt);
  field(dab, "barrelRotation", &brush::Dab::barrelRotation);
  field(dab, "direction", &brush::Dab::direction);
  field(dab, "speed", &brush::Dab::speed);
  field(dab, "distance", &brush::Dab::distance);
  field(dab, "progress", &brush::Dab::progress);
  field(dab, "tiltDirection", &brush::Dab::tiltDirection);
  auto sample = record<brush::Sample>(module, "Sample");
  field(sample, "position", &brush::Sample::position);
  field(sample, "pressure", &brush::Sample::pressure);
  auto hatch = record<brush::Hatch>(module, "Hatch");
  field(hatch, "spacing", &brush::Hatch::spacing);
  field(hatch, "angle", &brush::Hatch::angle);
  field(hatch, "jitter", &brush::Hatch::jitter);
  field(hatch, "gradient", &brush::Hatch::gradient);
  field(hatch, "continuous", &brush::Hatch::continuous);
  auto wash = record<brush::Wash>(module, "Wash");
  field(wash, "color", &brush::Wash::color);
  field(wash, "opacity", &brush::Wash::opacity);
  field(wash, "bleed", &brush::Wash::bleed);
  field(wash, "texture", &brush::Wash::texture);
  field(wash, "border", &brush::Wash::border);
  field(wash, "scatter", &brush::Wash::scatter);
  field(wash, "bleedDirection", &brush::Wash::bleedDirection);
  field(wash, "bleedAngle", &brush::Wash::bleedAngle);
  field(wash, "layers", &brush::Wash::layers);
  field(wash, "blend", &brush::Wash::blend);
  auto mass = record<brush::Mass>(module, "Mass");
  field(mass, "precision", &brush::Mass::precision);
  field(mass, "strength", &brush::Mass::strength);
  field(mass, "gradient", &brush::Mass::gradient);
  field(mass, "outline", &brush::Mass::outline);
  auto depositOptions = record<brush::DepositOptions>(module, "DepositOptions");
  field(depositOptions, "start", &brush::DepositOptions::start);
  field(depositOptions, "end", &brush::DepositOptions::end);
  auto line = record<brush::Line>(module, "Line");
  field(line, "from", &brush::Line::from);
  field(line, "to", &brush::Line::to);
  auto vortex = record<brush::Vortex>(module, "Vortex");
  field(vortex, "center", &brush::Vortex::center);
  field(vortex, "direction", &brush::Vortex::direction);
  field(vortex, "pull", &brush::Vortex::pull);
  auto wave = record<brush::Wave>(module, "Wave");
  field(wave, "direction", &brush::Wave::direction);
  field(wave, "amplitude", &brush::Wave::amplitude);
  field(wave, "wavelength", &brush::Wave::wavelength);
  field(wave, "speed", &brush::Wave::speed);

  pressure
      .def(py::init([](float start, float middle, float end) {
             return brush::Pressure{start, middle, end};
           }),
           py::arg("start"), py::arg("middle"), py::arg("end"))
      .def("at", &brush::Pressure::at)
      .def_static("gaussianProfile", &brush::Pressure::gaussianProfile,
                  py::arg("centerJitter"), py::arg("widthJitter"),
                  py::arg("minimum"), py::arg("maximum"));
  pressure.attr("Gaussian") = module.attr("Gaussian");
  pressure.attr("Variation") = module.attr("Variation");
  curve.def("at", &brush::Curve::at).def_static("flat", &brush::Curve::flat);
  response.def("at", &brush::Response::at, py::arg("dab"), py::arg("pressure"),
               py::arg("speedReference"));
  dynamics.def("empty", &brush::Dynamics::empty);
  sample.def(py::init([](py::handle position, float pressure) {
               return brush::Sample{point(position), pressure};
             }),
             py::arg("position"), py::arg("pressure") = 1.0f);
  input.def(
      py::init([](py::handle position, float pressure, float tilt,
                  float barrelRotation, double seconds, float tiltDirection) {
        return brush::Input{point(position), pressure, tilt,
                            barrelRotation,  seconds,  tiltDirection};
      }),
      py::arg("position"), py::arg("pressure") = 1.0f, py::arg("tilt") = 0.0f,
      py::arg("barrelRotation") = 0.0f, py::arg("seconds") = 0.0,
      py::arg("tiltDirection") = 0.0f);
  tool.def_property(
      "customTip",
      [](const brush::Tool& tool) -> py::object {
        if (!tool.customTip) return py::none();
        return py::cpp_function(
            [tip = tool.customTip](BorrowedPen& pen, const brush::Dab& dab) {
              tip(pen.get(), dab);
            });
      },
      [](brush::Tool& tool, py::handle value) {
        if (value.is_none()) {
          tool.customTip = {};
          return;
        }
        const auto callable = py::reinterpret_borrow<py::function>(value);
        const int count = py::module_::import("sigil._loader")
                              .attr("arity")(callable, 2)
                              .cast<int>();
        auto held = retainCallback(callable);
        tool.customTip = [held, count](draw::Pen& pen, const brush::Dab& dab) {
          const py::gil_scoped_acquire lock;
          auto borrowed = std::make_shared<BorrowedPen>(pen);
          struct Invalidate {
            BorrowedPen& pen;
            ~Invalidate() { pen.invalidate(); }
          } invalidate{*borrowed};
          try {
            if (count == 0)
              held->get()();
            else if (count == 1)
              held->get()(borrowed);
            else
              held->get()(borrowed, dab);
          } catch (const py::error_already_set& error) {
            throw std::runtime_error(error.what());
          }
        };
      });

  py::class_<brush::Curl>(module, "Curl")
      .def(py::init<uint32_t, float, float>(), py::arg("seed") = 0,
           py::arg("scale") = 0.004f, py::arg("drift") = 0.08f)
      .def_property_readonly("seed", &brush::Curl::seed)
      .def_property_readonly("scale", &brush::Curl::scale)
      .def_property_readonly("drift", &brush::Curl::drift)
      .def(
          "__call__",
          [](const brush::Curl& field, py::handle position, float seconds) {
            return field(point(position), seconds);
          },
          py::arg("position"), py::arg("seconds") = 0.0f);
  vortex.def(
      "__call__",
      [](const brush::Vortex& field, py::handle position, float seconds) {
        return field(point(position), seconds);
      },
      py::arg("position"), py::arg("seconds") = 0.0f);
  wave.def(
      "__call__",
      [](const brush::Wave& field, py::handle position, float seconds) {
        return field(point(position), seconds);
      },
      py::arg("position"), py::arg("seconds") = 0.0f);
  py::class_<Direction>(module, "Direction")
      .def(py::init(
          [](py::handle field) { return Direction{direction(field)}; }))
      .def(
          "__call__",
          [](const Direction& field, py::handle position, float seconds) {
            return field.value ? field.value(point(position), seconds) : 0.0f;
          },
          py::arg("position"), py::arg("seconds") = 0.0f);
  module.def("stockFields", [] {
    py::dict fields;
    for (auto& [name, value] : brush::stockFields())
      fields[py::str(name)] = py::cast(Direction{std::move(value)});
    return fields;
  });

  line.def(py::init([](py::handle start, py::handle end) {
             return brush::Line{point(start), point(end)};
           }),
           py::arg("start"), py::arg("end"));
  field(line, "from_", &brush::Line::from);

  auto polygon =
      py::class_<brush::Polygon>(module, "Polygon")
          .def(py::init<>())
          .def(py::init([](py::iterable vertices) {
                 return brush::Polygon(points(vertices));
               }),
               py::arg("vertices"))
          .def_property(
              "vertices",
              [](const brush::Polygon& value) { return value.vertices; },
              [](brush::Polygon& value, py::iterable vertices) {
                value.vertices = points(vertices);
              })
          .def("intersect", &brush::Polygon::intersect)
          .def("translated", &brush::Polygon::translated)
          .def("empty", &brush::Polygon::empty)
          .def("copy", [](const brush::Polygon& value) { return value; });
  polygon.def("draw",
              [](const brush::Polygon& value, BorrowedPen& pen,
                 const brush::Tool& tool) { value.draw(pen.get(), tool); });
  polygon.def("fill",
              [](const brush::Polygon& value, BorrowedPen& pen,
                 const brush::Wash& style) { value.fill(pen.get(), style); });
  polygon.def("wash",
              [](const brush::Polygon& value, BorrowedPen& pen,
                 const brush::Wash& style) { value.wash(pen.get(), style); });
  polygon.def(
      "hatch",
      [](const brush::Polygon& value, BorrowedPen& pen, const brush::Tool& tool,
         const brush::Hatch& style) { value.hatch(pen.get(), tool, style); },
      py::arg("pen"), py::arg("tool"), py::arg("style") = brush::Hatch{});
  polygon.def(
      "mass",
      [](const brush::Polygon& value, BorrowedPen& pen, const brush::Tool& tool,
         const brush::Mass& style) { value.mass(pen.get(), tool, style); },
      py::arg("pen"), py::arg("tool"), py::arg("style") = brush::Mass{});
  polygon.def("draw", [](const brush::Polygon& value, BorrowedPen& pen,
                         const brush::Engine& engine) {
    value.draw(pen.get(), engine);
  });
  polygon.def("fill", [](const brush::Polygon& value, BorrowedPen& pen,
                         const brush::Engine& engine) {
    value.fill(pen.get(), engine);
  });
  polygon.def("wash", [](const brush::Polygon& value, BorrowedPen& pen,
                         const brush::Engine& engine) {
    value.wash(pen.get(), engine);
  });
  polygon.def("hatch", [](const brush::Polygon& value, BorrowedPen& pen,
                          const brush::Engine& engine) {
    value.hatch(pen.get(), engine);
  });
  polygon.def("mass", [](const brush::Polygon& value, BorrowedPen& pen,
                         const brush::Engine& engine) {
    value.mass(pen.get(), engine);
  });
  polygon.def("show", [](const brush::Polygon& value, BorrowedPen& pen,
                         const brush::Engine& engine) {
    value.show(pen.get(), engine);
  });

  auto plot =
      py::class_<brush::Plot>(module, "Plot")
          .def(py::init<brush::PlotType>(),
               py::arg("type") = brush::PlotType::Curve)
          .def("type", &brush::Plot::type)
          .def("addSegment", &brush::Plot::addSegment, py::arg("angle"),
               py::arg("length"), py::arg("pressure") = 1.0f)
          .def("endPlot", &brush::Plot::endPlot, py::arg("angle"),
               py::arg("pressure") = 1.0f)
          .def("rotate", &brush::Plot::rotate)
          .def("length", &brush::Plot::length)
          .def("angle", &brush::Plot::angle)
          .def("pressure", &brush::Plot::pressure)
          .def("empty", &brush::Plot::empty)
          .def("copy", [](const brush::Plot& value) { return value; })
          .def(
              "path",
              [](const brush::Plot& value, py::handle origin, float spacing,
                 float curvature, float scale) {
                return value.path(point(origin), spacing, curvature, scale);
              },
              py::arg("origin") = py::make_tuple(0, 0),
              py::arg("spacing") = 1.0f, py::arg("curvature") = 0.5f,
              py::arg("scale") = 1.0f)
          .def("polygon", &brush::Plot::polygon, py::arg("x") = 0.0f,
               py::arg("y") = 0.0f, py::arg("spacing") = 1.0f,
               py::arg("curvature") = 0.5f, py::arg("scale") = 1.0f)
          .def_static(
              "fromStroke",
              [](py::iterable stroke, brush::PlotType type) {
                return brush::Plot::fromStroke(samples(stroke), type);
              },
              py::arg("stroke"), py::arg("type") = brush::PlotType::Curve);
  plot.def(
      "draw",
      [](const brush::Plot& value, BorrowedPen& pen, const brush::Tool& tool,
         float x, float y,
         float scale) { value.draw(pen.get(), tool, x, y, scale); },
      py::arg("pen"), py::arg("tool"), py::arg("x") = 0.0f, py::arg("y") = 0.0f,
      py::arg("scale") = 1.0f);
  plot.def(
      "fill",
      [](const brush::Plot& value, BorrowedPen& pen, const brush::Wash& style,
         float x, float y,
         float scale) { value.fill(pen.get(), style, x, y, scale); },
      py::arg("pen"), py::arg("style"), py::arg("x") = 0.0f,
      py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  plot.def(
      "wash",
      [](const brush::Plot& value, BorrowedPen& pen, const brush::Wash& style,
         float x, float y,
         float scale) { value.wash(pen.get(), style, x, y, scale); },
      py::arg("pen"), py::arg("style"), py::arg("x") = 0.0f,
      py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  plot.def(
      "hatch",
      [](const brush::Plot& value, BorrowedPen& pen, const brush::Tool& tool,
         const brush::Hatch& style, float x, float y,
         float scale) { value.hatch(pen.get(), tool, style, x, y, scale); },
      py::arg("pen"), py::arg("tool"), py::arg("style") = brush::Hatch{},
      py::arg("x") = 0.0f, py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  plot.def(
      "mass",
      [](const brush::Plot& value, BorrowedPen& pen, const brush::Tool& tool,
         const brush::Mass& style, float x, float y,
         float scale) { value.mass(pen.get(), tool, style, x, y, scale); },
      py::arg("pen"), py::arg("tool"), py::arg("style") = brush::Mass{},
      py::arg("x") = 0.0f, py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  plot.def(
      "draw",
      [](const brush::Plot& value, BorrowedPen& pen,
         const brush::Engine& engine, float x, float y,
         float scale) { value.draw(pen.get(), engine, x, y, scale); },
      py::arg("pen"), py::arg("engine"), py::arg("x") = 0.0f,
      py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  plot.def(
      "fill",
      [](const brush::Plot& value, BorrowedPen& pen,
         const brush::Engine& engine, float x, float y,
         float scale) { value.fill(pen.get(), engine, x, y, scale); },
      py::arg("pen"), py::arg("engine"), py::arg("x") = 0.0f,
      py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  plot.def(
      "wash",
      [](const brush::Plot& value, BorrowedPen& pen,
         const brush::Engine& engine, float x, float y,
         float scale) { value.wash(pen.get(), engine, x, y, scale); },
      py::arg("pen"), py::arg("engine"), py::arg("x") = 0.0f,
      py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  plot.def(
      "hatch",
      [](const brush::Plot& value, BorrowedPen& pen,
         const brush::Engine& engine, float x, float y,
         float scale) { value.hatch(pen.get(), engine, x, y, scale); },
      py::arg("pen"), py::arg("engine"), py::arg("x") = 0.0f,
      py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  plot.def(
      "mass",
      [](const brush::Plot& value, BorrowedPen& pen,
         const brush::Engine& engine, float x, float y,
         float scale) { value.mass(pen.get(), engine, x, y, scale); },
      py::arg("pen"), py::arg("engine"), py::arg("x") = 0.0f,
      py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  plot.def(
      "show",
      [](const brush::Plot& value, BorrowedPen& pen,
         const brush::Engine& engine, float x, float y,
         float scale) { value.show(pen.get(), engine, x, y, scale); },
      py::arg("pen"), py::arg("engine"), py::arg("x") = 0.0f,
      py::arg("y") = 0.0f, py::arg("scale") = 1.0f);

  auto placed = record<brush::PlacedPlot>(module, "PlacedPlot");
  field(placed, "plot", &brush::PlacedPlot::plot);
  field(placed, "origin", &brush::PlacedPlot::origin);
  placed.def("empty", &brush::PlacedPlot::empty);

  py::class_<brush::Position>(module, "Position")
      .def(py::init([](float x, float y, py::handle field, float seconds,
                       py::handle bounds) {
             return brush::Position(
                 x, y, direction(field), seconds,
                 bounds.is_none() ? std::nullopt : std::optional(rect(bounds)));
           }),
           py::arg("x") = 0.0f, py::arg("y") = 0.0f,
           py::arg("field") = py::none(), py::arg("seconds") = 0.0f,
           py::arg("bounds") = py::none())
      .def("x", &brush::Position::x)
      .def("y", &brush::Position::y)
      .def("plotted", &brush::Position::plotted)
      .def(
          "moveTo",
          [](brush::Position& position, float heading, float length, float step,
             py::handle field, float seconds) {
            return position.moveTo(heading, length, step, direction(field),
                                   seconds);
          },
          py::arg("direction"), py::arg("length"), py::arg("stepLength") = 1.0f,
          py::arg("field") = py::none(), py::arg("seconds") = 0.0f)
      .def("plotTo", &brush::Position::plotTo, py::arg("plot"),
           py::arg("length"), py::arg("stepLength"), py::arg("scale") = 1.0f)
      .def("angle", py::overload_cast<>(&brush::Position::angle, py::const_))
      .def(
          "angle",
          [](const brush::Position& position, py::handle field, float seconds) {
            return position.angle(direction(field), seconds);
          },
          py::arg("field"), py::arg("seconds") = 0.0f)
      .def("isIn", &brush::Position::isIn)
      .def("isInCanvas", &brush::Position::isInCanvas)
      .def("place", &brush::Position::place)
      .def("reset", &brush::Position::reset)
      .def(
          "field",
          [](brush::Position& position, py::handle value, float seconds) {
            position.field(direction(value), seconds);
          },
          py::arg("value"), py::arg("seconds") = 0.0f);

  py::class_<brush::Sampler>(module, "Sampler")
      .def(py::init<float>(),
           py::arg("speedFilterSeconds") = brush::kSpeedFilterSeconds)
      .def("begin", &brush::Sampler::begin)
      .def("move", &brush::Sampler::move)
      .def("end", &brush::Sampler::end)
      .def("cancel", &brush::Sampler::cancel)
      .def("active", &brush::Sampler::active)
      .def("distance", &brush::Sampler::distance);

  py::class_<brush::Catalogue>(module, "Catalogue")
      .def(py::init<>())
      .def_static("stock", &brush::Catalogue::stock)
      .def("add",
           [](brush::Catalogue& catalogue, const std::string& name,
              const brush::Tool& tool) {
             return toolCopy(catalogue.add(name, tool));
           })
      .def("find",
           [](const brush::Catalogue& catalogue, const std::string& name) {
             return toolCopy(catalogue.find(name));
           })
      .def("contains", &brush::Catalogue::contains)
      .def("names", &brush::Catalogue::names)
      .def("scale", &brush::Catalogue::scale);

  auto engine =
      py::class_<brush::Engine>(module, "Engine")
          .def(py::init<>())
          .def(py::init<brush::Catalogue>())
          .def("add",
               [](brush::Engine& engine, const std::string& name,
                  const brush::Tool& tool) {
                 return toolCopy(engine.add(name, tool));
               })
          .def("pick",
               [](brush::Engine& engine, const std::string& name) {
                 return toolCopy(engine.pick(name));
               })
          .def(
              "set",
              [](brush::Engine& engine, const std::string& name,
                 py::handle pigment, float weight) {
                return toolCopy(engine.set(name, color(pigment), weight));
              },
              py::arg("name"), py::arg("color"), py::arg("weight") = 1.0f)
          .def("names", &brush::Engine::names)
          .def("scaleBrushes", &brush::Engine::scaleBrushes)
          .def("stroke",
               [](brush::Engine& engine, py::handle pigment) {
                 engine.stroke(color(pigment));
               })
          .def("noStroke", &brush::Engine::noStroke)
          .def("strokeWeight", &brush::Engine::strokeWeight)
          .def("hasStroke", &brush::Engine::hasStroke)
          .def("tool", py::overload_cast<>(&brush::Engine::tool, py::const_))
          .def(
              "fill",
              [](brush::Engine& engine, py::handle pigment, float opacity) {
                engine.fill(color(pigment), opacity);
              },
              py::arg("color"), py::arg("opacity") = 150.0f / 255.0f)
          .def(
              "wash",
              [](brush::Engine& engine, py::handle pigment, float opacity) {
                engine.wash(color(pigment), opacity);
              },
              py::arg("color"), py::arg("opacity") = 150.0f / 255.0f)
          .def("noFill", &brush::Engine::noFill)
          .def("noWash", &brush::Engine::noWash)
          .def("fillBleed", &brush::Engine::fillBleed, py::arg("bleed"),
               py::arg("direction") = brush::BleedDirection::Out,
               py::arg("angle") = py::none())
          .def("fillTexture", &brush::Engine::fillTexture,
               py::arg("texture") = 0.4f, py::arg("border") = 0.4f,
               py::arg("scatter") = true)
          .def("hatch",
               py::overload_cast<const brush::Hatch&>(&brush::Engine::hatch),
               py::arg("style") = brush::Hatch{})
          .def(
              "hatch",
              [](brush::Engine& engine, BorrowedPen& pen, float spacing,
                 float angle, float jitter, float gradient, bool continuous) {
                engine.hatch(pen.get(), spacing, angle, jitter, gradient,
                             continuous);
              },
              py::arg("pen"), py::arg("spacing"), py::arg("angle"),
              py::arg("jitter") = 0.0f, py::arg("gradient") = 0.0f,
              py::arg("continuous") = false)
          .def("noHatch", &brush::Engine::noHatch)
          .def(
              "hatchStyle",
              [](brush::Engine& engine, const std::string& name,
                 py::handle pigment, float weight) {
                return toolCopy(
                    engine.hatchStyle(name, color(pigment), weight));
              },
              py::arg("name"), py::arg("color") = "#000000",
              py::arg("weight") = 1.0f)
          .def(
              "mass",
              [](brush::Engine& engine, const std::string& name,
                 py::handle pigment, const brush::Mass& style) {
                return toolCopy(engine.mass(name, color(pigment), style));
              },
              py::arg("name"), py::arg("color"),
              py::arg("style") = brush::Mass{})
          .def("noMass", &brush::Engine::noMass)
          .def(
              "addField",
              [](brush::Engine& engine, const std::string& name,
                 py::handle field, draw::Constant units) {
                return engine.addField(name, direction(field), units);
              },
              py::arg("name"), py::arg("field"),
              py::arg("units") = draw::RADIANS)
          .def("listFields", &brush::Engine::listFields)
          .def("field", &brush::Engine::field)
          .def("noField", &brush::Engine::noField)
          .def("wiggle", &brush::Engine::wiggle, py::arg("amount") = 1.0f)
          .def("clip", [](brush::Engine& engine,
                          py::handle region) { engine.clip(rect(region)); })
          .def("clip",
               [](brush::Engine& engine, BorrowedPen& pen, py::handle region) {
                 engine.clip(pen.get(), rect(region));
               })
          .def("noClip", &brush::Engine::noClip)
          .def("push", &brush::Engine::push)
          .def("pop", &brush::Engine::pop)
          .def(
              "paint",
              [](const brush::Engine& engine, BorrowedPen& pen,
                 py::iterable path) { engine.paint(pen.get(), samples(path)); })
          .def(
              "line",
              [](const brush::Engine& engine, BorrowedPen& pen, py::handle from,
                 py::handle to, float p0, float p1) {
                engine.line(pen.get(), point(from), point(to), p0, p1);
              },
              py::arg("pen"), py::arg("from_"), py::arg("to"),
              py::arg("startPressure") = 1.0f, py::arg("endPressure") = 1.0f)
          .def(
              "flowLine",
              [](const brush::Engine& engine, BorrowedPen& pen,
                 py::handle start, float length, float heading) {
                engine.flowLine(pen.get(), point(start), length, heading);
              },
              py::arg("pen"), py::arg("start"), py::arg("length"),
              py::arg("direction"))
          .def(
              "spline",
              [](const brush::Engine& engine, BorrowedPen& pen,
                 py::iterable path, float curvature) {
                return engine.spline(pen.get(), samples(path), curvature);
              },
              py::arg("pen"), py::arg("controls"), py::arg("curvature") = 0.5f)
          .def("polygon",
               [](const brush::Engine& engine, BorrowedPen& pen,
                  py::iterable vertices) {
                 return engine.polygon(pen.get(), points(vertices));
               })
          .def("polygon",
               [](const brush::Engine& engine, BorrowedPen& pen,
                  const brush::Polygon& polygon) {
                 engine.polygon(pen.get(), polygon);
               })
          .def(
              "rect",
              [](const brush::Engine& engine, BorrowedPen& pen, float x,
                 float y, float width, float height, draw::Constant mode) {
                engine.rect(pen.get(), x, y, width, height, mode);
              },
              py::arg("pen"), py::arg("x"), py::arg("y"), py::arg("width"),
              py::arg("height"), py::arg("mode") = draw::CORNER)
          .def(
              "rect",
              [](const brush::Engine& engine, BorrowedPen& pen, float x,
                 float y, float width, float height, float radius) {
                engine.rect(pen.get(), x, y, width, height, radius);
              },
              py::arg("pen"), py::arg("x"), py::arg("y"), py::arg("width"),
              py::arg("height"), py::arg("radius"))
          .def(
              "circle",
              [](const brush::Engine& engine, BorrowedPen& pen, float x,
                 float y, float radius, float irregularity) {
                return engine.circle(pen.get(), x, y, radius, irregularity);
              },
              py::arg("pen"), py::arg("x"), py::arg("y"), py::arg("radius"),
              py::arg("irregularity") = 0.0f)
          .def("arc",
               [](const brush::Engine& engine, BorrowedPen& pen, float x,
                  float y, float radius, float start, float stop) {
                 return engine.arc(pen.get(), x, y, radius, start, stop);
               })
          .def("beginShape", &brush::Engine::beginShape,
               py::arg("curvature") = 0.0f)
          .def("vertex", &brush::Engine::vertex, py::arg("x"), py::arg("y"),
               py::arg("pressure") = 1.0f)
          .def(
              "endShape",
              [](brush::Engine& engine, BorrowedPen& pen, bool close) {
                return engine.endShape(pen.get(), close);
              },
              py::arg("pen"), py::arg("close") = false)
          .def("position",
               py::overload_cast<float, float>(&brush::Engine::position,
                                               py::const_),
               py::arg("x") = 0.0f, py::arg("y") = 0.0f)
          .def(
              "position",
              [](const brush::Engine& engine, BorrowedPen& pen, float x,
                 float y) { return engine.position(pen.get(), x, y); },
              py::arg("pen"), py::arg("x") = 0.0f, py::arg("y") = 0.0f)
          .def("beginStroke",
               [](brush::Engine& engine, brush::PlotType kind, py::handle at) {
                 engine.beginStroke(kind, point(at));
               })
          .def(
              "move",
              [](brush::Engine& engine, BorrowedPen& pen, float angle,
                 float length, float pressure) {
                engine.move(pen.get(), angle, length, pressure);
              },
              py::arg("pen"), py::arg("angle"), py::arg("length"),
              py::arg("pressure") = 1.0f)
          .def(
              "endStroke",
              [](brush::Engine& engine, BorrowedPen& pen, float angle,
                 float pressure) {
                return engine.endStroke(pen.get(), angle, pressure);
              },
              py::arg("pen"), py::arg("angle"), py::arg("pressure") = 1.0f)
          .def("cancelStroke", &brush::Engine::cancelStroke)
          .def("cancelInput", &brush::Engine::cancelInput);
  engine.def("beginInput",
             [](brush::Engine& engine, BorrowedPen& pen, brush::Input input) {
               engine.beginInput(pen.get(), input);
             });
  engine.def("moveInput",
             [](brush::Engine& engine, BorrowedPen& pen, brush::Input input) {
               engine.moveInput(pen.get(), input);
             });
  engine.def("endInput",
             [](brush::Engine& engine, BorrowedPen& pen, brush::Input input) {
               engine.endInput(pen.get(), input);
             });
  engine
      .def("draw",
           [](const brush::Engine& engine, BorrowedPen& pen,
              const brush::Polygon& polygon) {
             engine.draw(pen.get(), polygon);
           })
      .def(
          "draw",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Plot& plot, float x, float y,
             float scale) { engine.draw(pen.get(), plot, x, y, scale); },
          py::arg("pen"), py::arg("plot"), py::arg("x") = 0.0f,
          py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  engine
      .def("fill",
           [](const brush::Engine& engine, BorrowedPen& pen,
              const brush::Polygon& polygon) {
             engine.fill(pen.get(), polygon);
           })
      .def(
          "fill",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Plot& plot, float x, float y,
             float scale) { engine.fill(pen.get(), plot, x, y, scale); },
          py::arg("pen"), py::arg("plot"), py::arg("x") = 0.0f,
          py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  engine
      .def("wash",
           [](const brush::Engine& engine, BorrowedPen& pen,
              const brush::Polygon& polygon) {
             engine.wash(pen.get(), polygon);
           })
      .def(
          "wash",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Plot& plot, float x, float y,
             float scale) { engine.wash(pen.get(), plot, x, y, scale); },
          py::arg("pen"), py::arg("plot"), py::arg("x") = 0.0f,
          py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  engine
      .def("hatch",
           [](const brush::Engine& engine, BorrowedPen& pen,
              const brush::Polygon& polygon) {
             engine.hatch(pen.get(), polygon);
           })
      .def(
          "hatch",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Plot& plot, float x, float y,
             float scale) { engine.hatch(pen.get(), plot, x, y, scale); },
          py::arg("pen"), py::arg("plot"), py::arg("x") = 0.0f,
          py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  engine
      .def("mass",
           [](const brush::Engine& engine, BorrowedPen& pen,
              const brush::Polygon& polygon) {
             engine.mass(pen.get(), polygon);
           })
      .def(
          "mass",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Plot& plot, float x, float y,
             float scale) { engine.mass(pen.get(), plot, x, y, scale); },
          py::arg("pen"), py::arg("plot"), py::arg("x") = 0.0f,
          py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  engine
      .def("hatchArray",
           [](const brush::Engine& engine, BorrowedPen& pen,
              const std::vector<brush::Polygon>& polygons) {
             engine.hatchArray(pen.get(), polygons);
           })
      .def("hatchArray", [](const brush::Engine& engine, BorrowedPen& pen,
                            const brush::Polygon& polygon) {
        engine.hatchArray(pen.get(), polygon);
      });
  engine
      .def("massArray",
           [](const brush::Engine& engine, BorrowedPen& pen,
              const std::vector<brush::Polygon>& polygons) {
             engine.massArray(pen.get(), polygons);
           })
      .def("massArray", [](const brush::Engine& engine, BorrowedPen& pen,
                           const brush::Polygon& polygon) {
        engine.massArray(pen.get(), polygon);
      });
  module.def(
      "pencil",
      [](py::handle pigment, float width) {
        return brush::pencil(color(pigment), width);
      },
      py::arg("color"), py::arg("width") = 1.4f);
  module.def(
      "charcoal",
      [](py::handle pigment, float width) {
        return brush::charcoal(color(pigment), width);
      },
      py::arg("color"), py::arg("width") = 9.0f);
  module.def(
      "marker",
      [](py::handle pigment, float width) {
        return brush::marker(color(pigment), width);
      },
      py::arg("color"), py::arg("width") = 16.0f);
  module.def(
      "watercolor",
      [](py::handle pigment, float width) {
        return brush::watercolor(color(pigment), width);
      },
      py::arg("color"), py::arg("width") = 22.0f);
  module.def(
      "spray",
      [](py::handle pigment, float width) {
        return brush::spray(color(pigment), width);
      },
      py::arg("color"), py::arg("width") = 18.0f);

  module.def("spacingOf", &brush::spacingOf);
  module.def("prepareStroke", [](BorrowedPen& pen, const brush::Tool& tool) {
    return brush::prepareStroke(pen.get(), tool);
  });
  module.def(
      "segment",
      [](py::handle from, py::handle to, float spacing, float p0, float p1) {
        return brush::segment(point(from), point(to), spacing, p0, p1);
      },
      py::arg("from_"), py::arg("to"), py::arg("spacing") = 1.0f,
      py::arg("startPressure") = 1.0f, py::arg("endPressure") = 1.0f);
  module.def(
      "spline",
      [](py::iterable controls, float spacing, float curvature) {
        return brush::spline(samples(controls), spacing, curvature);
      },
      py::arg("controls"), py::arg("spacing") = 1.0f,
      py::arg("curvature") = 0.5f);
  module.def(
      "spline",
      [](BorrowedPen& pen, const brush::Tool& tool, py::iterable controls,
         float curvature) {
        brush::spline(pen.get(), tool, samples(controls), curvature);
      },
      py::arg("pen"), py::arg("tool"), py::arg("controls"),
      py::arg("curvature") = 0.5f);
  module.def(
      "line",
      [](BorrowedPen& pen, const brush::Tool& tool, py::handle from,
         py::handle to, float p0, float p1) {
        brush::line(pen.get(), tool, point(from), point(to), p0, p1);
      },
      py::arg("pen"), py::arg("tool"), py::arg("from_"), py::arg("to"),
      py::arg("startPressure") = 1.0f, py::arg("endPressure") = 1.0f);
  module.def("paint", [](BorrowedPen& pen, const brush::Tool& tool,
                         py::iterable stroke) {
    brush::paint(pen.get(), tool, samples(stroke));
  });
  module.def(
      "trace",
      [](py::handle start, float length, float spacing, float seconds,
         py::handle field, float pressure) {
        return brush::trace(point(start), length, spacing, seconds,
                            direction(field), pressure);
      },
      py::arg("start"), py::arg("length"), py::arg("spacing"),
      py::arg("seconds"), py::arg("field"), py::arg("pressure") = 1.0f);
  module.def(
      "warp",
      [](py::iterable polygon, float spacing, float amount, float seconds,
         py::handle field, float pressure) {
        return brush::warp(points(polygon), spacing, amount, seconds,
                           direction(field), pressure);
      },
      py::arg("polygon"), py::arg("spacing"), py::arg("amount"),
      py::arg("seconds"), py::arg("field"), py::arg("pressure") = 1.0f);
  module.def(
      "flowLine",
      [](BorrowedPen& pen, const brush::Tool& tool, py::handle start,
         float length, float seconds, py::handle field, float pressure) {
        brush::flowLine(pen.get(), tool, point(start), length, seconds,
                        direction(field), pressure);
      },
      py::arg("pen"), py::arg("tool"), py::arg("start"), py::arg("length"),
      py::arg("seconds"), py::arg("field"), py::arg("pressure") = 1.0f);
  module.def(
      "dabs",
      [](const std::vector<brush::Input>& input, float spacing,
         float speedFilterSeconds) {
        return brush::dabs(input, spacing, speedFilterSeconds);
      },
      py::arg("input"), py::arg("spacing"),
      py::arg("speedFilterSeconds") = brush::kSpeedFilterSeconds);
  module.def(
      "deposit",
      [](BorrowedPen& pen, const brush::Tool& tool,
         const std::vector<brush::Dab>& dabs, brush::DepositOptions options) {
        brush::deposit(pen.get(), tool, dabs, options);
      },
      py::arg("pen"), py::arg("tool"), py::arg("dabs"),
      py::arg("options") = brush::DepositOptions{});
  module.def("wash", [](BorrowedPen& pen, const brush::Wash& pigment,
                        py::iterable polygon) {
    brush::wash(pen.get(), pigment, points(polygon));
  });
  module.def(
      "hatch",
      [](BorrowedPen& pen, const brush::Tool& tool, py::iterable polygon,
         const brush::Hatch& style) {
        brush::hatch(pen.get(), tool, points(polygon), style);
      },
      py::arg("pen"), py::arg("tool"), py::arg("polygon"),
      py::arg("style") = brush::Hatch{});
  module.def(
      "hatchArray",
      [](BorrowedPen& pen, const brush::Tool& tool,
         const std::vector<brush::Polygon>& polygons,
         const brush::Hatch& style) {
        brush::hatchArray(pen.get(), tool, polygons, style);
      },
      py::arg("pen"), py::arg("tool"), py::arg("polygons"),
      py::arg("style") = brush::Hatch{});
  module.def(
      "mass",
      [](BorrowedPen& pen, const brush::Tool& tool, py::iterable polygon,
         const brush::Mass& style) {
        brush::mass(pen.get(), tool, points(polygon), style);
      },
      py::arg("pen"), py::arg("tool"), py::arg("polygon"),
      py::arg("style") = brush::Mass{});
  module.def(
      "massArray",
      [](BorrowedPen& pen, const brush::Tool& tool,
         const std::vector<brush::Polygon>& polygons,
         const brush::Mass& style) {
        brush::massArray(pen.get(), tool, polygons, style);
      },
      py::arg("pen"), py::arg("tool"), py::arg("polygons"),
      py::arg("style") = brush::Mass{});

  module.def("randomBelow", [](BorrowedPen& pen, float total) {
    return brush::randomBelow(pen.get(), total);
  });
  module.attr("Stroke") = py::module_::import("builtins").attr("list");

  auto format = module.def_submodule("format");
  format.def("encodeBrush", &brush::format::encodeBrush);
  format.def(
      "decodeBrush",
      [](py::bytes content, const std::string& hint) {
        const auto source = py::cast<std::string>(content);
        return brush::format::decodeBrush(bytes(source), hint);
      },
      py::arg("bytes"), py::arg("hint") = "");
  format.def(
      "assembleBrush",
      [](py::bytes description, py::bytes shape, py::bytes grain) {
        const auto descriptionBytes = py::cast<std::string>(description);
        const auto shapeBytes = py::cast<std::string>(shape);
        const auto grainBytes = py::cast<std::string>(grain);
        return brush::format::assembleBrush(
            bytes(descriptionBytes), bytes(shapeBytes), bytes(grainBytes));
      },
      py::arg("description") = py::bytes(), py::arg("shape") = py::bytes(),
      py::arg("grain") = py::bytes());
  format.def("isPhotoshopBrushes", [](py::bytes content) {
    const auto source = py::cast<std::string>(content);
    return brush::format::isPhotoshopBrushes(bytes(source));
  });
  format.def("decodePhotoshopBrushes", [](py::bytes content) {
    const auto source = py::cast<std::string>(content);
    return brush::format::decodePhotoshopBrushes(bytes(source));
  });
  format.def("decodeProcreateBrush", [](py::bytes content) {
    const auto source = py::cast<std::string>(content);
    return brush::format::decodeProcreateBrush(bytes(source));
  });
}

}  // namespace sigil::sketch::python
