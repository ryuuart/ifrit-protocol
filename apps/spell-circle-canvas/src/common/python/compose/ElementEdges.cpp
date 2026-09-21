#include <pybind11/pybind11.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Registration.h>

#include <string_view>

namespace sigil::python {
namespace py = pybind11;

namespace {

constexpr auto fluent = py::return_value_policy::reference_internal;
using compose::Band;
using compose::Element;
using compose::Image;
using compose::Text;

/** The box model's shorthands, on whichever kind of node states them. */
template <class Class>
void bindEdges(Class& element) {
  using Node = typename Class::type;
  using DimensionVerb = Node& (Node::*)(compose::Dimension);
  // The per-side verbs write ONE side and leave the other three as they
  // stand, which is what CSS spells padding-left and margin-top.
  const auto side = [&element](const char* name, DimensionVerb setter) {
    element.def(
        name,
        [setter](Node& self, py::object value) -> Node& {
          return (self.*setter)(dimension(value));
        },
        py::arg("value"), fluent);
  };
  side("paddingTop", &Node::paddingTop);
  side("paddingRight", &Node::paddingRight);
  side("paddingBottom", &Node::paddingBottom);
  side("paddingLeft", &Node::paddingLeft);
  side("marginTop", &Node::marginTop);
  side("marginRight", &Node::marginRight);
  side("marginBottom", &Node::marginBottom);
  side("marginLeft", &Node::marginLeft);

  // One, two, three and four lengths are four arities, so each is its own
  // overload with its own named inputs, and each runs in CSS's order: a
  // keyword call reads the same as `inset` below, and pybind names every
  // form when none matches.
  for (const char* name : {"padding", "margin"}) {
    const bool padding = std::string_view{name} == "padding";
    const auto write = [padding](Node& self,
                                 const compose::Edges& edges) -> Node& {
      return padding ? self.padding(edges) : self.margin(edges);
    };
    element.def(
        name,
        [padding](Node& self, py::object all) -> Node& {
          return padding ? self.padding(dimension(all))
                         : self.margin(dimension(all));
        },
        py::arg("all"), fluent);
    element.def(
        name,
        [write](Node& self, py::object vertical,
                py::object horizontal) -> Node& {
          const compose::Dimension down = dimension(vertical);
          const compose::Dimension across = dimension(horizontal);
          return write(self, {down, across, down, across});
        },
        py::arg("vertical"), py::arg("horizontal"), fluent);
    element.def(
        name,
        [write](Node& self, py::object top, py::object horizontal,
                py::object bottom) -> Node& {
          const compose::Dimension across = dimension(horizontal);
          return write(self,
                       {dimension(top), across, dimension(bottom), across});
        },
        py::arg("top"), py::arg("horizontal"), py::arg("bottom"), fluent);
    element.def(
        name,
        [write](Node& self, py::object top, py::object right,
                py::object bottom, py::object left) -> Node& {
          return write(self, {dimension(top), dimension(right),
                              dimension(bottom), dimension(left)});
        },
        py::arg("top"), py::arg("right"), py::arg("bottom"), py::arg("left"),
        fluent);
    // The named-sides form: any subset of the four, each side saying which
    // it is, and a side left out staying zero. A full four by keyword
    // matches the overload above it, which writes the same four sides.
    element.def(
        name,
        [padding](Node& self, py::object top, py::object right,
                  py::object bottom, py::object left) -> Node& {
          const auto named = [](py::object value) {
            return value.is_none() ? compose::Dimension(0.0f)
                                   : dimension(value);
          };
          const compose::Edges edges{named(top), named(right), named(bottom),
                                     named(left)};
          return padding ? self.padding(edges) : self.margin(edges);
        },
        py::kw_only(), py::arg("top") = py::none(),
        py::arg("right") = py::none(), py::arg("bottom") = py::none(),
        py::arg("left") = py::none(), fluent);
  }

  element.def(
      "inset",
      [](Node& self, py::object all) -> Node& {
        return self.inset(dimension(all));
      },
      py::arg("all"), fluent);
  element.def(
      "inset",
      [](Node& self, py::object vertical,
         py::object horizontal) -> Node& {
        return self.inset(dimension(vertical), dimension(horizontal));
      },
      py::arg("vertical"), py::arg("horizontal"), fluent);
  element.def(
      "inset",
      [](Node& self, py::object top, py::object horizontal,
         py::object bottom) -> Node& {
        return self.inset(dimension(top), dimension(horizontal),
                          dimension(bottom));
      },
      py::arg("top"), py::arg("horizontal"), py::arg("bottom"), fluent);
  element.def(
      "inset",
      [](Node& self, py::object top, py::object right, py::object bottom,
         py::object left) -> Node& {
        return self.inset(dimension(top), dimension(right), dimension(bottom),
                          dimension(left));
      },
      py::arg("top"), py::arg("right"), py::arg("bottom"), py::arg("left"),
      fluent);
  // The named-sides form: any subset of the four, and a side left out
  // stays unpinned, so the node's own size or the opposite inset sizes it.
  element.def(
      "inset",
      [](Node& self, py::object top, py::object right, py::object bottom,
         py::object left) -> Node& {
        const auto named = [](py::object value) {
          return value.is_none() ? compose::autoDimension() : dimension(value);
        };
        return self.inset(compose::Edges{named(top), named(right),
                                         named(bottom), named(left)});
      },
      py::kw_only(), py::arg("top") = py::none(), py::arg("right") = py::none(),
      py::arg("bottom") = py::none(), py::arg("left") = py::none(), fluent);
}

}  // namespace

void bindComposeElementEdges(py::module_& module) {
  auto element = extend<Element>(module, "compose.Element");
  auto textLeaf = extend<Text>(module, "compose.Text");
  auto imageLeaf = extend<Image>(module, "compose.Image");
  auto bandLeaf = extend<Band>(module, "compose.Band");
  bindEdges(element);
  bindEdges(textLeaf);
  bindEdges(imageLeaf);
  bindEdges(bandLeaf);
}

}  // namespace sigil::python
