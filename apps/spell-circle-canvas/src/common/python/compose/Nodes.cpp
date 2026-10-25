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
#include <sigilgeometry/advanced/Skia.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Nodes.h>
#include <sigilpython/compose/Operators.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/query/Selector.h>

#include <cctype>
#include <chrono>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace {

/** A bound `compose.Attributes`, spelled under its Python name. The
 *  class is registered after this file, so a signature naming the
 *  native type would be written before that name exists; this one
 *  carries the name itself and refuses anything that is not a table of
 *  facts at the call. */
class AttributesObject : public py::object {
 public:
  using py::object::object;
  static bool check_(py::handle value) {
    return py::isinstance<compose::Attributes>(value);
  }
};

}  // namespace
}  // namespace sigil::python

namespace pybind11::detail {
template <>
struct handle_type_name<sigil::python::AttributesObject> {
  static constexpr auto name = const_name("_sigil.compose.Attributes");
};
}  // namespace pybind11::detail

namespace sigil::python {
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;
using compose::Band;
using compose::CellSpan;
using compose::Corners;
using compose::Decoration;
using compose::Element;
using compose::Image;
using compose::Rule;
using compose::Spans;
using compose::Text;

/** A length a font field takes, read as every length is: a number of
 *  pixels, a `weave.Length`, a `compose.Dimension` or text, in any unit a
 *  font holds — px, pt, em, rem, lh, ch, or a percentage of the type
 *  size. Any other unit raises, naming @p verb. */
/** Whether @p value is the text @p word, in any case, with blank space
 *  around it ignored. */
bool isWord(py::handle value, std::string_view word) {
  if (!py::isinstance<py::str>(value)) return false;
  std::string_view text = value.cast<std::string_view>();
  while (!text.empty() && std::isspace((unsigned char)text.front()))
    text.remove_prefix(1);
  while (!text.empty() && std::isspace((unsigned char)text.back()))
    text.remove_suffix(1);
  if (text.size() != word.size()) return false;
  for (size_t index = 0; index < text.size(); ++index)
    if (std::tolower((unsigned char)text[index]) != word[index]) return false;
  return true;
}

weave::Length fontLength(py::handle value, const char* verb) {
  if (py::isinstance<weave::Length>(value)) return value.cast<weave::Length>();
  if (const std::optional<weave::Length> length =
          compose::fontLength(dimension(value)))
    return *length;
  throw py::value_error(
      std::string(verb) +
      "() takes a length a font holds: a number of pixels, or '12px', "
      "'9pt', '1.5em', '2rem', '1lh', '3ch' or '150%' (of the type size), "
      "as text or as compose.em(1.5), compose.percent(150) or weave.em(1.5). "
      "A viewport unit, a custom property, a sum and auto are not one.");
}
}  // namespace

template <class Node>
void bindFontVerbs(py::class_<Node>& element) {
  element.def("font", &Node::font, py::arg("type"), fluent)
      .def(
          "fontFamily",
          [](Node& self, std::string family,
             std::optional<std::string> style) -> Node& {
            if (style) return self.fontFamily(std::move(family), *style);
            return self.fontFamily(std::move(family));
          },
          py::arg("family"), py::kw_only(), py::arg("style") = py::none(),
          fluent,
          "CSS's font-family, as its list of names: the run is set in the "
          "first family the font context has, at the weight and style in "
          "force. style names one of the family's styles by the font's own "
          "name for it, such as 'Book' or 'Demi Bold Italic', as "
          "weave.fonts.family(name).styles lists them; the face it names "
          "keeps its own weight and style until a fontWeight or fontStyle "
          "after it, or below it, overrides that trait, the other one "
          "standing.")
      .def("fontFamily",
           static_cast<Node& (Node::*)(weave::Face)>(&Node::fontFamily),
           py::arg("family"), fluent)
      .def(
          "fontSize",
          [](Node& self, py::object value) -> Node& {
            return self.fontSize(fontLength(value, "fontSize"));
          },
          py::arg("size"), fluent)
      .def("fontWeight", &Node::fontWeight, py::arg("weight"), fluent)
      .def(
          "fontStyle",
          [](Node& self, py::object style) -> Node& {
            // A FontStyle, or an angle, which is the oblique of that
            // lean: a bare number of degrees, a typed angle or CSS angle
            // text.
            // Text no angle grammar reads is refused as a type, since a
            // style is not written as a word here: FontStyle.Italic is.
            if (py::isinstance<compose::FontStyle>(style))
              return self.fontStyle(style.cast<compose::FontStyle>());
            if (py::isinstance<py::str>(style)) {
              const std::string text = style.cast<std::string>();
              if (const std::optional<core::angle::Angle> written =
                      core::angle::read(text))
                return self.fontStyle(
                    compose::FontStyle::oblique(written->inDegrees()));
              throw py::type_error(
                  "fontStyle() takes a FontStyle or an angle, a number of "
                  "degrees or angle text such as '14deg'; '" +
                  text + "' is neither. A style is written FontStyle.Italic "
                  "or FontStyle.Normal.");
            }
            if (const std::optional<core::angle::Angle> written =
                    writtenAngle(style))
              return self.fontStyle(
                  compose::FontStyle::oblique(written->inDegrees()));
            if (!py::isinstance<py::int_>(style) &&
                !py::isinstance<py::float_>(style))
              throw py::type_error(
                  "fontStyle() takes a FontStyle or an angle, a number of "
                  "degrees or angle text such as '14deg', not " +
                  py::str(py::type::of(style)).cast<std::string>());
            return self.fontStyle(
                compose::FontStyle::oblique(style.cast<float>()));
          },
          py::arg("style"), fluent)
      .def(
          "letterSpacing",
          [](Node& self, py::object value) -> Node& {
            return self.letterSpacing(fontLength(value, "letterSpacing"));
          },
          py::arg("tracking"), fluent)
      .def(
          "textShadow",
          [](Node& self, py::object shadows) -> Node& {
            // CSS's text-shadow text, one shadow, or any iterable of them,
            // the first drawn nearest the glyph.
            if (py::isinstance<py::str>(shadows)) {
              const auto text = shadows.cast<std::string>();
              if (std::optional<std::vector<compose::TextShadow>> read =
                      compose::readTextShadow(text))
                return self.textShadow(std::move(*read));
              throw py::value_error(
                  "'" + text +
                  "' is no CSS text-shadow this reads. A text shadow is a "
                  "comma-separated list, each two or three lengths in px or "
                  "pt — the offset and a blur radius — with a CSS colour or "
                  "currentcolor before or after them; or 'none'. A length "
                  "against the font or the viewport is not read.");
            }
            if (py::isinstance<compose::TextShadow>(shadows))
              return self.textShadow(shadows.cast<compose::TextShadow>());
            std::vector<compose::TextShadow> list;
            for (py::handle shadow : shadows)
              list.push_back(shadow.cast<compose::TextShadow>());
            return self.textShadow(std::move(list));
          },
          py::arg("shadows"), fluent,
          "The shadows each glyph casts, CSS's text-shadow: its text, such "
          "as '1px 1px 2px black, 0 0 4px blue', a TextShadow or a list of "
          "them, the first drawn nearest the glyph.")
      .def(
          "ink",
          [](Node& self, py::object value, compose::PaintBox box) -> Node& {
            if (py::isinstance<compose::VarRef>(value))
              return self.ink(value.cast<compose::VarRef>());
            if (const std::optional<compose::VarRef> property =
                    customPropertyText(value))
              return self.ink(*property);
            // A material carries effects a fill does not, so it takes the
            // material's own route.
            if (py::isinstance<material::Material>(value))
              return self.ink(value.cast<material::Material>(), box);
            // The ink takes everything a fill takes. A colour — the
            // ordinary case, and the one an author writes as a tuple or
            // a name — is the inherited, easing lane; anything else is a
            // paint, and a reference that will not resolve without the
            // tree says so rather than leaving a standing ink untouched
            // and the author guessing why.
            const compose::Fill paint = fill(value);
            if (paint.references())
              throw py::type_error(
                  "An ink paint is stored as one paint and resolved without "
                  "the tree, so the ink in force and a custom property have "
                  "no paint to give it. State a colour, or clear the paint "
                  "with None.");
            return self.ink(paint, box);
          },
          py::arg("value"), py::arg("box") = compose::PaintBox::Element,
          fluent);
}

template void bindFontVerbs(py::class_<Element>&);
template void bindFontVerbs(py::class_<Text>&);
template void bindFontVerbs(py::class_<Image>&);
template void bindFontVerbs(py::class_<Band>&);
template void bindFontVerbs(py::class_<Rule>&);
template void bindFontVerbs(py::class_<compose::SpanStyle>&);

template <class Node>
void bindDeclarationVerbs(py::class_<Node>& element) {
  bindFontVerbs(element);
  element.def("row", &Node::row, fluent)
      .def("column", &Node::column, fluent)
      .def("flexGrow", &Node::flexGrow, py::arg("factor") = 1.0f, fluent)
      .def("flexShrink", &Node::flexShrink, py::arg("factor"), fluent)
      .def("absolute", &Node::absolute, fluent)
      .def(
          "size",
          [](Node& self, py::object width, py::object height) -> Node& {
            return self.width(dimension(width)).height(dimension(height));
          },
          py::arg("width"), py::arg("height"), fluent)
      .def(
          "fill",
          [](Node& self, py::object value, compose::PaintBox box) -> Node& {
            if (py::isinstance<material::Material>(value))
              return self.fill(value.cast<material::Material>(), box);
            // One conversion for every surface-colouring parameter, so a
            // material reaches the node's fill exactly as it reaches a
            // stroke's or a kit ground's, and all three spellings of
            // nothing — None, an empty Fill and an empty paint — clear it.
            const motion::Animatable<compose::Fill> surface = motionFill(value);
            // A box places a paint's unit square, so a fill with no paint
            // to place is applied whole, as the native verb does.
            if (const compose::Fill* plain = surface.constant())
              return self.fill(*plain, box);
            return self.fill(surface);
          },
          py::arg("value"), py::arg("box") = compose::PaintBox::Element, fluent)
      .def(
          "borderRadius",
          [](Node& self, py::object all) -> Node& {
            // A Corners, or one radius for all four in any length: a
            // number of pixels, '1em', '50%' of the box.
            if (py::isinstance<Corners>(all))
              return self.borderRadius(all.cast<Corners>());
            return self.borderRadius(Corners(dimension(all)));
          },
          py::arg("all"), fluent)
      .def(
          "borderRadius",
          [](Node& self, py::object tl, py::object tr, py::object br,
             py::object bl) -> Node& {
            return self.borderRadius(Corners(dimension(tl), dimension(tr),
                                             dimension(br), dimension(bl)));
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
          py::arg("x"), py::arg("y"), py::arg("z") = py::none(), fluent);

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
            return self.centerAt(geometry::path::fromSk(point(value)));
          },
          py::arg("point"), fluent)
      .def(
          "at",
          [](Node& self, py::object value) -> Node& {
            return self.at(geometry::path::fromSk(point(value)));
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
            return self.rect(geometry::path::fromSk(rect(value)));
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
      .def(
          "gridCellAlign",
          [](Node& self, py::object x, py::object y) -> Node& {
            return self.gridCellAlign(alignment(x), alignment(y));
          },
          py::arg("horizontal"), py::arg("vertical"), fluent)
      .def(
          "overflow",
          [](Node& self, py::handle value) -> Node& {
            return self.overflow(overflowing(value));
          },
          py::arg("overflow"), fluent)
      .def("paragraph",
           py::overload_cast<weave::ParagraphBlock>(&Node::paragraph),
           py::arg("partial"), fluent)
      .def(
          "lineHeight",
          [](Node& self, py::object value) -> Node& {
            // CSS's four forms: a number is a multiple of the type size, a
            // length is an absolute pitch, "normal" is the face's own; a
            // Leading says any of those and the baseline grid besides.
            if (py::isinstance<weave::Leading>(value))
              return self.lineHeight(value.cast<weave::Leading>());
            if (isWord(value, "normal"))
              return self.lineHeight(weave::Leading::face());
            if ((py::isinstance<py::int_>(value) &&
                 !py::isinstance<py::bool_>(value)) ||
                py::isinstance<py::float_>(value))
              return self.lineHeight(value.cast<float>());
            const compose::Dimension length = dimension(value);
            if (length.unit == compose::Dimension::Unit::Auto)
              throw py::value_error(
                  "lineHeight() takes a number (a multiple of the type "
                  "size), a length "
                  "('24px', '1.5em', '150%'), 'normal' or a Leading; auto is "
                  "no pitch.");
            return self.lineHeight(length);
          },
          py::arg("leading"), fluent)
      .def("textAlign", &Node::textAlign, py::arg("alignment"), fluent)
      .def(
          "textIndent",
          [](Node& self, py::object indent) -> Node& {
            return self.textIndent(dimension(indent));
          },
          py::arg("indent"), fluent)
      .def("writingMode", &Node::writingMode, py::arg("mode"), fluent)
      .def("hyphens", &Node::hyphens, py::arg("hyphenation"), fluent)
      .def("textWrap", &Node::textWrap, py::arg("wrap"), fluent)
      .def("textJustify", &Node::textJustify, py::arg("method"), fluent)
      .def(
          "var",
          [](Node& self, const std::string& name, py::object value) -> Node& {
            return std::visit(
                [&](const auto& converted) -> Node& {
                  if constexpr (std::is_same_v<
                                    std::decay_t<decltype(converted)>,
                                    compose::Fill>)
                    return self.var(name, *converted.material());
                  else
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
                  "Default properties require string names and color, "
                  "material or dimension values");
            }
            return self.varDefaults(std::move(table));
          },
          py::arg("defaults"), fluent)
      .def(
          "imageRendering",
          [](Node& self, const SkSamplingOptions& sampling) -> Node& {
            return self.imageRendering(sampling.filter == SkFilterMode::kNearest
                                           ? material::Sampling::Nearest
                                           : material::Sampling::Linear);
          },
          py::arg("sampling"), fluent)
      .def("inherit", &Node::inherit, py::arg("property"), fluent)
      .def("initial", &Node::initial, py::arg("property"), fluent)
      .def("unset", &Node::unset, py::arg("property"), fluent)
      .def("blendMode", &Node::blendMode, py::arg("mode"), fluent)
      .def("zIndex", &Node::zIndex, py::arg("index"), fluent);
  for (const auto& [name, setter] : std::initializer_list<
           std::pair<const char*, Node& (Node::*)(motion::Animatable<float>)>>{
           {"opacity", &Node::opacity},
           {"scale", &Node::scale},
           {"scaleX", &Node::scaleX},
           {"scaleY", &Node::scaleY}})
    element.def(
        name,
        [setter, name](Node& self, py::object value) -> Node& {
          // CSS reads a percentage here as the fraction it is: "50%" is a
          // half.
          if (py::isinstance<py::str>(value) ||
              py::isinstance<compose::Dimension>(value))
            return (self.*setter)(fraction(value, name));
          return (self.*setter)(motionAnimatable(value));
        },
        py::arg("value"), fluent);
  // A translate is a lane that animates, or a length written with its
  // unit: '50%' of the node's own box, '1em', compose.vw(10).
  using Lane = Node& (Node::*)(motion::Animatable<float>);
  using Length = Node& (Node::*)(compose::Dimension);
  for (const auto& [name, lane, length] :
       std::initializer_list<std::tuple<const char*, Lane, Length>>{
           {"translateX", &Node::translateX, &Node::translateX},
           {"translateY", &Node::translateY, &Node::translateY}})
    element.def(
        name,
        [lane, length](Node& self, py::object value) -> Node& {
          if (py::isinstance<py::str>(value) ||
              py::isinstance<compose::Dimension>(value) ||
              py::isinstance<weave::Length>(value) ||
              py::isinstance<compose::VarRef>(value))
            return (self.*length)(dimension(value));
          return (self.*lane)(motionAnimatable(value));
        },
        py::arg("value"), fluent);
  // An angle verb reads a number as degrees, and a typed angle or CSS
  // angle text in the measure it names.
  for (const auto& [name, setter] : std::initializer_list<
           std::pair<const char*, Node& (Node::*)(motion::Animatable<float>)>>{
           {"rotate", &Node::rotate},
           {"skewX", &Node::skewX},
           {"skewY", &Node::skewY}})
    element.def(
        name,
        [setter](Node& self, py::object value) -> Node& {
          return (self.*setter)(motionAngle(value));
        },
        py::arg("value"), fluent);
}

template <class Node>
void bindNodeVerbs(py::class_<Node>& element) {
  bindDeclarationVerbs(element);
  element.def("copy", [](const Node& value) { return value; })
      .def("__copy__", [](const Node& value) { return value; })
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
      .def("gridArea", &Node::gridArea, py::arg("name"), fluent)
      .def(
          "shape",
          [](Node& self, py::object value) -> Node& {
            return self.shape(shape(value));
          },
          py::arg("value"), fluent)
      .def("styleClass", &Node::styleClass, py::arg("name"), fluent)
      .def(
          "attribute",
          [](Node& self, const std::string& name, py::handle value) -> Node& {
            compose::Attributes one;
            stateFact(one, name, value);
            return self.attributes(std::move(one));
          },
          py::arg("name"), py::arg("value"), fluent,
          "A typed fact this node states, for whatever reads it — the "
          "scheme or operator on the parent placing this node by its tier, "
          "an operator building something from it — and inert where nothing "
          "does. A later fact under the same name replaces the earlier one.")
      .def(
          "attributes",
          [](Node& self, const AttributesObject& facts) -> Node& {
            return self.attributes(facts.template cast<compose::Attributes>());
          },
          py::arg("facts"), fluent,
          "Every fact of this table laid over the node's own, same names "
          "replaced.")
      .def(
          "operators",
          [](Node& self, py::handle list) -> Node& {
            return self.operators(operatorList(list));
          },
          py::arg("operators"), fluent,
          "The operators this node runs over its children, in list order: "
          "each is handed the children measured, with their facts and where "
          "the operators before it left them. A later call appends.")
      .def(
          "role",
          [](Node& self, std::string name, std::optional<weave::Type> font,
             std::optional<weave::ParagraphBlock> paragraph) -> Node& {
            return self.role(std::move(name), font.value_or(weave::Type{}),
                             paragraph.value_or(weave::ParagraphBlock{}));
          },
          py::arg("name"), py::arg("font") = py::none(),
          py::arg("paragraph") = py::none(), fluent,
          "A semantic role — what a bare word in a selector names — with "
          "the font and paragraph partials a component falls back to for "
          "it, under every rule that matches the node; the node's own "
          "`font` and `paragraph` stand over everything. A later call "
          "replaces the role and its defaults together.")
      .def("hitTestable", &Node::hitTestable, py::arg("enabled"), fluent)
      .def("exit", &Node::exit, py::arg("enabled") = true, fluent,
           "This node exits as it entered: when a later description leaves "
           "this keyed node out, it keeps its place, answers no hit, and "
           "plays every entrance in its subtree backwards — each "
           "animate(from_=a, to=b) back to a over the same duration on its "
           "curve run backwards, the last in the first out — and goes on "
           "the frame the last of them lands. Named again before then, it "
           "turns round where it stands.")
      .def("decorationOutline", &Node::decorationOutline, py::arg("source"),
           py::arg("coverage") = 0.5f, fluent)
      .def("filter", &Node::filter, py::arg("effect"), fluent)
      .def("backdropFilter", &Node::backdropFilter, py::arg("effect"), fluent)
      .def("travel", &Node::travel, py::arg("path"), fluent)
      .def("preserve3d", &Node::preserve3d, py::arg("preserve") = true, fluent)
      .def("backface", &Node::backface, py::arg("visibility"), fluent)
      .def("cacheScale", &Node::cacheScale, py::arg("scale"), fluent)
      .def(
          "transition",
          [](Node& self, py::handle how) -> Node& {
            return self.transition(motionTransition(how));
          },
          py::arg("transition"), fluent,
          "How the plain values on this node ease when they change: a "
          "Tween, whose from_ and to are not read, or a number of seconds "
          "for one of that duration.")
      .def(
          "perspectiveOrigin",
          [](Node& self, py::object x, py::object y) -> Node& {
            return self.perspectiveOrigin(originLength(x), originLength(y));
          },
          py::arg("x"), py::arg("y"), fluent);
  for (const auto& [name, setter] : std::initializer_list<
           std::pair<const char*, Node& (Node::*)(motion::Animatable<float>)>>{
           {"translateZ", &Node::translateZ},
           {"scaleZ", &Node::scaleZ},
           {"perspective", &Node::perspective}})
    element.def(
        name,
        [setter](Node& self, py::object value) -> Node& {
          return (self.*setter)(motionAnimatable(value));
        },
        py::arg("value"), fluent);
  for (const auto& [name, setter] : std::initializer_list<
           std::pair<const char*, Node& (Node::*)(motion::Animatable<float>)>>{
           {"rotateX", &Node::rotateX},
           {"rotateY", &Node::rotateY}})
    element.def(
        name,
        [setter](Node& self, py::object value) -> Node& {
          return (self.*setter)(motionAngle(value));
        },
        py::arg("value"), fluent);
  element.def(
      "stroke",
      [](Node& self, const material::Material& material,
         const std::variant<float, std::string, compose::Dimension>& width,
         material::StrokePosition position) -> Node& {
        return self.stroke(material, {pixels(py::cast(width), "A stroke's width"),
                                      position});
      },
      py::arg("material"), py::arg("width") = 1.0f,
      py::arg("position") = material::StrokePosition::Outside, fluent);
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
  element.def(
      "boxShadow",
      [](Node& self, py::object shadows, py::object offset, py::object blur,
         py::object spread, py::object color, bool inset) -> Node& {
        if (!shadows.is_none()) {
          if (!offset.is_none() || !blur.is_none() || !spread.is_none() ||
              !color.is_none() || inset)
            throw py::type_error(
                "boxShadow() takes a Shadow or a list of them, or one "
                "shadow's fields as keywords, not both.");
          if (py::isinstance<py::str>(shadows)) {
            const auto text = shadows.cast<std::string>();
            if (std::optional<std::vector<compose::Shadow>> read =
                    compose::readShadow(text))
              return self.boxShadow(std::move(*read));
            throw py::value_error(
                "'" + text +
                "' is no CSS box-shadow this reads. A box shadow is a "
                "comma-separated list, each two to four lengths in px or pt "
                "— the offset, a blur radius and a spread — with a CSS "
                "colour or currentcolor and 'inset' before or after them; or "
                "'none'. A length against the font or the viewport is not "
                "read.");
          }
          if (py::isinstance<compose::Shadow>(shadows))
            return self.boxShadow(shadows.cast<compose::Shadow>());
          return self.boxShadow(shadows.cast<std::vector<compose::Shadow>>());
        }
        compose::Shadow shadow;
        if (!offset.is_none())
          shadow.offset = pixelOffset(offset, "A box shadow's offset");
        if (!blur.is_none()) shadow.blur = pixels(blur, "A box shadow's blur");
        if (!spread.is_none())
          shadow.spread = pixels(spread, "A box shadow's spread");
        if (!color.is_none()) shadow.color = fill(color);
        shadow.inset = inset;
        return self.boxShadow(std::move(shadow));
      },
      py::arg("shadow") = py::none(), py::kw_only(),
      py::arg("offset") = py::none(), py::arg("blur") = py::none(),
      py::arg("spread") = py::none(), py::arg("color") = py::none(),
      py::arg("inset") = false, fluent,
      "CSS's box-shadow: a shadow of the node's outline moved by offset, "
      "grown by spread and softened by a blur radius of blur pixels, in "
      "color (the ink in force when unstated) — beneath the fill, or with "
      "inset a band hugging the inner edge over it. Takes the fields as "
      "keywords, CSS's box-shadow text such as '0 4px 12px 2px "
      "rgba(0 0 0 / .3) inset', one Shadow, or a list of them with the "
      "first on top.");
}

template void bindDeclarationVerbs(py::class_<Element>&);
template void bindDeclarationVerbs(py::class_<Text>&);
template void bindDeclarationVerbs(py::class_<Image>&);
template void bindDeclarationVerbs(py::class_<Band>&);
template void bindDeclarationVerbs(py::class_<Rule>&);
template void bindNodeVerbs(py::class_<Element>&);
template void bindNodeVerbs(py::class_<Text>&);
template void bindNodeVerbs(py::class_<Image>&);
template void bindNodeVerbs(py::class_<Band>&);

template <class Declaring>
void bindTextPropertyVerbs(py::class_<Declaring>& element) {
  element
      .def("paragraphStyles",
           py::overload_cast<std::vector<weave::ParagraphStyle>>(
               &Declaring::paragraphStyles),
           py::arg("blocks"), fluent)
      .def(
          "paragraphStyles",
          [](Declaring& self,
             const std::vector<std::string>& names) -> Declaring& {
            std::vector<std::string_view> views(names.begin(), names.end());
            return self.paragraphStyles(views);
          },
          py::arg("names"), fluent)
      .def("initialLetter", &Declaring::initialLetter, py::arg("initial"),
           fluent)
      .def("textFirstBaseline", &Declaring::textFirstBaseline, py::arg("rule"),
           py::arg("offset") = 0.0f, fluent)
      .def("textVerticalAlign", &Declaring::textVerticalAlign, py::arg("rule"),
           py::arg("maximumInterlineSpacing") = 0.0f, fluent)
      .def("textLineMargin", &Declaring::textLineMargin, py::arg("band"),
           fluent)
      .def("textWillChange", &Declaring::textWillChange,
           py::arg("enabled") = true, py::arg("candidates") = 0, fluent)
      .def(
          "textOverflow",
          [](Declaring& self, const std::string& marker) -> Declaring& {
            return self.textOverflow(marker);
          },
          py::arg("marker"), fluent)
      .def("maxTextLines", &Declaring::maxTextLines, py::arg("lines"), fluent)
      .def(
          "textStroke",
          [](Declaring& self, float width, py::object value) -> Declaring& {
            // The outline is one comparable Fill on the node, so the
            // flat-mark reading is the widest set it can honour: a
            // static paint collapses onto it and a live or
            // geometry-dependent one raises, naming the verb that does
            // resolve against the frame.
            return self.textStroke(width, fill(value));
          },
          py::arg("width"), py::arg("paint"), fluent);
}

template void bindTextPropertyVerbs(py::class_<Text>&);
template void bindTextPropertyVerbs(py::class_<Rule>&);

void bindTextVerbs(py::class_<Text>& element) {
  bindTextPropertyVerbs(element);
  element.def("textThreadTo", &Text::textThreadTo, py::arg("key"), fluent)
      .def("textThreadBalance", &Text::textThreadBalance,
           py::arg("throughLine") = ~0u, fluent)
      .def("contentFlowAround", &Text::contentFlowAround, py::arg("key"),
           py::arg("margin") = 0.0f, fluent)
      .def("textOnPath", &Text::textOnPath, py::arg("path"), fluent)
      .def("span", &Text::span, py::arg("where"), py::arg("style"), fluent)
      .def("textAnnotation", &Text::textAnnotation, py::arg("reading"), fluent)
      .def(
          "textAttach",
          [](Text& self, weave::Selector where, py::handle what) -> Text& {
            return self.textAttach(std::move(where), node(what));
          },
          py::arg("where"), py::arg("what"), fluent,
          "Anchors `what` to the unit `where` resolves to: a child whose "
          "parent box is that unit's rect, reserving nothing in the text.")
      .def("atRest", &Text::atRest);
}

void bindImageVerbs(py::class_<Image>& element) {
  element.def(
      "imageRegion",
      [](Image& self, py::object value) -> Image& {
        return self.imageRegion(geometry::path::fromSk(rect(value)));
      },
      py::arg("rect"), fluent);
}

void bindBandVerbs(py::class_<Band>& element) {
  element.def("bandAlignment", &Band::bandAlignment, py::arg("formation"),
              fluent);
}

}  // namespace sigil::python
