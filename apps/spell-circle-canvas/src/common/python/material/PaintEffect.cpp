#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkRuntimeEffect.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilmaterial/color/Ramp.h>
#include <sigilmaterial/core/Gradient.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/material/Registration.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;

/** Whether @p value is a plain number, which in a stop is its offset. */
bool isNumber(py::handle value) {
  return (py::isinstance<py::int_>(value) || py::isinstance<py::float_>(value)) &&
         !py::isinstance<py::bool_>(value);
}

/** A gradient's stops as Python writes them: a `Ramp`, a list of
 *  `ColorStop`s, pairs in CSS's order `(colour, position)` — the position
 *  a fraction or percentage text — or in the offset-first order
 *  `(offset, colour)`, or plain colours spaced evenly. A pair is read by
 *  which of its two entries is the number. */
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
        if (isNumber(pair[0]))
          stops.push_back({py::cast<float>(pair[0]), color(pair[1])});
        else
          stops.push_back({fraction(pair[1], "A gradient stop's position"),
                           color(pair[0])});
        continue;
      }
    }
    colors.push_back(color(value));
  }
  if (!stops.empty() && !colors.empty())
    throw py::value_error(
        "A gradient's stops are all (color, position) pairs or all plain "
        "colors.");
  if (stops.size() + colors.size() < 2)
    throw py::value_error("A gradient needs at least two stops.");
  if (!colors.empty()) return material::ColorStops(colors);
  return material::ColorStops(std::move(stops));
}

/** CSS's side-or-corner words, `"right"` or `"top left"` in either order,
 *  with or without the `to` CSS writes before them. */
material::Toward toward(py::handle value) {
  if (py::isinstance<material::Toward>(value))
    return value.cast<material::Toward>();
  std::string text = value.cast<std::string>();
  bool top = false, bottom = false, left = false, right = false;
  std::string word;
  std::vector<std::string> words;
  for (const char letter : text + " ") {
    if (letter == ' ') {
      if (!word.empty()) words.push_back(word);
      word.clear();
    } else {
      word += letter;
    }
  }
  if (!words.empty() && words.front() == "to") words.erase(words.begin());
  for (const std::string& name : words) {
    if (name == "top") top = true;
    else if (name == "bottom") bottom = true;
    else if (name == "left") left = true;
    else if (name == "right") right = true;
    else
      throw py::value_error("A gradient runs toward 'top', 'bottom', 'left', "
                            "'right' or a corner such as 'top left', not '" +
                            text + "'.");
  }
  if (words.empty() || words.size() > 2 || (top && bottom) || (left && right))
    throw py::value_error("A gradient runs toward one side or one corner, "
                          "such as 'right' or 'top left', not '" + text + "'.");
  if (top) return left ? material::Toward::TopLeft
                       : right ? material::Toward::TopRight
                               : material::Toward::Top;
  if (bottom) return left ? material::Toward::BottomLeft
                          : right ? material::Toward::BottomRight
                                  : material::Toward::Bottom;
  return left ? material::Toward::Left : material::Toward::Right;
}

/** CSS's `at <position>` as fractions of the box: a pair of numbers or
 *  percentage text, or the keywords `center`, `top`, `bottom`, `left`
 *  and `right` alone or two together. None is the middle. */
glm::vec2 position(py::handle value) {
  if (value.is_none()) return {0.5f, 0.5f};
  if (!py::isinstance<py::str>(value)) {
    const auto pair = py::cast<py::sequence>(value);
    if (pair.size() != 2)
      throw py::value_error("A gradient's centre is two fractions of the box.");
    return {fraction(pair[0], "A gradient's centre"),
            fraction(pair[1], "A gradient's centre")};
  }
  const std::string text = value.cast<std::string>();
  glm::vec2 at = {0.5f, 0.5f};
  std::string word;
  for (const char letter : text + " ") {
    if (letter != ' ') {
      word += letter;
      continue;
    }
    if (word.empty()) continue;
    if (word == "left") at.x = 0;
    else if (word == "right") at.x = 1;
    else if (word == "top") at.y = 0;
    else if (word == "bottom") at.y = 1;
    else if (word != "center")
      throw py::value_error("A gradient's centre is 'center', 'top', "
                            "'bottom', 'left', 'right' or two of them, or a "
                            "pair of fractions, not '" + text + "'.");
    word.clear();
  }
  return at;
}

/** CSS's `circle` and `ellipse`. */
material::RadialShape radialShape(py::handle value) {
  if (py::isinstance<material::RadialShape>(value))
    return value.cast<material::RadialShape>();
  const std::string name = value.cast<std::string>();
  if (name == "ellipse") return material::RadialShape::Ellipse;
  if (name == "circle") return material::RadialShape::Circle;
  throw py::value_error("A radial gradient's shape is 'circle' or "
                        "'ellipse', not '" + name + "'.");
}

/** CSS's four extent keywords. */
material::RadialExtent radialExtent(py::handle value) {
  if (py::isinstance<material::RadialExtent>(value))
    return value.cast<material::RadialExtent>();
  const std::string name = value.cast<std::string>();
  if (name == "farthest-corner") return material::RadialExtent::FarthestCorner;
  if (name == "closest-side") return material::RadialExtent::ClosestSide;
  if (name == "closest-corner") return material::RadialExtent::ClosestCorner;
  if (name == "farthest-side") return material::RadialExtent::FarthestSide;
  throw py::value_error("A radial gradient's extent is 'closest-side', "
                        "'closest-corner', 'farthest-side' or "
                        "'farthest-corner', not '" + name + "'.");
}

/** A point as Python writes it, as the paint's own vector. */
glm::vec2 vector(py::handle value) {
  const SkPoint at = point(value);
  return {at.x(), at.y()};
}

sk_sp<SkRuntimeEffect> runtimeEffect(const std::string& source) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(source));
  if (!effect) throw py::value_error(error.c_str());
  return effect;
}

material::Paint& setParameter(material::Paint& paint, const std::string& name,
                              py::handle value) {
  if (py::isinstance<py::int_>(value) || py::isinstance<py::float_>(value))
    return paint.set(name, py::cast<float>(value));
  // Explicit colors are four components; numeric sequences keep their
  // array meaning. Typed motion values retain their shared cells.
  if (py::isinstance<material::Color>(value) || py::isinstance<py::str>(value))
    return paint.set(name, color(value));
  if (py::isinstance<motion::Animatable<material::Color>>(value) ||
      py::isinstance<motion::Tween<material::Color>>(value))
    return paint.bind(name, motionInk(value));
  if (py::isinstance<motion::Animatable<float>>(value) ||
      py::isinstance<motion::Tween<float>>(value))
    return paint.bind(name, motionAnimatable(value));
  const auto values = py::cast<std::vector<float>>(value);
  if (values.size() == 2)
    return paint.set(name, std::array<float, 2>{values[0], values[1]});
  if (values.size() == 4)
    return paint.set(
        name, std::array<float, 4>{values[0], values[1], values[2], values[3]});
  return paint.set(name, values);
}

material::Paint sksl(py::handle effect, py::dict uniforms) {
  auto paint =
      material::skia::sksl(py::isinstance<py::str>(effect)
                               ? runtimeEffect(py::cast<std::string>(effect))
                               : py::cast<sk_sp<SkRuntimeEffect>>(effect));
  for (const auto& [name, value] : uniforms)
    setParameter(paint, py::cast<std::string>(name), value);
  return paint;
}
}  // namespace

void bindMaterialPaintEffect(py::module_& module) {
  auto nativePaint = submodule(module, "material.skia");
  py::enum_<material::BlendMode>(nativePaint, "BlendMode")
      .value("Normal", material::BlendMode::Normal)
      .value("Multiply", material::BlendMode::Multiply)
      .value("Screen", material::BlendMode::Screen)
      .value("Overlay", material::BlendMode::Overlay)
      .value("Darken", material::BlendMode::Darken)
      .value("Lighten", material::BlendMode::Lighten)
      .value("ColorDodge", material::BlendMode::ColorDodge)
      .value("ColorBurn", material::BlendMode::ColorBurn)
      .value("HardLight", material::BlendMode::HardLight)
      .value("SoftLight", material::BlendMode::SoftLight)
      .value("Difference", material::BlendMode::Difference)
      .value("Exclusion", material::BlendMode::Exclusion)
      .value("Hue", material::BlendMode::Hue)
      .value("Saturation", material::BlendMode::Saturation)
      .value("Color", material::BlendMode::Color)
      .value("Luminosity", material::BlendMode::Luminosity)
      .value("PlusLighter", material::BlendMode::PlusLighter)
      .value("Modulate", material::BlendMode::Modulate)
      .value("Clear", material::BlendMode::Clear)
      .value("Source", material::BlendMode::Source)
      .value("Destination", material::BlendMode::Destination)
      .value("SourceIn", material::BlendMode::SourceIn)
      .value("SourceOut", material::BlendMode::SourceOut)
      .value("SourceAtop", material::BlendMode::SourceAtop)
      .value("DestinationOver", material::BlendMode::DestinationOver)
      .value("DestinationIn", material::BlendMode::DestinationIn)
      .value("DestinationOut", material::BlendMode::DestinationOut)
      .value("DestinationAtop", material::BlendMode::DestinationAtop)
      .value("Xor", material::BlendMode::Xor);

  bindRecord<material::ShadowOptions>(nativePaint, "ShadowOptions",
                                      "Unknown ShadowOptions field: ")
      .def_property(
          "blur",
          [](const material::ShadowOptions& options) { return options.blur; },
          [](material::ShadowOptions& options, py::handle value) {
            options.blur = pixels(value, "A shadow's blur");
          })
      .def_property(
          "offset",
          [](const material::ShadowOptions& options) {
            return py::make_tuple(options.offset.x, options.offset.y);
          },
          [](material::ShadowOptions& options, py::handle value) {
            options.offset = pixelOffset(value, "A shadow's offset");
          })
      .def_property(
          "spread",
          [](const material::ShadowOptions& options) { return options.spread; },
          [](material::ShadowOptions& options, py::handle value) {
            options.spread = pixels(value, "A shadow's spread");
          })
      .def_readwrite("inset", &material::ShadowOptions::inset);
  py::enum_<material::StrokePosition>(nativePaint, "StrokePosition")
      .value("Inside", material::StrokePosition::Inside)
      .value("Center", material::StrokePosition::Center)
      .value("Outside", material::StrokePosition::Outside);
  bindRecord<material::StrokeOptions>(nativePaint, "StrokeOptions",
                                      "Unknown StrokeOptions field: ")
      .def_readwrite("width", &material::StrokeOptions::width)
      .def_readwrite("position", &material::StrokeOptions::position);
  bindRecord<material::BevelOptions>(nativePaint, "BevelOptions",
                                     "Unknown BevelOptions field: ")
      .def_readwrite("depth", &material::BevelOptions::depth)
      .def_readwrite("size", &material::BevelOptions::size)
      .def_property(
          "angleDegrees", angleGetter(&material::BevelOptions::angleDegrees),
          angleSetter<AngleInDegrees>(&material::BevelOptions::angleDegrees))
      .def_property(
          "altitudeDegrees",
          angleGetter(&material::BevelOptions::altitudeDegrees),
          angleSetter<AngleInDegrees>(&material::BevelOptions::altitudeDegrees))
      .def_readwrite("highlight", &material::BevelOptions::highlight)
      .def_readwrite("shadow", &material::BevelOptions::shadow)
      .def_readwrite("gloss", &material::BevelOptions::gloss)
      .def_readwrite("reflection", &material::BevelOptions::reflection)
      .def_readwrite("bend", &material::BevelOptions::bend)
      .def_readwrite("normal", &material::BevelOptions::normal);
  nativePaint.def("horizonReflection", &material::horizonReflection,
                  py::arg("horizon") = 0.5f);
  bindRecord<material::BloomOptions>(nativePaint, "BloomOptions",
                                     "Unknown BloomOptions field: ")
      .def_readwrite("sigma", &material::BloomOptions::sigma)
      .def_readwrite("strength", &material::BloomOptions::strength)
      .def_readwrite("spread", &material::BloomOptions::spread)
      .def_readwrite("tail", &material::BloomOptions::tail)
      .def_readwrite("threshold", &material::BloomOptions::threshold)
      .def_readwrite("knee", &material::BloomOptions::knee)
      .def_readwrite("softness", &material::BloomOptions::softness)
      .def_readwrite("whitening", &material::BloomOptions::whitening)
      .def_readwrite("dilation", &material::BloomOptions::dilation)
      .def_readwrite("deepening", &material::BloomOptions::deepening)
      .def_readwrite("maximumOpacity", &material::BloomOptions::maximumOpacity);
  bindRecord<material::GlassOptions>(nativePaint, "GlassOptions",
                                     "Unknown GlassOptions field: ")
      .def_readwrite("ior", &material::GlassOptions::ior)
      .def_readwrite("thickness", &material::GlassOptions::thickness)
      .def_readwrite("sampleRadius", &material::GlassOptions::sampleRadius)
      .def_readwrite("normal", &material::GlassOptions::normal)
      .def_readwrite("normalDirectX", &material::GlassOptions::normalDirectX);

  py::class_<material::Filter>(nativePaint, "Filter")
      .def(py::init<>())
      .def_static(
          "of",
          [](const material::Material& program,
             std::optional<float> sampleRadius) {
            if (!sampleRadius) return material::Filter::of(program);
            if (!(*sampleRadius >= 0))
              throw py::value_error(
                  "A sampling reach is a distance in local pixels, zero or "
                  "more.");
            return material::Filter::of(program, *sampleRadius);
          },
          py::arg("program"), py::arg("sampleRadius") = py::none(),
          "A program over the layer: `program` runs with the rendered layer "
          "in its `content` slot. `sampleRadius` bounds the largest offset, "
          "in local pixels, at which the body samples the layer, so a body "
          "that reads its neighbours past a tile or a layer's edge is handed "
          "them.")
      .def_static(
          "program",
          [](py::handle effect, py::dict parameters,
             std::optional<float> sampleRadius) {
            std::vector<std::pair<std::string, float>> values;
            for (const auto& [name, value] : parameters)
              values.emplace_back(py::cast<std::string>(name),
                                  py::cast<float>(value));
            sk_sp<SkRuntimeEffect> program =
                py::isinstance<py::str>(effect)
                    ? runtimeEffect(py::cast<std::string>(effect))
                    : py::cast<sk_sp<SkRuntimeEffect>>(effect);
            if (!sampleRadius)
              return material::skia::program(std::move(program),
                                             std::move(values));
            if (!(*sampleRadius >= 0) || !std::isfinite(*sampleRadius))
              throw py::value_error(
                  "A sampling reach is a distance in local pixels, zero or "
                  "more.");
            return material::skia::program(std::move(program),
                                           std::move(values), *sampleRadius);
          },
          py::arg("effect"), py::arg("parameters") = py::dict(),
          py::arg("sampleRadius") = py::none(),
          "An SkSL program over the layer, its source or a compiled "
          "effect: the layer arrives in its `content` slot and `parameters` "
          "set float uniforms by name. `sampleRadius` bounds the largest "
          "offset, in local pixels, at which the body samples the layer, as "
          "`Filter.of` states one.")
      .def_static(
          "glow",
          [](py::object ink, float sigma) {
            return material::Filter::glow(color(ink), sigma);
          },
          py::arg("ink"), py::arg("sigma"))
      .def_static(
          "dropShadow",
          [](py::object ink, const material::ShadowOptions& options) {
            return material::Filter::dropShadow(color(ink), options);
          },
          py::arg("color"), py::arg("options") = material::ShadowOptions{})
      .def_static(
          "shadow",
          [](py::object ink, const material::ShadowOptions& options) {
            return material::Filter::shadow(color(ink), options);
          },
          py::arg("color"), py::arg("options") = material::ShadowOptions{})
      .def_static(
          "stroke",
          [](py::object ink, const material::StrokeOptions& options) {
            return material::Filter::stroke(color(ink), options);
          },
          py::arg("color"), py::arg("options") = material::StrokeOptions{})
      .def_static("bevel", &material::Filter::bevel,
                  py::arg("options") = material::BevelOptions{})
      .def_static("bloom", &material::Filter::bloom,
                  py::arg("options") = material::BloomOptions{})
      .def_static("glass", &material::Filter::glass,
                  py::arg("options") = material::GlassOptions{})
      .def_static(
          "brightness",
          [](py::handle amount) {
            return material::Filter::brightness(
                fraction(amount, "brightness()'s amount"));
          },
          py::arg("amount"))
      .def_static(
          "contrast",
          [](py::handle amount) {
            return material::Filter::contrast(
                fraction(amount, "contrast()'s amount"));
          },
          py::arg("amount"))
      .def_static(
          "saturate",
          [](py::handle amount) {
            return material::Filter::saturate(
                fraction(amount, "saturate()'s amount"));
          },
          py::arg("amount"))
      .def_static(
          "grayscale",
          [](py::handle amount) {
            return material::Filter::grayscale(
                fraction(amount, "grayscale()'s amount"));
          },
          py::arg("amount") = 1.0f)
      .def_static(
          "sepia",
          [](py::handle amount) {
            return material::Filter::sepia(
                fraction(amount, "sepia()'s amount"));
          },
          py::arg("amount") = 1.0f)
      .def_static(
          "invert",
          [](py::handle amount) {
            return material::Filter::invert(
                fraction(amount, "invert()'s amount"));
          },
          py::arg("amount") = 1.0f)
      .def_static(
          "opacity",
          [](py::handle amount) {
            return material::Filter::opacity(
                fraction(amount, "opacity()'s amount"));
          },
          py::arg("amount"))
      .def_static(
          "hueRotate",
          [](AngleInDegrees degrees) {
            return material::Filter::hueRotate(degrees.value);
          },
          py::arg("degrees"))
      .def_static("brightPass", &material::Filter::brightPass,
                  py::arg("threshold") = 0.68f, py::arg("knee") = 0.30f)
      .def_static("phosphorBloom", &material::Filter::phosphorBloom,
                  py::arg("radius") = 9.0f, py::arg("threshold") = 0.52f,
                  py::arg("intensity") = 0.46f, py::arg("chroma") = 0.80f,
                  py::arg("hueDrift") = 0.0f, py::arg("tail") = 0.0f)
      .def_static(
          "directionalBlur",
          [](float sigma, AngleInDegrees angleDegrees, float across) {
            return material::Filter::directionalBlur(sigma, angleDegrees.value,
                                                     across);
          },
          py::arg("sigma"), py::arg("angleDegrees"), py::arg("across") = 0.0f)
      .def_static(
          "blur",
          py::overload_cast<material::Paint, float>(&material::Filter::blur),
          py::arg("sigmaMap"), py::arg("maximumSigma"))
      .def_static(
          "blur",
          [](py::handle sigma) {
            return material::Filter::blur(pixels(sigma, "blur()'s sigma"));
          },
          py::arg("sigma"))
      .def_static("dilate", &material::Filter::dilate, py::arg("pixels"))
      .def_static("deepen", &material::Filter::deepen, py::arg("amount"))
      .def_static("whiten", &material::Filter::whiten, py::arg("amount"),
                  py::arg("threshold") = 0.2f, py::arg("knee") = 0.2f)
      .def("slot", &material::Filter::slot, py::arg("name"), py::arg("paint"),
           fluent)
      .def(
          "set",
          [](material::Filter& self, const std::string& name,
             py::object value) -> material::Filter& {
            // A colour is four floats here as it is on a paint's
            // parameter — the colour class or a CSS string — so a filter
            // and a paint take a colour written the same way.
            if (py::isinstance<material::Color>(value) ||
                py::isinstance<py::str>(value)) {
              const SkColor4f tint = color(value);
              return self.set(name, std::array<float, 4>{tint.fR, tint.fG,
                                                         tint.fB, tint.fA});
            }
            if (py::isinstance<py::list>(value) ||
                py::isinstance<py::tuple>(value))
              return self.set(name, value.cast<std::vector<float>>());
            return self.set(name, py::cast<float>(value));
          },
          py::arg("name"), py::arg("value"), fluent)
      .def(
          "bind",
          [](material::Filter& self, const std::string& name,
             py::object value) -> material::Filter& {
            return self.bind(name, motionAnimatable(value));
          },
          py::arg("name"), py::arg("value"), fluent)
      .def("then", &material::Filter::then, py::arg("filter"))
      .def("emit", &material::Filter::emit, py::arg("light"),
           py::arg("mode") = material::BlendMode::Screen)
      .def("isNone", &material::Filter::isNone)
      .def("isRunning", &material::Filter::isRunning)
      .def("usesWorldSpace", &material::Filter::usesWorldSpace)
      .def(py::self == py::self);

  py::enum_<material::GradientUnits>(nativePaint, "GradientUnits")
      .value("Box", material::GradientUnits::Box)
      .value("Pixels", material::GradientUnits::Pixels);
  py::enum_<material::RadialExtent>(nativePaint, "RadialExtent")
      .value("FarthestCorner", material::RadialExtent::FarthestCorner)
      .value("ClosestSide", material::RadialExtent::ClosestSide)
      .value("ClosestCorner", material::RadialExtent::ClosestCorner)
      .value("FarthestSide", material::RadialExtent::FarthestSide);
  py::enum_<material::RadialShape>(nativePaint, "RadialShape")
      .value("Ellipse", material::RadialShape::Ellipse)
      .value("Circle", material::RadialShape::Circle);
  py::enum_<material::Toward>(nativePaint, "Toward")
      .value("Top", material::Toward::Top)
      .value("TopRight", material::Toward::TopRight)
      .value("Right", material::Toward::Right)
      .value("BottomRight", material::Toward::BottomRight)
      .value("Bottom", material::Toward::Bottom)
      .value("BottomLeft", material::Toward::BottomLeft)
      .value("Left", material::Toward::Left)
      .value("TopLeft", material::Toward::TopLeft);
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
      .def_property(
          "startDegrees", angleGetter(&material::GradientOptions::startDegrees),
          angleSetter<AngleInDegrees>(&material::GradientOptions::startDegrees))
      .def_property(
          "endDegrees", angleGetter(&material::GradientOptions::endDegrees),
          angleSetter<AngleInDegrees>(&material::GradientOptions::endDegrees))
      .def(py::self == py::self);
  py::enum_<material::Fit>(nativePaint, "Fit")
      .value("Contain", material::Fit::Contain)
      .value("Cover", material::Fit::Cover)
      .value("Stretch", material::Fit::Stretch)
      .value("Native", material::Fit::Native);
  py::class_<material::Paint>(nativePaint, "Paint")
      .def(py::init<>())
      .def(py::init<const material::Paint&>(), py::arg("paint"))
      .def(py::init([](const material::Material& recipe) {
             return material::Paint::recipe(recipe);
           }),
           py::arg("material"))
      .def("copy", [](const material::Paint& paint) { return paint; })
      .def_static(
          "solid",
          [](py::handle value) { return material::Paint::solid(color(value)); },
          py::arg("color"))
      .def_static(
          "linearGradient",
          [](py::handle start, py::handle end, py::handle stops,
             const material::GradientOptions& options) {
            return material::Paint::linearGradient(vector(start), vector(end),
                                                   colorStops(stops), options);
          },
          py::arg("start"), py::arg("end"), py::arg("stops"),
          py::arg("options") = material::GradientOptions{})
      .def_static(
          "radialGradient",
          [](py::handle center, float radius, py::handle stops,
             const material::GradientOptions& options) {
            return material::Paint::radialGradient(vector(center), radius,
                                                   colorStops(stops), options);
          },
          py::arg("center"), py::arg("radius"), py::arg("stops"),
          py::arg("options") = material::GradientOptions{})
      .def_static(
          "conicGradient",
          [](py::handle center, py::handle stops,
             const material::GradientOptions& options) {
            return material::Paint::conicGradient(vector(center),
                                                  colorStops(stops), options);
          },
          py::arg("center"), py::arg("stops"),
          py::arg("options") = material::GradientOptions{})
      .def_static(
          "image",
          [](sk_sp<SkImage> image, material::Repeat horizontal,
             material::Repeat vertical, const SkMatrix& local) {
            return material::skia::image(std::move(image), horizontal, vertical,
                                         local);
          },
          py::arg("image"), py::arg("horizontal") = material::Repeat::Pad,
          py::arg("vertical") = material::Repeat::Pad,
          py::arg("local") = SkMatrix::I())
      .def_static("sksl", &sksl, py::arg("effect"),
                  py::arg("uniforms") = py::dict())
      .def_static("recipe", &material::Paint::recipe, py::arg("material"))
      .def_static("blend", &material::Paint::blend, py::arg("layers"))
      .def("set", &setParameter, py::arg("name"), py::arg("value"), fluent)
      .def(
          "bind",
          [](material::Paint& self, const std::string& name,
             py::object value) -> material::Paint& {
            // A bound number is a single value, so every sequence here is
            // a colour's three or four channels, as is a CSS string.
            if (py::isinstance<material::Color>(value) ||
                py::isinstance<py::str>(value) ||
                py::isinstance<py::tuple>(value) ||
                py::isinstance<py::list>(value) ||
                py::isinstance<motion::Animatable<material::Color>>(value) ||
                py::isinstance<motion::Tween<material::Color>>(value))
              return self.bind(name, motionInk(value));
            return self.bind(name, motionAnimatable(value));
          },
          py::arg("name"), py::arg("value"), fluent)
      .def("slot", &material::Paint::slot, py::arg("name"), py::arg("paint"),
           fluent)
      .def("amount", &material::Paint::amount, py::arg("amount"), fluent)
      .def("fit", &material::Paint::fit, py::arg("fit"), fluent)
      .def("worldSpace", py::overload_cast<bool>(&material::Paint::worldSpace),
           py::arg("on") = true, fluent)
      .def("quantizeTime", &material::Paint::quantizeTime, py::arg("rate"),
           fluent)
      .def("isRunning", &material::Paint::isRunning)
      .def("isNone", &material::Paint::isNone)
      .def(py::self == py::self);
  // A paint is a material's base, so every place that takes a material
  // takes a gradient, an image or a runtime effect built as a paint.
  py::reinterpret_borrow<py::class_<material::Material>>(
      py::type::of<material::Material>())
      .def(py::init([](const material::Paint& paint) {
             return material::skia::base(paint);
           }),
           py::arg("paint"));
  py::implicitly_convertible<material::Paint, material::Material>();
  // A recipe instance is one kind of paint, so everything that takes a
  // paint takes a material: a slot, a blend layer, an effect's source.
  // The native constructor stays spelled, because a C++ overload set
  // holding both would become ambiguous.
  py::implicitly_convertible<material::Material, material::Paint>();

  // The CSS-shaped gradients: the stops first, then what CSS writes
  // before them, measured against the box the material paints.
  nativePaint.def(
      "linearGradient",
      [](py::handle stops, py::object angle, py::object towardSide,
         const material::GradientOptions& options) {
        if (!angle.is_none() && !towardSide.is_none())
          throw py::type_error(
              "linearGradient() runs at an angle or toward a side, not both.");
        material::LinearDirection direction;
        if (!towardSide.is_none())
          direction = toward(towardSide);
        else if (!angle.is_none())
          direction = angle.cast<AngleInDegrees>().value;
        return material::linearGradient(colorStops(stops), direction, options);
      },
      py::arg("stops"), py::arg("angle") = py::none(), py::kw_only(),
      py::arg("toward") = py::none(),
      py::arg("options") = material::GradientOptions{},
      "CSS's linear-gradient: the stops across the box, toward the bottom "
      "unless told — angle in degrees clockwise from straight up (or angle "
      "text such as '0.25turn'), or toward a side or a corner such as "
      "'right' or 'top left'. Its ends land on the box's corners, as CSS's "
      "do. Stops are colours, or (colour, position) pairs with the position "
      "a fraction or a percentage.");
  nativePaint.def(
      "radialGradient",
      [](py::handle stops, py::handle shape, py::handle extent,
         py::handle at, const material::GradientOptions& options) {
        return material::radialGradient(
            colorStops(stops),
            {.shape = radialShape(shape),
             .extent = radialExtent(extent),
             .at = position(at)},
            options);
      },
      py::arg("stops"), py::kw_only(), py::arg("shape") = "ellipse",
      py::arg("extent") = "farthest-corner", py::arg("at") = py::none(),
      py::arg("options") = material::GradientOptions{},
      "CSS's radial-gradient: the stops out from `at` — fractions of the "
      "box or position keywords, the middle unless told — to the side or "
      "the corner `extent` names, measured from `at`, as an 'ellipse' in "
      "the box's proportions or a 'circle'.");
  nativePaint.def(
      "conicGradient",
      [](py::handle stops, AngleInDegrees from, py::handle at,
         const material::GradientOptions& options) {
        return material::conicGradient(
            colorStops(stops), {.fromDegrees = from.value, .at = position(at)},
            options);
      },
      py::arg("stops"), py::kw_only(), py::arg("from_") = AngleInDegrees{},
      py::arg("at") = py::none(),
      py::arg("options") = material::GradientOptions{},
      "CSS's conic-gradient: the stops swept clockwise around `at` from the "
      "angle `from_`, in degrees with 0 at 12 o'clock, or angle text.");
}

}  // namespace sigil::python
