/** @file
 * The brush's value records in Python: the tip, rotation, drive and mask
 * enumerations, the pressure, curve, response and dynamics a tool reads,
 * the shape and grain, the tool itself with its custom tip, the input and
 * the dab, and the hatch, wash, mass and deposit styles.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigildraw/brush/Brush.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/skia/Values.h>

#include <memory>
#include <stdexcept>

#include "BrushBinding.h"

namespace sigil::python {
using namespace brush_binding;

void bindBrushRecords(py::module_& module) {
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
  tool.def_property(
      "angle", angleGetter(&brush::Tool::angle),
      angleSetter<AngleInRadians>(&brush::Tool::angle));
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
  input.def_property(
      "barrelRotation", angleGetter(&brush::Input::barrelRotation),
      angleSetter<AngleInRadians>(&brush::Input::barrelRotation));
  field(input, "seconds", &brush::Input::seconds);
  input.def_property(
      "tiltDirection", angleGetter(&brush::Input::tiltDirection),
      angleSetter<AngleInRadians>(&brush::Input::tiltDirection));
  auto dab = record<brush::Dab>(module, "Dab");
  field(dab, "position", &brush::Dab::position);
  field(dab, "pressure", &brush::Dab::pressure);
  field(dab, "tilt", &brush::Dab::tilt);
  dab.def_property(
      "barrelRotation", angleGetter(&brush::Dab::barrelRotation),
      angleSetter<AngleInRadians>(&brush::Dab::barrelRotation))
      .def_property(
          "direction", angleGetter(&brush::Dab::direction),
          angleSetter<AngleInRadians>(&brush::Dab::direction));
  field(dab, "speed", &brush::Dab::speed);
  field(dab, "distance", &brush::Dab::distance);
  field(dab, "progress", &brush::Dab::progress);
  dab.def_property(
      "tiltDirection", angleGetter(&brush::Dab::tiltDirection),
      angleSetter<AngleInRadians>(&brush::Dab::tiltDirection));
  auto sample = record<brush::Sample>(module, "Sample");
  field(sample, "position", &brush::Sample::position);
  field(sample, "pressure", &brush::Sample::pressure);
  auto hatch = record<brush::Hatch>(module, "Hatch");
  field(hatch, "pattern", &brush::Hatch::pattern);
  field(hatch, "jitter", &brush::Hatch::jitter);
  field(hatch, "continuous", &brush::Hatch::continuous);
  module.def("gradientTaper", &brush::gradientTaper, py::arg("gradient"),
             "The pattern taper p5's gradient dial names: a tenth of one "
             "step per lane.");
  auto wash = record<brush::Wash>(module, "Wash");
  field(wash, "color", &brush::Wash::color);
  field(wash, "opacity", &brush::Wash::opacity);
  field(wash, "bleed", &brush::Wash::bleed);
  field(wash, "texture", &brush::Wash::texture);
  field(wash, "border", &brush::Wash::border);
  field(wash, "scatter", &brush::Wash::scatter);
  field(wash, "bleedDirection", &brush::Wash::bleedDirection);
  wash.def_property(
      "bleedAngle", angleGetter(&brush::Wash::bleedAngle),
      angleSetter<AngleInRadians>(&brush::Wash::bleedAngle));
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
  auto vortex = record<brush::Vortex>(module, "Vortex");
  field(vortex, "center", &brush::Vortex::center);
  field(vortex, "direction", &brush::Vortex::direction);
  field(vortex, "pull", &brush::Vortex::pull);
  auto wave = record<brush::Wave>(module, "Wave");
  wave.def_property(
      "direction", angleGetter(&brush::Wave::direction),
      angleSetter<AngleInRadians>(&brush::Wave::direction));
  field(wave, "amplitude", &brush::Wave::amplitude);
  field(wave, "wavelength", &brush::Wave::wavelength);
  field(wave, "speed", &brush::Wave::speed);

  pressure
      .def(py::init([](float start, float middle, float end) {
             return brush::Pressure{start, middle, end};
           }),
           py::arg("start"), py::arg("middle"), py::arg("end"))
      .def("at", &brush::Pressure::at, py::arg("progress"))
      .def_static("gaussianProfile", &brush::Pressure::gaussianProfile,
                  py::arg("centerJitter"), py::arg("widthJitter"),
                  py::arg("minimum"), py::arg("maximum"));
  pressure.attr("Gaussian") = module.attr("Gaussian");
  pressure.attr("Variation") = module.attr("Variation");
  curve.def("at", &brush::Curve::at, py::arg("input"))
      .def_static("flat", &brush::Curve::flat, py::arg("value"));
  response.def("at", &brush::Response::at, py::arg("dab"), py::arg("pressure"),
               py::arg("speedReference"));
  dynamics.def("empty", &brush::Dynamics::empty);
  sample.def(py::init([](py::handle position, float pressure) {
               return brush::Sample{point(position), pressure};
             }),
             py::arg("position"), py::arg("pressure") = 1.0f);
  input.def(
      py::init([](py::handle position, float pressure, float tilt,
                  AngleInRadians barrelRotation, double seconds,
                  AngleInRadians tiltDirection) {
        return brush::Input{point(position),      pressure, tilt,
                            barrelRotation.value, seconds,  tiltDirection.value};
      }),
      py::arg("position"), py::arg("pressure") = 1.0f, py::arg("tilt") = 0.0f,
      py::arg("barrelRotation") = AngleInRadians{}, py::arg("seconds") = 0.0,
      py::arg("tiltDirection") = AngleInRadians{});
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
        const int count = py::module_::import("sigil._callbacks")
                              .attr("arity")(callable, 2)
                              .cast<int>();
        auto held = retainCallback(callable);
        tool.customTip = [held, count](draw::Pen& pen, const brush::Dab& dab) {
          const py::gil_scoped_acquire lock;
          const CallbackBoundary boundary;
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
}

}  // namespace sigil::python
