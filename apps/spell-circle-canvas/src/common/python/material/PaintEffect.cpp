#include <include/core/SkBlendMode.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkRuntimeEffect.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilmaterial/color/Ramp.h>
#include <sigilmaterial/core/Gradient.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Bloom.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Registration.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;

/** A gradient's stops as Python writes them: a `Ramp`, a list of
 *  `ColorStop`s or `(offset, colour)` pairs, or plain colours spaced
 *  evenly. */
material::ColorStops colorStops(py::handle values) {
  if (py::isinstance<material::Ramp>(values))
    return material::ColorStops(py::cast<const material::Ramp&>(values));
  std::vector<material::ColorStop> stops;
  std::vector<material::Color> colors;
  for (auto value : py::cast<py::iterable>(values)) {
    if (py::isinstance<material::ColorStop>(value)) {
      stops.push_back(py::cast<material::ColorStop>(value));
      continue;
    }
    if (py::isinstance<py::tuple>(value) || py::isinstance<py::list>(value)) {
      const auto pair = py::cast<py::sequence>(value);
      if (pair.size() == 2) {
        stops.push_back({py::cast<float>(pair[0]), color(pair[1])});
        continue;
      }
    }
    colors.push_back(color(value));
  }
  if (!stops.empty() && !colors.empty())
    throw py::value_error(
        "A gradient's stops are all (offset, color) pairs or all plain "
        "colors.");
  if (stops.size() + colors.size() < 2)
    throw py::value_error("A gradient needs at least two stops.");
  if (!colors.empty()) return material::ColorStops(colors);
  return material::ColorStops(std::move(stops));
}

sk_sp<SkRuntimeEffect> runtimeEffect(const std::string& source) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(source));
  if (!effect) throw py::value_error(error.c_str());
  return effect;
}

material::skia::Paint& uniform(material::skia::Paint& paint, const std::string& name,
                      py::handle value) {
  if (py::isinstance<py::int_>(value) || py::isinstance<py::float_>(value))
    return paint.uniform(name, py::cast<float>(value));
  // A colour is four floats, written as the colour class or as a CSS
  // string, and a live scalar is what makes an sksl paint animate — the
  // same two readings an effect's uniform takes, so one uniform is
  // written the same way whichever of the two seams it is set on.
  if (py::isinstance<material::Color>(value) || py::isinstance<py::str>(value))
    return paint.uniform(name, color(value));
  if (py::isinstance<motion::Animatable<float>>(value) ||
      py::isinstance<motion::Transitioned<float>>(value) ||
      py::isinstance<choreograph::Output<float>>(value) ||
      py::isinstance<motion::Bound>(value))
    return paint.uniform(name, motionAnimatable(value));
  const auto values = py::cast<std::vector<float>>(value);
  if (values.size() == 2)
    return paint.uniform(name, std::array<float, 2>{values[0], values[1]});
  if (values.size() == 4)
    return paint.uniform(
        name, std::array<float, 4>{values[0], values[1], values[2], values[3]});
  return paint.uniform(name, values);
}

material::skia::Paint sksl(py::handle effect, py::dict uniforms) {
  auto paint =
      material::skia::Paint::sksl(py::isinstance<py::str>(effect)
                             ? runtimeEffect(py::cast<std::string>(effect))
                             : py::cast<sk_sp<SkRuntimeEffect>>(effect));
  for (const auto& [name, value] : uniforms)
    uniform(paint, py::cast<std::string>(name), value);
  return paint;
}
}  // namespace

void bindMaterialPaintEffect(py::module_& module) {
  auto nativePaint = submodule(module, "material.skia");
  py::class_<material::skia::Effect>(nativePaint, "Effect")
      .def_static(
          "recipe",
          py::overload_cast<const material::Material&>(&material::skia::Effect::recipe),
          py::arg("material"))
      .def_static(
          "glow",
          [](py::object ink, float sigma) {
            return material::skia::Effect::glow(color(ink), sigma);
          },
          py::arg("ink"), py::arg("sigma"))
      .def_static("brightPass", &material::skia::Effect::brightPass,
                  py::arg("threshold") = 0.68f, py::arg("knee") = 0.30f)
      .def_static("phosphorBloom", &material::skia::Effect::phosphorBloom,
                  py::arg("radius") = 9.0f, py::arg("threshold") = 0.52f,
                  py::arg("intensity") = 0.46f, py::arg("chroma") = 0.80f,
                  py::arg("hueDrift") = 0.0f, py::arg("tail") = 0.0f)
      .def_static(
          "shader", &material::skia::Effect::shader, py::arg("effect"),
          py::arg("uniforms") = std::vector<std::pair<std::string, float>>{})
      .def_static("directionalBlur", &material::skia::Effect::directionalBlur,
                  py::arg("sigma"), py::arg("angleDegrees"),
                  py::arg("across") = 0.0f)
      .def_static("blur",
                  py::overload_cast<material::skia::Paint, float>(&material::skia::Effect::blur),
                  py::arg("sigmaMap"), py::arg("maxSigma"))
      .def_static("blur", py::overload_cast<float>(&material::skia::Effect::blur),
                  py::arg("sigma"))
      .def_static("dilate", &material::skia::Effect::dilate, py::arg("pixels"))
      .def_static("deepen", &material::skia::Effect::deepen, py::arg("amount"))
      .def_static("whiten", &material::skia::Effect::whiten, py::arg("amount"),
                  py::arg("threshold") = 0.2f, py::arg("knee") = 0.2f)
      .def("slot", &material::skia::Effect::slot, py::arg("name"), py::arg("paint"),
           fluent)
      .def(
          "uniform",
          [](material::skia::Effect& self, const std::string& name,
             py::object value) -> material::skia::Effect& {
            // A colour is four floats here as it is on a paint's
            // uniform — the colour class or a CSS string — so one
            // effect and one paint take a colour uniform written the
            // same way, as they do a live scalar and an array.
            if (py::isinstance<material::Color>(value) ||
                py::isinstance<py::str>(value)) {
              const SkColor4f tint = color(value);
              return self.uniform(
                  name,
                  std::array<float, 4>{tint.fR, tint.fG, tint.fB, tint.fA});
            }
            if (py::isinstance<py::list>(value) ||
                py::isinstance<py::tuple>(value))
              return self.uniform(name, value.cast<std::vector<float>>());
            return self.uniform(name, motionAnimatable(value));
          },
          py::arg("name"), py::arg("value"), fluent)
      .def("then", &material::skia::Effect::then, py::arg("effect"))
      .def(py::init<>())
      .def("emit", &material::skia::Effect::emit, py::arg("light"),
           py::arg("mode") = SkBlendMode::kScreen)
      .def("isAnimated", &material::skia::Effect::isAnimated)
      .def("usesWorldSpace", &material::skia::Effect::usesWorldSpace)
      .def(py::self == py::self);

  bindRecord<material::skia::BloomParameters>(nativePaint, "BloomParameters",
                                     "Unknown bloom parameter: ")
      .def_readwrite("sigma", &material::skia::BloomParameters::sigma)
      .def_readwrite("strength", &material::skia::BloomParameters::strength)
      .def_readwrite("spread", &material::skia::BloomParameters::spread)
      .def_readwrite("tail", &material::skia::BloomParameters::tail)
      .def_readwrite("threshold", &material::skia::BloomParameters::threshold)
      .def_readwrite("knee", &material::skia::BloomParameters::knee)
      .def_readwrite("softness", &material::skia::BloomParameters::softness)
      .def_readwrite("whitening", &material::skia::BloomParameters::whitening)
      .def_readwrite("dilation", &material::skia::BloomParameters::dilation)
      .def_readwrite("deepening", &material::skia::BloomParameters::deepening)
      .def_readwrite("maxOpacity", &material::skia::BloomParameters::maxOpacity);
  nativePaint.def("bloom", &material::skia::bloom,
                  py::arg("parameters") = material::skia::BloomParameters{});

  py::enum_<material::GradientUnits>(nativePaint, "GradientUnits")
      .value("Box", material::GradientUnits::Box)
      .value("Pixels", material::GradientUnits::Pixels);
  py::enum_<material::RadialExtent>(nativePaint, "RadialExtent")
      .value("FarthestCorner", material::RadialExtent::FarthestCorner)
      .value("ClosestSide", material::RadialExtent::ClosestSide);
  py::enum_<material::Repeat>(nativePaint, "Repeat")
      .value("Pad", material::Repeat::Pad)
      .value("Repeat", material::Repeat::Repeat)
      .value("Mirror", material::Repeat::Mirror)
      .value("None_", material::Repeat::None);
  bindRecord<material::GradientOptions>(nativePaint, "GradientOptions",
                                        "Unknown GradientOptions field: ")
      .def_readwrite("units", &material::GradientOptions::units)
      .def_readwrite("extent", &material::GradientOptions::extent)
      .def_readwrite("repeat", &material::GradientOptions::repeat)
      .def_property(
          "focus",
          [](const material::GradientOptions& options) -> py::object {
            if (!options.focus) return py::none();
            return py::make_tuple(options.focus->x, options.focus->y);
          },
          [](material::GradientOptions& options, py::handle value) {
            if (value.is_none()) {
              options.focus.reset();
              return;
            }
            const SkPoint at = point(value);
            options.focus = glm::vec2{at.x(), at.y()};
          })
      .def_readwrite("focusRadius", &material::GradientOptions::focusRadius)
      .def_readwrite("startDegrees", &material::GradientOptions::startDegrees)
      .def_readwrite("endDegrees", &material::GradientOptions::endDegrees)
      .def(py::self == py::self);
  py::enum_<material::skia::Fit>(nativePaint, "Fit")
      .value("Contain", material::skia::Fit::Contain)
      .value("Cover", material::skia::Fit::Cover)
      .value("Stretch", material::skia::Fit::Stretch)
      .value("Native", material::skia::Fit::Native);
  py::class_<material::skia::Paint>(nativePaint, "Paint")
      .def(py::init<>())
      .def(py::init<const material::skia::Paint&>(), py::arg("paint"))
      .def(py::init([](const material::Material& recipe) {
             return material::skia::Paint::recipe(recipe);
           }),
           py::arg("material"))
      .def("copy", [](const material::skia::Paint& paint) { return paint; })
      .def_static(
          "solid",
          [](py::handle value) {
            return material::skia::Paint::solid(color(value));
          },
          py::arg("color"))
      .def_static(
          "linearGradient",
          [](py::handle start, py::handle end, py::handle stops,
             const material::GradientOptions& options) {
            return material::skia::Paint::linearGradient(
                point(start), point(end), colorStops(stops), options);
          },
          py::arg("start"), py::arg("end"), py::arg("stops"),
          py::arg("options") = material::GradientOptions{})
      .def_static(
          "radialGradient",
          [](py::handle center, float radius, py::handle stops,
             const material::GradientOptions& options) {
            return material::skia::Paint::radialGradient(
                point(center), radius, colorStops(stops), options);
          },
          py::arg("center"), py::arg("radius"), py::arg("stops"),
          py::arg("options") = material::GradientOptions{})
      .def_static(
          "conicGradient",
          [](py::handle center, py::handle stops,
             const material::GradientOptions& options) {
            return material::skia::Paint::conicGradient(
                point(center), colorStops(stops), options);
          },
          py::arg("center"), py::arg("stops"),
          py::arg("options") = material::GradientOptions{})
      .def_static(
          "image",
          [](sk_sp<SkImage> image, SkTileMode tileX, SkTileMode tileY,
             const SkMatrix& local) {
            return material::skia::Paint::image(std::move(image), tileX, tileY, local);
          },
          py::arg("image"), py::arg("tileX") = SkTileMode::kClamp,
          py::arg("tileY") = SkTileMode::kClamp,
          py::arg("local") = SkMatrix::I())
      .def_static("sksl", &sksl, py::arg("effect"),
                  py::arg("uniforms") = py::dict())
      .def_static("recipe", &material::skia::Paint::recipe, py::arg("material"))
      .def_static("blend", &material::skia::Paint::blend, py::arg("layers"))
      .def("uniform", &uniform, py::arg("name"), py::arg("value"), fluent)
      .def("slot", &material::skia::Paint::slot, py::arg("name"),
           py::arg("paint"), fluent)
      .def("amount", &material::skia::Paint::amount, py::arg("amount"), fluent)
      .def("fit", &material::skia::Paint::fit, py::arg("fit"), fluent)
      .def("worldSpace",
           py::overload_cast<bool>(&material::skia::Paint::worldSpace),
           py::arg("on") = true, fluent)
      .def("quantizeTime", &material::skia::Paint::quantizeTime,
           py::arg("rate"), fluent)
      .def("isAnimated", &material::skia::Paint::isAnimated)
      .def("isNone", &material::skia::Paint::isNone)
      .def(py::self == py::self);
  // A recipe instance is one kind of paint, so everything that takes a
  // paint takes a material: a slot, a blend layer, an effect's source.
  // The native constructor stays spelled, because a C++ overload set
  // holding both would become ambiguous.
  py::implicitly_convertible<material::Material, material::skia::Paint>();
}

}  // namespace sigil::python
