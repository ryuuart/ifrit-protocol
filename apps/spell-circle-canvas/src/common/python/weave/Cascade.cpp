#include <pybind11/stl.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/weave/Registration.h>
#include <sigilweave/kit/Features.h>
#include <sigilweave/kit/Labels.h>
#include <sigilweave/style/Type.h>

#include <optional>
#include <string>

namespace sigil::python {
namespace py = pybind11;

namespace {

/** Publishes @p value as the attribute @p name of @p features. A preset
 *  is a chosen tag and value rather than a calculation, so it stands in
 *  the module as the feature it is. The attribute holds a copy of the
 *  native constant, and it is the module's one copy: an author who edits
 *  the value on it edits what every later reader of that name sees, so a
 *  caller who wants a different value copies the preset first or spells
 *  the tag. */
void preset(py::module_& features, const char* name,
            const weave::FontFeature& value) {
  features.attr(name) = py::cast(value);
}

}  // namespace

void bindWeaveCascade(py::module_& module) {
  auto text = submodule(module, "weave");
  auto features = submodule(module, "weave.features");
  auto kit = submodule(module, "weave.kit");

  text.def("reshapes", &weave::reshapes, py::arg("partial"))
      .def("defaultFace", &weave::defaultFace)
      .def("toTextStyle", &weave::toTextStyle, py::arg("total"));

  // A style lists the features it wants, so a set is where a caller
  // collects them, and two features built from the same tag and value are
  // one member of it: the hash is taken from the pair equality is taken
  // from. The copy protocol beside it is what lets a preset be carried
  // out of the module and edited without editing the module's own.
  auto feature = extend<weave::FontFeature>(module, "weave.FontFeature");
  copyProtocol(feature).def("__hash__", [](const weave::FontFeature& value) {
    return py::hash(py::make_tuple(py::str(value.tag, 4), value.value));
  });

  preset(features, "tabularNumbers", weave::features::tabularNumbers);
  preset(features, "proportionalNumbers", weave::features::proportionalNumbers);
  preset(features, "oldstyleNumbers", weave::features::oldstyleNumbers);
  preset(features, "liningNumbers", weave::features::liningNumbers);
  preset(features, "slashedZero", weave::features::slashedZero);
  preset(features, "fractions", weave::features::fractions);
  preset(features, "ordinals", weave::features::ordinals);
  preset(features, "smallCaps", weave::features::smallCaps);
  preset(features, "capitalsToSmallCaps", weave::features::capitalsToSmallCaps);
  preset(features, "standardLigaturesOff",
         weave::features::standardLigaturesOff);
  preset(features, "contextualLigaturesOff",
         weave::features::contextualLigaturesOff);
  preset(features, "discretionaryLigatures",
         weave::features::discretionaryLigatures);
  preset(features, "historicalLigatures",
         weave::features::historicalLigatures);
  preset(features, "contextualAlternatesOff",
         weave::features::contextualAlternatesOff);
  preset(features, "swashes", weave::features::swashes);
  preset(features, "verticalFormsOff", weave::features::verticalFormsOff);
  preset(features, "verticalRotatedForms",
         weave::features::verticalRotatedForms);
  preset(features, "verticalAlternates", weave::features::verticalAlternates);
  preset(features, "proportionalVerticalMetrics",
         weave::features::proportionalVerticalMetrics);
  preset(features, "halfWidthVerticalMetrics",
         weave::features::halfWidthVerticalMetrics);
  preset(features, "verticalKana", weave::features::verticalKana);
  preset(features, "verticalKerning", weave::features::verticalKerning);
  features.def("stylisticSet", &weave::features::stylisticSet,
               py::arg("index"));

  kit.def(
         "makeStyle",
         [](float fontSize, SkColor4f color, const std::string& language,
            const std::optional<sk_sp<SkTypeface>>& typeface) {
           // The native call states its colour as one 8-bit word, so a
           // colour given here is quantised on the way in whatever it was
           // spelled as.
           return weave::kit::makeStyle(fontSize, color.toSkColor(),
                                        language.c_str(),
                                        typeface.value_or(nullptr));
         },
         py::arg("fontSize"), py::arg("color"), py::arg("language") = "",
         py::arg("typeface") = py::none())
      .def(
          "tracked",
          [](const std::optional<sk_sp<SkTypeface>>& face, float size,
             SkColor4f color, float trackPerMille, float condense) {
            return weave::kit::tracked(face.value_or(nullptr), size, color,
                                       trackPerMille, condense);
          },
          py::arg("face"), py::arg("size"), py::arg("color"),
          py::arg("trackPerMille") = 0.0f, py::arg("condense") = 1.0f);
}

}  // namespace sigil::python
