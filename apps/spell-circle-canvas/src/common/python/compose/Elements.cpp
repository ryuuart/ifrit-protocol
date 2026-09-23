#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/FontStyle.h>
#include <sigilcompose/core/LineSetting.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilcompose/typography/Annotation.h>
#include <sigilcompose/typography/Selector.h>
#include <sigilcompose/typography/TextPath.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Nodes.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>
#include <sigilweave/layout/Story.h>
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

/** Raises where @p length is auto, which is no length: native arithmetic
 *  on it warns and stands as auto, and here the call says so instead. */
void requireLength(const compose::Dimension& length) {
  if (length.unit == compose::Dimension::Unit::Auto)
    throw py::value_error("auto is no length, so no arithmetic takes it.");
}

/** @p left plus @p right times @p sign, raising where native arithmetic
 *  refuses the pair: auto on either side, or a percentage beside another
 *  unit — a zero of pixels aside, which adds nothing. */
compose::Dimension combined(const compose::Dimension& left,
                            const compose::Dimension& right, float sign) {
  using Unit = compose::Dimension::Unit;
  requireLength(left);
  requireLength(right);
  const bool leftPercent = left.unit == Unit::Pct;
  const bool rightPercent = right.unit == Unit::Pct;
  if (leftPercent != rightPercent) {
    const compose::Dimension& other = leftPercent ? right : left;
    if (!(other.unit == Unit::Px && other.value == 0.0f))
      throw py::value_error(
          "A percentage is laid out against the parent and shares a sum "
          "with no other unit. Use pw or ph to measure the canvas, or state "
          "the two on different properties.");
  }
  return sign > 0.0f ? left + right : left - right;
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
  py::enum_<compose::PaintAnchor>(composition, "PaintAnchor")
      .value("OwnBox", compose::PaintAnchor::OwnBox)
      .value("DeclaringBox", compose::PaintAnchor::DeclaringBox)
      .value("CanvasBox", compose::PaintAnchor::CanvasBox);
  py::enum_<compose::BackgroundOrigin>(composition, "BackgroundOrigin")
      .value("BorderBox", compose::BackgroundOrigin::BorderBox)
      .value("PaddingBox", compose::BackgroundOrigin::PaddingBox)
      .value("ContentBox", compose::BackgroundOrigin::ContentBox);
  py::class_<Element> element(composition, "Element");
  // The typed leaves: a node, plus what only that leaf can say. Each
  // converts to an Element, so a leaf drops into any children list.
  py::class_<compose::Text> textLeaf(composition, "Text");
  py::class_<compose::Image> imageLeaf(composition, "Image");
  py::class_<compose::Band> bandLeaf(composition, "Band");
  // What a text span states: the element's font and ink verbs, on a value
  // that belongs to no element.
  py::class_<compose::SpanDeclarations> spanDeclarations(
      composition, "SpanDeclarations",
      "What a text span states over the range it finds: the element's own "
      "font and ink verbs — `font`, its longhands and `ink` — written into "
      "a value that belongs to no element, handed to `Text.span`. What it "
      "leaves unsaid the range keeps. A paint stated here is laid in the "
      "passage's own coordinates, as it is.");
  spanDeclarations
      .def(py::init<>(), "A span's declarations, stating nothing yet.")
      .def("copy", [](const compose::SpanDeclarations& value) { return value; })
      .def("__copy__",
           [](const compose::SpanDeclarations& value) { return value; });
  element
      .def(py::init([](const compose::Text& leaf) { return Element(leaf); }),
           py::arg("leaf"))
      .def(py::init([](const compose::Image& leaf) { return Element(leaf); }),
           py::arg("leaf"))
      .def(py::init([](const compose::Band& leaf) { return Element(leaf); }),
           py::arg("leaf"));
  py::implicitly_convertible<compose::Text, Element>();
  py::implicitly_convertible<compose::Image, Element>();
  py::implicitly_convertible<compose::Band, Element>();
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
      .value("Ph", Dimension::Unit::Ph)
      .value("Ch", Dimension::Unit::Ch)
      .value("Pt", Dimension::Unit::Pt)
      .value("Calc", Dimension::Unit::Calc);
  dim.def(py::init<>())
      .def(py::init([](py::object value) { return dimension(value); }),
           py::arg("value"))
      // Read-only: a sum's value is a counted handle, which only the
      // arithmetic below may make or copy.
      .def_readonly("unit", &Dimension::unit)
      .def_readonly("value", &Dimension::value)
      .def("relative", &Dimension::relative)
      .def("reference", &Dimension::reference)
      .def(py::self == py::self)
      // CSS's calc(), as arithmetic: lengths in one unit stay in it, a sum
      // over several resolves where the node lands, and a number stands
      // for pixels. What native arithmetic refuses — a percentage beside
      // another unit, auto, a division by zero — raises here, at the call.
      .def(
          "__add__",
          [](const Dimension& self, py::handle other) {
            return combined(self, dimension(other), 1.0f);
          },
          py::arg("other"), py::is_operator())
      .def(
          "__radd__",
          [](const Dimension& self, py::handle other) {
            return combined(dimension(other), self, 1.0f);
          },
          py::arg("other"), py::is_operator())
      .def(
          "__sub__",
          [](const Dimension& self, py::handle other) {
            return combined(self, dimension(other), -1.0f);
          },
          py::arg("other"), py::is_operator())
      .def(
          "__rsub__",
          [](const Dimension& self, py::handle other) {
            return combined(dimension(other), self, -1.0f);
          },
          py::arg("other"), py::is_operator())
      .def(
          "__mul__",
          [](const Dimension& self, float factor) {
            requireLength(self);
            return self * factor;
          },
          py::arg("factor"), py::is_operator())
      .def(
          "__rmul__",
          [](const Dimension& self, float factor) {
            requireLength(self);
            return factor * self;
          },
          py::arg("factor"), py::is_operator())
      .def(
          "__truediv__",
          [](const Dimension& self, float divisor) {
            if (divisor == 0.0f)
              throw py::value_error("A length divided by zero is no length.");
            requireLength(self);
            return self / divisor;
          },
          py::arg("divisor"), py::is_operator())
      .def(
          "__neg__",
          [](const Dimension& self) {
            requireLength(self);
            return -self;
          },
          py::is_operator());
  composition.def("pct", &pct, py::arg("percent"))
      .def("pw", &pw, py::arg("percent"))
      .def("ph", &ph, py::arg("percent"))
      .def("autoDimension", &autoDimension);
  // The font-relative units and the point, each answering a Dimension, so
  // a length written here needs no second type to pass through. The
  // literals C++ spells them with have no equivalent in this language.
  composition
      .def(
          "em", [](float multiple) { return Dimension(weave::em(multiple)); },
          py::arg("multiple"))
      .def(
          "rem", [](float multiple) { return Dimension(weave::rem(multiple)); },
          py::arg("multiple"))
      .def(
          "lh", [](float multiple) { return Dimension(weave::lh(multiple)); },
          py::arg("multiple"))
      .def(
          "ch", [](float multiple) { return Dimension(weave::ch(multiple)); },
          py::arg("multiple"))
      .def(
          "pt", [](float amount) { return Dimension(weave::pt(amount)); },
          py::arg("amount"))
      .def("parseDimension", &parseDimension, py::arg("text"));
  // Every property a verb or a rule may state. The three keywords one may
  // be written as instead of a value are SigilWeave's, beside the text
  // partials that take the same three.
  py::enum_<Property>(composition, "Property")
      .value("Display", Property::Display)
      .value("BoxSizing", Property::BoxSizing)
      .value("Gap", Property::Gap)
      .value("PaddingTop", Property::PaddingTop)
      .value("PaddingRight", Property::PaddingRight)
      .value("PaddingBottom", Property::PaddingBottom)
      .value("PaddingLeft", Property::PaddingLeft)
      .value("MarginTop", Property::MarginTop)
      .value("MarginRight", Property::MarginRight)
      .value("MarginBottom", Property::MarginBottom)
      .value("MarginLeft", Property::MarginLeft)
      .value("Width", Property::Width)
      .value("Height", Property::Height)
      .value("MinWidth", Property::MinWidth)
      .value("MaxWidth", Property::MaxWidth)
      .value("MinHeight", Property::MinHeight)
      .value("MaxHeight", Property::MaxHeight)
      .value("AspectRatio", Property::AspectRatio)
      .value("FlexDirection", Property::FlexDirection)
      .value("FlexWrap", Property::FlexWrap)
      .value("FlexBasis", Property::FlexBasis)
      .value("FlexGrow", Property::FlexGrow)
      .value("FlexShrink", Property::FlexShrink)
      .value("AlignItems", Property::AlignItems)
      .value("AlignSelf", Property::AlignSelf)
      .value("JustifyContent", Property::JustifyContent)
      .value("Absolute", Property::Absolute)
      .value("Left", Property::Left)
      .value("Top", Property::Top)
      .value("Right", Property::Right)
      .value("Bottom", Property::Bottom)
      .value("CenterAt", Property::CenterAt)
      .value("GridCells", Property::GridCells)
      .value("GridCellAlign", Property::GridCellAlign)
      .value("GridArea", Property::GridArea)
      .value("BorderRadius", Property::BorderRadius)
      .value("Shape", Property::Shape)
      .value("Overflow", Property::Overflow)
      .value("Fill", Property::Fill)
      .value("Opacity", Property::Opacity)
      .value("BlendMode", Property::BlendMode)
      .value("BackgroundOrigin", Property::BackgroundOrigin)
      .value("ZIndex", Property::ZIndex)
      .value("TranslateX", Property::TranslateX)
      .value("TranslateY", Property::TranslateY)
      .value("Rotate", Property::Rotate)
      .value("Scale", Property::Scale)
      .value("ScaleX", Property::ScaleX)
      .value("ScaleY", Property::ScaleY)
      .value("SkewX", Property::SkewX)
      .value("SkewY", Property::SkewY)
      .value("TransformOrigin", Property::TransformOrigin)
      .value("RotateX", Property::RotateX)
      .value("RotateY", Property::RotateY)
      .value("TranslateZ", Property::TranslateZ)
      .value("ScaleZ", Property::ScaleZ)
      .value("Perspective", Property::Perspective)
      .value("PerspectiveOrigin", Property::PerspectiveOrigin)
      .value("TransformOriginZ", Property::TransformOriginZ)
      .value("Preserve3d", Property::Preserve3d)
      .value("Backface", Property::Backface)
      .value("DecorationOutline", Property::DecorationOutline)
      .value("Font", Property::Font)
      .value("Paragraph", Property::Paragraph)
      .value("Ink", Property::Ink)
      .value("CustomProperties", Property::CustomProperties)
      .value("ImageRendering", Property::ImageRendering);
  composition.def("inheritsByDefault", &inheritsByDefault, py::arg("property"))
      .def("propertyName", &propertyName, py::arg("property"));
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
  py::enum_<TextWrap>(composition, "TextWrap")
      .value("Auto", TextWrap::Auto)
      .value("Balance", TextWrap::Balance)
      .value("Stable", TextWrap::Stable)
      .value("Pretty", TextWrap::Pretty);
  py::enum_<TextJustify>(composition, "TextJustify")
      .value("Auto", TextJustify::Auto)
      .value("InterWord", TextJustify::InterWord)
      .value("InterCharacter", TextJustify::InterCharacter)
      .value("None_", TextJustify::None);
  // CSS's font-style as a value: the two keywords as class attributes, an
  // oblique made by name or from a bare number of degrees.
  py::class_<FontStyle> fontStyle(
      composition, "FontStyle",
      "How the type leans — CSS font-style: FontStyle.Normal, "
      "FontStyle.Italic, or FontStyle.oblique(degrees), positive leaning "
      "right and 14 when no angle is given. A bare number is that oblique. "
      "Italic is the family's italic face, else its ital axis, else an "
      "oblique of 14 degrees.");
  py::enum_<FontStyle::Kind>(fontStyle, "Kind")
      .value("Normal", FontStyle::Kind::Normal)
      .value("Italic", FontStyle::Kind::Italic)
      .value("Oblique", FontStyle::Kind::Oblique);
  fontStyle
      .def(py::init<float>(), py::arg("degrees"),
           "The oblique of `degrees`, positive leaning right.")
      .def_readonly_static("Normal", &FontStyle::Normal)
      .def_readonly_static("Italic", &FontStyle::Italic)
      .def_static("oblique", &FontStyle::oblique, py::arg("degrees") = 14.0f,
                  "A lean of `degrees`, positive to the right; 14 when no "
                  "angle is given.")
      .def_readonly("kind", &FontStyle::kind)
      .def_readonly("degrees", &FontStyle::degrees,
                    "The lean under Oblique, positive to the right; 0 "
                    "otherwise.")
      .def(py::self == py::self)
      .def("__hash__",
           [](const FontStyle& style) {
             return py::hash(
                 py::make_tuple(static_cast<int>(style.kind), style.degrees));
           })
      .def("__repr__", [](const FontStyle& style) -> std::string {
        switch (style.kind) {
          case FontStyle::Kind::Normal:
            return "FontStyle.Normal";
          case FontStyle::Kind::Italic:
            return "FontStyle.Italic";
          case FontStyle::Kind::Oblique:
            break;
        }
        return "FontStyle.oblique(" +
               py::repr(py::float_(style.degrees)).cast<std::string>() + ")";
      });
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

  bindNodeVerbs(element);
  bindNodeVerbs(textLeaf);
  bindNodeVerbs(imageLeaf);
  bindNodeVerbs(bandLeaf);
  bindTextVerbs(textLeaf);
  bindFontVerbs(spanDeclarations);
  bindImageVerbs(imageLeaf);
  bindBandVerbs(bandLeaf);

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
      "point", &compose::point,
      "A node with no extent: a place in its parent's box that carries a "
      "key and facts and draws nothing. Out of the flow, so it takes no "
      "room beside its siblings; put it where it names with the placement "
      "longhand or a centre pin. A skeleton of points is what an arranging "
      "operator places and an adding operator builds on.");
  composition.def(
      "text",
      [](const std::string& value, weave::TextStyle style) {
        return compose::text(value, std::move(style));
      },
      py::arg("value"), py::arg("style"), py::prepend());
}
}  // namespace sigil::python
