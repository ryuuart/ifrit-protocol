/** @file
 * The verbs EVERY node states, bound once over the value that states
 * them, and the verbs each typed leaf adds to them. Element, Text,
 * Image and Band are one vocabulary and three small additions, so a
 * chain reads the same in Python as it does in C++ and a verb only one
 * kind of leaf can use is on that leaf alone.
 */

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilcompose/typography/Annotation.h>
#include <sigilcompose/typography/Selector.h>
#include <sigilcompose/typography/TextPath.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Nodes.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/query/Selector.h>

#include <chrono>
#include <cmath>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;
using compose::Band;
using compose::CellSpan;
using compose::Corners;
using compose::Decoration;
using compose::Element;
using compose::Image;
using compose::Spans;
using compose::Text;
}  // namespace

template <class Node>
void bindNodeVerbs(py::class_<Node>& element) {
  element.def("copy", [](const Node& value) { return value; })
      .def("__copy__", [](const Node& value) { return value; })
      .def("row", &Node::row, fluent)
      .def("column", &Node::column, fluent)
      .def("flexGrow", &Node::flexGrow, py::arg("factor") = 1.0f, fluent)
      .def("flexShrink", &Node::flexShrink, py::arg("factor"), fluent)
      .def("absolute", &Node::absolute, fluent)
      .def("cover", &Node::cover, fluent)
      .def("key", &Node::key, py::arg("key"), fluent)
      .def("cache", &Node::cache, py::arg("policy"), fluent)
      .def(
          "children",
          [](Node& self, const std::vector<Element>& values) -> Node& {
            return self.children(values);
          },
          py::arg("children"), py::pos_only(), fluent)
      .def(
          "children",
          [](Node& self, py::args values) -> Node& {
            return self.children(elements(values));
          },
          fluent)
      .def(
          "size",
          [](Node& self, py::object width, py::object height) -> Node& {
            return self.width(dimension(width)).height(dimension(height));
          },
          py::arg("width"), py::arg("height"), fluent)
      .def(
          "fill",
          [](Node& self, py::object value) -> Node& {
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
          [](Node& self, py::object value) -> Node& {
            if (py::isinstance<compose::VarRef>(value))
              return self.ink(value.cast<compose::VarRef>());
            return self.ink(color(value));
          },
          py::arg("value"), fluent)
      .def("font", &Node::font, py::arg("type"), fluent)
      .def(
          "fontTrack",
          [](Node& self, py::object value) -> Node& {
            return self.font(
                {.track = py::isinstance<weave::Length>(value)
                              ? value.cast<weave::Length>()
                              : weave::Length{value.cast<float>()}});
          },
          py::arg("tracking"), fluent)
      .def(
          "fontSize",
          [](Node& self, py::object value) -> Node& {
            return self.font(
                {.size = py::isinstance<weave::Length>(value)
                             ? value.cast<weave::Length>()
                             : weave::Length{value.cast<float>()}});
          },
          py::arg("size"), fluent)
      .def(
          "fontWeight",
          [](Node& self, float value) -> Node& {
            return self.font({.weight = value});
          },
          py::arg("weight"), fluent)
      .def(
          "borderRadius",
          [](Node& self, float all) -> Node& {
            return self.borderRadius({all});
          },
          py::arg("all"), fluent)
      .def(
          "borderRadius",
          [](Node& self, float tl, float tr, float br, float bl) -> Node& {
            return self.borderRadius({tl, tr, br, bl});
          },
          py::arg("topLeft"), py::arg("topRight"), py::arg("bottomRight"),
          py::arg("bottomLeft"), fluent)
      .def(
          "transformOrigin",
          [](Node& self, py::object x, py::object y, py::object z) -> Node& {
            return self.transformOrigin(
                originLength(x), originLength(y),
                z.is_none() ? compose::Dimension(0.0f) : originLength(z));
          },
          py::arg("x"), py::arg("y"), py::arg("z") = py::none(), fluent)
      .def(
          "perspectiveOrigin",
          [](Node& self, py::object x, py::object y) -> Node& {
            return self.perspectiveOrigin(originLength(x), originLength(y));
          },
          py::arg("x"), py::arg("y"), fluent);

  const auto dimensionMethod = [&](const char* name,
                                   Node& (Node::*setter)(compose::Dimension)) {
    element.def(
        name,
        [setter](Node& self, py::object value) -> Node& {
          return (self.*setter)(dimension(value));
        },
        py::arg("value"), fluent);
  };
  dimensionMethod("width", &Node::width);
  dimensionMethod("height", &Node::height);
  dimensionMethod("minWidth", &Node::minWidth);
  dimensionMethod("minHeight", &Node::minHeight);
  dimensionMethod("maxWidth", &Node::maxWidth);
  dimensionMethod("maxHeight", &Node::maxHeight);
  dimensionMethod("gap", &Node::gap);
  dimensionMethod("left", &Node::left);
  dimensionMethod("top", &Node::top);
  dimensionMethod("right", &Node::right);
  dimensionMethod("bottom", &Node::bottom);

  element
      .def("flexDirection", &Node::flexDirection, py::arg("direction"), fluent)
      .def("flexWrap", &Node::flexWrap,
           py::arg("wrap") = compose::FlexWrap::Wrap, fluent)
      .def("boxSizing", &Node::boxSizing, py::arg("sizing"), fluent)
      .def("display", &Node::display, py::arg("display"), fluent)
      .def("aspectRatio", &Node::aspectRatio, py::arg("ratio"), fluent)
      .def(
          "flexBasis",
          [](Node& self, py::object value) -> Node& {
            return self.flexBasis(dimension(value));
          },
          py::arg("value"), fluent)
      .def(
          "alignItems",
          [](Node& self, py::object value) -> Node& {
            return self.alignItems(alignment(value));
          },
          py::arg("alignment"), fluent)
      .def(
          "alignSelf",
          [](Node& self, py::object value) -> Node& {
            return self.alignSelf(alignment(value));
          },
          py::arg("alignment"), fluent)
      .def(
          "justifyContent",
          [](Node& self, py::object value) -> Node& {
            return self.justifyContent(justification(value));
          },
          py::arg("alignment"), fluent)
      .def(
          "centerAt",
          [](Node& self, py::object value) -> Node& {
            return self.centerAt(point(value));
          },
          py::arg("point"), fluent)
      .def(
          "at",
          [](Node& self, py::object value) -> Node& {
            return self.at(point(value));
          },
          py::arg("point"), fluent)
      .def(
          "at",
          [](Node& self, py::object x, py::object y) -> Node& {
            return self.at(dimension(x), dimension(y));
          },
          py::arg("x"), py::arg("y"), fluent)
      .def(
          "rect",
          [](Node& self, py::object value) -> Node& {
            return self.rect(rect(value));
          },
          py::arg("rect"), fluent)
      .def(
          "rect",
          [](Node& self, py::object x, py::object y, py::object width,
             py::object height) -> Node& {
            return self.rect(dimension(x), dimension(y), dimension(width),
                             dimension(height));
          },
          py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
          fluent)
      .def("gridCells", py::overload_cast<int, int, int, int>(&Node::gridCells),
           py::arg("column"), py::arg("row"), py::arg("columns") = 1,
           py::arg("rows") = 1, fluent)
      .def("gridCells", py::overload_cast<CellSpan>(&Node::gridCells),
           py::arg("span"), fluent)
      .def("gridArea", &Node::gridArea, py::arg("name"), fluent)
      .def(
          "gridCellAlign",
          [](Node& self, py::object x, py::object y) -> Node& {
            return self.gridCellAlign(alignment(x), alignment(y));
          },
          py::arg("horizontal"), py::arg("vertical"), fluent)
      .def("borderRadius", py::overload_cast<Corners>(&Node::borderRadius),
           py::arg("radii"), fluent)
      .def(
          "shape",
          [](Node& self, py::object value) -> Node& {
            return self.shape(shape(value));
          },
          py::arg("value"), fluent)
      .def("overflow", &Node::overflow, py::arg("overflow"), fluent)
      .def("block", &Node::block, py::arg("block"), fluent)
      .def("styleSheet", &Node::styleSheet, py::arg("sheet"), fluent)
      .def("styleClass", &Node::styleClass, py::arg("name"), fluent)
      .def("role", py::overload_cast<weave::Rule>(&Node::role),
           py::arg("defaults"), fluent)
      .def("role", py::overload_cast<std::string>(&Node::role), py::arg("name"),
           fluent)
      .def(
          "var",
          [](Node& self, const std::string& name, py::object value) -> Node& {
            return std::visit(
                [&](const auto& converted) -> Node& {
                  return self.var(name, converted);
                },
                variable(value));
          },
          py::arg("name"), py::arg("value"), fluent)
      .def(
          "varDefaults",
          [](Node& self, const py::dict& defaults) -> Node& {
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
      .def("imageRendering", &Node::imageRendering, py::arg("sampling"), fluent)
      .def("hitTestable", &Node::hitTestable, py::arg("enabled"), fluent)
      .def("decorationOutline", &Node::decorationOutline, py::arg("source"),
           py::arg("coverage") = 0.5f, fluent)
      .def("layerStyle", &Node::layerStyle, py::arg("style"), fluent)
      .def("filter", &Node::filter, py::arg("effect"), fluent)
      .def("backdropFilter", &Node::backdropFilter, py::arg("effect"), fluent)
      .def("blendMode", &Node::blendMode, py::arg("mode"), fluent)
      .def("travel", &Node::travel, py::arg("path"), fluent)
      .def("zIndex", &Node::zIndex, py::arg("index"), fluent)
      .def("preserve3d", &Node::preserve3d, py::arg("preserve") = true, fluent)
      .def("backface", &Node::backface, py::arg("visibility"), fluent)
      .def("cacheScale", &Node::cacheScale, py::arg("scale"), fluent)
      .def("transition", &Node::transition, py::arg("transition"), fluent)

      ;
  for (const auto& [name, setter] : std::initializer_list<
           std::pair<const char*, Node& (Node::*)(motion::Animatable<float>)>>{
           {"opacity", &Node::opacity},
           {"rotate", &Node::rotate},
           {"scale", &Node::scale},
           {"scaleX", &Node::scaleX},
           {"scaleY", &Node::scaleY},
           {"translateX", &Node::translateX},
           {"translateY", &Node::translateY},
           {"skewX", &Node::skewX},
           {"skewY", &Node::skewY},
           {"rotateX", &Node::rotateX},
           {"rotateY", &Node::rotateY},
           {"translateZ", &Node::translateZ},
           {"scaleZ", &Node::scaleZ},
           {"perspective", &Node::perspective}})
    element.def(
        name,
        [setter](Node& self, py::object value) -> Node& {
          return (self.*setter)(motionAnimatable(value));
        },
        py::arg("value"), fluent);
  element.def(
      "staggerChildren",
      [](Node& self, double seconds, const std::string& origin) -> Node& {
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
  for (const auto& [name, setter] : std::initializer_list<
           std::pair<const char*, Node& (Node::*)(Decoration, std::string)>>{
           {"overlay", &Node::overlay},
           {"background", &Node::background},
           {"foreground", &Node::foreground},
           {"stroke", &Node::stroke}})
    element.def(
        name,
        [setter](Node& self, py::object value, const std::string& name)
            -> Node& { return (self.*setter)(decoration(value), name); },
        py::arg("decoration"), py::arg("name") = "", fluent);
  element.def(
      "stroke",
      [](Node& self, Spans spans, py::object value,
         const std::string& name) -> Node& {
        return self.stroke(std::move(spans), decoration(value), name);
      },
      py::arg("spans"), py::arg("decoration"), py::arg("name") = "", fluent);
  element.def(
      "background",
      [](Node& self, Spans spans, py::object value,
         const std::string& name) -> Node& {
        return self.background(std::move(spans), decoration(value), name);
      },
      py::arg("spans"), py::arg("decoration"), py::arg("name") = "", fluent);
}

template void bindNodeVerbs(py::class_<Element>&);
template void bindNodeVerbs(py::class_<Text>&);
template void bindNodeVerbs(py::class_<Image>&);
template void bindNodeVerbs(py::class_<Band>&);

void bindTextVerbs(py::class_<Text>& element) {
  element
      .def("paragraphStyles",
           py::overload_cast<std::vector<weave::ParagraphStyle>>(
               &Text::paragraphStyles),
           py::arg("blocks"), fluent)
      .def(
          "paragraphStyles",
          [](Text& self, const std::vector<std::string>& names) -> Text& {
            std::vector<std::string_view> views(names.begin(), names.end());
            return self.paragraphStyles(views);
          },
          py::arg("names"), fluent)
      .def("initialLetter", &Text::initialLetter, py::arg("initial"), fluent)
      .def("textFirstBaseline", &Text::textFirstBaseline, py::arg("rule"),
           py::arg("offset") = 0.0f, fluent)
      .def("textVerticalAlign", &Text::textVerticalAlign, py::arg("rule"),
           py::arg("maximumInterlineSpacing") = 0.0f, fluent)
      .def("textLineMargin", &Text::textLineMargin, py::arg("band"), fluent)
      .def("textWillChange", &Text::textWillChange, py::arg("enabled") = true,
           py::arg("candidates") = 0, fluent)
      .def(
          "textOverflow",
          [](Text& self, const std::string& marker) -> Text& {
            return self.textOverflow(marker);
          },
          py::arg("marker"), fluent)
      .def("maxTextLines", &Text::maxTextLines, py::arg("lines"), fluent)
      .def("textThreadTo", &Text::textThreadTo, py::arg("key"), fluent)
      .def("textThreadBalance", &Text::textThreadBalance,
           py::arg("throughLine") = ~0u, fluent)
      .def("contentFlowAround", &Text::contentFlowAround, py::arg("key"),
           py::arg("margin") = 0.0f, fluent)
      .def("textOnPath", &Text::textOnPath, py::arg("path"), fluent)
      .def("spanPaint", &Text::spanPaint, py::arg("where"), py::arg("paint"),
           fluent)
      .def("spanStyle",
           py::overload_cast<weave::Selector, weave::TextStyle>(
               &Text::spanStyle),
           py::arg("where"), py::arg("style"), fluent)
      .def("spanStyle",
           py::overload_cast<weave::Selector, weave::Type>(&Text::spanStyle),
           py::arg("where"), py::arg("type"), fluent)
      .def("textAnnotation", &Text::textAnnotation, py::arg("reading"), fluent)
      .def("atRest", &Text::atRest)
      .def(
          "textFill",
          [](Text& self, py::object value) -> Text& {
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
          [](Text& self, float width, py::object value) -> Text& {
            // The outline is one comparable Fill on the node, so the
            // flat-mark reading is the widest set it can honour: a
            // static paint collapses onto it and a live or
            // geometry-dependent one raises, naming the verb that does
            // resolve against the frame.
            return self.textStroke(width, fill(value));
          },
          py::arg("width"), py::arg("paint"), fluent);
}

void bindImageVerbs(py::class_<Image>& element) {
  element.def(
      "imageRegion",
      [](Image& self, py::object value) -> Image& {
        return self.imageRegion(rect(value));
      },
      py::arg("rect"), fluent);
}

void bindBandVerbs(py::class_<Band>& element) {
  element.def("bandAlignment", &Band::bandAlignment, py::arg("formation"),
              fluent);
}

}  // namespace sigil::python
