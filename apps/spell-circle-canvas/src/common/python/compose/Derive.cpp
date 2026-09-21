#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Derive.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/skia/Values.h>

#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

namespace {

/** Whether @p value carries a `route` attribute, which is what makes a
 *  Python value a route scheme rather than a route function. */
bool namesRoute(py::handle value) {
  return value.ptr() != nullptr && py::hasattr(value, "route");
}

/** Whether @p value is one of the two erased routers, which carry
 *  `route` themselves and are never read as a scheme to wrap again. */
bool isErasedRouter(py::handle value) {
  return py::isinstance<compose::Router>(value) ||
         py::isinstance<compose::RailRouter>(value);
}

/** A callable over the two endpoint rects, spelled as one in the
 *  signature. A value that names `route` is a scheme and is refused
 *  here, so the two constructors of a router never both accept one
 *  value. */
class RouteFunctionObject : public py::object {
 public:
  using py::object::object;
  static bool check_(py::handle value) {
    return value.ptr() != nullptr && PyCallable_Check(value.ptr()) != 0 &&
           !namesRoute(value);
  }
};

/** A callable over the resolved anchor run, on the same terms. */
class RailFunctionObject : public py::object {
 public:
  using py::object::object;
  static bool check_(py::handle value) {
    return RouteFunctionObject::check_(value);
  }
};

/** Any Python value with a `route` method: a scheme, compared by its own
 *  equality. The signature reads `object` because the method is looked
 *  up when the route is asked for and not when the router is built. */
class RouteSchemeObject : public py::object {
 public:
  using py::object::object;
  static bool check_(py::handle value) {
    return namesRoute(value) && !isErasedRouter(value);
  }
};

/** What a connector's router parameter takes: the erased router, or a
 *  route function it erases on the way in. */
class RouterObject : public py::object {
 public:
  using py::object::object;
  static bool check_(py::handle value) {
    return py::isinstance<compose::Router>(value) ||
           RouteFunctionObject::check_(value);
  }
};

/** What a rail's router parameter takes, on the same terms. */
class RailRouterObject : public py::object {
 public:
  using py::object::object;
  static bool check_(py::handle value) {
    return py::isinstance<compose::RailRouter>(value) ||
           RailFunctionObject::check_(value);
  }
};

}  // namespace
}  // namespace sigil::python

namespace pybind11::detail {
template <>
struct handle_type_name<sigil::python::RouteFunctionObject> {
  static constexpr auto name = const_name(
      "collections.abc.Callable[[_sigil.skia.Rect, _sigil.skia.Rect], "
      "_sigil.skia.Path]");
};
template <>
struct handle_type_name<sigil::python::RailFunctionObject> {
  static constexpr auto name = const_name(
      "collections.abc.Callable[[list[_sigil.skia.Point]], _sigil.skia.Path]");
};
template <>
struct handle_type_name<sigil::python::RouteSchemeObject> {
  static constexpr auto name = const_name("builtins.object");
};
template <>
struct handle_type_name<sigil::python::RouterObject> {
  static constexpr auto name = const_name(
      "_sigil.compose.Router | collections.abc.Callable[[_sigil.skia.Rect, "
      "_sigil.skia.Rect], _sigil.skia.Path]");
};
template <>
struct handle_type_name<sigil::python::RailRouterObject> {
  static constexpr auto name = const_name(
      "_sigil.compose.RailRouter | "
      "collections.abc.Callable[[list[_sigil.skia.Point]], _sigil.skia.Path]");
};
}  // namespace pybind11::detail

namespace sigil::python {
namespace {

constexpr auto fluent = py::return_value_policy::reference_internal;
constexpr auto copied = py::return_value_policy::copy;

using compose::Anchor;
using compose::Element;
using compose::RailRouter;
using compose::Router;
using compose::Tether;

/** Whether the class Python reads @p Native back as is registered yet. A
 *  signature is written when its function is registered, so a binding
 *  that names a class another file registers is offered only once that
 *  class has a Python name to be written under. */
template <class Native>
bool registered() {
  return py::detail::get_type_info(typeid(Native)) != nullptr;
}

/** A point read from @p value, refused as a type error: the shared
 *  reading raises a cast error, which reaches Python as a runtime one. */
SkPoint readPoint(py::handle value) {
  try {
    return point(value);
  } catch (const py::cast_error&) {
    throw py::type_error("A point is a Point, or an x and a y.");
  }
}

/** A rectangle read from @p value, on the same terms. */
SkRect readRect(py::handle value) {
  try {
    return rect(value);
  } catch (const py::cast_error&) {
    throw py::type_error(
        "A rectangle is a Rect, or an x, a y, a width and a height.");
  }
}

/** A size read from @p value, on the same terms. */
SkSize readSize(py::handle value) {
  constexpr const char* refusal = "A size is a Size, or a width and a height.";
  // None loads as a null size, which no conversion error reports.
  if (value.is_none()) throw py::type_error(refusal);
  try {
    return py::cast<SkSize>(value);
  } catch (const py::cast_error&) {
    throw py::type_error(refusal);
  }
}

/** The path a Python route answered. Anything else is refused in the
 *  router's own words, since the conversion error names a native type. */
SkPath readRoutedPath(const py::object& answer) {
  if (!py::isinstance<SkPath>(answer))
    throw std::runtime_error("A route answers a Path.");
  return answer.cast<SkPath>();
}

/** The callable behind @p held: the value itself for a route function,
 *  its `route` method for a scheme. Requires the interpreter lock, and
 *  raises once the lifetime the value was retained against has closed. */
py::object routeOf(const PythonValue& held, bool scheme) {
  py::object value = held.get();
  return scheme ? py::object(value.attr("route")) : value;
}

/** Asks Python for the path between two endpoint rects. The rects are
 *  handed over as copies, so one kept past the call reads its own
 *  storage. */
SkPath routeBetween(const PythonValue& held, bool scheme, const SkRect& from,
                    const SkRect& to) {
  const py::gil_scoped_acquire lock;
  const CallbackBoundary boundary;
  try {
    return readRoutedPath(
        routeOf(held, scheme)(py::cast(from, copied), py::cast(to, copied)));
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

/** Asks Python for the path through a resolved anchor run, handed over
 *  as a list of copied points. */
SkPath routeThrough(const PythonValue& held, bool scheme,
                    std::span<const SkPoint> anchors) {
  const py::gil_scoped_acquire lock;
  const CallbackBoundary boundary;
  try {
    py::list points;
    for (const SkPoint& anchor : anchors)
      points.append(py::cast(anchor, copied));
    return readRoutedPath(routeOf(held, scheme)(points));
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

/** A retained Python scheme's identity and equality. Two are equal when
 *  they hold one value or when Python says the two values are, asked
 *  under the interpreter lock; one whose callback lifetime has closed
 *  equals nothing but itself, so the node it routed re-patches instead
 *  of raising out of a reconcile. */
class PythonSchemeValue {
 public:
  explicit PythonSchemeValue(std::shared_ptr<PythonValue> value)
      : m_value(std::move(value)) {}

  const PythonValue& held() const { return *m_value; }

  bool operator==(const PythonSchemeValue& other) const {
    if (m_value == other.m_value) return true;
    const py::gil_scoped_acquire lock;
    py::object left;
    py::object right;
    try {
      left = m_value->get();
      right = other.m_value->get();
    } catch (const std::runtime_error&) {
      return false;
    }
    const int equal = PyObject_RichCompareBool(left.ptr(), right.ptr(), Py_EQ);
    if (equal < 0) {
      const py::error_already_set error;
      throw std::runtime_error(error.what());
    }
    return equal != 0;
  }

 private:
  std::shared_ptr<PythonValue> m_value;
};

/** A Python value with `route(from_, to)` as a native route scheme, so a
 *  router built over it is comparable and the node it routes can prune. */
struct PythonRouteScheme {
  PythonSchemeValue value;
  bool operator==(const PythonRouteScheme&) const = default;
  SkPath route(const SkRect& from, const SkRect& to) const {
    return routeBetween(value.held(), true, from, to);
  }
};

/** A Python value with `route(anchors)` as a native rail scheme. */
struct PythonRailScheme {
  PythonSchemeValue value;
  bool operator==(const PythonRailScheme&) const = default;
  SkPath route(std::span<const SkPoint> anchors) const {
    return routeThrough(value.held(), true, anchors);
  }
};

Router schemeRouter(const RouteSchemeObject& scheme) {
  return Router{PythonRouteScheme{PythonSchemeValue{retainValue(scheme)}}};
}

Router functionRouter(const py::object& function) {
  std::shared_ptr<PythonValue> held =
      retainCallback(py::reinterpret_borrow<py::function>(function));
  return Router{[held](const SkRect& from, const SkRect& to) {
    return routeBetween(*held, false, from, to);
  }};
}

RailRouter schemeRailRouter(const RouteSchemeObject& scheme) {
  return RailRouter{PythonRailScheme{PythonSchemeValue{retainValue(scheme)}}};
}

RailRouter functionRailRouter(const py::object& function) {
  std::shared_ptr<PythonValue> held =
      retainCallback(py::reinterpret_borrow<py::function>(function));
  return RailRouter{[held](std::span<const SkPoint> anchors) {
    return routeThrough(*held, false, anchors);
  }};
}

Router readRouter(const RouterObject& value) {
  if (py::isinstance<Router>(value)) return value.cast<Router>();
  return functionRouter(value);
}

RailRouter readRailRouter(const RailRouterObject& value) {
  if (py::isinstance<RailRouter>(value)) return value.cast<RailRouter>();
  return functionRailRouter(value);
}

/** One anchor read from @p value: the anchor itself, a node's key for
 *  the centre of that node, or the key with its normalized point and,
 *  after it, its gap — the forms a native initializer list spells. */
Anchor readAnchor(py::handle value) {
  if (py::isinstance<Anchor>(value)) return value.cast<Anchor>();
  if (py::isinstance<py::str>(value)) return Anchor(value.cast<std::string>());
  if (py::isinstance<py::tuple>(value) || py::isinstance<py::list>(value)) {
    const auto fields = py::reinterpret_borrow<py::sequence>(value);
    const auto count = py::len(fields);
    if (count == 2 || count == 3) {
      const py::object key = fields[0];
      const py::object norm = fields[1];
      if (py::isinstance<py::str>(key)) {
        float gap = 0.0f;
        if (count == 3) {
          const py::object stated = fields[2];
          try {
            gap = stated.cast<float>();
          } catch (const py::cast_error&) {
            throw py::type_error("An anchor's gap is a number.");
          }
        }
        return Anchor(key.cast<std::string>(), readPoint(norm), gap);
      }
    }
  }
  throw py::type_error(
      "An anchor is an Anchor, a node's key, or a key with its normalized "
      "point and, after it, its gap.");
}

/** Every anchor of a rail, read before the rail is built so a refused
 *  one leaves nothing half made. A string is one key and never a run of
 *  one-letter keys. */
std::vector<Anchor> readAnchors(py::handle values) {
  if (py::isinstance<py::str>(values) || !py::isinstance<py::iterable>(values))
    throw py::type_error("A rail's anchors are an iterable of anchors.");
  std::vector<Anchor> anchors;
  for (const py::handle value : py::reinterpret_borrow<py::iterable>(values))
    anchors.push_back(readAnchor(value));
  return anchors;
}

/** Every point of an anchor run, read for a router asked directly. */
std::vector<SkPoint> readPoints(py::handle values) {
  if (py::isinstance<py::str>(values) || !py::isinstance<py::iterable>(values))
    throw py::type_error("An anchor run is an iterable of points.");
  std::vector<SkPoint> points;
  for (const py::handle value : py::reinterpret_borrow<py::iterable>(values))
    points.push_back(readPoint(value));
  return points;
}

/** A point member of @p Record as a property: written through the point
 *  reading, so a pair of numbers sets it, and read as the member itself,
 *  so setting one coordinate of what was read sets the record's. */
template <class Record>
void pointField(py::class_<Record>& type, const char* name,
                SkPoint Record::* member, const char* documentation) {
  type.def_property(
      name, [member](Record& self) -> SkPoint& { return self.*member; },
      [member](Record& self, py::handle value) {
        self.*member = readPoint(value);
      },
      fluent, documentation);
}

void bindRouters(py::module_& composition) {
  py::class_<Router> router(
      composition, "Router",
      "The route between two endpoint rects that a connector holds. Built "
      "over a scheme — any value with route(from_, to) and equality — it "
      "compares by that value, and the connector it routes prunes while "
      "the value and the rects are unchanged. Built over a function it "
      "equals only its own copies, so keep the Router and pass it again "
      "instead of building one in every describe. An empty one leaves the "
      "connector its straight line.");
  router.def(py::init<>())
      .def(py::init([](const RouteSchemeObject& scheme) {
             return schemeRouter(scheme);
           }),
           py::arg("scheme"))
      .def(py::init([](const RouteFunctionObject& function) {
             return functionRouter(function);
           }),
           py::arg("function"))
      .def("__bool__",
           [](const Router& self) { return static_cast<bool>(self); })
      .def(
          "route",
          [](const Router& self, py::handle from, py::handle to) {
            return self.route(readRect(from), readRect(to));
          },
          py::arg("from_"), py::arg("to"),
          "The path between the two rects; empty from an empty router.")
      .def("comparable", &Router::comparable,
           "Whether this router takes part in structural equality, which a "
           "route function and an empty router do not.")
      .def("copy", [](const Router& self) { return self; })
      .def(py::self == py::self);
  copyProtocol(router);

  py::class_<RailRouter> railRouter(
      composition, "RailRouter",
      "The path through an ordered run of resolved anchor points that a "
      "rail holds. Comparable exactly as a Router is: over a scheme with "
      "route(anchors) and equality it compares by that value, and over a "
      "function it equals only its own copies. An empty one leaves the "
      "rail its straight polyline.");
  railRouter.def(py::init<>())
      .def(py::init([](const RouteSchemeObject& scheme) {
             return schemeRailRouter(scheme);
           }),
           py::arg("scheme"))
      .def(py::init([](const RailFunctionObject& function) {
             return functionRailRouter(function);
           }),
           py::arg("function"))
      .def("__bool__",
           [](const RailRouter& self) { return static_cast<bool>(self); })
      .def(
          "route",
          [](const RailRouter& self, py::handle anchors) {
            const std::vector<SkPoint> points = readPoints(anchors);
            return self.route(points);
          },
          py::arg("anchors"),
          "The path through the points; empty from an empty router.")
      .def("comparable", &RailRouter::comparable,
           "Whether this router takes part in structural equality, which a "
           "route function and an empty router do not.")
      .def("copy", [](const RailRouter& self) { return self; })
      .def(py::self == py::self);
  copyProtocol(railRouter);
}

void bindAnchor(py::module_& composition) {
  py::class_<Anchor> anchor(
      composition, "Anchor",
      "One endpoint or waypoint of a rail: a normalized point on a keyed "
      "node's resolved bounds, or a free point in the rail's own "
      "coordinates bound to nothing. `gap` pulls a terminal anchor back "
      "along its segment and is ignored on a waypoint.");

  // The two alternatives are records nested in the class they are the
  // choices of, registered by hand as a record at module scope is by
  // `bindRecord`, and ahead of the property whose signature names them.
  py::class_<Anchor::OnNode> onNode(
      anchor, "OnNode",
      "Bound to a node: `norm` is read on that node's resolved bounds, "
      "(0, 0) its top left and (1, 1) its bottom right.");
  onNode
      .def(py::init([](py::kwargs fields) {
        return keywordValue<Anchor::OnNode>(fields, "Unknown OnNode field: ");
      }))
      .def("copy", [](const Anchor::OnNode& self) { return self; })
      .def_readwrite("key", &Anchor::OnNode::key)
      .def(py::self == py::self);
  pointField(onNode, "norm", &Anchor::OnNode::norm,
             "The normalized point on the node's bounds.");
  copyProtocol(onNode);

  py::class_<Anchor::FreePoint> freePoint(
      anchor, "FreePoint",
      "Bound to nothing: `point` is read in the rail's own coordinates.");
  freePoint
      .def(py::init([](py::kwargs fields) {
        return keywordValue<Anchor::FreePoint>(fields,
                                               "Unknown FreePoint field: ");
      }))
      .def("copy", [](const Anchor::FreePoint& self) { return self; })
      .def(py::self == py::self);
  pointField(freePoint, "point", &Anchor::FreePoint::point,
             "The point, in the rail's own coordinates.");
  copyProtocol(freePoint);

  anchor.def(py::init<>())
      .def(py::init([](std::string key, py::handle norm, float gap) {
             return Anchor(std::move(key), readPoint(norm), gap);
           }),
           py::arg("key"), py::arg("norm") = py::make_tuple(0.5f, 0.5f),
           py::arg("gap") = 0.0f)
      .def_static(
          "on",
          [](std::string key, py::handle norm, float gap) {
            return Anchor::on(std::move(key), readPoint(norm), gap);
          },
          py::arg("key"), py::arg("norm") = py::make_tuple(0.5f, 0.5f),
          py::arg("gap") = 0.0f,
          "The bound form: a normalized point on the keyed node's bounds.")
      .def_static(
          "at",
          [](py::handle point, float gap) {
            return Anchor::at(readPoint(point), gap);
          },
          py::arg("point"), py::arg("gap") = 0.0f,
          "The free form: a point in the rail's own coordinates.")
      .def_property(
          "where", [](const Anchor& self) { return self.where; },
          [](Anchor& self,
             std::variant<Anchor::OnNode, Anchor::FreePoint> where) {
            self.where = std::move(where);
          },
          "Which of the two this anchor is, copied on read: assign an "
          "OnNode or a FreePoint to change it, since editing what was read "
          "edits the copy.")
      .def_readwrite("gap", &Anchor::gap)
      .def(
          "key", [](const Anchor& self) { return std::string(self.key()); },
          "The node this anchor is bound to, or empty for a free point.")
      .def("copy", [](const Anchor& self) { return self; })
      .def(py::self == py::self);
  copyProtocol(anchor);
}

void bindTether(py::module_& module, py::module_& composition) {
  auto tether =
      bindRecord<Tether>(composition, "Tether", "Unknown Tether field: ");
  tether.doc() =
      "Where a box hangs off a keyed one: `on` is the normalized point of "
      "the anchor's resolved rect the box hangs from, `at` the normalized "
      "point of the box that lands there, and `offset` how far from there "
      "in pixels. The stated tether is tried first and then each of "
      "`fallbacks` in order, and the first whose box stays inside `within` "
      "is taken; an empty `within` is the composer's own bounds. A key "
      "nothing carries places nothing.";
  tether.def_readwrite("key", &Tether::key);
  pointField(tether, "on", &Tether::on,
             "The normalized point of the anchor's rect the box hangs from.");
  pointField(tether, "at", &Tether::at,
             "The normalized point of the box that lands there.");
  pointField(tether, "offset", &Tether::offset,
             "How far from there, in pixels, in the composition's axes.");
  tether
      .def_property(
          "within", [](Tether& self) -> SkRect& { return self.within; },
          [](Tether& self, py::handle value) { self.within = readRect(value); },
          fluent,
          "The rect a placed box has to stay inside; empty is the "
          "composer's own bounds.")
      .def_property(
          "fallbacks", [](const Tether& self) { return self.fallbacks; },
          [](Tether& self, std::vector<Tether> fallbacks) {
            self.fallbacks = std::move(fallbacks);
          },
          "The tethers tried in order when the stated one does not fit, "
          "copied on read: assign a list to change them. A fallback's own "
          "fallbacks are not read.")
      .def(
          "place",
          [](const Tether& self, py::handle anchor, py::handle size) {
            return self.place(readRect(anchor), readSize(size));
          },
          py::arg("anchor"), py::arg("size"),
          "Where a box of `size` lands when this tether ties it to "
          "`anchor`, in the space both were given in. Fallbacks are not "
          "consulted.")
      .def(py::self == py::self);

  // The element class belongs to the file that registered it, and this
  // verb takes the record registered just above.
  extend<Element>(module, "compose.Element")
      .def("tether", &Element::tether, py::arg("tether"), fluent,
           "Hangs this node off a keyed one. It leaves the flow, and where "
           "it lands is resolved against the geometry the anchor resolved "
           "to and again whenever that moves. A later call replaces the "
           "tether.");
}

void bindSpines(py::module_& composition) {
  auto around = bindRecord<compose::Around>(composition, "Around",
                                            "Unknown Around field: ");
  around.doc() =
      "A band's spine borrowed from the keyed element's resolved shape.";
  around.def_readwrite("key", &compose::Around::key).def(py::self == py::self);
  composition.def(
      "around", [](const std::string& key) { return compose::around(key); },
      py::arg("key"), "A spine borrowed from the element keyed `key`.");

  // Built by `across` alone, which installs the engine that sweeps the
  // profile into a region; one made any other way would sweep nothing.
  py::class_<compose::Across> across(
      composition, "Across",
      "A band's width across its spine, compared by its profile.");
  across.def("copy", [](const compose::Across& self) { return self; })
      .def(py::self == py::self);
  copyProtocol(across);

  // The constant overload stands first, so a whole number is read as
  // pixels before any conversion a profile offers is tried.
  composition.def("across", py::overload_cast<float>(&compose::across),
                  py::arg("pixels"), "A constant band width, in pixels.");
  // The width law is a class of the geometry bindings, so the overload
  // and the reading that name it are written where it has a name.
  if (registered<geometry::path::Profile>()) {
    across.def_property_readonly(
        "profile", [](const compose::Across& self) { return self.profile; },
        "The width law, copied on read.");
    composition.def(
        "across",
        [](geometry::path::Profile profile) {
          return compose::across(std::move(profile));
        },
        py::arg("profile"),
        "A band width that varies along the spine, as a profile over arc "
        "length.");
  }

  // The borrowed spine stands first: an Around is no shape, and the shape
  // reading would refuse it as one rather than let the next overload try.
  composition.def(
      "band",
      [](const compose::Around& spine, const compose::Across& width) {
        return compose::band(spine, width);
      },
      py::arg("spine"), py::arg("width"),
      "A band over a spine borrowed from the element keyed by `spine`, "
      "resolved once that element's shape is, and re-swept whenever it "
      "moves.");
  composition.def(
      "band",
      [](py::object spine, const compose::Across& width) {
        return compose::band(shape(spine), width);
      },
      py::arg("spine"), py::arg("width"),
      "A band: the ribbon `spine` sweeps out at `width` across it. The "
      "spine is any shape a node takes — a generator, a path or a function "
      "of the node's size — and the band lays out, fills, clips and takes "
      "stroke passes like any other node.");

  composition.def(
      "bandPointAt", &compose::bandPointAt, py::arg("spine"), py::arg("along"),
      py::arg("acrossPx"),
      "The point at `along`, a fraction of the spine's arc length, and "
      "`acrossPx` pixels on its normal. Positive is to the left of travel, "
      "which with y pointing down is outside a clockwise path.");
}

void bindRoutes(py::module_& module, py::module_& composition) {
  composition.def(
      "connector",
      [](const std::string& fromKey, const std::string& toKey,
         const RouterObject& router, float gap) {
        return compose::connector(fromKey, toKey, readRouter(router), gap);
      },
      py::arg("fromKey"), py::arg("toKey"), py::arg("router") = Router{},
      py::arg("gap") = 0.0f,
      "An edge between two keyed nodes, routed against the rects they "
      "resolved to and again whenever either moves. The routed path is the "
      "node's own outline, so a stroke or any decoration dresses it, and "
      "`gap` pulls each end back along the route by that many pixels. "
      "Place it absolute over the nodes it connects. A key nothing carries "
      "draws nothing.");
  composition.def(
      "rail",
      [](py::handle anchors, const RailRouterObject& router) {
        return compose::rail(readAnchors(anchors), readRailRouter(router));
      },
      py::arg("anchors"), py::arg("router") = RailRouter{},
      "A path threaded through an ordered run of anchors, resolved against "
      "where the anchored nodes landed and again whenever one moves. Each "
      "anchor is an Anchor, a node's key, or a key with its normalized "
      "point and, after it, its gap. Place it absolute over the nodes it "
      "threads.");

  // The family under one name: the same functions, and the text member
  // as a free verb beside the method that chains.
  auto derive = submodule(module, "compose.derive");
  derive.attr("connector") = composition.attr("connector");
  derive.attr("rail") = composition.attr("rail");
  derive.attr("around") = composition.attr("around");
  derive.attr("band") = composition.attr("band");
  derive.def(
      "contentFlowAround",
      [](compose::Text leaf, const std::string& key, float margin) {
        return compose::derive::contentFlowAround(std::move(leaf), key, margin);
      },
      py::arg("text"), py::arg("key"), py::arg("margin") = 0.0f,
      "A copy of `text` whose lines flow around the keyed node, as "
      "Text.contentFlowAround sets on the leaf itself.");
}

}  // namespace

void bindComposeDerive(pybind11::module_& module) {
  auto composition = submodule(module, "compose");
  bindRouters(composition);
  bindAnchor(composition);
  bindTether(module, composition);
  bindSpines(composition);
  bindRoutes(module, composition);
}

}  // namespace sigil::python
