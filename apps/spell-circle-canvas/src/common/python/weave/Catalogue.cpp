/** @file
 * `weave.fonts`: the type catalogue, as Python reads it — family names,
 * a family's styles and axes, the face a family names at a style, and the
 * faces a font context adopted among them.
 */

#include <pybind11/stl.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/weave/FontContext.h>
#include <sigilpython/weave/Registration.h>
#include <sigilweave/ports/Catalogue.h>

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace sigil::python {

namespace py = pybind11;

namespace {

const char* slantWord(weave::FaceSlant slant) {
  switch (slant) {
    case weave::FaceSlant::Italic:
      return "italic";
    case weave::FaceSlant::Oblique:
      return "oblique";
    case weave::FaceSlant::Upright:
      break;
  }
  return "upright";
}

/** One number as a specimen prints it: whole where it is whole. */
std::string number(float value) {
  char text[32];
  std::snprintf(text, sizeof text, "%g", static_cast<double>(value));
  return text;
}

std::string styleLine(const weave::fonts::Style& style) {
  std::string line = style.name + " · " + std::to_string(style.weight) + " " +
                     style.weightName + " · width " +
                     std::to_string(style.width) + " · " +
                     slantWord(style.slant);
  if (!style.uri.empty()) line += " · " + style.uri;
  return line;
}

std::string familyLine(const weave::fonts::Family& family) {
  std::string line = family.name + " · " +
                     std::to_string(family.styles.size()) +
                     (family.styles.size() == 1 ? " style: " : " styles: ");
  for (size_t index = 0; index < family.styles.size(); ++index) {
    if (index > 0) line += ", ";
    line += family.styles[index].name;
  }
  if (!family.axes.empty()) {
    line += " · axes";
    for (const weave::VariationAxis& axis : family.axes)
      line += " " + std::string(axis.tag, 4) + " " + number(axis.minimum) +
              "–" + number(axis.maximum);
  }
  line += family.bundled ? " · bundled" : " · installed";
  return line;
}

}  // namespace

void bindWeaveCatalogue(py::module_& module) {
  auto weaveModule = module.attr("weave").cast<py::module_>();
  auto fonts = weaveModule.def_submodule(
      "fonts",
      "The type this program can set in: the families the platform has, "
      "each with its styles and variation axes, and the face a family "
      "names at a style. Handed a font context — a sketch's ctx.fonts — "
      "the faces its hub loaded stand among them, marked bundled.");
  py::class_<weave::fonts::Style>(
      fonts, "Style",
      "One face of a family: its CSS weight and the weight's name, its width "
      "class, its slant, the font's own name for it, and the resource a "
      "bundled face was read from.")
      .def_readonly("weight", &weave::fonts::Style::weight)
      .def_readonly("weightName", &weave::fonts::Style::weightName)
      .def_readonly("width", &weave::fonts::Style::width)
      .def_readonly("slant", &weave::fonts::Style::slant)
      .def_readonly("name", &weave::fonts::Style::name)
      .def_readonly("uri", &weave::fonts::Style::uri)
      .def(
          "faceStyle",
          [](const weave::fonts::Style& style) {
            return weave::FaceStyle{.weight = style.weight,
                                    .width = style.width,
                                    .slant = style.slant};
          },
          "The style as the face style `find` asks for.")
      .def(
          "__eq__",
          [](const weave::fonts::Style& left,
             const weave::fonts::Style& right) { return left == right; },
          py::arg("other"))
      .def("__repr__", [](const weave::fonts::Style& style) {
        return "Style(" + styleLine(style) + ")";
      });
  py::class_<weave::fonts::Family>(
      fonts, "Family",
      "One family: its name, every style it has, the variation axes its "
      "faces carry, and whether its faces are bundled — read from files a "
      "hub loaded — rather than installed.")
      .def_readonly("name", &weave::fonts::Family::name)
      .def_readonly("styles", &weave::fonts::Family::styles)
      .def_readonly("axes", &weave::fonts::Family::axes)
      .def_readonly("bundled", &weave::fonts::Family::bundled)
      .def(
          "__eq__",
          [](const weave::fonts::Family& left,
             const weave::fonts::Family& right) { return left == right; },
          py::arg("other"))
      .def("__repr__", [](const weave::fonts::Family& family) {
        return "Family(" + familyLine(family) + ")";
      });
  fonts.def(
      "families",
      [](const FontContextHandle* context) {
        return context ? weave::fonts::families(context->get())
                       : weave::fonts::families();
      },
      py::arg("context") = py::none(),
      "The family names the platform has; with a font context, the "
      "families it adopted first, then its manager's.");
  fonts.def(
      "family",
      [](const std::string& name, const FontContextHandle* context) {
        return context ? weave::fonts::family(name, context->get())
                       : weave::fonts::family(name);
      },
      py::arg("name"), py::arg("context") = py::none(),
      "The family under the name, with its styles and axes, or None where "
      "there is none. With a font context, a family it adopted answers "
      "before an installed one.");
  fonts.def(
      "find",
      [](const std::string& family, const weave::FaceStyle& style,
         const FontContextHandle* context) {
        return context ? weave::fonts::find(family, style, context->get())
                       : weave::fonts::find(family, style);
      },
      py::arg("family"), py::arg("style") = weave::FaceStyle{},
      py::arg("context") = py::none(),
      "The face the family names nearest the style, or None where no "
      "family has the name.");
  fonts.def(
      "weightName",
      [](int weight) { return std::string(weave::fonts::weightName(weight)); },
      py::arg("weight"),
      "CSS's name for a weight, to the nearest hundred: 600 is Semi Bold.");
}

}  // namespace sigil::python
