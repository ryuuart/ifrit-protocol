/** @file
 * The instance pool an author fills, bound as
 * `compose.instancing.Pool`: its lanes' accessors, the flight each
 * instance makes and the step that flies them, and the edits that
 * publish themselves beside the lane writes that wait for a commit.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Instances.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Instancing.h"

namespace sigil::python {
namespace py = pybind11;
namespace {

using compose::instancing::Pool;
using Flight = Pool::Flight;
using PoolClass = py::class_<Pool, std::shared_ptr<Pool>>;

/** Adds the accessor @p name to the pool class, answering the lane
 *  @p access reads. The lane is asked for once on the way out, so an
 *  opt-in lane exists from the call that names it, as it does natively. */
template <class Item>
void poolLane(PoolClass& pool, const char* name,
              typename Lane<Item>::Access access, const char* documentation) {
  pool.def(
      name,
      [access](std::shared_ptr<Pool> self) {
        Lane<Item> lane(std::move(self), access);
        (void)lane.items();
        return lane;
      },
      documentation);
}

// ---------------------------------------------------------------------------
// Flights

/** Whether the bindings registered the type of @p value, which is what
 *  tells a native curve from a callable Python wrote. */
bool nativeValue(py::handle value) {
  return py::detail::get_type_info(Py_TYPE(value.ptr())) != nullptr;
}

/** Steps every flight of @p pool to @p seconds through @p ease.
 *
 *  The native step asks the ease once per instance from inside its walk
 *  over the pool's own storage, and a Python callable asked there could
 *  change the pool's length under that walk. So a Python ease is never
 *  asked there. One native step, through an ease that answers the
 *  progress it is handed, records where each instance is; Python shapes
 *  those numbers with no native walk in flight; and a second native step
 *  replays what it answered. What the ease raises is the error the caller
 *  sees. */
void flyPool(Pool& pool, float seconds, py::handle ease) {
  if (ease.is_none()) {
    pool.fly(seconds);
    return;
  }
  if (nativeValue(ease) || !PyCallable_Check(ease.ptr())) {
    pool.fly(seconds, motionEase(ease));
    return;
  }
  std::vector<float> progress;
  pool.fly(seconds, [&progress](float unit) {
    progress.push_back(unit);
    return unit;
  });
  std::vector<float> eased;
  eased.reserve(progress.size());
  {
    const CallbackBoundary boundary;
    for (const float unit : progress) {
      const py::object answer = ease(unit);
      try {
        eased.push_back(py::cast<float>(answer));
      } catch (const py::cast_error&) {
        throw py::type_error("An ease answers a number.");
      }
    }
  }
  std::size_t next = 0;
  bool outrun = false;
  pool.fly(seconds, [&](float unit) {
    if (next < eased.size()) return eased[next++];
    outrun = true;
    return unit;
  });
  if (outrun || next != eased.size())
    throw std::runtime_error(
        "The ease changed the pool it was stepping, so the step it shaped "
        "is not the one the pool took.");
}

void bindFlight(PoolClass& pool) {
  // A record, registered by hand because it is nested in the class whose
  // lane it fills: constructible from keywords, copyable, and answering
  // the copy protocol, as `bindRecord` gives a record at module scope.
  py::class_<Flight> flight(
      pool, "Flight",
      "One instance's flight: where it starts, where it lands, and when it "
      "makes the trip. `start` and `duration` are seconds on whatever "
      "clock `Pool.fly` is stepped with, and they are per instance because "
      "the stagger is the point. Before `start` the instance stands at "
      "`from_`; a zero `duration` arrives at `start` and holds.");
  flight
      .def(py::init([](py::kwargs fields) {
        return keywordValue<Flight>(fields, "Unknown Flight field: ");
      }))
      .def("copy", [](const Flight& value) { return value; })
      .def_property(
          "rotateFrom", angleGetter(&Flight::rotateFrom),
          angleSetter<AngleInRadians>(&Flight::rotateFrom))
      .def_property(
          "rotateTo", angleGetter(&Flight::rotateTo),
          angleSetter<AngleInRadians>(&Flight::rotateTo))
      .def_readwrite("scaleFrom", &Flight::scaleFrom)
      .def_readwrite("scaleTo", &Flight::scaleTo)
      .def_readwrite("alphaFrom", &Flight::alphaFrom)
      .def_readwrite("alphaTo", &Flight::alphaTo)
      .def_property(
          "start", [](const Flight& self) { return self.start; },
          [](Flight& self, motion::Duration start) {
            self.start = (float)start.count();
          })
      .def_property(
          "duration", [](const Flight& self) { return self.duration; },
          [](Flight& self, motion::Duration duration) {
            self.duration = (float)duration.count();
          })
      .def(
          "__eq__",
          [](const Flight& value, const Flight& other) {
            return value == other;
          },
          py::is_operator(), py::arg("other"));
  constexpr const char* flightPoint =
      "A flight starts and lands at a Point, or an x and a y.";
  // `from` is a Python keyword, so the field answers to `from_` as well:
  // the native name reaches it through keywords and `getattr`, and the
  // other one through attribute syntax.
  for (const char* name : {"from", "from_"})
    flight.def_property(
        name, [](const Flight& value) { return value.from; },
        [flightPoint](Flight& value, py::handle position) {
          value.from = pointFrom(position, flightPoint);
        });
  flight.def_property(
      "to", [](const Flight& value) { return value.to; },
      [flightPoint](Flight& value, py::handle position) {
        value.to = pointFrom(position, flightPoint);
      });
  copyProtocol(flight);
}

// ---------------------------------------------------------------------------
// The pool

void bindPool(py::module_& module) {
  PoolClass pool(
      module, "Pool",
      "Per-instance data as parallel lanes: position, rotation, uniform "
      "scale, tint and frame, with opt-in lanes for a non-uniform size, an "
      "opacity, a window inside the sprite's cell and a flight. The caller "
      "owns and fills it; the stamping leaf only reads it. A position is "
      "the cell's CENTRE, not its top-left.\n\n"
      "`add`, `resize` and `clear` publish themselves. Writing through a "
      "lane does not: call `commit()` afterwards, or a `Mode.Data` leaf "
      "compares equal, prunes, and replays the picture of the pool as it "
      "was, with no diagnostic.");
  bindFlight(pool);
  // A signature is written when its function is registered, so the lane
  // of flights stands between the flight it holds and the accessor that
  // answers it.
  bindFlightLane(module);
  pool.def(py::init<>())
      .def(
          "add",
          [](Pool& self, py::handle position, int frame,
             AngleInRadians rotateRadians, float scale, material::Color tint) {
            return self.add(
                pointFrom(position,
                          "An instance stands at a Point, or an x and a y."),
                frame, rotateRadians.value, scale, tint);
          },
          py::arg("position"), py::arg("frame") = 0,
          py::arg("rotateRadians") = AngleInRadians{}, py::arg("scale") = 1.0f,
          py::arg("tint") = SkColors::kWhite,
          "Appends one instance and answers its index.")
      .def("clear", &Pool::clear,
           "Drops every instance, and every opt-in lane with them.")
      .def("resize", &Pool::resize, py::arg("count"),
           "Makes the pool `count` instances long. An appended instance "
           "stands at the origin, unrotated, at unit scale, untinted, on "
           "frame zero.")
      .def("size", &Pool::size, "How many instances the pool holds.")
      .def("__len__", &Pool::size)
      .def("hasTexWindows", &Pool::hasTexWindows,
           "Whether the window lane stands at the pool's length, which is "
           "what makes the stamp read it.")
      .def("hasSizes", &Pool::hasSizes,
           "Whether the size lane stands at the pool's length, which is "
           "what makes the stamp read it.")
      .def("hasAlphas", &Pool::hasAlphas,
           "Whether the opacity lane stands at the pool's length, which is "
           "what makes the stamp read it.")
      .def("hasFlights", &Pool::hasFlights,
           "Whether the pool carries the flight lane at all. A lane the "
           "pool has grown past is still the pool's: the instances "
           "appended to it materialise at rest at the next read or step, "
           "and `clear()` is what drops it.")
      .def(
          "fly",
          [](Pool& self, motion::Duration moment, py::handle ease) {
            flyPool(self, (float)moment.count(), ease);
          },
          py::arg("seconds"), py::arg("ease") = py::none(),
          "Steps every flight to `seconds`, writing the position, "
          "rotation, scale and opacity lanes, and publishes the edit. "
          "`ease` shapes each instance's own progress from zero to one: "
          "a native curve, or a callable over a fraction, and with none "
          "the progress is used as it is. A pool with no flight lane is "
          "left alone.")
      .def("commit", &Pool::commit,
           "Publishes what was written through the lanes: the next "
           "describe carries a new revision, so a `Mode.Data` leaf "
           "repaints once.")
      .def("revision", &Pool::revision,
           "What the pool has published, counted: `add`, `resize`, `clear`, "
           "`commit` and a step of its flights each make it a different "
           "number, and a lane write alone does not.");
  poolLane<SkPoint>(
      pool, "positions", +[](Pool& self) { return self.positions(); },
      "Where each instance's cell is centred.");
  poolLane<float>(
      pool, "rotations", +[](Pool& self) { return self.rotations(); },
      "Each instance's rotation about its centre, in radians.");
  poolLane<float>(
      pool, "scales", +[](Pool& self) { return self.scales(); },
      "Each instance's uniform scale.");
  poolLane<material::Color>(
      pool, "tints", +[](Pool& self) { return self.tints(); },
      "Each instance's tint, which MULTIPLIES the cell's colours. That "
      "makes it wrong for exact-palette work: bake one cell per palette "
      "and select by frame instead.");
  poolLane<int>(
      pool, "frames", +[](Pool& self) { return self.frames(); },
      "Which cell of the sheet each instance stamps.");
  poolLane<SkRect>(
      pool, "texWindows", +[](Pool& self) { return self.texWindows(); },
      "Each instance's window inside its sprite's cell, as fractions of "
      "that cell: (0, 0, 1, 1) is the whole of it. The lane exists from "
      "the first call that asks for it.");
  poolLane<SkSize>(
      pool, "sizes", +[](Pool& self) { return self.sizes(); },
      "Each instance's non-uniform scale, a width and a height multiplied "
      "on top of `scales()`. The lane exists from the first call that "
      "asks for it.");
  poolLane<float>(
      pool, "alphas", +[](Pool& self) { return self.alphas(); },
      "Each instance's opacity, multiplied into its tint's alpha when it "
      "is stamped. The lane exists from the first call that asks for it.");
  poolLane<Flight>(
      pool, "flights", +[](Pool& self) { return self.flights(); },
      "Each instance's flight. The lane exists from the first call that "
      "asks for it, and a flight nobody wrote rests where its instance "
      "already stands.");
}

}  // namespace

void bindComposeInstancingPool(py::module_& root) {
  auto module = submodule(root, "compose.instancing");
  bindPool(module);
}

}  // namespace sigil::python
