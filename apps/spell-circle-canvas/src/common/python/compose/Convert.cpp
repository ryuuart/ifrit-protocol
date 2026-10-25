/** @file
 * The readings the compose bindings share: a dimension, a fill, a paint,
 * an alignment, a justification, a shape, a decoration, an origin
 * length, a custom property's value and a list of children, each taken
 * from the forms Python spells it in.
 */

#include <pybind11/stl.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Factories.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/kit/Radial.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>

#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

compose::Dimension dimension(py::handle value) {
  if (py::isinstance<compose::Dimension>(value))
    return value.cast<compose::Dimension>();
  if (py::isinstance<weave::Length>(value)) return value.cast<weave::Length>();
  if (py::isinstance<compose::VarRef>(value))
    return value.cast<compose::VarRef>();
  if (!py::isinstance<py::str>(value))
    return compose::Dimension{value.cast<float>()};
  // The one length grammar the library reads, so a string written here
  // and a length written in a rule's text mean the same thing.
  const auto text = value.cast<std::string>();
  if (const std::optional<compose::Dimension> length =
          compose::parseDimension(text))
    return *length;
  throw py::value_error(
      "A dimension is a native Dimension or Length, a number, 'auto', or a "
      "length written as text: '12px', '1.5em', '2rem', '0.5lh', '3ch', "
      "'9pt', '50%', '10vw', '10vh', '10vmin', '10vmax' or var(name).");
}

float pixels(py::handle value, const char* slot) {
  if (!py::isinstance<py::str>(value) &&
      !py::isinstance<compose::Dimension>(value) &&
      !py::isinstance<weave::Length>(value))
    return value.cast<float>();
  const compose::Dimension length = dimension(value);
  if (length.unit == compose::Dimension::Unit::Px) return length.value;
  if (length.unit == compose::Dimension::Unit::Pt)
    return weave::pt(length.value).absolutePx();
  throw py::value_error(
      std::string(slot) +
      " is a length in pixels: a number, or text in an absolute unit "
      "('4px', '3pt'). A length against the font, the canvas or the box has "
      "nothing to be measured against here.");
}

float fraction(py::handle value, const char* slot) {
  if (py::isinstance<compose::Dimension>(value)) {
    const auto length = value.cast<compose::Dimension>();
    if (length.unit == compose::Dimension::Unit::Percent)
      return length.value / 100.0f;
  } else if (py::isinstance<py::str>(value)) {
    if (const std::optional<float> amount =
            compose::parseFraction(value.cast<std::string_view>()))
      return *amount;
  } else {
    return value.cast<float>();
  }
  throw py::value_error(std::string(slot) +
                        " is a fraction: a number, a percentage written as "
                        "text ('50%') or compose.percent(50).");
}

glm::vec2 pixelOffset(py::handle value, const char* slot) {
  if ((py::isinstance<py::tuple>(value) || py::isinstance<py::list>(value)) &&
      py::len(value) == 2) {
    const auto pair = py::reinterpret_borrow<py::sequence>(value);
    return {pixels(pair[0], slot), pixels(pair[1], slot)};
  }
  return geometry::path::fromSk(point(value));
}

std::optional<compose::VarRef> customPropertyText(py::handle value) {
  if (!py::isinstance<py::str>(value)) return std::nullopt;
  std::string_view text = value.cast<std::string_view>();
  while (!text.empty() && std::isspace((unsigned char)text.front()))
    text.remove_prefix(1);
  while (!text.empty() && std::isspace((unsigned char)text.back()))
    text.remove_suffix(1);
  if (text.size() < 5 || text.back() != ')') return std::nullopt;
  for (size_t index = 0; index < 4; ++index)
    if (std::tolower((unsigned char)text[index]) != "var("[index])
      return std::nullopt;
  std::string_view name = text.substr(4, text.size() - 5);
  while (!name.empty() && std::isspace((unsigned char)name.front()))
    name.remove_prefix(1);
  while (!name.empty() && std::isspace((unsigned char)name.back()))
    name.remove_suffix(1);
  // A leading `--` is CSS's mark for a custom property and names the same
  // property as the bare name does.
  if (name.starts_with("--")) name.remove_prefix(2);
  if (name.empty()) return std::nullopt;
  return compose::var(name);
}

namespace {

/** Whether @p value is the text `currentcolor`, in any case. */
bool currentColorText(py::handle value) {
  if (!py::isinstance<py::str>(value)) return false;
  std::string_view text = value.cast<std::string_view>();
  while (!text.empty() && std::isspace((unsigned char)text.front()))
    text.remove_prefix(1);
  while (!text.empty() && std::isspace((unsigned char)text.back()))
    text.remove_suffix(1);
  constexpr std::string_view kWord = "currentcolor";
  if (text.size() != kWord.size()) return false;
  for (size_t index = 0; index < text.size(); ++index)
    if (std::tolower((unsigned char)text[index]) != kWord[index]) return false;
  return true;
}

}  // namespace

compose::Fill fill(py::handle value) {
  if (value.is_none()) return compose::Fill::none();
  // CSS's two ways of naming a colour the tree resolves, as text.
  if (currentColorText(value)) return compose::Fill::currentInk();
  if (const std::optional<compose::VarRef> property = customPropertyText(value))
    return compose::Fill::var(*property);
  if (py::isinstance<compose::Fill>(value)) return value.cast<compose::Fill>();
  if (py::isinstance<compose::VarRef>(value))
    return compose::Fill::var(value.cast<compose::VarRef>());
  if (py::isinstance<material::Paint>(value))
    return compose::Fill(material::skia::base(value.cast<material::Paint>()));
  if (py::isinstance<material::Material>(value))
    return value.cast<material::Material>();
  return compose::Fill::color(color(value));
}

compose::ColorTerm colorTerm(py::handle value) {
  if (py::isinstance<compose::VarRef>(value))
    return compose::ColorTerm::var(value.cast<compose::VarRef>());
  const compose::Fill paint = fill(value);
  if (paint.kind != compose::Fill::Kind::Color)
    throw py::type_error(
        "A colour term is a colour, the ink in force (Fill.currentInk()) or "
        "a custom property (Fill.var(name)), never a paint.");
  compose::ColorTerm term(paint.colorValue);
  term.ref = paint.ref;
  term.varId = paint.varId;
  return term;
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
  if (name == "space-between") return compose::Justify::SpaceBetween;
  if (name == "space-around") return compose::Justify::SpaceAround;
  if (name == "space-evenly") return compose::Justify::SpaceEvenly;
  throw py::value_error("Unknown justification: " + name +
                        "; it is CSS's \"start\", \"center\", \"end\", "
                        "\"space-between\", \"space-around\" or "
                        "\"space-evenly\".");
}

compose::Overflow overflowing(py::handle value) {
  if (py::isinstance<compose::Overflow>(value))
    return value.cast<compose::Overflow>();
  if (!py::isinstance<py::str>(value))
    throw py::type_error(
        "An overflow is an Overflow, \"visible\", \"clip\" or \"hidden\".");
  const auto name = value.cast<std::string>();
  if (name == "visible") return compose::Overflow::Visible;
  if (name == "clip") return compose::Overflow::Clip;
  if (name == "hidden") return compose::Overflow::Hidden;
  throw py::value_error("Unknown overflow: " + name +
                        "; it is \"visible\", \"clip\" or \"hidden\".");
}

compose::Shape shape(py::handle value) {
  if (py::isinstance<compose::Shape>(value))
    return value.cast<compose::Shape>();
  // Geometry's shape values compare, so a node shaped by one prunes.
  if (py::isinstance<geometry::shapes::Radial>(value))
    return value.cast<geometry::shapes::Radial>();
  if (py::isinstance<geometry::shapes::Ellipse>(value))
    return value.cast<geometry::shapes::Ellipse>();
  if (py::isinstance<geometry::shapes::Fitted>(value))
    return value.cast<geometry::shapes::Fitted>();
  if (py::isinstance<geometry::shapes::Blob>(value))
    return value.cast<geometry::shapes::Blob>();
  if (py::isinstance<geometry::shapes::Parallelogram>(value))
    return value.cast<geometry::shapes::Parallelogram>();
  if (py::isinstance<geometry::shapes::Arrow>(value))
    return value.cast<geometry::shapes::Arrow>();
  if (py::isinstance<geometry::shapes::Chevron>(value))
    return value.cast<geometry::shapes::Chevron>();
  if (py::isinstance<geometry::path::Outline>(value))
    return compose::heldPath(value.cast<geometry::path::Outline>());
  if (PyCallable_Check(value.ptr())) {
    auto callback = retainCallback(py::reinterpret_borrow<py::function>(value));
    return compose::Shape{[callback](glm::vec2 size) {
      const py::gil_scoped_acquire lock;
      const CallbackBoundary boundary;
      try {
        return geometry::path::fromSk(
            callback->get()(size.x, size.y).cast<SkPath>());
      } catch (const py::error_already_set& error) {
        throw std::runtime_error(error.what());
      }
    }};
  }
  return compose::heldPath(geometry::path::fromSk(value.cast<SkPath>()));
}

compose::Decoration decoration(py::handle value) {
  if (py::isinstance<compose::Decoration>(value))
    return value.cast<compose::Decoration>();
  if (py::isinstance<compose::PathFormat>(value))
    return value.cast<compose::PathFormat>();
  if (py::isinstance<compose::Shadow>(value))
    return value.cast<compose::Shadow>();
  if (py::isinstance<compose::brush::Ribbon>(value))
    return value.cast<compose::brush::Ribbon>();
  throw py::type_error(
      "A decoration is a Decoration, PathFormat, Shadow or brush.Ribbon.");
}

compose::Dimension originLength(py::handle value) {
  if (py::isinstance<py::float_>(value) || py::isinstance<py::int_>(value))
    throw py::type_error(
        "An origin is written with its unit: percent(50) or '50%' of the node's "
        "box, Dimension(12) for pixels, a Length or a custom property. A "
        "bare number is refused.");
  return dimension(value);
}

compose::VarValue variable(py::handle value) {
  // A paint, lowered once as the native verb lowers it; a flat one is its
  // colour.
  if (py::isinstance<material::Material>(value) ||
      py::isinstance<material::Paint>(value)) {
    compose::Fill paint = fill(value);
    if (paint.kind == compose::Fill::Kind::Color) return paint.colorValue;
    return paint;
  }
  // Text is a length where the length grammar reads it and a colour
  // where the colour grammar does; the two grammars share no text.
  if (py::isinstance<py::str>(value)) {
    const auto text = value.cast<std::string>();
    if (const std::optional<compose::Dimension> length =
            compose::parseDimension(text))
      return *length;
    if (const std::optional<material::Color> read = material::readColor(text))
      return *read;
    throw py::value_error(
        "'" + text +
        "' is neither a length nor a colour. A custom property holds a "
        "length written as text ('12px', '1.5em', '50%', 'auto'), a colour "
        "written as CSS ('#18252e', 'steelblue', 'oklch(0.7 0.1 240)'), a "
        "number, a Color, a Dimension, a Material or a Paint.");
  }
  if (py::isinstance<material::Color>(value) ||
      py::isinstance<py::tuple>(value) || py::isinstance<py::list>(value))
    return color(value);
  return dimension(value);
}

bool isNode(py::handle value) {
  return py::isinstance<compose::Element>(value) ||
         py::isinstance<compose::Text>(value) ||
         py::isinstance<compose::Image>(value) ||
         py::isinstance<compose::Band>(value);
}

compose::Element node(py::handle value) {
  if (py::isinstance<compose::Text>(value)) return value.cast<compose::Text>();
  if (py::isinstance<compose::Image>(value))
    return value.cast<compose::Image>();
  if (py::isinstance<compose::Band>(value)) return value.cast<compose::Band>();
  return value.cast<compose::Element>();
}

std::vector<compose::Element> elements(py::args values) {
  try {
    if (values.size() == 1 && (py::isinstance<py::str>(values[0]) ||
                               py::isinstance<py::bytes>(values[0])))
      throw py::cast_error();
    // A typed leaf is one child, exactly as an element is: only a
    // genuine sequence is flattened.
    const auto children = values.size() == 1 && !isNode(values[0])
                              ? py::tuple(values[0])
                              : py::tuple(values);
    return children.cast<std::vector<compose::Element>>();
  } catch (const py::cast_error&) {
    throw py::type_error(
        "children expects Elements or one iterable of Elements");
  }
}
}  // namespace sigil::python
