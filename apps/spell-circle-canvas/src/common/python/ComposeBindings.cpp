#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/ComposeBindings.h>
#include <sigilpython/MotionBindings.h>
#include <sigilpython/ValueBindings.h>
#include <sigilweave/layout/StyleSheet.h>

#include <algorithm>
#include <any>
#include <chrono>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>

namespace sigil::python {
namespace py = pybind11;
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;
using compose::Element;
using Model = std::shared_ptr<PythonValue>;

compose::Decoration decoration(py::handle value) {
  if (py::isinstance<compose::Decoration>(value))
    return value.cast<compose::Decoration>();
  if (py::isinstance<compose::PathFormat>(value))
    return value.cast<compose::PathFormat>();
  if (py::isinstance<compose::Shadow>(value))
    return value.cast<compose::Shadow>();
  throw py::type_error("A decoration is a Decoration, PathFormat, or Shadow.");
}

void bindTypography(py::module_& module) {
  auto text = module.def_submodule("weave");
  using namespace weave;
  py::enum_<TextTransform>(text, "TextTransform")
      .value("None_", TextTransform::kNone)
      .value("Uppercase", TextTransform::kUppercase)
      .value("Lowercase", TextTransform::kLowercase)
      .value("Capitalize", TextTransform::kCapitalize);
  py::enum_<VerticalForm>(text, "VerticalForm")
      .value("Auto", VerticalForm::kAuto)
      .value("Upright", VerticalForm::kUpright)
      .value("Rotated", VerticalForm::kRotated)
      .value("TateChuYoko", VerticalForm::kTateChuYoko);
  py::class_<FontFeature>(text, "FontFeature")
      .def(py::init([](const std::string& tag, uint32_t value) {
             if (tag.size() != 4)
               throw py::value_error("An OpenType tag needs four bytes.");
             FontFeature result;
             std::copy_n(tag.begin(), 4, result.tag);
             result.value = value;
             return result;
           }),
           py::arg("tag"), py::arg("value") = 1)
      .def_property_readonly(
          "tag",
          [](const FontFeature& self) { return std::string(self.tag, 4); })
      .def_readwrite("value", &FontFeature::value)
      .def(py::self == py::self);
  py::class_<FontVariation>(text, "FontVariation")
      .def(py::init([](const std::string& tag, float value) {
             if (tag.size() != 4)
               throw py::value_error("An OpenType tag needs four bytes.");
             FontVariation result;
             std::copy_n(tag.begin(), 4, result.tag);
             result.value = value;
             return result;
           }),
           py::arg("tag"), py::arg("value"))
      .def_property_readonly(
          "tag",
          [](const FontVariation& self) { return std::string(self.tag, 4); })
      .def_readwrite("value", &FontVariation::value)
      .def(py::self == py::self);
  py::class_<ShapingStyle>(text, "ShapingStyle")
      .def(py::init(
          [](py::kwargs fields) { return keywordValue<ShapingStyle>(fields); }))
      .def_readwrite("typeface", &ShapingStyle::typeface)
      .def_readwrite("fontSize", &ShapingStyle::fontSize)
      .def_readwrite("letterSpacing", &ShapingStyle::letterSpacing)
      .def_readwrite("scaleX", &ShapingStyle::scaleX)
      .def_readwrite("wordSpacing", &ShapingStyle::wordSpacing)
      .def_readwrite("languageTag", &ShapingStyle::languageTag)
      .def_property(
          "fontFeatures",
          [](const ShapingStyle& self) { return self.fontFeatures; },
          [](ShapingStyle& self, std::vector<FontFeature> value) {
            self.fontFeatures = std::move(value);
          })
      .def_property(
          "variations",
          [](const ShapingStyle& self) { return self.variations; },
          [](ShapingStyle& self, std::vector<FontVariation> value) {
            self.variations = std::move(value);
          })
      .def_readwrite("textTransform", &ShapingStyle::textTransform)
      .def_readwrite("verticalForm", &ShapingStyle::verticalForm)
      .def_readwrite("aliased", &ShapingStyle::aliased)
      .def_readwrite("opticalKerning", &ShapingStyle::opticalKerning)
      .def(py::self == py::self);
  py::class_<PaintStyle>(text, "PaintStyle")
      .def(py::init(
          [](py::kwargs fields) { return keywordValue<PaintStyle>(fields); }))
      .def_readwrite("foreground", &PaintStyle::foreground)
      .def_readwrite("baselineShift", &PaintStyle::baselineShift)
      .def(py::self == py::self);
  py::class_<TextStyle>(text, "TextStyle")
      .def(py::init(
          [](py::kwargs fields) { return keywordValue<TextStyle>(fields); }))
      .def_readwrite("shaping", &TextStyle::shaping)
      .def_readwrite("paint", &TextStyle::paint)
      .def("weight", &TextStyle::weight, fluent)
      .def("opticalSize", &TextStyle::opticalSize, fluent)
      .def("condense", &TextStyle::condense, fluent)
      .def(
          "variation",
          [](TextStyle& self, const std::string& tag,
             float value) -> TextStyle& {
            if (tag.size() != 4)
              throw py::value_error("An OpenType tag needs four bytes.");
            char bytes[5] = {};
            std::copy_n(tag.begin(), 4, bytes);
            return self.variation(bytes, value);
          },
          fluent)
      .def(py::self == py::self);
  text.def("textStyle", &weave::textStyle)
      .def("initialType", &weave::initialType);
  py::enum_<TextAlignment>(text, "TextAlignment")
      .value("Start", TextAlignment::kStart)
      .value("Center", TextAlignment::kCenter)
      .value("End", TextAlignment::kEnd)
      .value("Justify", TextAlignment::kJustify);
  py::enum_<LineBreakStrategy>(text, "LineBreakStrategy")
      .value("Greedy", LineBreakStrategy::kGreedy)
      .value("KnuthPlass", LineBreakStrategy::kKnuthPlass);
  py::enum_<WritingMode>(text, "WritingMode")
      .value("Horizontal", WritingMode::kHorizontal)
      .value("VerticalRL", WritingMode::kVerticalRL);
  auto leading = py::class_<Leading>(text, "Leading");
  py::enum_<Leading::Kind>(leading, "Kind")
      .value("Face", Leading::Kind::kFace)
      .value("Multiple", Leading::Kind::kMultiple)
      .value("Absolute", Leading::Kind::kAbsolute)
      .value("Grid", Leading::Kind::kGrid);
  leading.def(py::init<>())
      .def_static("face", &Leading::face)
      .def_static("multiple", &Leading::multiple)
      .def_static("absolute", &Leading::absolute)
      .def_static("grid", &Leading::grid)
      .def_readwrite("kind", &Leading::kind)
      .def_readwrite("value", &Leading::value)
      .def(py::self == py::self);
  py::class_<Block>(text, "Block")
      .def(py::init(
          [](py::kwargs fields) { return keywordValue<Block>(fields); }))
      .def_property(
          "leading", [](const Block& self) { return self.leading; },
          [](Block& self, std::optional<Leading> value) {
            self.leading = std::move(value);
          })
      .def_readwrite("halfLeading", &Block::halfLeading)
      .def_readwrite("alignment", &Block::alignment)
      .def_readwrite("firstLineIndent", &Block::firstLineIndent)
      .def_readwrite("lastLineIndent", &Block::lastLineIndent)
      .def_readwrite("widowLines", &Block::widowLines)
      .def_readwrite("orphanLines", &Block::orphanLines)
      .def_readwrite("balanceRaggedLines", &Block::balanceRaggedLines)
      .def_readwrite("writingMode", &Block::writingMode)
      .def_readwrite("lineBreakLocale", &Block::lineBreakLocale)
      .def_readwrite("lineBreak", &Block::lineBreak)
      .def_readwrite("lastLineAlignment", &Block::lastLineAlignment)
      .def_readwrite("justifyLastLine", &Block::justifyLastLine)
      .def_readwrite("tsume", &Block::tsume)
      .def("empty", &Block::empty)
      .def(py::self == py::self);
  py::class_<Rule>(text, "Rule")
      .def(py::init<std::string>())
      .def(py::init<std::string, Type>())
      .def(py::init<std::string, Block>())
      .def("font", &Rule::font, fluent)
      .def("block", py::overload_cast<Block>(&Rule::block), fluent)
      .def("name", &Rule::name)
      .def("type", &Rule::type, py::return_value_policy::copy)
      .def("block", py::overload_cast<>(&Rule::block, py::const_),
           py::return_value_policy::copy)
      .def(py::self == py::self);
  text.def("rule", &weave::rule);
  py::class_<StyleSheet>(text, "StyleSheet")
      .def(py::init([](const std::vector<Rule>& rules) {
             StyleSheet value;
             for (const auto& rule : rules) value.set(rule);
             return value;
           }),
           py::arg("rules") = std::vector<Rule>{})
      .def(py::init<TextStyle>())
      .def("base", py::overload_cast<TextStyle>(&StyleSheet::base), fluent)
      .def("base", py::overload_cast<>(&StyleSheet::base, py::const_),
           py::return_value_policy::copy)
      .def("set", py::overload_cast<const Rule&>(&StyleSheet::set), fluent)
      .def("set", py::overload_cast<std::string, Type>(&StyleSheet::set),
           fluent)
      .def("set", py::overload_cast<std::string, Block>(&StyleSheet::set),
           fluent)
      .def("__getitem__", &StyleSheet::operator[])
      .def("find",
           [](const StyleSheet& self,
              const std::string& name) -> std::optional<Rule> {
             if (auto rule = self.find(name)) return *rule;
             return {};
           })
      .def("contains", &StyleSheet::contains)
      .def("__contains__", &StyleSheet::contains)
      .def("rules", &StyleSheet::rules, py::return_value_policy::copy)
      .def("__len__", &StyleSheet::size)
      .def("empty", &StyleSheet::empty)
      .def(py::self == py::self);
}

Element memo(py::object properties, py::function describe) {
  // Capture a model value, rather than an alias whose mutation would make
  // the old and new declarations compare against the same changed object.
  const CallbackBoundary boundary;
  auto snapshot =
      retainValue(py::module_::import("copy").attr("deepcopy")(properties));
  auto callback = retainCallback(std::move(describe));
  return compose::detail::makeMemo(
      std::any(std::move(snapshot)),
      [](const std::any& left, const std::any& right) {
        const py::gil_scoped_acquire lock;
        const CallbackBoundary boundary;
        try {
          const auto a = std::any_cast<const Model&>(left)->get();
          const auto b = std::any_cast<const Model&>(right)->get();
          const int equal = PyObject_RichCompareBool(a.ptr(), b.ptr(), Py_EQ);
          if (equal < 0) throw py::error_already_set();
          return equal != 0;
        } catch (const py::error_already_set& error) {
          throw std::runtime_error(error.what());
        }
      },
      [callback](const std::any& value) {
        const py::gil_scoped_acquire lock;
        const CallbackBoundary boundary;
        try {
          return callback->get()(std::any_cast<const Model&>(value)->get())
              .cast<Element>();
        } catch (const py::error_already_set& error) {
          throw std::runtime_error(error.what());
        }
      });
}
}  // namespace

compose::Dimension dimension(py::handle value) {
  if (py::isinstance<compose::Dimension>(value))
    return value.cast<compose::Dimension>();
  if (py::isinstance<weave::Length>(value)) return value.cast<weave::Length>();
  if (py::isinstance<compose::VarRef>(value))
    return value.cast<compose::VarRef>();
  if (!py::isinstance<py::str>(value))
    return compose::Dimension{value.cast<float>()};
  const auto text = value.cast<std::string>();
  if (text == "auto") return compose::autoDimension();
  if (text.size() > 1 && text.back() == '%') {
    std::size_t end = 0;
    const float amount = std::stof(text, &end);
    if (end == text.size() - 1 && std::isfinite(amount))
      return compose::pct(amount);
  }
  throw py::value_error(
      "A dimension is a native Dimension or Length, number, percentage string, "
      "or 'auto'.");
}

compose::Fill fill(py::handle value) {
  if (value.is_none()) return compose::Fill::none();
  if (py::isinstance<compose::Fill>(value)) return value.cast<compose::Fill>();
  if (py::isinstance<compose::VarRef>(value))
    return compose::Fill::var(value.cast<compose::VarRef>());
  if (py::isinstance<SkShader>(value))
    return compose::Fill::shader(value.cast<sk_sp<SkShader>>());
  return compose::Fill::color(color(value));
}

compose::SurfacePaint surfacePaint(py::handle value) {
  if (py::isinstance<compose::SurfacePaint>(value))
    return value.cast<compose::SurfacePaint>();
  if (py::isinstance<material::skia::Paint>(value))
    return value.cast<material::skia::Paint>();
  if (py::isinstance<material::Material>(value))
    return value.cast<material::Material>();
  return compose::SurfacePaint{motionFill(value)};
}

compose::Align alignment(py::handle value) {
  if (py::isinstance<compose::Align>(value))
    return value.cast<compose::Align>();
  const auto name = value.cast<std::string>();
  if (name == "auto") return compose::Align::Auto;
  if (name == "start") return compose::Align::Start;
  if (name == "center") return compose::Align::Center;
  if (name == "end") return compose::Align::End;
  if (name == "stretch") return compose::Align::Stretch;
  if (name == "baseline") return compose::Align::Baseline;
  throw py::value_error("Unknown alignment: " + name);
}

compose::Justify justification(py::handle value) {
  if (py::isinstance<compose::Justify>(value))
    return value.cast<compose::Justify>();
  const auto name = value.cast<std::string>();
  if (name == "start") return compose::Justify::Start;
  if (name == "center") return compose::Justify::Center;
  if (name == "end") return compose::Justify::End;
  if (name == "space_between") return compose::Justify::SpaceBetween;
  if (name == "space_around") return compose::Justify::SpaceAround;
  if (name == "space_evenly") return compose::Justify::SpaceEvenly;
  throw py::value_error("Unknown justification: " + name);
}

compose::Shape shape(py::handle value) {
  if (py::isinstance<compose::Shape>(value))
    return value.cast<compose::Shape>();
  if (PyCallable_Check(value.ptr())) {
    auto callback = retainCallback(py::reinterpret_borrow<py::function>(value));
    return compose::Shape{[callback](SkSize size) {
      const py::gil_scoped_acquire lock;
      const CallbackBoundary boundary;
      try {
        return callback->get()(size.width(), size.height()).cast<SkPath>();
      } catch (const py::error_already_set& error) {
        throw std::runtime_error(error.what());
      }
    }};
  }
  return compose::heldPath(value.cast<SkPath>());
}

void bindCompose(py::module_& module) {
  bindTypography(module);
  auto composition = module.def_submodule("compose");
  py::class_<compose::Composer::Stats>(composition, "ComposerStats")
#define SIGIL_STAT(name) .def_readonly(#name, &compose::Composer::Stats::name)
      SIGIL_STAT(instances) SIGIL_STAT(yogaNodes) SIGIL_STAT(describedNodes)
          SIGIL_STAT(memoHits) SIGIL_STAT(patchedNodes) SIGIL_STAT(picturesLive)
              SIGIL_STAT(texturesLive) SIGIL_STAT(picturesRecorded)
                  SIGIL_STAT(texturesBaked) SIGIL_STAT(nodesPainted)
                      SIGIL_STAT(reconcileMs) SIGIL_STAT(layoutMs)
                          SIGIL_STAT(volatileMs) SIGIL_STAT(paintMs);
#undef SIGIL_STAT
  py::class_<compose::TextSettling>(composition, "TextSettling")
      .def_readonly("live", &compose::TextSettling::live)
      .def_readonly("reused", &compose::TextSettling::reused)
      .def_readonly("degraded", &compose::TextSettling::degraded);

  py::enum_<compose::Cache>(composition, "Cache")
      .value("Auto", compose::Cache::Auto)
      .value("Picture", compose::Cache::Picture)
      .value("Texture", compose::Cache::Texture)
      .value("Group", compose::Cache::Group)
      .value("None_", compose::Cache::None);
  py::class_<Element> element(composition, "Element");
  using namespace compose;
  py::class_<VarRef>(composition, "VarRef")
      .def_readonly("id", &VarRef::id)
      .def(py::self == py::self);
  composition.def("var", &compose::var);
  auto dim = py::class_<Dimension>(composition, "Dimension");
  py::enum_<Dimension::Unit>(dim, "Unit")
      .value("Px", Dimension::Unit::Px)
      .value("Pct", Dimension::Unit::Pct)
      .value("Auto", Dimension::Unit::Auto)
      .value("Em", Dimension::Unit::Em)
      .value("Rem", Dimension::Unit::Rem)
      .value("Lh", Dimension::Unit::Lh)
      .value("Var", Dimension::Unit::Var)
      .value("Pw", Dimension::Unit::Pw)
      .value("Ph", Dimension::Unit::Ph);
  dim.def(py::init<>())
      .def(py::init([](py::object value) { return dimension(value); }))
      .def_readwrite("unit", &Dimension::unit)
      .def_readwrite("value", &Dimension::value)
      .def("relative", &Dimension::relative)
      .def("reference", &Dimension::reference)
      .def(py::self == py::self);
  composition.def("pct", &pct)
      .def("pw", &pw)
      .def("ph", &ph)
      .def("autoDimension", &autoDimension);
  py::enum_<Align>(composition, "Align")
      .value("Auto", Align::Auto)
      .value("Start", Align::Start)
      .value("Center", Align::Center)
      .value("End", Align::End)
      .value("Stretch", Align::Stretch)
      .value("Baseline", Align::Baseline);
  py::enum_<Justify>(composition, "Justify")
      .value("Start", Justify::Start)
      .value("Center", Justify::Center)
      .value("End", Justify::End)
      .value("SpaceBetween", Justify::SpaceBetween)
      .value("SpaceAround", Justify::SpaceAround)
      .value("SpaceEvenly", Justify::SpaceEvenly);
  py::enum_<Backface>(composition, "Backface")
      .value("Visible", Backface::Visible)
      .value("Hidden", Backface::Hidden);
  py::enum_<Boundary>(composition, "Boundary")
      .value("Auto", Boundary::Auto)
      .value("Outline", Boundary::Outline)
      .value("Glyphs", Boundary::Glyphs)
      .value("Coverage", Boundary::Coverage);
  py::enum_<Fit>(composition, "Fit")
      .value("Stretch", Fit::Stretch)
      .value("Contain", Fit::Contain)
      .value("Cover", Fit::Cover);
  py::class_<Corners>(composition, "Corners")
      .def(py::init<>())
      .def(py::init<float>())
      .def(py::init<float, float, float, float>())
      .def_readwrite("topLeft", &Corners::topLeft)
      .def_readwrite("topRight", &Corners::topRight)
      .def_readwrite("bottomRight", &Corners::bottomRight)
      .def_readwrite("bottomLeft", &Corners::bottomLeft)
      .def("any", &Corners::any)
      .def(py::self == py::self);
  py::class_<Fill>(composition, "Fill")
      .def(py::init<>())
      .def(py::init([](py::object value) { return fill(value); }))
      .def_static("none", &Fill::none)
      .def_static("currentInk", &Fill::currentInk)
      .def_static("color",
                  [](py::object value) { return Fill::color(color(value)); })
      .def_static("var", py::overload_cast<VarRef>(&Fill::var))
      .def_readonly("colorValue", &Fill::colorValue)
      .def(py::self == py::self);
  py::class_<SurfacePaint>(composition, "SurfacePaint")
      .def(py::init<>())
      .def(py::init([](py::object value) { return surfacePaint(value); }))
      .def("none", &SurfacePaint::none)
      .def("isAnimated", &SurfacePaint::isAnimated)
      .def(py::self == py::self);
  py::class_<CellSpan>(composition, "CellSpan")
      .def(py::init<>())
      .def_readwrite("column", &CellSpan::column)
      .def_readwrite("row", &CellSpan::row)
      .def_readwrite("columns", &CellSpan::columns)
      .def_readwrite("rows", &CellSpan::rows)
      .def_readwrite("across", &CellSpan::across)
      .def_readwrite("down", &CellSpan::down)
      .def_readwrite("declared", &CellSpan::declared)
      .def_readwrite("alignDeclared", &CellSpan::alignDeclared);
  py::class_<Shape>(composition, "Shape")
      .def(py::init<>())
      .def(py::init([](py::object value) { return shape(value); }))
      .def("path", [](const Shape& self, float w,
                      float h) { return self.path(SkSize::Make(w, h)); })
      .def("comparable", &Shape::comparable)
      .def(py::self == py::self);
  composition.def("shape", [](py::object value) { return shape(value); });
  composition.def("heldPath",
                  [](SkPath path) { return Shape{heldPath(std::move(path))}; });
  py::class_<MotionPath>(composition, "MotionPath")
      .def(py::init([](py::object path, py::object progress, float look) {
             return MotionPath{shape(path), motionAnimatable(progress), look};
           }),
           py::arg("path"), py::arg("t") = 0.0f, py::arg("lookAhead") = 0.0f)
      .def_readwrite("path", &MotionPath::path)
      .def_readwrite("lookAhead", &MotionPath::lookAhead)
      .def_property(
          "t", [](const MotionPath& self) { return self.t; },
          [](MotionPath& self, py::object value) {
            self.t = motionAnimatable(value);
          });

  auto format = py::class_<PathFormat>(composition, "PathFormat");
  py::enum_<PathFormat::Align>(format, "Align")
      .value("Center", PathFormat::Align::Center)
      .value("Inner", PathFormat::Align::Inner)
      .value("Outer", PathFormat::Align::Outer);
  format.def(py::init<>())
      .def_readwrite("width", &PathFormat::width)
      .def_property(
          "strokeFill", [](const PathFormat& self) { return self.strokeFill; },
          [](PathFormat& self, py::object value) {
            self.strokeFill = surfacePaint(value);
          })
      .def_readwrite("align", &PathFormat::align)
      .def_readwrite("dashIntervals", &PathFormat::dashIntervals)
      .def_readwrite("cap", &PathFormat::cap)
      .def_readwrite("join", &PathFormat::join)
      .def_readwrite("antiAlias", &PathFormat::antiAlias)
      .def_readwrite("dashPhase", &PathFormat::dashPhase)
      .def_readwrite("stampPath", &PathFormat::stampPath)
      .def_readwrite("stampAdvance", &PathFormat::stampAdvance)
      .def_readwrite("trimStart", &PathFormat::trimStart)
      .def_readwrite("trimEnd", &PathFormat::trimEnd)
      .def_readwrite("trimOffset", &PathFormat::trimOffset)
      .def("isAnimated", &PathFormat::isAnimated)
      .def_property(
          "trimPhase", [](const PathFormat& self) { return self.trimPhase; },
          [](PathFormat& self, py::object value) {
            self.trimPhase = value.is_none()
                                 ? std::nullopt
                                 : std::optional{motionAnimatable(value)};
          })
      .def_property(
          "dashPhaseBinding",
          [](const PathFormat& self) { return self.dashPhaseBinding; },
          [](PathFormat& self, py::object value) {
            self.dashPhaseBinding =
                value.is_none() ? std::nullopt
                                : std::optional{motionAnimatable(value)};
          });
  composition.def(
      "stroke",
      [](float width, py::object paint, PathFormat::Align align) {
        return compose::stroke(width,
                               paint.is_none()
                                   ? SurfacePaint{Fill::currentInk()}
                                   : surfacePaint(paint),
                               align);
      },
      py::arg("width"), py::arg("paint") = py::none(),
      py::arg("align") = PathFormat::Align::Center);
  py::class_<Shadow>(composition, "Shadow")
      .def(py::init<>())
      .def_property(
          "color", [](const Shadow& self) { return self.color; },
          [](Shadow& self, py::object value) { self.color = color(value); })
      .def_property(
          "offset", [](const Shadow& self) { return self.offset; },
          [](Shadow& self, py::object value) { self.offset = point(value); })
      .def_readwrite("blur", &Shadow::blur)
      .def_readwrite("maxBind", &Shadow::maxBind)
      .def_readwrite("knockout", &Shadow::knockout);
  composition.def(
      "shadow",
      [](py::object ink, py::object offset, float blur) {
        return compose::shadow(color(ink), point(offset), blur);
      },
      py::arg("color"), py::arg("offset"), py::arg("blur"));
  py::class_<Decoration>(composition, "Decoration")
      .def(py::init([](py::object value) { return decoration(value); }))
      .def("isAnimated", &Decoration::isAnimated)
      .def("blends", &Decoration::blends)
      .def(py::self == py::self);
  py::implicitly_convertible<PathFormat, Decoration>();
  py::implicitly_convertible<Shadow, Decoration>();
  py::class_<LayerStyle>(composition, "LayerStyle")
      .def(py::init<>())
      .def_property(
          "under", [](const LayerStyle& self) { return self.under; },
          [](LayerStyle& self, std::vector<Decoration> value) {
            self.under = std::move(value);
          })
      .def_property(
          "over", [](const LayerStyle& self) { return self.over; },
          [](LayerStyle& self, std::vector<Decoration> value) {
            self.over = std::move(value);
          });
  py::class_<Spans>(composition, "Spans")
      .def(
          "__or__", [](const Spans& a, const Spans& b) { return a | b; },
          py::is_operator());
  auto selections = composition.def_submodule("spans");
  selections.def("range", [](py::object a, py::object b) {
    return spans::range(motionAnimatable(a), motionAnimatable(b));
  });
  selections.def("wrap", [](py::object a, py::object b) {
    return spans::wrap(motionAnimatable(a), motionAnimatable(b));
  });
  selections.def("upTo", [](py::object end) {
    return spans::upTo(motionAnimatable(end));
  });
  selections.def("corners", &spans::corners, py::arg("arm"),
                 py::arg("angle") = 30.0f);
  selections.def("edges", &spans::edges, py::arg("arm"),
                 py::arg("angle") = 30.0f);
  selections.def("every", &spans::every, py::arg("count"),
                 py::arg("duty") = 1.0f);
  selections.def("at", &spans::at);
  selections.def("fit", &spans::fit, py::arg("key"), py::arg("margin") = 0.0f);
  selections.def("rest", py::overload_cast<>(&spans::rest));
  selections.def("rest", py::overload_cast<std::string_view>(&spans::rest));

  element.def("copy", [](const Element& value) { return value; })
      .def("__copy__", [](const Element& value) { return value; })
      .def("row", &Element::row, fluent)
      .def("column", &Element::column, fluent)
      .def("grow", &Element::grow, py::arg("factor") = 1.0f, fluent)
      .def("shrink", &Element::shrink, fluent)
      .def("absolute", &Element::absolute, fluent)
      .def("cover", &Element::cover, fluent)
      .def("key", &Element::key, fluent)
      .def("cache", &Element::cache, fluent)
      .def(
          "children",
          [](Element& self, const std::vector<Element>& values) -> Element& {
            return self.children(values);
          },
          fluent)
      .def(
          "size",
          [](Element& self, py::object width, py::object height) -> Element& {
            return self.width(dimension(width)).height(dimension(height));
          },
          py::arg("width"), py::arg("height"), fluent)
      .def(
          "fill",
          [](Element& self, py::object value) -> Element& {
            if (py::isinstance<compose::SurfacePaint>(value))
              return value.cast<compose::SurfacePaint>().apply(self);
            if (py::isinstance<material::skia::Paint>(value))
              return self.fill(value.cast<material::skia::Paint>());
            return self.fill(motionFill(value));
          },
          fluent)
      .def(
          "ink",
          [](Element& self, py::object value) -> Element& {
            if (py::isinstance<compose::VarRef>(value))
              return self.ink(value.cast<compose::VarRef>());
            return self.ink(color(value));
          },
          fluent)
      .def("font", &Element::font, fluent)
      .def(
          "fontTrack",
          [](Element& self, py::object value) -> Element& {
            return self.font(
                {.track = py::isinstance<weave::Length>(value)
                              ? value.cast<weave::Length>()
                              : weave::Length{value.cast<float>()}});
          },
          fluent)
      .def(
          "fontSize",
          [](Element& self, py::object value) -> Element& {
            return self.font(
                {.size = py::isinstance<weave::Length>(value)
                             ? value.cast<weave::Length>()
                             : weave::Length{value.cast<float>()}});
          },
          fluent)
      .def(
          "fontWeight",
          [](Element& self, float value) -> Element& {
            return self.font({.weight = value});
          },
          fluent)
      .def(
          "corners",
          [](Element& self, float all) -> Element& {
            return self.corners({all});
          },
          fluent)
      .def(
          "corners",
          [](Element& self, float tl, float tr, float br,
             float bl) -> Element& { return self.corners({tl, tr, br, bl}); },
          fluent)
      .def("inset", py::overload_cast<float>(&Element::inset), fluent)
      .def("transformOrigin",
           py::overload_cast<float, float>(&Element::transformOrigin), fluent);

  const auto dimensionMethod =
      [&](const char* name, Element& (Element::*setter)(compose::Dimension)) {
        element.def(
            name,
            [setter](Element& self, py::object value) -> Element& {
              return (self.*setter)(dimension(value));
            },
            fluent);
      };
  dimensionMethod("width", &Element::width);
  dimensionMethod("height", &Element::height);
  dimensionMethod("minWidth", &Element::minWidth);
  dimensionMethod("minHeight", &Element::minHeight);
  dimensionMethod("maxWidth", &Element::maxWidth);
  dimensionMethod("maxHeight", &Element::maxHeight);
  dimensionMethod("gap", &Element::gap);
  dimensionMethod("left", &Element::left);
  dimensionMethod("top", &Element::top);
  dimensionMethod("right", &Element::right);
  dimensionMethod("bottom", &Element::bottom);

  element.def("wrapLines", &Element::wrapLines, py::arg("wrap") = true, fluent)
      .def("aspect", &Element::aspect, fluent)
      .def(
          "basis",
          [](Element& self, py::object value) -> Element& {
            return self.basis(dimension(value));
          },
          fluent)
      .def(
          "alignItems",
          [](Element& self, py::object value) -> Element& {
            return self.alignItems(alignment(value));
          },
          fluent)
      .def(
          "alignSelf",
          [](Element& self, py::object value) -> Element& {
            return self.alignSelf(alignment(value));
          },
          fluent)
      .def(
          "justify",
          [](Element& self, py::object value) -> Element& {
            return self.justify(justification(value));
          },
          fluent)
      .def(
          "centerAt",
          [](Element& self, py::object value) -> Element& {
            return self.centerAt(point(value));
          },
          fluent)
      .def(
          "at",
          [](Element& self, py::object value) -> Element& {
            return self.at(point(value));
          },
          fluent)
      .def(
          "rect",
          [](Element& self, py::object value) -> Element& {
            return self.rect(rect(value));
          },
          fluent)
      .def("cells", py::overload_cast<int, int, int, int>(&Element::cells),
           py::arg("column"), py::arg("row"), py::arg("columns") = 1,
           py::arg("rows") = 1, fluent)
      .def("cells", py::overload_cast<CellSpan>(&Element::cells), fluent)
      .def("area", &Element::area, fluent)
      .def(
          "cellAlign",
          [](Element& self, py::object x, py::object y) -> Element& {
            return self.cellAlign(alignment(x), alignment(y));
          },
          fluent)
      .def("corners", py::overload_cast<Corners>(&Element::corners), fluent)
      .def(
          "shape",
          [](Element& self, py::object value) -> Element& {
            return self.shape(shape(value));
          },
          fluent)
      .def("centered", &Element::centered, fluent)
      .def("inward", &Element::inward, fluent)
      .def("outward", &Element::outward, fluent)
      .def("clip", &Element::clip, py::arg("clip") = true, fluent)
      .def("block", &Element::block, fluent)
      .def("styleSheet", &Element::styleSheet, fluent)
      .def("styleClass", &Element::styleClass, fluent)
      .def(
          "var",
          [](Element& self, const std::string& name,
             py::object value) -> Element& {
            if (py::isinstance<py::str>(value)) {
              const auto text = value.cast<std::string>();
              if (text != "auto" && !text.ends_with("%"))
                return self.var(name, color(value));
            }
            if (py::isinstance<SkColor4f>(value) ||
                py::isinstance<py::tuple>(value) ||
                py::isinstance<py::list>(value))
              return self.var(name, color(value));
            return self.var(name, dimension(value));
          },
          fluent)
      .def("sampling", &Element::sampling, fluent)
      .def("hitTestable", &Element::hitTestable, fluent)
      .def("boundary", &Element::boundary, fluent)
      .def("threshold", &Element::threshold, fluent)
      .def("style", &Element::style, fluent)
      .def(
          "echo",
          [](Element& self, py::object offset, py::object ink) -> Element& {
            return self.echo(point(offset), color(ink));
          },
          fluent)
      .def("effect", &Element::effect, fluent)
      .def("backdrop", &Element::backdrop, fluent)
      .def("appear", &Element::appear, fluent)
      .def("blend", &Element::blend, fluent)
      .def("travel", &Element::travel, fluent)
      .def(
          "transformOriginPx",
          [](Element& self, py::object value) -> Element& {
            return self.transformOriginPx(point(value));
          },
          fluent)
      .def("zIndex", &Element::zIndex, fluent)
      .def("perspectiveOrigin", &Element::perspectiveOrigin, fluent)
      .def("transformOrigin3d", &Element::transformOrigin3d, fluent)
      .def("preserve3d", &Element::preserve3d, py::arg("preserve") = true,
           fluent)
      .def("backface", &Element::backface, fluent)
      .def("atRest", &Element::atRest)
      .def("bakeScale", &Element::bakeScale, fluent)
      .def("transition", &Element::transition, fluent)
      .def("flowAround", &Element::flowAround, py::arg("key"),
           py::arg("margin") = 0.0f, fluent)
      .def(
          "region",
          [](Element& self, py::object value) -> Element& {
            return self.region(rect(value));
          },
          fluent)
      .def("thread", &Element::thread, fluent)
      .def("maxLines", &Element::maxLines, fluent)
      .def(
          "ellipsis",
          [](Element& self, const std::string& value) -> Element& {
            return self.ellipsis(value);
          },
          fluent)
      .def("textFill", &Element::textFill, fluent)
      .def(
          "textStroke",
          [](Element& self, float width, py::object value) -> Element& {
            return self.textStroke(width, fill(value));
          },
          fluent);
  for (const auto& [name, setter] : std::initializer_list<std::pair<
           const char*, Element& (Element::*)(motion::Animatable<float>)>>{
           {"opacity", &Element::opacity},
           {"rotate", &Element::rotate},
           {"scale", &Element::scale},
           {"scaleX", &Element::scaleX},
           {"scaleY", &Element::scaleY},
           {"translateX", &Element::translateX},
           {"translateY", &Element::translateY},
           {"skewX", &Element::skewX},
           {"skewY", &Element::skewY},
           {"rotateX", &Element::rotateX},
           {"rotateY", &Element::rotateY},
           {"rotateZ", &Element::rotateZ},
           {"translateZ", &Element::translateZ},
           {"scaleZ", &Element::scaleZ},
           {"perspective", &Element::perspective}})
    element.def(
        name,
        [setter](Element& self, py::object value) -> Element& {
          return (self.*setter)(motionAnimatable(value));
        },
        fluent);
  for (const char* name : {"padding", "margin"}) {
    element.def(
        name,
        [name](Element& self, py::args values) -> Element& {
          std::vector<Dimension> dimensions;
          for (const auto& value : values)
            dimensions.push_back(dimension(value));
          const bool padding = std::string_view{name} == "padding";
          if (dimensions.size() == 1)
            return padding ? self.padding(dimensions[0])
                           : self.margin(dimensions[0]);
          if (dimensions.size() == 2)
            return padding ? self.padding(dimensions[0], dimensions[1])
                           : self.margin(dimensions[0], dimensions[1]);
          if (dimensions.size() == 4)
            return padding ? self.padding(dimensions[0], dimensions[1],
                                          dimensions[2], dimensions[3])
                           : self.margin(dimensions[0], dimensions[1],
                                         dimensions[2], dimensions[3]);
          throw py::type_error(
              "padding and margin accept one, two, or four dimensions.");
        },
        fluent);
  }
  element.def(
      "staggerChildren",
      [](Element& self, double seconds, const std::string& origin) -> Element& {
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 1e12)
          throw py::value_error("Stagger needs nonnegative finite seconds.");
        const auto from = origin == "start" ? motion::Spread::From::Start
                          : origin == "end" ? motion::Spread::From::End
                                            : motion::Spread::From::Center;
        if (origin != "start" && origin != "end" && origin != "center")
          throw py::value_error("Stagger origin is start, center, or end.");
        return self.staggerChildren(
            std::chrono::milliseconds{static_cast<int64_t>(seconds * 1000)},
            from);
      },
      py::arg("seconds"), py::arg("from_") = "start", fluent);
  element.def(
      "inset",
      [](Element& self, py::object l, py::object t, py::object r,
         py::object b) -> Element& {
        return self.inset(dimension(l), dimension(t), dimension(r),
                          dimension(b));
      },
      fluent);
  for (const auto& [name, setter] : std::initializer_list<std::pair<
           const char*, Element& (Element::*)(Decoration, std::string)>>{
           {"overlay", &Element::overlay},
           {"background", &Element::background},
           {"foreground", &Element::foreground},
           {"stroke", &Element::stroke}})
    element.def(
        name,
        [setter](Element& self, py::object value, const std::string& name)
            -> Element& { return (self.*setter)(decoration(value), name); },
        py::arg("decoration"), py::arg("name") = "", fluent);
  element.def(
      "stroke",
      [](Element& self, Spans spans, py::object value,
         const std::string& name) -> Element& {
        return self.stroke(std::move(spans), decoration(value), name);
      },
      py::arg("spans"), py::arg("decoration"), py::arg("name") = "", fluent);
  element.def(
      "background",
      [](Element& self, Spans spans, py::object value,
         const std::string& name) -> Element& {
        return self.background(std::move(spans), decoration(value), name);
      },
      py::arg("spans"), py::arg("decoration"), py::arg("name") = "", fluent);

  composition.def("box", &compose::box);
  composition.def(
      "text",
      [](const std::string& value, py::object size, py::object ink) {
        auto element = compose::text(value);
        if (!size.is_none())
          element.font({.size = py::isinstance<weave::Length>(size)
                                    ? size.cast<weave::Length>()
                                    : weave::Length{size.cast<float>()}});
        if (!ink.is_none()) element.ink(color(ink));
        return element;
      },
      py::arg("value"), py::arg("size") = py::none(),
      py::arg("color") = py::none());
  composition.def(
      "graphics",
      [](const std::string& key, py::function program, compose::Cache cache) {
        auto callback = retainCallback(std::move(program));
        return compose::graphics(
            key,
            [callback](draw::Pen& pen) {
              const py::gil_scoped_acquire lock;
              invokePen(callback->get(), pen);
            },
            cache);
      },
      py::arg("key"), py::arg("program"),
      py::arg("cache") = compose::Cache::None);
  composition.def(
      "pen",
      [](const std::string& key, py::function program, compose::Cache cache) {
        auto callback = retainCallback(std::move(program));
        return compose::pen(
            key,
            [callback](draw::Pen& pen) {
              const py::gil_scoped_acquire lock;
              invokePen(callback->get(), pen);
            },
            cache);
      },
      py::arg("key"), py::arg("program"),
      py::arg("cache") = compose::Cache::None);

  composition.def("memo", &memo, py::arg("properties"), py::arg("describe"));
  composition.def("stack", &stack)
      .def("positioned", &positioned)
      .def("slot", &slot);
  composition.def("image",
                  py::overload_cast<sk_sp<SkImage>, Fit>(&compose::image),
                  py::arg("image"), py::arg("fit") = Fit::Contain);
  composition.def(
      "picture", [](sk_sp<SkPicture> value, float width, float height) {
        return picture(std::move(value), SkSize::Make(width, height));
      });
  composition.def("pathFigure", &pathFigure, py::arg("path"),
                  py::arg("bleed") = 0.0f);
  composition.def(
      "text",
      [](const std::string& value, weave::TextStyle style) {
        return compose::text(value, std::move(style));
      },
      py::arg("value"), py::arg("style"), py::prepend());
}
}  // namespace sigil::python
