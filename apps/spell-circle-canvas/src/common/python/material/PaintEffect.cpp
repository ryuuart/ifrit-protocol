#include <include/core/SkBlendMode.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkRuntimeEffect.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
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
namespace mskia = material::skia;
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;

std::vector<mskia::Stop> stops(py::iterable values) {
  std::vector<mskia::Stop> result;
  for (auto value : values) {
    const auto pair = py::cast<py::sequence>(value);
    if (pair.size() != 2)
      throw py::value_error("A gradient stop is (position, color).");
    result.push_back({py::cast<float>(pair[0]), color(pair[1])});
  }
  if (result.size() < 2)
    throw py::value_error("A gradient needs at least two stops.");
  return result;
}

sk_sp<SkRuntimeEffect> runtimeEffect(const std::string& source) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(source));
  if (!effect) throw py::value_error(error.c_str());
  return effect;
}

mskia::Paint& uniform(mskia::Paint& paint, const std::string& name,
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

mskia::Paint sksl(py::handle effect, py::dict uniforms) {
  auto paint =
      mskia::Paint::sksl(py::isinstance<py::str>(effect)
                             ? runtimeEffect(py::cast<std::string>(effect))
                             : py::cast<sk_sp<SkRuntimeEffect>>(effect));
  for (const auto& [name, value] : uniforms)
    uniform(paint, py::cast<std::string>(name), value);
  return paint;
}
}  // namespace

void bindMaterialPaintEffect(py::module_& module) {
  auto nativePaint = submodule(module, "material.skia");
  py::class_<mskia::Effect>(nativePaint, "Effect")
      .def_static(
          "recipe",
          py::overload_cast<const material::Material&>(&mskia::Effect::recipe),
          py::arg("material"))
      .def_static(
          "glow",
          [](py::object ink, float sigma) {
            return mskia::Effect::glow(color(ink), sigma);
          },
          py::arg("ink"), py::arg("sigma"))
      .def_static("brightPass", &mskia::Effect::brightPass,
                  py::arg("threshold") = 0.68f, py::arg("knee") = 0.30f)
      .def_static("phosphorBloom", &mskia::Effect::phosphorBloom,
                  py::arg("radius") = 9.0f, py::arg("threshold") = 0.52f,
                  py::arg("intensity") = 0.46f, py::arg("chroma") = 0.80f,
                  py::arg("hueDrift") = 0.0f, py::arg("tail") = 0.0f)
      .def_static(
          "shader", &mskia::Effect::shader, py::arg("effect"),
          py::arg("uniforms") = std::vector<std::pair<std::string, float>>{})
      .def_static("directionalBlur", &mskia::Effect::directionalBlur,
                  py::arg("sigma"), py::arg("angleDeg"),
                  py::arg("across") = 0.0f)
      .def_static("blur",
                  py::overload_cast<mskia::Paint, float>(&mskia::Effect::blur),
                  py::arg("sigmaMap"), py::arg("maxSigma"))
      .def_static("blur", py::overload_cast<float>(&mskia::Effect::blur),
                  py::arg("sigma"))
      .def_static("dilate", &mskia::Effect::dilate, py::arg("pixels"))
      .def_static("deepen", &mskia::Effect::deepen, py::arg("amount"))
      .def_static("whiten", &mskia::Effect::whiten, py::arg("amount"),
                  py::arg("threshold") = 0.2f, py::arg("knee") = 0.2f)
      .def("slot", &mskia::Effect::slot, py::arg("name"), py::arg("paint"),
           fluent)
      .def(
          "uniform",
          [](mskia::Effect& self, const std::string& name,
             py::object value) -> mskia::Effect& {
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
      .def("then", &mskia::Effect::then, py::arg("effect"))
      .def(py::init<>())
      .def("emit", &mskia::Effect::emit, py::arg("light"),
           py::arg("mode") = SkBlendMode::kScreen)
      .def("isAnimated", &mskia::Effect::isAnimated)
      .def("usesWorldSpace", &mskia::Effect::usesWorldSpace)
      .def(py::self == py::self);

  bindRecord<mskia::BloomParameters>(nativePaint, "BloomParameters",
                                     "Unknown bloom parameter: ")
      .def_readwrite("sigma", &mskia::BloomParameters::sigma)
      .def_readwrite("strength", &mskia::BloomParameters::strength)
      .def_readwrite("spread", &mskia::BloomParameters::spread)
      .def_readwrite("tail", &mskia::BloomParameters::tail)
      .def_readwrite("threshold", &mskia::BloomParameters::threshold)
      .def_readwrite("knee", &mskia::BloomParameters::knee)
      .def_readwrite("softness", &mskia::BloomParameters::softness)
      .def_readwrite("whitening", &mskia::BloomParameters::whitening)
      .def_readwrite("dilation", &mskia::BloomParameters::dilation)
      .def_readwrite("deepening", &mskia::BloomParameters::deepening)
      .def_readwrite("maxOpacity", &mskia::BloomParameters::maxOpacity);
  nativePaint.def("bloom", &mskia::bloom,
                  py::arg("parameters") = mskia::BloomParameters{});

  py::enum_<mskia::Fit>(nativePaint, "Fit")
      .value("Contain", mskia::Fit::Contain)
      .value("Cover", mskia::Fit::Cover)
      .value("Stretch", mskia::Fit::Stretch)
      .value("Native", mskia::Fit::Native);
  py::class_<mskia::Paint>(nativePaint, "Paint")
      .def(py::init<>())
      .def(py::init<const mskia::Paint&>(), py::arg("paint"))
      .def(py::init([](const material::Material& recipe) {
             return mskia::Paint::recipe(recipe);
           }),
           py::arg("material"))
      .def("copy", [](const mskia::Paint& paint) { return paint; })
      .def_static(
          "solid",
          [](py::handle value) { return mskia::Paint::solid(color(value)); },
          py::arg("color"))
      .def_static(
          "linear",
          [](py::handle from, py::handle to, py::iterable gradient,
             SkTileMode tile) {
            return mskia::Paint::linear(point(from), point(to),
                                        stops(gradient), tile);
          },
          py::arg("from_"), py::arg("to"), py::arg("stops"),
          py::arg("tile") = SkTileMode::kClamp)
      .def_static(
          "radial",
          [](py::handle center, float radius, py::iterable gradient,
             SkTileMode tile) {
            return mskia::Paint::radial(point(center), radius, stops(gradient),
                                        tile);
          },
          py::arg("center"), py::arg("radius"), py::arg("stops"),
          py::arg("tile") = SkTileMode::kClamp)
      .def_static(
          "conical",
          [](py::handle start, float startRadius, py::handle end,
             float endRadius, py::iterable gradient) {
            return mskia::Paint::conical(point(start), startRadius, point(end),
                                         endRadius, stops(gradient));
          },
          py::arg("start"), py::arg("startRadius"), py::arg("end"),
          py::arg("endRadius"), py::arg("stops"))
      .def_static(
          "sweep",
          [](py::handle center, py::iterable gradient, float start,
             float end) {
            return mskia::Paint::sweep(point(center), stops(gradient), start,
                                       end);
          },
          py::arg("center"), py::arg("stops"), py::arg("start") = 0,
          py::arg("end") = 360)
      .def_static(
          "linearUnit",
          [](py::handle start, py::handle end, py::iterable gradient) {
            return mskia::Paint::linearUnit(point(start), point(end),
                                            stops(gradient));
          },
          py::arg("start"), py::arg("end"), py::arg("stops"))
      .def_static(
          "radialUnit",
          [](py::handle center, float radius, py::iterable gradient) {
            return mskia::Paint::radialUnit(point(center), radius,
                                            stops(gradient));
          },
          py::arg("center"), py::arg("radius"), py::arg("stops"))
      .def_static(
          "glowUnit",
          [](py::handle center, float radius, py::iterable gradient) {
            return mskia::Paint::glowUnit(point(center), radius,
                                          stops(gradient));
          },
          py::arg("center"), py::arg("radius"), py::arg("stops"))
      .def_static(
          "image",
          [](sk_sp<SkImage> image, SkTileMode tileX, SkTileMode tileY,
             const SkMatrix& local) {
            return mskia::Paint::image(std::move(image), tileX, tileY, local);
          },
          py::arg("image"), py::arg("tileX") = SkTileMode::kClamp,
          py::arg("tileY") = SkTileMode::kClamp,
          py::arg("local") = SkMatrix::I())
      .def_static("sksl", &sksl, py::arg("effect"),
                  py::arg("uniforms") = py::dict())
      .def_static("recipe", &mskia::Paint::recipe, py::arg("material"))
      .def_static("blend", &mskia::Paint::blend, py::arg("layers"))
      .def("uniform", &uniform, py::arg("name"), py::arg("value"), fluent)
      .def("slot", &mskia::Paint::slot, py::arg("name"), py::arg("paint"),
           fluent)
      .def("amount", &mskia::Paint::amount, py::arg("amount"), fluent)
      .def("fit", &mskia::Paint::fit, py::arg("fit"), fluent)
      .def("worldSpace", py::overload_cast<bool>(&mskia::Paint::worldSpace),
           py::arg("on") = true, fluent)
      .def("quantizeTime", &mskia::Paint::quantizeTime, py::arg("hz"), fluent)
      .def("isAnimated", &mskia::Paint::isAnimated)
      .def("isNone", &mskia::Paint::isNone)
      .def(py::self == py::self);
  // A recipe instance is one kind of paint, so everything that takes a
  // paint takes a material: a slot, a blend layer, an effect's source.
  // The native constructor stays spelled, because a C++ overload set
  // holding both would become ambiguous.
  py::implicitly_convertible<material::Material, mskia::Paint>();
}

}  // namespace sigil::python
