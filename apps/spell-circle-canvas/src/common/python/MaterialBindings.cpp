#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/Dither.h>
#include <sigilmaterial/color/Extract.h>
#include <sigilmaterial/color/Harmony.h>
#include <sigilmaterial/color/Ramp.h>
#include <sigilmaterial/field/Field.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/ValueBindings.h>

namespace sigil::python {
namespace py = pybind11;
namespace {
material::Color ink(py::handle value) { return material::Color(color(value)); }
std::vector<material::Color> inks(py::iterable values) {
  std::vector<material::Color> result;
  for (auto value : values) result.push_back(ink(value));
  return result;
}
}  // namespace

void bindColor(py::module_& root) {
  using material::Color;
  auto module = root.def_submodule("material");
  py::class_<Color>(module, "Color")
      .def(py::init<float, float, float, float>(), py::arg("red") = 0,
           py::arg("green") = 0, py::arg("blue") = 0, py::arg("alpha") = 1)
      .def(py::init([](py::handle value) { return ink(value); }),
           py::arg("value"))
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
  // itself, not only the parameters that read a handle by hand.
  py::implicitly_convertible<py::str, Color>();
  py::implicitly_convertible<py::tuple, Color>();
  py::implicitly_convertible<py::list, Color>();
}

void bindMaterial(py::module_& root) {
  using namespace material;
  auto module = root.attr("material").cast<py::module_>();
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
      .def(py::init([](float x, float y, float z, float alpha) {
             return Oklch{x, y, z, alpha};
           }),
           py::arg("L") = 0.0f, py::arg("chroma") = 0.0f,
           py::arg("hueDegrees") = 0.0f, py::arg("alpha") = 1.0f)
      .def("copy", [](const Oklch& value) { return value; })
      .def_readwrite("L", &Oklch::L)
      .def_readwrite("chroma", &Oklch::chroma)
      .def_readwrite("hueDegrees", &Oklch::hueDegrees)
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
  py::class_<RampStop>(module, "RampStop")
      .def(py::init([](float position, py::handle value) {
             return RampStop{position, ink(value)};
           }),
           py::arg("pos"), py::arg("color"))
      .def_readwrite("pos", &RampStop::pos)
      .def_property(
          "color", [](const RampStop& stop) { return stop.color; },
          [](RampStop& stop, py::handle value) { stop.color = ink(value); })
      .def(py::self == py::self);
  py::class_<Palette>(module, "Palette")
      .def(py::init([](py::iterable values) { return Palette{inks(values)}; }),
           py::arg("entries") = py::tuple())
      .def_property(
          "entries", [](const Palette& value) { return value.entries; },
          [](Palette& value, py::iterable entries) {
            value.entries = inks(entries);
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
  dither.def("threshold", &Dither::threshold, py::arg("x"), py::arg("y"))
      .def("on", &Dither::on, py::arg("value"), py::arg("x"), py::arg("y"))
      .def(
          "at",
          [](const Dither& d, py::handle c, int x, int y) {
            return d.at(ink(c), x, y);
          },
          py::arg("color"), py::arg("x"), py::arg("y"));
  module.def("rgb", &rgb, py::arg("hex"), py::arg("alpha") = 1.0f);
  module.def("hsv", &hsv, py::arg("hueDegrees"), py::arg("saturation"),
             py::arg("value"), py::arg("alpha") = 1.0f);
  module.def(
      "toOklab", [](py::handle value) { return material::toOklab(ink(value)); },
      py::arg("color"));
  module.def(
      "toOklch", [](py::handle value) { return material::toOklch(ink(value)); },
      py::arg("color"));
  module.def(
      "toLab", [](py::handle value) { return material::toLab(ink(value)); },
      py::arg("color"));
  module.def(
      "luminance",
      [](py::handle value) { return material::luminance(ink(value)); },
      py::arg("color"));
  module.def("fromOklab", &material::fromOklab, py::arg("lab"));
  module.def("fromOklch", &material::fromOklch, py::arg("lch"));
  module.def("fromLab", &material::fromLab, py::arg("lab"));
  module.def("linearOf", &material::linearOf, py::arg("lab"));
  module.def("oklabOf", &material::oklabOf, py::arg("lch"));
  module.def("oklchOf", &material::oklchOf, py::arg("lab"));
  module.def("fitToSrgb", &material::fitToSrgb, py::arg("lch"));
  module.def("srgbToLinear", &material::srgbToLinear, py::arg("channel"));
  module.def("linearToSrgb", &material::linearToSrgb, py::arg("channel"));
  module.def(
      "mixLinear",
      [](py::handle a, py::handle b, float t) {
        return material::mixLinear(ink(a), ink(b), t);
      },
      py::arg("a"), py::arg("b"), py::arg("t"));
  module.def(
      "lerpOklab",
      [](py::handle a, py::handle b, float t) {
        return material::lerpOklab(ink(a), ink(b), t);
      },
      py::arg("a"), py::arg("b"), py::arg("t"));
  module.def("inSrgbGamut", &inSrgbGamut, py::arg("lab"),
             py::arg("slack") = 1e-4f);
  module.def(
      "withAlpha", [](py::handle c, float a) { return withAlpha(ink(c), a); },
      py::arg("color"), py::arg("alpha"));
  module.def(
      "scale",
      [](py::handle c, float k, float a) { return scale(ink(c), k, a); },
      py::arg("color"), py::arg("factor"), py::arg("alpha") = -1.0f);
  module.def(
      "lighten", [](py::handle c, float k) { return lighten(ink(c), k); },
      py::arg("color"), py::arg("amount"));
  module.def(
      "mixToward",
      [](py::handle c, py::handle target, float t, float a) {
        return mixToward(ink(c), ink(target), t, a);
      },
      py::arg("color"), py::arg("target"), py::arg("t"), py::arg("alpha"));
  module.def(
      "deltaE",
      [](py::handle a, py::handle b) { return deltaE(ink(a), ink(b)); },
      py::arg("a"), py::arg("b"));
  module.def(
      "rotateHue",
      [](py::handle c, float degrees) { return rotateHue(ink(c), degrees); },
      py::arg("color"), py::arg("degrees"));
  module.def(
      "harmony",
      [](py::handle c, Scheme scheme, float spread) {
        return harmony(ink(c), scheme, spread);
      },
      py::arg("color"), py::arg("scheme"), py::arg("spreadDegrees") = 30.0f);
  module.def("palette", py::overload_cast<const Ramp&, int>(&material::palette),
             py::arg("ramp"), py::arg("entries"));
  module.def(
      "palette",
      [](py::iterable pixels, const PaletteOptions& options) {
        return material::palette(inks(pixels), options);
      },
      py::arg("pixels"), py::arg("options") = PaletteOptions{});
  module.def("ramp", &material::ramp, py::arg("palette"),
             py::arg("space") = RampSpace::Oklab);
  module.def(
      "closestEntry",
      [](const Palette& p, py::handle c) { return closestEntry(p, ink(c)); },
      py::arg("palette"), py::arg("color"));
  module.def(
      "sampleRamp",
      [](const std::vector<RampStop>& stops, float t) {
        return sampleRamp(stops, t);
      },
      py::arg("stops"), py::arg("t"));
  module.def(
      "rampBracket",
      [](const std::vector<RampStop>& stops, float t) {
        if (stops.empty())
          throw py::value_error("A ramp bracket requires at least one stop.");
        return rampBracket(stops, t);
      },
      py::arg("stops"), py::arg("t"));
  auto field = module.attr("field").cast<py::module_>();
  field.def("ripple", &field::ripple, py::arg("amplitudePx"),
            py::arg("wavelengthPx"), py::arg("phase") = 0.0f,
            py::arg("vertical") = false);
  field.def(
      "halftoneRamp",
      [](float spacing, float rMin, float rMax, py::handle c, float angle,
         float from, float to) {
        return field::halftoneRamp(spacing, rMin, rMax, ink(c), angle, from,
                                   to);
      },
      py::arg("spacing"), py::arg("rMin"), py::arg("rMax"), py::arg("color"),
      py::arg("angleDeg") = 0.0f, py::arg("rampFrom") = 0.0f,
      py::arg("rampTo") = 1.0f);
  bindRecord<field::CrtOverlayParameters>(field, "CrtOverlayParameters",
                                          "Unknown CRT field: ")
      .def_readwrite("uScanPitch", &field::CrtOverlayParameters::uScanPitch)
      .def_readwrite("uScanStrength",
                     &field::CrtOverlayParameters::uScanStrength)
      .def_readwrite("uVigInner", &field::CrtOverlayParameters::uVigInner)
      .def_readwrite("uVigOuter", &field::CrtOverlayParameters::uVigOuter)
      .def_readwrite("uVigStrength", &field::CrtOverlayParameters::uVigStrength)
      .def_readwrite("uSqueeze", &field::CrtOverlayParameters::uSqueeze)
      .def_readwrite("uBeamPitch", &field::CrtOverlayParameters::uBeamPitch)
      .def_readwrite("uBeamFalloff", &field::CrtOverlayParameters::uBeamFalloff)
      .def_readwrite("uBeamStrength",
                     &field::CrtOverlayParameters::uBeamStrength)
      .def_readwrite("uBeatPitch", &field::CrtOverlayParameters::uBeatPitch)
      .def_readwrite("uBeatFalloff", &field::CrtOverlayParameters::uBeatFalloff)
      .def_readwrite("uBeatStrength",
                     &field::CrtOverlayParameters::uBeatStrength)
      .def_readwrite("uGrain", &field::CrtOverlayParameters::uGrain);
  field.def(
      "crtOverlay",
      py::overload_cast<const field::CrtOverlayParameters&>(&field::crtOverlay),
      py::arg("parameters") = field::CrtOverlayParameters{});
}
}  // namespace sigil::python
