/** @file
 * A Substance archive as a Python Material: `material.substance(hub, uri,
 * **inputs)` with the tier-2 options as keywords, and `material.sbsar`
 * for what describes a graph — the description, its inputs and outputs,
 * whether this build cooks at all, and the wait for pending cooks. An
 * input keyword may be the identifier as authored (`Hue_Shift`) or in
 * Python's spelling (`hue_shift`): letters and digits are compared with
 * case and underscores set aside. The graph's inputs are then written
 * and followed with the Material's own `set` and `bind`.
 */

#include <pybind11/stl.h>
#include <sigilmaterial/substance/Substance.h>
#include <sigilpython/Extend.h>
#include <sigilpython/io/Hub.h>
#include <sigilpython/material/Registration.h>

#include <cctype>
#include <string>
#include <vector>

namespace sigil::python {

namespace py = pybind11;

namespace {

std::string folded(std::string_view name) {
  std::string out;
  for (const char character : name)
    if (std::isalnum((unsigned char)character))
      out += (char)std::tolower((unsigned char)character);
  return out;
}

std::vector<float> numbers(py::handle value) {
  if (py::isinstance<py::bool_>(value) || py::isinstance<py::int_>(value) ||
      py::isinstance<py::float_>(value))
    return {value.cast<float>()};
  std::vector<float> out;
  for (py::handle item : py::iter(value)) out.push_back(item.cast<float>());
  return out;
}

material::Material substance(const HubHandle& hub, const std::string& uri,
                             const std::string& preset, py::object seed,
                             int resolution, py::iterable outputs,
                             const std::string& graph, py::kwargs inputs) {
  material::SubstanceOptions options;
  options.preset = preset;
  if (!seed.is_none()) options.seed = seed.cast<int>();
  options.resolution = resolution;
  options.graph = graph;
  for (py::handle output : outputs) {
    if (py::isinstance<material::sbsar::OutputRequest>(output))
      options.outputs.push_back(output.cast<material::sbsar::OutputRequest>());
    else
      options.outputs.push_back({output.cast<std::string>()});
  }
  if (inputs.size() > 0) {
    // A keyword names an input by its identifier or by Python's spelling
    // of it; the description says which identifier that is.
    const material::sbsar::Description described =
        material::sbsar::describe(hub.get(), uri, graph);
    for (const auto& [key, value] : inputs) {
      std::string identifier = key.cast<std::string>();
      const std::string wanted = folded(identifier);
      for (const material::sbsar::Input& input : described.inputs)
        if (folded(input.name) == wanted) {
          identifier = input.name;
          break;
        }
      options.inputs.emplace_back(identifier, numbers(value));
    }
  }
  return material::substance(hub.get(), uri, std::move(options));
}

}  // namespace

void bindMaterialSubstance(py::module_& module) {
  using namespace material::sbsar;
  auto sbsar = submodule(module, "material.sbsar");
  py::enum_<InputType>(sbsar, "InputType")
      .value("Float", InputType::Float)
      .value("Float2", InputType::Float2)
      .value("Float3", InputType::Float3)
      .value("Float4", InputType::Float4)
      .value("Integer", InputType::Integer)
      .value("Integer2", InputType::Integer2)
      .value("Integer3", InputType::Integer3)
      .value("Integer4", InputType::Integer4)
      .value("Image", InputType::Image)
      .value("Text", InputType::Text)
      .value("Other", InputType::Other);
  py::enum_<Widget>(sbsar, "Widget")
      .value("Unspecified", Widget::Unspecified)
      .value("Slider", Widget::Slider)
      .value("Angle", Widget::Angle)
      .value("Color", Widget::Color)
      .value("Toggle", Widget::Toggle)
      .value("Buttons", Widget::Buttons)
      .value("Combobox", Widget::Combobox)
      .value("Image", Widget::Image)
      .value("Position", Widget::Position);
  py::enum_<Encoding>(sbsar, "Encoding")
      .value("Srgb", Encoding::Srgb)
      .value("Linear", Encoding::Linear)
      .value("Raw", Encoding::Raw);
  py::enum_<Format>(sbsar, "Format")
      .value("Automatic", Format::Automatic)
      .value("Unorm8", Format::Unorm8)
      .value("Unorm16", Format::Unorm16)
      .value("Float16", Format::Float16)
      .value("Float32", Format::Float32);
  py::class_<Choice>(sbsar, "Choice")
      .def_readonly("value", &Choice::value)
      .def_readonly("label", &Choice::label);
  py::class_<Input>(sbsar, "Input")
      .def_readonly("name", &Input::name)
      .def_readonly("label", &Input::label)
      .def_readonly("group", &Input::group)
      .def_readonly("description", &Input::description)
      .def_readonly("type", &Input::type)
      .def_readonly("widget", &Input::widget)
      .def_readonly("default_value", &Input::defaultValue)
      .def_readonly("minimum", &Input::minimum)
      .def_readonly("maximum", &Input::maximum)
      .def_readonly("step", &Input::step)
      .def_readonly("clamp", &Input::clamp)
      .def_readonly("choices", &Input::choices)
      .def_readonly("visible_if", &Input::visibleIf);
  py::class_<Output>(sbsar, "Output")
      .def_readonly("name", &Output::name)
      .def_readonly("label", &Output::label)
      .def_readonly("usage", &Output::usage)
      .def_readonly("encoding", &Output::encoding)
      .def_readonly("image", &Output::image);
  py::class_<Description>(sbsar, "Description")
      .def_readonly("graph", &Description::graph)
      .def_readonly("inputs", &Description::inputs)
      .def_readonly("presets", &Description::presets)
      .def_readonly("outputs", &Description::outputs);
  py::class_<OutputRequest>(sbsar, "OutputRequest")
      .def(py::init([](std::string usage, Format format) {
             return OutputRequest{std::move(usage), format};
           }),
           py::arg("usage"), py::arg("format") = Format::Automatic)
      .def_readwrite("usage", &OutputRequest::usage)
      .def_readwrite("format", &OutputRequest::format);
  sbsar.def("available", &available,
            "Whether this build found the Substance SDK and its engine "
            "starts.");
  sbsar.def(
      "describe",
      [](const HubHandle& hub, const std::string& uri,
         const std::string& graph) { return describe(hub.get(), uri, graph); },
      py::arg("hub"), py::arg("uri"), py::arg("graph") = "",
      "The graph at uri described: its inputs, presets and outputs. Empty "
      "when the archive cannot be read or there is no SDK.");
  sbsar.def("settle", &settle, py::arg("material"),
            "Holds until every cook the material scheduled has landed.");

  auto materials = submodule(module, "material");
  materials.def("substance", &substance, py::arg("hub"), py::arg("uri"),
                py::kw_only(), py::arg("preset") = "",
                py::arg("seed") = py::none(), py::arg("resolution") = 0,
                py::arg("outputs") = py::tuple(), py::arg("graph") = "",
                "The Substance graph at uri, cooked, as a Material. Keywords "
                "past the options are the graph's inputs by identifier.");
}

}  // namespace sigil::python
