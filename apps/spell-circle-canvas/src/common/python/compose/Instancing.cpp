/** @file
 * The instanced leaf: the cell sheet a pool's sprites are baked into,
 * the one element that stamps the pool, and the pick that answers which
 * sprite stands under a point.
 */

#include "Instancing.h"

#include <include/core/SkImage.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSize.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Instances.h>
#include <sigilmaterial/core/BlendMode.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/skia/Values.h>

#include <memory>
#include <optional>
#include <utility>

namespace sigil::python {
namespace py = pybind11;
namespace {

namespace instancing = compose::instancing;
using compose::Element;
using instancing::CellSheet;
using instancing::Pool;

constexpr auto fluent = py::return_value_policy::reference_internal;

/** How many of the one parameter a variant recipe is offered it names. A
 *  callable Python cannot read a signature from is taken to name it. */
int recipeParameters(const py::function& make) {
  try {
    return py::module_::import("sigil._callbacks")
        .attr("arity")(make, 1)
        .cast<int>();
  } catch (py::error_already_set& error) {
    if (!error.matches(PyExc_ValueError)) throw;
    return 1;
  }
}

/** Registers @p count variants of the recipe @p make on @p sheet and
 *  answers the first frame. What the recipe answers decides how a variant
 *  is sized: a tree alone takes @p shared, and a tree with a size brings
 *  its own. The recipe runs on the thread and under the interpreter lock
 *  of the Python call that asked, so what it raises travels out as the
 *  Python error it is. */
int registerVariants(CellSheet& sheet, int count, std::optional<SkSize> shared,
                     const py::function& make) {
  const int named = recipeParameters(make);
  return sheet.variants(count, [&](int index) -> std::pair<Element, SkSize> {
    const py::object answer = named == 0 ? make() : make(index);
    if (isNode(answer)) {
      if (!shared)
        throw py::type_error(
            "A variant recipe given no shared size answers a node and "
            "its size.");
      return {node(answer), *shared};
    }
    if ((py::isinstance<py::tuple>(answer) ||
         py::isinstance<py::list>(answer)) &&
        py::len(answer) == 2) {
      const auto pair = py::reinterpret_borrow<py::sequence>(answer);
      const py::object tree = pair[0];
      if (isNode(tree))
        return {node(tree),
                sizeFrom(pair[1],
                         "A variant's size is a Size, or a width and a "
                         "height.")};
    }
    throw py::type_error(
        "A variant recipe answers a node, or a node and its size.");
  });
}

void bindCellSheet(py::module_& module) {
  constexpr const char* cellSize =
      "A cell's logical size is a Size, or a width and a height.";
  py::class_<CellSheet, std::shared_ptr<CellSheet>>(
      module, "CellSheet",
      "A sprite sheet of element-tree cells. Cells register at a LOGICAL "
      "size; the sheet bakes on the first stamp, oversampled so a stamp at "
      "any scale up to `oversample` never magnifies baked pixels. "
      "Registering a cell after the bake drops the sheet and the next "
      "stamp bakes it again, so cells are cheap to add at setup and "
      "expensive to add per frame. Hold one wherever assets are held: it "
      "outlives any one describe.")
      .def(py::init<float>(), py::arg("oversample") = 2.0f)
      .def("filter", py::overload_cast<SkFilterMode>(&CellSheet::filter),
           py::arg("mode"), fluent,
           "How stamps sample the baked sheet. Linear suits soft sprites "
           "and softens every edge of deliberately blocky art; pass "
           "Nearest for a tilemap or a bitmap font.")
      .def("filter", py::overload_cast<>(&CellSheet::filter, py::const_),
           "How stamps sample the baked sheet as it stands.")
      .def(
          "cell",
          [cellSize](CellSheet& self, const Element& tree,
                     py::handle logicalSize) {
            return self.cell(tree, sizeFrom(logicalSize, cellSize));
          },
          py::arg("tree"), py::arg("logicalSize"),
          "Registers one cell and answers its frame index for "
          "`Pool.frames()`. The tree is forced to exactly the logical "
          "size.")
      .def(
          "variants",
          [cellSize](CellSheet& self, int count, py::handle logicalSize,
                     const py::function& make) {
            return registerVariants(self, count,
                                    sizeFrom(logicalSize, cellSize), make);
          },
          py::arg("count"), py::arg("logicalSize"), py::arg("make"),
          "Several bakes of one recipe: `make` is called for every index "
          "below `count` and each answer is registered as its own frame. "
          "The answer is the FIRST frame, so variant v is frame first + v. "
          "A recipe that answers a tree is registered at `logicalSize`, "
          "and one that answers a tree and a size brings its own. The "
          "index is offered, and a recipe that does not read it names "
          "nothing.")
      .def(
          "variants",
          [](CellSheet& self, int count, const py::function& make) {
            return registerVariants(self, count, std::nullopt, make);
          },
          py::arg("count"), py::arg("make"),
          "The same, where every variant answers a tree and its own size, "
          "so there is no shared one to state.")
      .def("frameCount", &CellSheet::frameCount,
           "How many cells the sheet has registered, which is one more than "
           "the last frame index it answers for.")
      .def("frameSize", &CellSheet::frameSize, py::arg("frame"),
           "The logical size of `frame`, or an empty size for a frame "
           "the sheet does not have.")
      .def("frameTex", &CellSheet::frameTex, py::arg("frame"),
           "Where `frame` was baked, in the sheet's own pixels; empty "
           "until the sheet has baked.")
      .def("oversample", &CellSheet::oversample,
           "How many baked pixels stand behind one logical pixel along each "
           "axis. The sheet keeps at least a half, whatever it was built "
           "with.")
      .def("revision", &CellSheet::revision,
           "What the sheet has become, counted: a registration or a filter "
           "change makes it a different picture.")
      .def(
          "image",
          [](const CellSheet& self) -> sk_sp<SkImage> { return self.image(); },
          "The baked sheet, or None until the first stamp has baked it.");
}

}  // namespace

void bindComposeInstancing(py::module_& root) {
  auto module = submodule(root, "compose.instancing");
  bindCellSheet(module);

  py::enum_<instancing::Mode>(module, "Mode")
      .value("Data", instancing::Mode::Data,
             "Cached: mutate, commit, render. A pool mutated without a "
             "commit prunes and replays the old picture.")
      .value("Live", instancing::Mode::Live,
             "Uncached: the leaf reads the pool every frame.");

  module.def(
      "instances",
      [](std::shared_ptr<CellSheet> atlas, std::shared_ptr<Pool> pool,
         instancing::Mode mode, material::BlendMode blend) {
        return instancing::instances(std::move(atlas), std::move(pool), mode,
                                     blend);
      },
      py::arg("atlas").none(false), py::arg("pool").none(false),
      py::arg("mode") = instancing::Mode::Data,
      py::arg("blend") = material::BlendMode::Normal,
      "The single-draw stamping leaf. It FILLS ITS PARENT, so wrap it in a "
      "sized or positioned box and the pool's positions are that box's "
      "local pixels. `blend` is per sprite, which is what lets "
      "overlapping sprites accumulate; `Element.blendMode` on the leaf would "
      "flatten the whole field into one layer first. The element keeps the "
      "sheet and the pool it names.");
  module.def(
      "pick",
      [](const Pool& pool, const CellSheet& atlas, py::handle position) {
        return instancing::pick(
            pool, atlas,
            pointFrom(position,
                      "A pick is asked about a Point, or an x and a y."));
      },
      py::arg("pool"), py::arg("atlas"), py::arg("point"),
      "The index of the topmost instance whose drawn quad contains "
      "`point`, in the same local pixels the pool's positions are in, or "
      "None. A hit test cannot see inside the leaf, so this is the only "
      "way to ask which sprite is under a point. A fully faded instance "
      "still picks.");
}

}  // namespace sigil::python
