#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilcompose/typography/Annotation.h>
#include <sigilcompose/typography/Selector.h>
#include <sigilcompose/typography/TextPath.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>
#include <sigilweave/layout/Story.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/query/Selector.h>

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

compose::VarValue variable(py::handle value) {
  if (py::isinstance<py::str>(value)) {
    const auto text = value.cast<std::string>();
    if (text != "auto" && !text.ends_with("%")) return color(value);
  }
  if (py::isinstance<material::Color>(value) ||
      py::isinstance<py::tuple>(value) || py::isinstance<py::list>(value))
    return color(value);
  return dimension(value);
}

compose::Decoration decoration(py::handle value) {
  if (py::isinstance<compose::Decoration>(value))
    return value.cast<compose::Decoration>();
  if (py::isinstance<compose::PathFormat>(value))
    return value.cast<compose::PathFormat>();
  if (py::isinstance<compose::Shadow>(value))
    return value.cast<compose::Shadow>();
  throw py::type_error("A decoration is a Decoration, PathFormat, or Shadow.");
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
/** One length of a transform or perspective origin. A bare number is
 *  refused as the native verb refuses it: it reads as a fraction of the
 *  box as readily as a pixel count. */
compose::Dimension originLength(py::handle value) {
  if (py::isinstance<py::float_>(value) || py::isinstance<py::int_>(value))
    throw py::type_error(
        "An origin is written with its unit: pct(50) or '50%' of the node's "
        "box, Dimension(12) for pixels, a Length or a custom property. A "
        "bare number is refused.");
  return dimension(value);
}
}  // namespace

void bindCompose(py::module_& module) {
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
  composition.def("var", &compose::var, py::arg("name"));
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
      .def(py::init([](py::object value) { return dimension(value); }),
           py::arg("value"))
      .def_readwrite("unit", &Dimension::unit)
      .def_readwrite("value", &Dimension::value)
      .def("relative", &Dimension::relative)
      .def("reference", &Dimension::reference)
      .def(py::self == py::self);
  composition.def("pct", &pct, py::arg("percent"))
      .def("pw", &pw, py::arg("percent"))
      .def("ph", &ph, py::arg("percent"))
      .def("autoDimension", &autoDimension);
  py::enum_<FlexDirection>(composition, "FlexDirection")
      .value("Column", FlexDirection::Column)
      .value("ColumnReverse", FlexDirection::ColumnReverse)
      .value("Row", FlexDirection::Row)
      .value("RowReverse", FlexDirection::RowReverse);
  py::enum_<FlexWrap>(composition, "FlexWrap")
      .value("NoWrap", FlexWrap::NoWrap)
      .value("Wrap", FlexWrap::Wrap)
      .value("WrapReverse", FlexWrap::WrapReverse);
  py::enum_<Display>(composition, "Display")
      .value("Flex", Display::Flex)
      .value("None_", Display::None)
      .value("Contents", Display::Contents);
  py::enum_<BoxSizing>(composition, "BoxSizing")
      .value("BorderBox", BoxSizing::BorderBox)
      .value("ContentBox", BoxSizing::ContentBox);
  py::enum_<Overflow>(composition, "Overflow")
      .value("Visible", Overflow::Visible)
      .value("Clip", Overflow::Clip);
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
  py::enum_<Boundary>(composition, "Boundary")
      .value("Auto", Boundary::Auto)
      .value("Outline", Boundary::Outline)
      .value("Glyphs", Boundary::Glyphs)
      .value("Coverage", Boundary::Coverage);
  py::class_<Corners>(composition, "Corners")
      .def(py::init<>())
      .def(py::init<float>(), py::arg("radius"))
      .def(py::init<float, float, float, float>(), py::arg("topLeft"),
           py::arg("topRight"), py::arg("bottomRight"), py::arg("bottomLeft"))
      .def_readwrite("topLeft", &Corners::topLeft)
      .def_readwrite("topRight", &Corners::topRight)
      .def_readwrite("bottomRight", &Corners::bottomRight)
      .def_readwrite("bottomLeft", &Corners::bottomLeft)
      .def("any", &Corners::any)
      .def(py::self == py::self);
  py::class_<Fill>(composition, "Fill")
      .def(py::init<>())
      .def(py::init([](py::object value) { return fill(value); }),
           py::arg("value"))
      .def_static("none", &Fill::none)
      .def_static("currentInk", &Fill::currentInk)
      .def_static(
          "color", [](py::object value) { return Fill::color(color(value)); },
          py::arg("value"))
      .def_static("var", py::overload_cast<VarRef>(&Fill::var),
                  py::arg("reference"))
      .def_readonly("colorValue", &Fill::colorValue)
      .def(py::self == py::self);
  py::class_<SurfacePaint>(composition, "SurfacePaint")
      .def(py::init<>())
      .def(py::init([](py::object value) { return surfacePaint(value); }),
           py::arg("value"))
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
      .def(py::init([](py::object value) { return shape(value); }),
           py::arg("value"))
      .def(
          "path",
          [](const Shape& self, float w, float h) {
            return self.path(SkSize::Make(w, h));
          },
          py::arg("width"), py::arg("height"))
      .def("comparable", &Shape::comparable)
      .def(py::self == py::self);
  composition.def(
      "shape", [](py::object value) { return shape(value); }, py::arg("value"));
  composition.def(
      "heldPath", [](SkPath path) { return Shape{heldPath(std::move(path))}; },
      py::arg("path"));
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
      .def(py::init([](py::object value) { return decoration(value); }),
           py::arg("value"))
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
          })
      .def_static(
          "echo",
          [](py::object offset, py::object ink) {
            return LayerStyle::echo(point(offset), color(ink));
          },
          py::arg("offset"), py::arg("ink"));
  py::class_<Spans>(composition, "Spans")
      .def(
          "__or__", [](const Spans& a, const Spans& b) { return a | b; },
          py::is_operator(), py::arg("other"));
  auto selections = composition.def_submodule("spans");
  selections.def(
      "range",
      [](py::object a, py::object b) {
        return spans::range(motionAnimatable(a), motionAnimatable(b));
      },
      py::arg("start"), py::arg("end"));
  selections.def(
      "wrap",
      [](py::object a, py::object b) {
        return spans::wrap(motionAnimatable(a), motionAnimatable(b));
      },
      py::arg("start"), py::arg("end"));
  selections.def(
      "upTo", [](py::object end) { return spans::upTo(motionAnimatable(end)); },
      py::arg("end"));
  selections.def("corners", &spans::corners, py::arg("arm"),
                 py::arg("angle") = 30.0f);
  selections.def("edges", &spans::edges, py::arg("arm"),
                 py::arg("angle") = 30.0f);
  selections.def("every", &spans::every, py::arg("count"),
                 py::arg("duty") = 1.0f);
  selections.def("at", &spans::at, py::arg("index"), py::arg("count"));
  selections.def("fit", &spans::fit, py::arg("key"), py::arg("margin") = 0.0f);
  selections.def("rest", py::overload_cast<>(&spans::rest));
  selections.def("rest", py::overload_cast<std::string_view>(&spans::rest),
                 py::arg("name"));

  auto annotation = bindRecord<Annotation>(composition, "Annotation",
                                           "Unknown annotation field: ");
  py::enum_<Annotation::Side>(annotation, "Side")
      .value("Before", Annotation::Side::Before)
      .value("After", Annotation::Side::After);
  annotation.def_readwrite("where", &Annotation::where)
      .def_readwrite("unit", &Annotation::unit)
      .def_readwrite("readings", &Annotation::readings)
      .def_readwrite("style", &Annotation::style)
      .def_readwrite("side", &Annotation::side)
      .def_readwrite("gap", &Annotation::gap)
      .def_readwrite("reserve", &Annotation::reserve)
      .def(py::self == py::self);
  auto textPath =
      bindRecord<TextPath>(composition, "TextPath", "Unknown TextPath field: ");
  py::enum_<TextPath::Align>(textPath, "Align")
      .value("Start", TextPath::Align::Start)
      .value("Center", TextPath::Align::Center)
      .value("End", TextPath::Align::End);
  py::enum_<TextPath::Orient>(textPath, "Orient")
      .value("Tangent", TextPath::Orient::Tangent)
      .value("Radial", TextPath::Orient::Radial)
      .value("Upright", TextPath::Orient::Upright);
  textPath
      .def_property(
          "path", [](const TextPath& value) { return value.path; },
          [](TextPath& value, py::object path) { value.path = shape(path); })
      .def_property(
          "at", [](const TextPath& value) { return value.at; },
          [](TextPath& value, py::object at) {
            value.at = motionAnimatable(at);
          })
      .def_readwrite("align", &TextPath::align)
      .def_readwrite("offset", &TextPath::offset)
      .def_readwrite("autoFlip", &TextPath::autoFlip)
      .def_readwrite("orient", &TextPath::orient)
      .def_readwrite("exactTangent", &TextPath::exactTangent);
  auto textSelections = composition.def_submodule("selectors");
  textSelections.def("style", &selectors::style, py::arg("name"))
      .def("inFrame", &selectors::inFrame, py::arg("key"));

  element.def("copy", [](const Element& value) { return value; })
      .def("__copy__", [](const Element& value) { return value; })
      .def("row", &Element::row, fluent)
      .def("column", &Element::column, fluent)
      .def("flexGrow", &Element::flexGrow, py::arg("factor") = 1.0f, fluent)
      .def("flexShrink", &Element::flexShrink, py::arg("factor"), fluent)
      .def("absolute", &Element::absolute, fluent)
      .def("cover", &Element::cover, fluent)
      .def("key", &Element::key, py::arg("key"), fluent)
      .def("cache", &Element::cache, py::arg("policy"), fluent)
      .def(
          "children",
          [](Element& self, const std::vector<Element>& values) -> Element& {
            return self.children(values);
          },
          py::arg("children"), py::pos_only(), fluent)
      .def(
          "children",
          [](Element& self, py::args values) -> Element& {
            return self.children(elements(values));
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
            // One conversion for every surface-colouring parameter, so a
            // material reaches the node's fill exactly as it reaches a
            // stroke's or a kit ground's. Empty is STATED rather than
            // applied, because applying nothing leaves a standing fill
            // where it is — and all three spellings of nothing, None, an
            // empty Fill and an empty paint, must clear the same way.
            const compose::SurfacePaint paint = surfacePaint(value);
            if (paint.none()) return self.fill(compose::Fill::none());
            return paint.apply(self);
          },
          py::arg("value"), fluent)
      .def(
          "ink",
          [](Element& self, py::object value) -> Element& {
            if (py::isinstance<compose::VarRef>(value))
              return self.ink(value.cast<compose::VarRef>());
            return self.ink(color(value));
          },
          py::arg("value"), fluent)
      .def("font", &Element::font, py::arg("type"), fluent)
      .def(
          "fontTrack",
          [](Element& self, py::object value) -> Element& {
            return self.font(
                {.track = py::isinstance<weave::Length>(value)
                              ? value.cast<weave::Length>()
                              : weave::Length{value.cast<float>()}});
          },
          py::arg("tracking"), fluent)
      .def(
          "fontSize",
          [](Element& self, py::object value) -> Element& {
            return self.font(
                {.size = py::isinstance<weave::Length>(value)
                             ? value.cast<weave::Length>()
                             : weave::Length{value.cast<float>()}});
          },
          py::arg("size"), fluent)
      .def(
          "fontWeight",
          [](Element& self, float value) -> Element& {
            return self.font({.weight = value});
          },
          py::arg("weight"), fluent)
      .def(
          "borderRadius",
          [](Element& self, float all) -> Element& {
            return self.borderRadius({all});
          },
          py::arg("all"), fluent)
      .def(
          "borderRadius",
          [](Element& self, float tl, float tr, float br, float bl)
              -> Element& { return self.borderRadius({tl, tr, br, bl}); },
          py::arg("topLeft"), py::arg("topRight"), py::arg("bottomRight"),
          py::arg("bottomLeft"), fluent)
      .def(
          "inset",
          [](Element& self, py::object all) -> Element& {
            return self.inset(dimension(all));
          },
          py::arg("all"), fluent)
      .def(
          "transformOrigin",
          [](Element& self, py::object x, py::object y,
             py::object z) -> Element& {
            return self.transformOrigin(
                originLength(x), originLength(y),
                z.is_none() ? compose::Dimension(0.0f) : originLength(z));
          },
          py::arg("x"), py::arg("y"), py::arg("z") = py::none(), fluent)
      .def(
          "perspectiveOrigin",
          [](Element& self, py::object x, py::object y) -> Element& {
            return self.perspectiveOrigin(originLength(x), originLength(y));
          },
          py::arg("x"), py::arg("y"), fluent);

  const auto dimensionMethod =
      [&](const char* name, Element& (Element::*setter)(compose::Dimension)) {
        element.def(
            name,
            [setter](Element& self, py::object value) -> Element& {
              return (self.*setter)(dimension(value));
            },
            py::arg("value"), fluent);
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

  element
      .def("flexDirection", &Element::flexDirection, py::arg("direction"),
           fluent)
      .def("flexWrap", &Element::flexWrap,
           py::arg("wrap") = compose::FlexWrap::Wrap, fluent)
      .def("boxSizing", &Element::boxSizing, py::arg("sizing"), fluent)
      .def("display", &Element::display, py::arg("display"), fluent)
      .def("aspectRatio", &Element::aspectRatio, py::arg("ratio"), fluent)
      .def(
          "flexBasis",
          [](Element& self, py::object value) -> Element& {
            return self.flexBasis(dimension(value));
          },
          py::arg("value"), fluent)
      .def(
          "alignItems",
          [](Element& self, py::object value) -> Element& {
            return self.alignItems(alignment(value));
          },
          py::arg("alignment"), fluent)
      .def(
          "alignSelf",
          [](Element& self, py::object value) -> Element& {
            return self.alignSelf(alignment(value));
          },
          py::arg("alignment"), fluent)
      .def(
          "justifyContent",
          [](Element& self, py::object value) -> Element& {
            return self.justifyContent(justification(value));
          },
          py::arg("alignment"), fluent)
      .def(
          "centerAt",
          [](Element& self, py::object value) -> Element& {
            return self.centerAt(point(value));
          },
          py::arg("point"), fluent)
      .def(
          "at",
          [](Element& self, py::object value) -> Element& {
            return self.at(point(value));
          },
          py::arg("point"), fluent)
      .def(
          "at",
          [](Element& self, py::object x, py::object y) -> Element& {
            return self.at(dimension(x), dimension(y));
          },
          py::arg("x"), py::arg("y"), fluent)
      .def(
          "rect",
          [](Element& self, py::object value) -> Element& {
            return self.rect(rect(value));
          },
          py::arg("rect"), fluent)
      .def(
          "rect",
          [](Element& self, py::object x, py::object y, py::object width,
             py::object height) -> Element& {
            return self.rect(dimension(x), dimension(y), dimension(width),
                             dimension(height));
          },
          py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
          fluent)
      .def("gridCells",
           py::overload_cast<int, int, int, int>(&Element::gridCells),
           py::arg("column"), py::arg("row"), py::arg("columns") = 1,
           py::arg("rows") = 1, fluent)
      .def("gridCells", py::overload_cast<CellSpan>(&Element::gridCells),
           py::arg("span"), fluent)
      .def("gridArea", &Element::gridArea, py::arg("name"), fluent)
      .def(
          "gridCellAlign",
          [](Element& self, py::object x, py::object y) -> Element& {
            return self.gridCellAlign(alignment(x), alignment(y));
          },
          py::arg("horizontal"), py::arg("vertical"), fluent)
      .def("borderRadius", py::overload_cast<Corners>(&Element::borderRadius),
           py::arg("radii"), fluent)
      .def(
          "shape",
          [](Element& self, py::object value) -> Element& {
            return self.shape(shape(value));
          },
          py::arg("value"), fluent)
      .def("centered", &Element::centered, fluent)
      .def("inward", &Element::inward, fluent)
      .def("outward", &Element::outward, fluent)
      .def("overflow", &Element::overflow, py::arg("overflow"), fluent)
      .def("block", &Element::block, py::arg("block"), fluent)
      .def("paragraphs",
           py::overload_cast<std::vector<weave::ParagraphStyle>>(
               &Element::paragraphs),
           py::arg("blocks"), fluent)
      .def(
          "paragraphs",
          [](Element& self, const std::vector<std::string>& names) -> Element& {
            std::vector<std::string_view> views(names.begin(), names.end());
            return self.paragraphs(views);
          },
          py::arg("names"), fluent)
      .def("initialLetter", &Element::initialLetter, py::arg("initial"), fluent)
      .def("textFirstBaseline", &Element::textFirstBaseline, py::arg("rule"),
           py::arg("offset") = 0.0f, fluent)
      .def("distribute", &Element::distribute, py::arg("rule"),
           py::arg("maximumInterlineSpacing") = 0.0f, fluent)
      .def("reserve", &Element::reserve, py::arg("band"), fluent)
      .def("live", &Element::live, py::arg("enabled") = true,
           py::arg("candidates") = 0, fluent)
      .def(
          "textOverflow",
          [](Element& self, const std::string& marker) -> Element& {
            return self.textOverflow(marker);
          },
          py::arg("marker"), fluent)
      .def("maxTextLines", &Element::maxTextLines, py::arg("lines"), fluent)
      .def("thread", &Element::thread, py::arg("key"), fluent)
      .def("balanceChain", &Element::balanceChain, py::arg("throughLine") = ~0u,
           fluent)
      .def("contentFlowAround", &Element::contentFlowAround, py::arg("key"),
           py::arg("margin") = 0.0f, fluent)
      .def("textOnPath", &Element::textOnPath, py::arg("path"), fluent)
      .def("spanPaint", &Element::spanPaint, py::arg("where"), py::arg("paint"),
           fluent)
      .def("spanStyle",
           py::overload_cast<weave::Selector, weave::TextStyle>(
               &Element::spanStyle),
           py::arg("where"), py::arg("style"), fluent)
      .def("spanStyle",
           py::overload_cast<weave::Selector, weave::Type>(&Element::spanStyle),
           py::arg("where"), py::arg("type"), fluent)
      .def("styleSheet", &Element::styleSheet, py::arg("sheet"), fluent)
      .def("styleClass", &Element::styleClass, py::arg("name"), fluent)
      .def("role", py::overload_cast<weave::Rule>(&Element::role),
           py::arg("defaults"), fluent)
      .def("role", py::overload_cast<std::string>(&Element::role),
           py::arg("name"), fluent)
      .def(
          "var",
          [](Element& self, const std::string& name,
             py::object value) -> Element& {
            return std::visit(
                [&](const auto& converted) -> Element& {
                  return self.var(name, converted);
                },
                variable(value));
          },
          py::arg("name"), py::arg("value"), fluent)
      .def(
          "varDefaults",
          [](Element& self, const py::dict& defaults) -> Element& {
            compose::VarTable table;
            try {
              for (auto [name, value] : defaults)
                table.set(compose::var(name.cast<std::string>()),
                          variable(value));
            } catch (const py::cast_error&) {
              throw py::type_error(
                  "Default properties require string names and color or "
                  "dimension values");
            }
            return self.varDefaults(std::move(table));
          },
          py::arg("defaults"), fluent)
      .def("imageRendering", &Element::imageRendering, py::arg("sampling"),
           fluent)
      .def("hitTestable", &Element::hitTestable, py::arg("enabled"), fluent)
      .def("decorationOutline", &Element::decorationOutline, py::arg("source"),
           py::arg("coverage") = 0.5f, fluent)
      .def("layerStyle", &Element::layerStyle, py::arg("style"), fluent)
      .def("annotate", &Element::annotate, py::arg("reading"), fluent)
      .def("filter", &Element::filter, py::arg("effect"), fluent)
      .def("backdropFilter", &Element::backdropFilter, py::arg("effect"),
           fluent)
      .def("appear", &Element::appear, py::arg("entrance"), fluent)
      .def("blendMode", &Element::blendMode, py::arg("mode"), fluent)
      .def("travel", &Element::travel, py::arg("path"), fluent)
      .def("zIndex", &Element::zIndex, py::arg("index"), fluent)
      .def("preserve3d", &Element::preserve3d, py::arg("preserve") = true,
           fluent)
      .def("backface", &Element::backface, py::arg("visibility"), fluent)
      .def("atRest", &Element::atRest)
      .def("cacheScale", &Element::cacheScale, py::arg("scale"), fluent)
      .def("transition", &Element::transition, py::arg("transition"), fluent)

      .def(
          "region",
          [](Element& self, py::object value) -> Element& {
            return self.region(rect(value));
          },
          py::arg("rect"), fluent)
      .def(
          "textFill",
          [](Element& self, py::object value) -> Element& {
            // A glyph paint is stored as one paint and resolved without
            // the tree, so the spellings it cannot store say so here
            // rather than leaving a standing override untouched and the
            // author guessing why.
            const compose::SurfacePaint paint = surfacePaint(value);
            if (!paint.none() && !paint.collapsedPaint())
              throw py::type_error(
                  "A glyph paint is stored as one paint and resolved without "
                  "the tree, so the ink in force, a custom property and a "
                  "bound fill have no paint to give it. The glyphs already "
                  "take the ink in force where no glyph paint reaches; clear "
                  "one with None.");
            return self.textFill(paint);
          },
          py::arg("paint"), fluent)
      .def(
          "textStroke",
          [](Element& self, float width, py::object value) -> Element& {
            // The outline is one comparable Fill on the node, so the
            // flat-mark reading is the widest set it can honour: a
            // static paint collapses onto it and a live or
            // geometry-dependent one raises, naming the verb that does
            // resolve against the frame.
            return self.textStroke(width, fill(value));
          },
          py::arg("width"), py::arg("paint"), fluent);
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
           {"translateZ", &Element::translateZ},
           {"scaleZ", &Element::scaleZ},
           {"perspective", &Element::perspective}})
    element.def(
        name,
        [setter](Element& self, py::object value) -> Element& {
          return (self.*setter)(motionAnimatable(value));
        },
        py::arg("value"), fluent);
  // One, two and four dimensions are three arities, so each is its own
  // overload with its own named inputs: a keyword call reads the same as
  // `inset` next door, and pybind names the three forms when none matches.
  for (const char* name : {"padding", "margin"}) {
    const bool padding = std::string_view{name} == "padding";
    element.def(
        name,
        [padding](Element& self, py::object all) -> Element& {
          return padding ? self.padding(dimension(all))
                         : self.margin(dimension(all));
        },
        py::arg("all"), fluent);
    element.def(
        name,
        [padding](Element& self, py::object horizontal,
                  py::object vertical) -> Element& {
          return padding
                     ? self.padding(dimension(horizontal), dimension(vertical))
                     : self.margin(dimension(horizontal), dimension(vertical));
        },
        py::arg("horizontal"), py::arg("vertical"), fluent);
    element.def(
        name,
        [padding](Element& self, py::object left, py::object top,
                  py::object right, py::object bottom) -> Element& {
          return padding ? self.padding(dimension(left), dimension(top),
                                        dimension(right), dimension(bottom))
                         : self.margin(dimension(left), dimension(top),
                                       dimension(right), dimension(bottom));
        },
        py::arg("left"), py::arg("top"), py::arg("right"), py::arg("bottom"),
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
      py::arg("left"), py::arg("top"), py::arg("right"), py::arg("bottom"),
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

  composition.def("text", py::overload_cast<weave::RichText>(&compose::text),
                  py::arg("content"));
  composition.def("frame", &compose::frame, py::arg("story"));
  composition.def("box", [](py::args children) {
    return compose::box().children(elements(children));
  });
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
  composition.def("memo", &memo, py::arg("properties"), py::arg("describe"));
  composition
      .def("stack",
           [](py::args children) {
             return stack().children(elements(children));
           })
      .def("positioned",
           [](py::args children) {
             return positioned().children(elements(children));
           })
      .def("slot", &slot, py::arg("name"));
  composition.def(
      "image",
      py::overload_cast<sk_sp<SkImage>, material::skia::Fit>(&compose::image),
      py::arg("image"), py::arg("fit") = material::skia::Fit::Contain);
  composition.def(
      "picture",
      [](sk_sp<SkPicture> value, float width, float height) {
        return picture(std::move(value), SkSize::Make(width, height));
      },
      py::arg("picture"), py::arg("width"), py::arg("height"));
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
