#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/Dither.h>
#include <sigilmaterial/color/Extract.h>
#include <sigilmaterial/color/Harmony.h>
#include <sigilmaterial/color/Ramp.h>
#include <sigilmaterial/core/Backface.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/material/Registration.h>

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

void bindColor(py::module_& root) {
  using material::Color;
  auto module = submodule(root, "material");
  py::class_<Color>(module, "Color")
      .def(py::init<float, float, float, float>(), py::arg("red") = 0,
           py::arg("green") = 0, py::arg("blue") = 0, py::arg("alpha") = 1)
      .def(py::init([](py::handle value) { return materialColor(value); }),
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
  dither
      .def("threshold", &Dither::threshold, py::arg("column"), py::arg("row"))
      .def("on", &Dither::on, py::arg("value"), py::arg("column"),
           py::arg("row"))
      .def(
          "at",
          [](const Dither& self, py::handle color, int column, int row) {
            return self.at(materialColor(color), column, row);
          },
          py::arg("color"), py::arg("column"), py::arg("row"));
  module.def("rgb", &rgb, py::arg("hex"), py::arg("alpha") = 1.0f);
  module.def("hsv", &hsv, py::arg("hueDegrees"), py::arg("saturation"),
             py::arg("value"), py::arg("alpha") = 1.0f);
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
  module.def("srgbToLinear",
             py::overload_cast<float>(&material::srgbToLinear),
             py::arg("channel"));
  module.def("linearToSrgb",
             py::overload_cast<float>(&material::linearToSrgb),
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
      py::arg("color"), py::arg("target"), py::arg("amount"),
      py::arg("alpha"));
  module.def(
      "deltaE",
      [](py::handle first, py::handle second) {
        return deltaE(materialColor(first), materialColor(second));
      },
      py::arg("first"), py::arg("second"));
  module.def(
      "rotateHue",
      [](py::handle c, float degrees) {
        return rotateHue(materialColor(c), degrees);
      },
      py::arg("color"), py::arg("degrees"));
  module.def(
      "harmony",
      [](py::handle c, Scheme scheme, float spread) {
        return harmony(materialColor(c), scheme, spread);
      },
      py::arg("color"), py::arg("scheme"), py::arg("spreadDegrees") = 30.0f);
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
