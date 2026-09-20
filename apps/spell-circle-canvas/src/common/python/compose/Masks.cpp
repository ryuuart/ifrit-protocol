#include <include/core/SkPath.h>
#include <include/core/SkRect.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Mask.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

namespace {

constexpr auto fluent = py::return_value_policy::reference_internal;
using compose::Element;
using compose::Gate;
using compose::Parts;
using compose::Region;

/** The region a shape gate keeps or cuts away: the node's own outline, a
 *  rectangle, an oval or a path, all in the node's local space. */
void bindRegion(py::module_& composition) {
  py::class_<Region> region(
      composition, "Region",
      "A comparable region in a node's own local space, which is what a "
      "shape gate keeps. It is a value rather than a callable so a node "
      "masked by one still compares, and so still prunes.");

  py::enum_<Region::Kind>(region, "Kind", "Which shape a region is.")
      .value("Own", Region::Kind::Own, "The node's own shape.")
      .value("Rect", Region::Kind::Rect, "A rectangle in local coordinates.")
      .value("Oval", Region::Kind::Oval,
             "The oval inscribed in a local rectangle.")
      .value("Path", Region::Kind::Path, "An explicit local path.");

  region.def(py::init<>(), "The node's own shape, as `own` answers it.")
      .def_static("own", &Region::own,
                  "The node's own silhouette, which is the region `clip` "
                  "uses.")
      .def_static(
          "rect", [](py::handle bounds) { return Region::rect(rect(bounds)); },
          py::arg("bounds"), "A rectangle in the node's local coordinates.")
      .def_static(
          "oval", [](py::handle bounds) { return Region::oval(rect(bounds)); },
          py::arg("bounds"),
          "The oval inscribed in a rectangle of the node's local "
          "coordinates.")
      .def_static("path", &Region::path, py::arg("path"),
                  "An explicit path in the node's local space. A path "
                  "compares by its structure, so this region prunes as the "
                  "others do.")
      .def("kind", &Region::kind, "Which shape this region is.")
      .def("resolve", &Region::resolve, py::arg("ownShape"),
           "The path this region covers, given the node's own silhouette.")
      .def("copy", [](const Region& self) { return self; })
      .def(py::self == py::self);
  copyProtocol(region);
}

/** The selection of a node's paint outputs, its bits, and the factories
 *  that name each class of output. */
void bindParts(py::module_& composition, py::module_& selections) {
  py::class_<Parts> parts(
      composition, "Parts",
      "Which of a node's paint outputs a mask applies to: the surface, the "
      "marks, the content leaf, the children, or one mark by its local "
      "name. Selections combine with `|`.");

  // Arithmetic, so two bits joined with `|` give the whole number the
  // `bits` field holds.
  py::enum_<Parts::Bits>(parts, "Bits", py::arithmetic(),
                         "The four things a node paints, as one bit each.")
      .value("kSurface", Parts::kSurface,
             "The fill, and whatever fills the shape.")
      .value("kMarks", Parts::kMarks,
             "The decorations: strokes, glows, shadows.")
      .value("kContent", Parts::kContent,
             "The leaf's own text, image or program.")
      .value("kChildren", Parts::kChildren, "Everything under the node.")
      .value("kAll", Parts::kAll, "All four.");

  parts
      .def(py::init([](std::uint8_t bits, std::vector<std::string> names) {
             return Parts{bits, std::move(names)};
           }),
           py::arg("bits") = std::uint8_t{0},
           py::arg("names") = std::vector<std::string>{},
           "A selection holding `bits` and the local mark labels `names`.")
      .def_readwrite("bits", &Parts::bits,
                     "Which of `Bits` this selection holds.")
      .def_readwrite("names", &Parts::names,
                     "The local mark labels this selection reaches, in "
                     "declaration order. Read as a copy.")
      .def("selects", &Parts::selects, py::arg("what"),
           "Whether this selection holds the bit `what`.")
      .def("selectsMark", &Parts::selectsMark, py::arg("label"),
           "Whether this selection reaches a mark carrying `label`, which "
           "may be empty.")
      .def("isEverything", &Parts::isEverything,
           "Whether this selection is everything the node paints.")
      .def(
          "__or__",
          [](const Parts& left, const Parts& right) { return left | right; },
          py::arg("other"), py::is_operator(),
          "The union of two selections: their bits joined, and the second's "
          "names after the first's.")
      .def("copy", [](const Parts& self) { return self; })
      .def(py::self == py::self);
  copyProtocol(parts);

  selections.doc() =
      "The selection factories: which of a node's paint outputs a mask "
      "applies to.";
  selections.def("all", &compose::parts::all,
                 "Every output, children included, which is what a mask "
                 "given a gate alone selects.");
  selections.def("marks", &compose::parts::marks,
                 "Every decoration in every slot and every span pass, on "
                 "both sides of the content.");
  selections.def("surface", &compose::parts::surface,
                 "The fill surface and its echoes.");
  selections.def("content", &compose::parts::content,
                 "The text, image or custom leaf.");
  selections.def("children", &compose::parts::children,
                 "Everything under the node.");
  selections.def("named", &compose::parts::named, py::arg("name"),
                 "One mark, by the local name its slot call gave it. A name "
                 "that matches nothing selects nothing.");
}

/** The gate value and the factories that build it. A gate is made by a
 *  factory alone, because the factory installs the resolver the kernel
 *  reads the gate through and a gate without one gates nothing. */
void bindGates(py::module_& composition, py::module_& gates) {
  py::class_<Gate> gate(
      composition, "Gate",
      "How paint arrives past a mask: a comparable value built by the `by` "
      "factories. Only the fields its kind reads are meaningful; the rest "
      "keep their defaults so the value compares.");

  py::enum_<Gate::Kind>(gate, "Kind", "What decides whether paint arrives.")
      .value("Spans", Gate::Kind::Spans, "Runs of the boundary, by arc length.")
      .value("Edge", Gate::Kind::Edge,
             "A straight wipe at an angle, to a fraction.")
      .value("Shape", Gate::Kind::Shape, "A region in the node's local space.")
      .value("Coverage", Gate::Kind::Coverage,
             "Another paint's alpha or luma, per pixel.");
  py::enum_<Gate::Channel>(
      gate, "Channel", "Which channel of a coverage paint becomes coverage.")
      .value("Alpha", Gate::Channel::Alpha, "The coverage paint's alpha.")
      .value("Luma", Gate::Channel::Luma,
             "The coverage paint's luma, taken on the encoded, premultiplied "
             "colour.");

  gate.def_readwrite("kind", &Gate::kind,
                     "Which of the fields below this gate reads.")
      .def_property(
          "where", [](const Gate& self) { return self.where; },
          [](Gate& self, compose::Spans where) {
            self.where = std::move(where);
          },
          "The runs of the boundary a spans gate shows. Read as a copy.")
      .def_readwrite("angleDeg", &Gate::angleDeg,
                     "The direction an edge gate wipes in, in degrees.")
      .def_property(
          "fraction", [](const Gate& self) { return self.fraction; },
          [](Gate& self, py::object fraction) {
            self.fraction = motionAnimatable(fraction);
          },
          "How much an edge gate shows, between zero and one. Read as a "
          "copy; assigned from a number or anything a number animates "
          "from.")
      .def_property(
          "region", [](const Gate& self) { return self.region; },
          [](Gate& self, Region region) { self.region = std::move(region); },
          "The region a shape gate keeps or cuts away. Read as a copy.")
      .def_readwrite("outside", &Gate::outside,
                     "Whether a shape or coverage gate keeps the complement "
                     "of what it names.")
      .def_readwrite("channel", &Gate::channel,
                     "Which channel of the coverage paint a coverage gate "
                     "reads.")
      .def_property(
          "coverage",
          [](const Gate& self) -> std::optional<material::skia::Paint> {
            if (!self.coverage) return std::nullopt;
            return *self.coverage;
          },
          [](Gate& self, std::optional<material::skia::Paint> coverage) {
            self.coverage = coverage
                                ? std::make_shared<const material::skia::Paint>(
                                      std::move(*coverage))
                                : nullptr;
          },
          "The paint a coverage gate reads, or None. Read as a copy.")
      .def("valueCount", &Gate::valueCount,
           "How many animatable numbers this gate carries: three per spans "
           "term, one for an edge fraction, none for a shape or a coverage "
           "gate.")
      .def("copy", [](const Gate& self) { return self; })
      .def(py::self == py::self);
  copyProtocol(gate);

  gates.doc() =
      "The gate factories: how the selected paint arrives. A gate is a "
      "show set, and its complement is a factory of its own.";
  gates.def("spans", &compose::by::spans, py::arg("where"),
            "Arc length along the node's boundary. It addresses the paint "
            "that traces the boundary, the surface and the marks, and does "
            "nothing to content or children.");
  gates.def(
      "edge",
      [](float angleDeg, py::object fraction) {
        return compose::by::edge(angleDeg, motionAnimatable(fraction));
      },
      py::arg("angleDeg"), py::arg("fraction"),
      "A straight edge at `angleDeg` across the node's laid-out box, "
      "showing the fraction lying before it: 0 runs left to right, 90 top "
      "to bottom, 180 right to left, 270 from the bottom.");
  gates.def("shape", &compose::by::shape, py::arg("region"),
            "A region of the node's local space, kept.");
  gates.def("outside", &compose::by::outside, py::arg("region"),
            "Everything outside a region of the node's local space.");
  gates.def("alpha", &compose::by::alpha, py::arg("coverage"),
            "The selected paint keeps the coverage paint's alpha.");
  gates.def("alphaOut", &compose::by::alphaOut, py::arg("coverage"),
            "The selected paint keeps what the coverage paint does not "
            "cover.");
  gates.def("luma", &compose::by::luma, py::arg("coverage"),
            "The selected paint keeps the coverage paint's luma, weighted "
            "on the encoded, premultiplied colour, so a transparent matte "
            "hides as a black one does.");
  gates.def("lumaOut", &compose::by::lumaOut, py::arg("coverage"),
            "The selected paint keeps what the coverage paint's luma "
            "leaves dark.");
}

}  // namespace

void bindComposeMasks(pybind11::module_& module) {
  auto composition = submodule(module, "compose");
  auto selections = submodule(module, "compose.parts");
  auto gates = submodule(module, "compose.by");

  bindRegion(composition);
  bindParts(composition, selections);
  bindGates(composition, gates);

  // The element class belongs to the file that registered it, and both
  // forms of this verb take the values registered just above. The native
  // parameter is spelled `with`, which Python reserves.
  extend<Element>(module, "compose.Element")
      .def(
          "mask",
          [](Element& self, Gate with) -> Element& {
            return self.mask(std::move(with));
          },
          py::arg("with_"), fluent,
          "Gates everything this node paints. A gate addresses only the "
          "paint it can address. Paint only: a mask never relayouts, and "
          "hit-testing keeps the unmasked shape.")
      .def(
          "mask",
          [](Element& self, Parts what, Gate with) -> Element& {
            return self.mask(std::move(what), std::move(with));
          },
          py::arg("what"), py::arg("with_"), fluent,
          "Gates some of what this node paints. Repeated calls append, and "
          "masks whose selections overlap intersect on the overlap: both "
          "gates must pass.");
}

}  // namespace sigil::python
