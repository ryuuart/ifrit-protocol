#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/Dither.h>
#include <sigilmaterial/color/Extract.h>
#include <sigilmaterial/color/Harmony.h>
#include <sigilmaterial/color/Mix.h>
#include <sigilmaterial/color/Ramp.h>
#include <sigilmaterial/core/Backface.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/material/Registration.h>

#include <cctype>
#include <charconv>
#include <cmath>
#include <optional>
#include <string>

namespace sigil::python {
namespace py = pybind11;
namespace {
std::vector<material::Color> materialColors(py::iterable values) {
  std::vector<material::Color> result;
  for (auto value : values) result.push_back(materialColor(value));
  return result;
}
}  // namespace

material::Color materialColor(py::handle value) {
  return material::Color(color(value));
}

namespace {

/** A channel written as a number, or as CSS percentage text read against
 *  @p hundredPercent. */
float channelOf(py::handle value, float hundredPercent) {
  if (py::isinstance<py::str>(value)) {
    std::string text = value.cast<std::string>();
    while (!text.empty() && std::isspace((unsigned char)text.back()))
      text.pop_back();
    if (text.empty() || text.back() != '%')
      throw py::value_error("'" + value.cast<std::string>() +
                            "' is no channel: a channel is a number or a "
                            "percentage written as text, such as '50%'.");
    text.pop_back();
    const double percent = py::float_(py::str(text)).cast<double>();
    return (float)(percent / 100.0 * hundredPercent);
  }
  return value.cast<float>();
}

/** Whether @p channel is exactly one of the 256 eight-bit steps, and
 *  which. */
std::optional<int> eightBit(float channel) {
  const float scaled = std::round(channel * 255.0f);
  if (scaled < 0.0f || scaled > 255.0f) return std::nullopt;
  if ((float)(int)scaled / 255.0f != channel) return std::nullopt;
  return (int)scaled;
}

/** The shortest decimal that reads back as @p value. */
std::string shortest(float value) {
  char buffer[32];
  const auto written = std::to_chars(buffer, buffer + sizeof buffer, value);
  return std::string(buffer, written.ptr);
}

/** `#rrggbb` or `#rrggbbaa` where every channel is an eight-bit step. */
std::optional<std::string> hexText(const material::Color& color) {
  const float channels[4] = {color.r, color.g, color.b, color.a};
  std::string text = "#";
  for (int index = 0; index < 4; ++index) {
    const std::optional<int> step = eightBit(channels[index]);
    if (!step) return std::nullopt;
    if (index == 3 && *step == 255) break;
    constexpr char kDigits[] = "0123456789abcdef";
    text += kDigits[*step / 16];
    text += kDigits[*step % 16];
  }
  return text;
}

/** The CSS text for @p color: its hex where every channel is an
 *  eight-bit step, otherwise `color(srgb …)`, which carries any float. */
std::string cssText(const material::Color& color) {
  if (const std::optional<std::string> hex = hexText(color)) return *hex;
  std::string text = "color(srgb " + shortest(color.r) + " " +
                     shortest(color.g) + " " + shortest(color.b);
  if (color.a != 1.0f) text += " / " + shortest(color.a);
  return text + ")";
}

std::string reprText(const material::Color& color) {
  if (const std::optional<std::string> hex = hexText(color))
    return "Color('" + *hex + "')";
  std::string text = "Color.srgb(" + shortest(color.r) + ", " +
                     shortest(color.g) + ", " + shortest(color.b);
  if (color.a != 1.0f) text += ", alpha=" + shortest(color.a);
  return text + ")";
}

material::HueArc hueArcNamed(const std::string& name) {
  if (name == "shorter") return material::HueArc::Shorter;
  if (name == "longer") return material::HueArc::Longer;
  if (name == "increasing") return material::HueArc::Increasing;
  if (name == "decreasing") return material::HueArc::Decreasing;
  throw py::value_error("Unknown hue arc: '" + name +
                        "'; it is 'shorter', 'longer', 'increasing' or "
                        "'decreasing', as CSS names them.");
}

}  // namespace

void bindColor(py::module_& root) {
  using material::Color;
  auto module = submodule(root, "material");
  py::class_<Color>(module, "Color")
      .def(py::init([](py::handle value) { return materialColor(value); }),
           py::arg("value"),
           "A colour from one CSS colour: its text ('#6e99bb', 'steelblue', "
           "'rgb(110 153 187)', 'oklch(0.7 0.1 240)'), a Color, or three or "
           "four unit floats. Channels are written through the named "
           "constructors, rgb() on 0..255 and srgb() on 0..1.")
      .def_static(
          "rgb",
          [](py::handle red, py::handle green, py::handle blue,
             py::handle opacity) {
            return material::rgb(
                channelOf(red, 255.0f), channelOf(green, 255.0f),
                channelOf(blue, 255.0f), channelOf(opacity, 1.0f));
          },
          py::arg("red"), py::arg("green"), py::arg("blue"),
          py::arg("alpha") = 1.0f,
          "CSS rgb(): channels on 0..255 or as '50%', alpha on 0..1, each "
          "clamped to its range.")
      .def_static(
          "srgb",
          [](py::handle red, py::handle green, py::handle blue,
             py::handle opacity) {
            return Color{channelOf(red, 1.0f), channelOf(green, 1.0f),
                         channelOf(blue, 1.0f), channelOf(opacity, 1.0f)};
          },
          py::arg("red"), py::arg("green"), py::arg("blue"),
          py::arg("alpha") = 1.0f,
          "CSS color(srgb r g b / alpha): unit floats, kept as written, so a "
          "channel above 1 is the brighter-than-white value a float colour "
          "can hold.")
      .def_static(
          "hsl",
          [](AngleInDegrees hue, py::handle saturation, py::handle lightness,
             py::handle opacity) {
            return material::hsl(hue.value, channelOf(saturation, 100.0f),
                                 channelOf(lightness, 100.0f),
                                 channelOf(opacity, 1.0f));
          },
          py::arg("hue"), py::arg("saturation"), py::arg("lightness"),
          py::arg("alpha") = 1.0f,
          "CSS hsl(): the hue in degrees or as angle text such as '0.5turn', "
          "saturation and lightness on 0..100 or as '50%'.")
      .def_static(
          "hwb",
          [](AngleInDegrees hue, py::handle whiteness, py::handle blackness,
             py::handle opacity) {
            return material::hwb(hue.value, channelOf(whiteness, 100.0f),
                                 channelOf(blackness, 100.0f),
                                 channelOf(opacity, 1.0f));
          },
          py::arg("hue"), py::arg("whiteness"), py::arg("blackness"),
          py::arg("alpha") = 1.0f,
          "CSS hwb(): the hue in degrees or as angle text such as '0.5turn', "
          "whiteness and blackness on 0..100 or as '50%'.")
      .def_static(
          "lab",
          [](py::handle lightness, py::handle greenRed, py::handle blueYellow,
             py::handle opacity) {
            return material::lab(
                channelOf(lightness, 100.0f), channelOf(greenRed, 125.0f),
                channelOf(blueYellow, 125.0f), channelOf(opacity, 1.0f));
          },
          py::arg("lightness"), py::arg("greenRed"), py::arg("blueYellow"),
          py::arg("alpha") = 1.0f,
          "CSS lab(), under the D50 white: lightness on 0..100 and the two "
          "axes, brought inside sRGB by reducing chroma.")
      .def_static(
          "lch",
          [](py::handle lightness, py::handle chroma, AngleInDegrees hue,
             py::handle opacity) {
            return material::lch(channelOf(lightness, 100.0f),
                                 channelOf(chroma, 150.0f), hue.value,
                                 channelOf(opacity, 1.0f));
          },
          py::arg("lightness"), py::arg("chroma"), py::arg("hue"),
          py::arg("alpha") = 1.0f,
          "CSS lch(), under the D50 white: lightness on 0..100, the chroma "
          "and the hue in degrees or as angle text, brought inside sRGB by "
          "reducing chroma.")
      .def_static(
          "oklab",
          [](py::handle lightness, py::handle greenRed, py::handle blueYellow,
             py::handle opacity) {
            return material::oklab(
                channelOf(lightness, 1.0f), channelOf(greenRed, 0.4f),
                channelOf(blueYellow, 0.4f), channelOf(opacity, 1.0f));
          },
          py::arg("lightness"), py::arg("greenRed"), py::arg("blueYellow"),
          py::arg("alpha") = 1.0f,
          "CSS oklab(): lightness on 0..1 and the two axes, brought inside "
          "sRGB by reducing chroma.")
      .def_static(
          "oklch",
          [](py::handle lightness, py::handle chroma, AngleInDegrees hue,
             py::handle opacity) {
            return material::oklch(channelOf(lightness, 1.0f),
                                   channelOf(chroma, 0.4f), hue.value,
                                   channelOf(opacity, 1.0f));
          },
          py::arg("lightness"), py::arg("chroma"), py::arg("hue"),
          py::arg("alpha") = 1.0f,
          "CSS oklch(): lightness on 0..1, the chroma and the hue in "
          "degrees or as angle text, brought inside sRGB by reducing chroma.")
      .def_static(
          "hex",
          [](py::handle value, std::optional<float> opacity) {
            Color read;
            if (py::isinstance<py::str>(value)) {
              std::string text = value.cast<std::string>();
              if (!text.starts_with("#")) text = "#" + text;
              const std::optional<Color> parsed = material::readColor(text);
              if (!parsed)
                throw py::value_error(
                    "'" + value.cast<std::string>() +
                    "' is no hex colour: one is 3, 4, 6 or 8 hex digits, "
                    "with or without the leading #.");
              read = *parsed;
            } else {
              read = material::hexColor(value.cast<uint32_t>());
            }
            if (opacity) read.a = std::clamp(*opacity, 0.0f, 1.0f);
            return read;
          },
          py::arg("value"), py::arg("alpha") = py::none(),
          "A colour from its hex: text such as '#6e99bb' or '#6e99bb80', or "
          "a packed 0xRRGGBB integer; alpha, when given, replaces the "
          "alpha the digits say.")
      .def_static(
          "named",
          [](const std::string& name) {
            if (const std::optional<Color> read = material::namedColor(name))
              return *read;
            throw py::value_error("'" + name +
                                  "' is not one of CSS's named colours.");
          },
          py::arg("name"), "One of CSS's 148 named colours.")
      .def_static(
          "mix",
          [](py::handle first, py::handle second, float amount,
             const std::string& space, const std::string& hue) {
            const std::optional<material::ColorSpace> named =
                material::readColorSpace(space);
            if (!named)
              throw py::value_error(
                  "Unknown colour space: '" + space +
                  "'; it is one CSS names: 'srgb', 'srgb-linear', "
                  "'display-p3', 'rec2020', 'a98-rgb', 'prophoto-rgb', "
                  "'xyz', 'xyz-d50', 'xyz-d65', 'lab', 'lch', 'oklab', "
                  "'oklch', 'hsl' or 'hwb'.");
            return material::colorMix(materialColor(first),
                                      materialColor(second), amount, *named,
                                      hueArcNamed(hue));
          },
          py::arg("first"), py::arg("second"), py::arg("amount") = 0.5f,
          py::arg("space") = "oklab", py::arg("hue") = "shorter",
          "CSS color-mix(): amount of the way from first to second, mixed in "
          "the named space, premultiplied by alpha, a hue walked along the "
          "named arc.")
      .def("css", &cssText,
           "The colour as CSS text: its hex where every channel is an "
           "eight-bit step, otherwise color(srgb ...), which carries any "
           "float.")
      .def("__str__", &cssText)
      .def("__repr__", &reprText)
      .def_readwrite("r", &Color::r)
      .def_readwrite("g", &Color::g)
      .def_readwrite("b", &Color::b)
      .def_readwrite("a", &Color::a)
      .def("__iter__",
           [](const Color& c) {
             return py::iter(py::make_tuple(c.r, c.g, c.b, c.a));
           })
      .def("copy", [](const Color& c) { return c; })
      .def(py::self == py::self);
  // Every colour spelling reaches a parameter declared as the class
  // itself, not only the parameters that read a handle by hand. Text is
  // converted here rather than by a plain implicit conversion, which
  // would turn the reader's ValueError naming the text into a TypeError
  // that does not.
  py::detail::get_type_info(typeid(Color))
      ->implicit_conversions.emplace_back(
          [](PyObject* object, PyTypeObject*) -> PyObject* {
            if (!PyUnicode_Check(object)) return nullptr;
            const Color read = colorFromText(
                py::reinterpret_borrow<py::str>(object).cast<std::string>());
            return py::cast(read).release().ptr();
          });
  py::implicitly_convertible<py::tuple, Color>();
  py::implicitly_convertible<py::list, Color>();
}

void bindMaterial(py::module_& root) {
  using namespace material;
  submodule(root, "material.sdf");
  submodule(root, "material.ocio");
  submodule(root, "material.slang");
  auto module = submodule(root, "material");
  py::enum_<Backface>(module, "Backface")
      .value("Visible", Backface::Visible)
      .value("Hidden", Backface::Hidden);
  py::class_<Oklab>(module, "Oklab")
      .def(py::init([](float x, float y, float z, float alpha) {
             return Oklab{x, y, z, alpha};
           }),
           py::arg("L") = 0.0f, py::arg("a") = 0.0f, py::arg("b") = 0.0f,
           py::arg("alpha") = 1.0f)
      .def("copy", [](const Oklab& value) { return value; })
      .def_readwrite("L", &Oklab::L)
      .def_readwrite("a", &Oklab::a)
      .def_readwrite("b", &Oklab::b)
      .def_readwrite("alpha", &Oklab::alpha);
  py::class_<Oklch>(module, "Oklch")
      .def(py::init([](float x, float y, AngleInDegrees hue, float alpha) {
             return Oklch{x, y, hue.value, alpha};
           }),
           py::arg("L") = 0.0f, py::arg("chroma") = 0.0f,
           py::arg("hueDegrees") = AngleInDegrees{}, py::arg("alpha") = 1.0f)
      .def("copy", [](const Oklch& value) { return value; })
      .def_readwrite("L", &Oklch::L)
      .def_readwrite("chroma", &Oklch::chroma)
      .def_property(
          "hueDegrees", angleGetter(&Oklch::hueDegrees),
          angleSetter<AngleInDegrees>(&Oklch::hueDegrees))
      .def_readwrite("alpha", &Oklch::alpha);
  py::class_<Lab>(module, "Lab")
      .def(py::init([](float x, float y, float z, float alpha) {
             return Lab{x, y, z, alpha};
           }),
           py::arg("L") = 0.0f, py::arg("a") = 0.0f, py::arg("b") = 0.0f,
           py::arg("alpha") = 1.0f)
      .def("copy", [](const Lab& value) { return value; })
      .def_readwrite("L", &Lab::L)
      .def_readwrite("a", &Lab::a)
      .def_readwrite("b", &Lab::b)
      .def_readwrite("alpha", &Lab::alpha);
  bindRecord<LinearRgb>(module, "LinearRgb", "Unknown LinearRgb field: ")
      .def_readwrite("r", &LinearRgb::r)
      .def_readwrite("g", &LinearRgb::g)
      .def_readwrite("b", &LinearRgb::b);
  bindRecord<RampBracket>(module, "RampBracket", "Unknown RampBracket field: ")
      .def_readwrite("low", &RampBracket::low)
      .def_readwrite("high", &RampBracket::high)
      .def_readwrite("fraction", &RampBracket::fraction);
  py::class_<ColorStop>(module, "ColorStop")
      .def(py::init([](float offset, py::handle value) {
             return ColorStop{offset, materialColor(value)};
           }),
           py::arg("offset"), py::arg("color"))
      .def_readwrite("offset", &ColorStop::offset)
      .def_property(
          "color", [](const ColorStop& stop) { return stop.color; },
          [](ColorStop& stop, py::handle value) {
            stop.color = materialColor(value);
          })
      .def(py::self == py::self);
  py::class_<Palette>(module, "Palette")
      .def(py::init([](py::iterable values) {
             return Palette{materialColors(values)};
           }),
           py::arg("entries") = py::tuple())
      .def_property(
          "entries", [](const Palette& value) { return value.entries; },
          [](Palette& value, py::iterable entries) {
            value.entries = materialColors(entries);
          })
      .def("size", &Palette::size)
      .def("__len__", &Palette::size)
      .def("empty", &Palette::empty)
      .def("at", &Palette::at, py::arg("index"))
      .def("nearest", &Palette::nearest, py::arg("position"))
      .def(
          "__getitem__",
          [](const Palette& p, py::ssize_t index) {
            if (index < 0) index += static_cast<py::ssize_t>(p.size());
            if (index < 0 || static_cast<size_t>(index) >= p.size())
              throw py::index_error("Palette index out of range.");
            return p.entries[index];
          },
          py::arg("index"))
      .def("__iter__",
           [](const Palette& p) { return py::iter(py::cast(p.entries)); })
      .def(py::self == py::self);
  py::enum_<RampSpace>(module, "RampSpace")
      .value("Srgb", RampSpace::Srgb)
      .value("Linear", RampSpace::Linear)
      .value("Oklab", RampSpace::Oklab)
      .value("Oklch", RampSpace::Oklch);
  py::enum_<HueArc>(module, "HueArc")
      .value("Shorter", HueArc::Shorter)
      .value("Longer", HueArc::Longer)
      .value("Increasing", HueArc::Increasing)
      .value("Decreasing", HueArc::Decreasing);
  py::enum_<Scheme>(module, "Scheme")
      .value("Complement", Scheme::Complement)
      .value("SplitComplement", Scheme::SplitComplement)
      .value("Analogous", Scheme::Analogous)
      .value("Triad", Scheme::Triad)
      .value("Tetrad", Scheme::Tetrad);
  py::enum_<DitherKind>(module, "DitherKind")
      .value("Ordered", DitherKind::Ordered)
      .value("Noise", DitherKind::Noise);
  py::enum_<PaletteMethod>(module, "PaletteMethod")
      .value("KMeans", PaletteMethod::KMeans)
      .value("MedianCut", PaletteMethod::MedianCut);
  auto ramp = bindRecord<Ramp>(module, "Ramp", "Unknown Ramp field: ");
  ramp.def_readwrite("stops", &Ramp::stops)
      .def_readwrite("space", &Ramp::space)
      .def_readwrite("arc", &Ramp::arc)
      .def_readwrite("easing", &Ramp::easing)
      .def_readwrite("reverse", &Ramp::reverse)
      .def_readwrite("domainLow", &Ramp::domainLow)
      .def_readwrite("domainHigh", &Ramp::domainHigh)
      .def(py::self == py::self);
  auto dither = bindRecord<Dither>(module, "Dither", "Unknown Dither field: ");
  dither.def_readwrite("kind", &Dither::kind)
      .def_readwrite("matrix", &Dither::matrix)
      .def_readwrite("levels", &Dither::levels)
      .def_readwrite("amount", &Dither::amount)
      .def(py::self == py::self);
  auto paletteoptions = bindRecord<PaletteOptions>(
      module, "PaletteOptions", "Unknown PaletteOptions field: ");
  paletteoptions.def_readwrite("entries", &PaletteOptions::entries)
      .def_readwrite("method", &PaletteOptions::method)
      .def_readwrite("iterations", &PaletteOptions::iterations)
      .def_readwrite("stride", &PaletteOptions::stride)
      .def_readwrite("minimumAlpha", &PaletteOptions::minimumAlpha)
      .def_readwrite("sortByLightness", &PaletteOptions::sortByLightness)
      .def(py::self == py::self);
  ramp.def("at", &Ramp::at, py::arg("value"))
      .def("__call__", &Ramp::operator(), py::arg("value"))
      .def("position", &Ramp::position, py::arg("value"));
  dither.def("threshold", &Dither::threshold, py::arg("column"), py::arg("row"))
      .def("on", &Dither::on, py::arg("value"), py::arg("column"),
           py::arg("row"))
      .def(
          "at",
          [](const Dither& self, py::handle color, int column, int row) {
            return self.at(materialColor(color), column, row);
          },
          py::arg("color"), py::arg("column"), py::arg("row"));
  module.def("hexColor", &hexColor, py::arg("rrggbb"), py::arg("alpha") = 1.0f);
  module.def(
      "hsv",
      [](AngleInDegrees hue, float saturation, float value, float alpha) {
        return hsv(hue.value, saturation, value, alpha);
      },
      py::arg("hueDegrees"), py::arg("saturation"), py::arg("value"),
      py::arg("alpha") = 1.0f);
  module.def(
      "toOklab",
      [](py::handle value) { return material::toOklab(materialColor(value)); },
      py::arg("color"));
  module.def(
      "toOklch",
      [](py::handle value) { return material::toOklch(materialColor(value)); },
      py::arg("color"));
  module.def(
      "toLab",
      [](py::handle value) { return material::toLab(materialColor(value)); },
      py::arg("color"));
  module.def(
      "luminance",
      [](py::handle value) {
        return material::luminance(materialColor(value));
      },
      py::arg("color"));
  module.def("fromOklab", &material::fromOklab, py::arg("lab"));
  module.def("fromOklch", &material::fromOklch, py::arg("lch"));
  module.def("fromLab", &material::fromLab, py::arg("lab"));
  module.def("linearOf", &material::linearOf, py::arg("lab"));
  module.def("oklabOf", &material::oklabOf, py::arg("lch"));
  module.def("oklchOf", &material::oklchOf, py::arg("lab"));
  module.def("fitToSrgb", &material::fitToSrgb, py::arg("lch"));
  module.def("srgbToLinear", py::overload_cast<float>(&material::srgbToLinear),
             py::arg("channel"));
  module.def("linearToSrgb", py::overload_cast<float>(&material::linearToSrgb),
             py::arg("channel"));
  module.def(
      "mixLinear",
      [](py::handle start, py::handle end, float amount) {
        return material::mixLinear(materialColor(start), materialColor(end),
                                   amount);
      },
      py::arg("start"), py::arg("end"), py::arg("amount"));
  module.def(
      "lerpOklab",
      [](py::handle start, py::handle end, float amount) {
        return material::lerpOklab(materialColor(start), materialColor(end),
                                   amount);
      },
      py::arg("start"), py::arg("end"), py::arg("amount"));
  module.def("inSrgbGamut", &inSrgbGamut, py::arg("lab"),
             py::arg("slack") = 1e-4f);
  module.def(
      "withAlpha",
      [](py::handle color, float alpha) {
        return withAlpha(materialColor(color), alpha);
      },
      py::arg("color"), py::arg("alpha"));
  module.def(
      "scale",
      [](py::handle color, float factor, float alpha) {
        return scale(materialColor(color), factor, alpha);
      },
      py::arg("color"), py::arg("factor"), py::arg("alpha") = -1.0f);
  module.def(
      "lighten",
      [](py::handle color, float amount) {
        return lighten(materialColor(color), amount);
      },
      py::arg("color"), py::arg("amount"));
  module.def(
      "mixToward",
      [](py::handle color, py::handle target, float amount, float alpha) {
        return mixToward(materialColor(color), materialColor(target), amount,
                         alpha);
      },
      py::arg("color"), py::arg("target"), py::arg("amount"), py::arg("alpha"));
  module.def(
      "deltaE",
      [](py::handle first, py::handle second) {
        return deltaE(materialColor(first), materialColor(second));
      },
      py::arg("first"), py::arg("second"));
  module.def(
      "rotateHue",
      [](py::handle c, AngleInDegrees degrees) {
        return rotateHue(materialColor(c), degrees.value);
      },
      py::arg("color"), py::arg("degrees"));
  module.def(
      "harmony",
      [](py::handle c, Scheme scheme, AngleInDegrees spread) {
        return harmony(materialColor(c), scheme, spread.value);
      },
      py::arg("color"), py::arg("scheme"),
      py::arg("spreadDegrees") = AngleInDegrees{30.0f});
  module.def("palette", py::overload_cast<const Ramp&, int>(&material::palette),
             py::arg("ramp"), py::arg("entries"));
  module.def(
      "palette",
      [](py::iterable pixels, const PaletteOptions& options) {
        return material::palette(materialColors(pixels), options);
      },
      py::arg("pixels"), py::arg("options") = PaletteOptions{});
  module.def("ramp", &material::ramp, py::arg("palette"),
             py::arg("space") = RampSpace::Oklab);
  module.def(
      "closestEntry",
      [](const Palette& p, py::handle c) {
        return closestEntry(p, materialColor(c));
      },
      py::arg("palette"), py::arg("color"));
  module.def(
      "sampleRamp",
      [](const std::vector<ColorStop>& stops, float position) {
        return sampleRamp(stops, position);
      },
      py::arg("stops"), py::arg("position"));
  module.def(
      "rampBracket",
      [](const std::vector<ColorStop>& stops, float position) {
        if (stops.empty())
          throw py::value_error("A ramp bracket requires at least one stop.");
        return rampBracket(stops, position);
      },
      py::arg("stops"), py::arg("position"));
}
}  // namespace sigil::python
