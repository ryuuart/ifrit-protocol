/** @file
 * A shader as a Python Material: `material.shader(source, parameters,
 * key=, target=, sampling=, textures=)` and the same over a file read
 * through a hub. The parameters are a dict or a NamedTuple whose fields are
 * the body's uniforms, in the order given: a number is a float, a pair a
 * float2, a `Color` a colour, four numbers a float4, nine a float3x3 and
 * any other count an array. Typed motion values declare a scalar or color
 * field and keep their binding through construction and copies.
 */

#include <include/core/SkImage.h>
#include <pybind11/stl.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/Image.h>
#include <sigilpython/Extend.h>
#include <sigilpython/io/Hub.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/material/Registration.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sigil::python {

namespace py = pybind11;

namespace {

/** One uniform's layout, initial components and optional typed binding. */
struct Uniform {
  material::Field field;
  std::vector<float> floats;
  std::variant<std::monostate, motion::Animatable<float>,
               motion::Animatable<material::Color>>
      binding;
};

Uniform uniformOf(const std::string& name, py::handle value) {
  Uniform out;
  out.field.name = name;
  if (py::isinstance<motion::Animatable<material::Color>>(value) ||
      py::isinstance<motion::Tween<material::Color>>(value)) {
    const auto binding = motionInk(value);
    const auto color = binding.value();
    out.field.kind = material::ParameterType::Color;
    out.floats = {color.r, color.g, color.b, color.a};
    out.binding = binding;
  } else if (py::isinstance<motion::Animatable<float>>(value) ||
             py::isinstance<motion::Tween<float>>(value)) {
    const auto binding = motionAnimatable(value);
    out.field.kind = material::ParameterType::Float;
    out.floats = {binding.value()};
    out.binding = binding;
  } else if (py::isinstance<material::Color>(value) ||
             py::isinstance<py::str>(value)) {
    const auto color = materialColor(value);
    out.field.kind = material::ParameterType::Color;
    out.floats = {color.r, color.g, color.b, color.a};
  } else if (py::isinstance<py::bool_>(value) ||
             py::isinstance<py::int_>(value) ||
             py::isinstance<py::float_>(value)) {
    out.field.kind = material::ParameterType::Float;
    out.floats = {value.cast<float>()};
  } else {
    for (py::handle item : py::iter(value))
      out.floats.push_back(item.cast<float>());
    switch (out.floats.size()) {
      case 2:
        out.field.kind = material::ParameterType::Vec2;
        break;
      case 4:
        out.field.kind = material::ParameterType::Vec4;
        break;
      case 9:
        out.field.kind = material::ParameterType::Mat3;
        break;
      default:
        out.field.kind = material::ParameterType::FloatArray;
        break;
    }
  }
  out.field.floats = out.floats.size();
  return out;
}

/** The uniforms of @p parameters in the order given: a dict's items, or a
 *  NamedTuple's fields. */
std::vector<Uniform> uniformsOf(py::handle parameters) {
  std::vector<Uniform> out;
  if (parameters.is_none()) return out;
  py::dict items = py::hasattr(parameters, "_asdict")
                       ? py::dict(parameters.attr("_asdict")())
                       : parameters.cast<py::dict>();
  for (const auto& [key, value] : items)
    out.push_back(uniformOf(key.cast<std::string>(), value));
  return out;
}

/** The layout of @p uniforms, laid out as a struct of them would be. */
material::Schema layoutOf(const std::vector<Uniform>& uniforms) {
  std::vector<material::Field> fields;
  for (const Uniform& uniform : uniforms) fields.push_back(uniform.field);
  return material::packedSchema(std::move(fields));
}

media::PixelSource pixelsOf(py::handle value) {
  if (value.is_none()) return {};
  if (py::isinstance<media::Image>(value))
    return media::PixelSource(std::shared_ptr<const media::Image>(
        value.cast<std::shared_ptr<media::Image>>()));
  return media::PixelSource(value.cast<sk_sp<SkImage>>());
}

material::ShaderOptions optionsOf(const std::string& key,
                                  std::optional<material::Target> target,
                                  std::optional<material::Sampling> sampling,
                                  py::object textures) {
  material::ShaderOptions options{
      .key = key, .target = target, .sampling = sampling};
  if (!textures.is_none())
    for (const auto& [name, pixels] : textures.cast<py::dict>())
      options.textures.push_back({name.cast<std::string>(), pixelsOf(pixels)});
  return options;
}

material::Material instanced(std::shared_ptr<const material::Recipe> definition,
                             const std::vector<Uniform>& uniforms,
                             const material::ShaderOptions& options) {
  if (!definition) return material::Color{0, 0, 0, 0};
  material::Material made(std::move(definition));
  for (const Uniform& uniform : uniforms) {
    made.set(uniform.field.name, std::span<const float>(uniform.floats));
    if (const auto* scalar =
            std::get_if<motion::Animatable<float>>(&uniform.binding))
      made.bind(uniform.field.name, *scalar);
    else if (const auto* color =
                 std::get_if<motion::Animatable<material::Color>>(
                     &uniform.binding))
      made.bind(uniform.field.name, *color);
  }
  return material::detail::withTextures(std::move(made), options);
}

}  // namespace

void bindMaterialShader(py::module_& module) {
  auto materials = submodule(module, "material");
  py::enum_<material::Target>(materials, "Target")
      .value("SkSL", material::Target::SkSL)
      .value("Slang", material::Target::Slang);
  py::enum_<material::Sampling>(materials, "Sampling")
      .value("Nearest", material::Sampling::Nearest)
      .value("Linear", material::Sampling::Linear);
  materials.def(
      "shader",
      [](const HubHandle& hub, const std::string& uri, py::object parameters,
         const std::string& key, std::optional<material::Target> target,
         std::optional<material::Sampling> sampling, py::object textures) {
        const std::vector<Uniform> uniforms = uniformsOf(parameters);
        const material::ShaderOptions options =
            optionsOf(key, target, sampling, textures);
        std::shared_ptr<const material::Recipe> definition =
            material::detail::shaderFileDefinition(hub.get(), uri,
                                                   layoutOf(uniforms), options);
        if (!definition) return material::placeholder();
        return instanced(std::move(definition), uniforms, options);
      },
      py::arg("hub"), py::arg("uri"), py::arg("parameters") = py::none(),
      py::kw_only(), py::arg("key") = "", py::arg("target") = py::none(),
      py::arg("sampling") = py::none(), py::arg("textures") = py::none(),
      "The program file at uri, read through hub, as a Material whose "
      "uniforms are the parameters' fields. A text that does not compile "
      "never replaces one that did; while none has, placeholder() paints; "
      "what is wrong stands on hub.problems() under uri.");
  materials.def("placeholder", &material::placeholder,
                "The magenta and black checker a program file paints while "
                "none of its texts has compiled: a diagnostic, never a look.");
  materials.def(
      "shader",
      [](const std::string& source, py::object parameters,
         const std::string& key, std::optional<material::Target> target,
         std::optional<material::Sampling> sampling, py::object textures) {
        const std::vector<Uniform> uniforms = uniformsOf(parameters);
        const material::ShaderOptions options =
            optionsOf(key, target, sampling, textures);
        return instanced(material::detail::shaderDefinition(
                             source, layoutOf(uniforms), options),
                         uniforms, options);
      },
      py::arg("source"), py::arg("parameters") = py::none(), py::kw_only(),
      py::arg("key") = "", py::arg("target") = py::none(),
      py::arg("sampling") = py::none(), py::arg("textures") = py::none(),
      "The shader source as a Material whose uniforms are the parameters' "
      "fields: a dict or a NamedTuple, in order. SkSL unless target says "
      "Slang; textures maps a name the body samples to an image.");
}

}  // namespace sigil::python
