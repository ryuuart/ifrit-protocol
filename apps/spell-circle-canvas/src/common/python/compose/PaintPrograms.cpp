#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/PaintPrograms.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/draw/Canvas.h>
#include <sigilpython/skia/Values.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/paragraph/Paragraph.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

namespace {

/** The value a keyed shape is compared by, which is any Python value. It
 *  is a type of its own so the signature reads `object` and not an
 *  erased argument another refinement would mistake for its own. */
class ShapeKeyObject : public py::object {
 public:
  using py::object::object;
  static bool check_(py::handle value) { return value.ptr() != nullptr; }
};

/** A bound `weave.Paragraph`, spelled under its Python name. The class
 *  is registered after this file, so a signature naming the native type
 *  would be written before the name exists; this one carries the name
 *  itself and refuses anything that is not a paragraph at the call. */
class ParagraphObject : public py::object {
 public:
  using py::object::object;
  static bool check_(py::handle value) {
    return py::isinstance<weave::Paragraph>(value);
  }
};

/** A bound `weave.ParagraphLayoutOptions` or None, spelled under its
 *  Python name for the same reason as the paragraph beside it. */
class ParagraphLayoutOptionsObject : public py::object {
 public:
  using py::object::object;
  static bool check_(py::handle value) {
    return value.is_none() ||
           py::isinstance<weave::ParagraphLayoutOptions>(value);
  }
};

}  // namespace
}  // namespace sigil::python

namespace pybind11::detail {
template <>
struct handle_type_name<sigil::python::ShapeKeyObject> {
  static constexpr auto name = const_name("builtins.object");
};
template <>
struct handle_type_name<sigil::python::ParagraphObject> {
  static constexpr auto name = const_name("_sigil.weave.Paragraph");
};
template <>
struct handle_type_name<sigil::python::ParagraphLayoutOptionsObject> {
  static constexpr auto name =
      const_name("_sigil.weave.ParagraphLayoutOptions | None");
};
}  // namespace pybind11::detail

namespace sigil::python {
namespace {

constexpr auto fluent = py::return_value_policy::reference_internal;

py::function callable(py::handle value, const char* refusal) {
  if (!PyCallable_Check(value.ptr())) throw py::type_error(refusal);
  return py::reinterpret_borrow<py::function>(value);
}

/** How many of the @p offered parameters @p function names, counted
 *  from the first. A callable Python cannot read a signature from names
 *  @p unreadable of them; one that names more than is offered raises. */
int namedParameters(const py::function& function, int offered,
                    int unreadable) {
  try {
    return py::module_::import("sigil._callbacks")
        .attr("arity")(function, offered)
        .cast<int>();
  } catch (py::error_already_set& error) {
    if (!error.matches(PyExc_ValueError)) throw;
    return unreadable;
  }
}

/** A Python value that keys a shape. Two keys are equal when Python says
 *  the values are, asked under the interpreter lock; a key whose callback
 *  lifetime has closed equals nothing, so the node it keyed re-patches
 *  instead of raising out of a reconcile. */
class PythonShapeKey {
 public:
  explicit PythonShapeKey(std::shared_ptr<PythonValue> value)
      : m_value(std::move(value)) {}

  bool operator==(const PythonShapeKey& other) const {
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

/** @p function made comparable by @p key. The function names the box's
 *  width and height, or nothing when the outline is the same path
 *  whatever the box is. */
compose::Shape shapeKeyedBy(py::object key, py::function function) {
  const int named = namedParameters(function, 2, 2);
  if (named == 1)
    throw py::type_error(
        "A shape function takes the box's width and height, or nothing.");
  auto held = retainCallback(std::move(function));
  return compose::Shape{compose::keyedShape(
      PythonShapeKey{retainValue(std::move(key))}, [held, named](SkSize size) {
        const py::gil_scoped_acquire lock;
        const CallbackBoundary boundary;
        try {
          const py::function outline = held->get();
          const py::object path =
              named == 0 ? outline() : outline(size.width(), size.height());
          return path.cast<SkPath>();
        } catch (const py::error_already_set& error) {
          throw std::runtime_error(error.what());
        }
      })};
}

compose::VarRef variableReference(py::handle name) {
  if (py::isinstance<compose::VarRef>(name))
    return name.cast<compose::VarRef>();
  if (py::isinstance<py::str>(name))
    return compose::var(name.cast<std::string>());
  throw py::type_error("A custom property is named by a VarRef or a string.");
}

/** The colours and stops of a gradient, checked before Skia reads the
 *  stops as one position per colour. */
void checkGradient(const std::vector<SkColor4f>& colors,
                   const std::vector<float>& stops) {
  if (colors.empty()) throw py::value_error("A gradient needs a colour.");
  if (!stops.empty() && stops.size() != colors.size())
    throw py::value_error(
        "A gradient's stops are one position per colour, or none.");
}

}  // namespace

BorrowedPaintContext::BorrowedPaintContext(const compose::PaintContext& context)
    : m_thread(std::this_thread::get_id()), m_context(&context) {}

const compose::PaintContext& BorrowedPaintContext::get() const {
  if (std::this_thread::get_id() != m_thread)
    throw std::runtime_error(
        "A paint context can only be read on the thread that paints.");
  if (!m_context)
    throw std::runtime_error(
        "This paint context is no longer inside its paint call.");
  return *m_context;
}

void BorrowedPaintContext::invalidate() { m_context = nullptr; }

PaintContextLoan::PaintContextLoan(const compose::PaintContext& context)
    : m_view(std::make_shared<BorrowedPaintContext>(context)),
      m_object(py::cast(m_view)) {}

PaintContextLoan::~PaintContextLoan() { m_view->invalidate(); }

/** The canvas of one native call. It is asked for the canvas at every
 *  verb, so it refuses from the moment the call has returned, whoever
 *  still holds the wrapper over it. */
class CanvasLoan::Source final : public CanvasSource {
 public:
  explicit Source(SkCanvas& canvas)
      : m_thread(std::this_thread::get_id()), m_canvas(&canvas) {}

  SkCanvas& canvas() const override {
    if (std::this_thread::get_id() != m_thread)
      throw std::runtime_error(
          "A canvas can only be drawn on from the thread that paints.");
    if (!m_canvas)
      throw std::runtime_error(
          "This canvas is no longer inside its paint call.");
    return *m_canvas;
  }

  void end() { m_canvas = nullptr; }

 private:
  const std::thread::id m_thread;
  SkCanvas* m_canvas;
};

CanvasLoan::CanvasLoan(SkCanvas& canvas)
    : m_source(std::make_shared<Source>(canvas)),
      m_object(borrowedCanvas(m_source)) {}

CanvasLoan::~CanvasLoan() { m_source->end(); }

compose::PaintProgram paintProgram(py::handle value) {
  auto function = callable(value, "A paint program is a callable.");
  const int named = namedParameters(function, 2, 1);
  auto held = retainCallback(std::move(function));
  return [held, named](SkCanvas& canvas, const compose::PaintContext& context) {
    const py::gil_scoped_acquire lock;
    const py::function program = held->get();
    // The loans outlive the callback boundary, so a native scope Python
    // left open is closed while the canvas it drew on is still lent.
    std::optional<CanvasLoan> lent;
    std::optional<PaintContextLoan> frame;
    if (named >= 1) lent.emplace(canvas);
    if (named >= 2) frame.emplace(context);
    try {
      const CallbackBoundary boundary;
      if (named == 0)
        program();
      else if (named == 1)
        program(lent->canvas());
      else
        program(lent->canvas(), frame->context());
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

compose::PenProgram penProgram(py::handle value) {
  auto function = callable(value, "A pen program is a callable.");
  const int named = namedParameters(function, 2, 1);
  auto held = retainCallback(std::move(function));
  return [held, named](draw::Pen& pen, const compose::PaintContext& context) {
    const py::gil_scoped_acquire lock;
    const py::function program = held->get();
    if (named == 1) {
      invokePen(program, pen);
      return;
    }
    std::shared_ptr<BorrowedPen> borrowed;
    std::optional<PaintContextLoan> frame;
    if (named == 2) {
      borrowed = std::make_shared<BorrowedPen>(pen);
      frame.emplace(context);
    }
    struct EndLoan {
      BorrowedPen* pen;
      ~EndLoan() {
        if (pen) pen->invalidate();
      }
    } endLoan{borrowed.get()};
    try {
      const CallbackBoundary boundary;
      if (named == 0)
        program();
      else
        program(borrowed, frame->context());
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

void bindComposePaintPrograms(py::module_& module) {
  using compose::Element;
  using compose::KeyState;
  using compose::PaintContext;
  using compose::VarTable;
  auto composition = submodule(module, "compose");

  // The composer's dials name this policy too, and whichever of the two
  // files is registered first gives it its Python name.
  if (!py::detail::get_type_info(typeid(compose::PromotionPolicy)))
    py::enum_<compose::PromotionPolicy>(composition, "PromotionPolicy")
        .value("Off", compose::PromotionPolicy::Off)
        .value("ByCost", compose::PromotionPolicy::ByCost)
        .value("Eager", compose::PromotionPolicy::Eager);

  auto keyState =
      bindRecord<KeyState>(composition, "KeyState", "Unknown KeyState field: ");
  keyState.def_readwrite("pressed", &KeyState::pressed)
      .def_readwrite("key", &KeyState::key)
      .def_readwrite("keyCode", &KeyState::keyCode)
      .def_readwrite("down", &KeyState::down);

  // Read only: a table is what the tree resolved at a node, and a node's
  // own properties are written with Element.var and Element.varDefaults.
  py::class_<VarTable> varTable(composition, "VarTable");
  varTable.def("copy", [](const VarTable& table) { return table; })
      .def(
          "find",
          [](const VarTable& table,
             py::handle name) -> std::optional<compose::VarValue> {
            const auto* found = table.find(variableReference(name));
            if (!found) return std::nullopt;
            return *found;
          },
          py::arg("name"))
      .def("entries", [](const VarTable& table) { return table.entries(); })
      .def("empty", &VarTable::empty)
      .def("__len__",
           [](const VarTable& table) { return table.entries().size(); })
      .def(
          "__contains__",
          [](const VarTable& table, py::handle name) {
            return table.find(variableReference(name)) != nullptr;
          },
          py::arg("name"))
      .def(py::self == py::self);
  copyProtocol(varTable);

  py::class_<BorrowedPaintContext, std::shared_ptr<BorrowedPaintContext>>
      context(composition, "PaintContext");
  // A record, registered by hand because it is nested in the class it
  // reports about: constructible from keywords, copyable, and answering
  // the copy protocol, as `bindRecord` gives a record at module scope.
  py::class_<PaintContext::Pointer> pointer(context, "Pointer");
  pointer
      .def(py::init([](py::kwargs fields) {
        return keywordValue<PaintContext::Pointer>(fields,
                                                   "Unknown Pointer field: ");
      }))
      .def("copy", [](const PaintContext::Pointer& value) { return value; })
      .def_property(
          "at", [](const PaintContext::Pointer& value) { return value.at; },
          [](PaintContext::Pointer& value, py::handle at) {
            value.at = point(at);
          })
      .def_readwrite("pressed", &PaintContext::Pointer::pressed);
  copyProtocol(pointer);

  // Every reading copies, so nothing Python keeps points into the
  // composer once the paint call has returned.
  context
      .def_property_readonly(
          "size",
          [](const BorrowedPaintContext& self) { return self.get().size; })
      .def_property_readonly(
          "outline",
          [](const BorrowedPaintContext& self) { return self.get().outline; })
      .def_property_readonly("silhouette",
                             [](const BorrowedPaintContext& self) {
                               return self.get().silhouette;
                             })
      .def_property_readonly("elapsedSeconds",
                             [](const BorrowedPaintContext& self) {
                               return self.get().elapsedSeconds;
                             })
      .def_property_readonly("contentScale",
                             [](const BorrowedPaintContext& self) {
                               return self.get().contentScale;
                             })
      .def_property_readonly(
          "animating",
          [](const BorrowedPaintContext& self) { return self.get().animating; })
      .def_property_readonly(
          "borrowed",
          [](const BorrowedPaintContext& self) {
            const auto* borrowed = self.get().borrowed;
            return borrowed ? *borrowed
                            : std::vector<std::pair<std::string, SkPath>>{};
          })
      .def(
          "borrowedPath",
          [](const BorrowedPaintContext& self, const std::string& key) {
            return self.get().borrowedPath(key);
          },
          py::arg("key"))
      .def_property_readonly(
          "toRoot",
          [](const BorrowedPaintContext& self) { return self.get().toRoot; })
      .def_property_readonly(
          "rootSize",
          [](const BorrowedPaintContext& self) { return self.get().rootSize; })
      .def_property_readonly(
          "ink", [](const BorrowedPaintContext& self) { return self.get().ink; })
      .def_property_readonly(
          "font",
          [](const BorrowedPaintContext& self) { return self.get().font; })
      .def_property_readonly(
          "vars",
          [](const BorrowedPaintContext& self) -> std::optional<VarTable> {
            const auto* vars = self.get().vars;
            if (!vars) return std::nullopt;
            return *vars;
          })
      .def_property_readonly(
          "pointer",
          [](const BorrowedPaintContext& self) { return self.get().pointer; })
      .def_property_readonly(
          "keys",
          [](const BorrowedPaintContext& self) -> std::optional<KeyState> {
            const auto* keys = self.get().keys;
            if (!keys) return std::nullopt;
            return *keys;
          })
      .def_property_readonly("bakeDensity",
                             [](const BorrowedPaintContext& self) {
                               return self.get().bakeDensity;
                             })
      .def_property_readonly("promotion", [](const BorrowedPaintContext& self) {
        return self.get().promotion;
      });

  composition.def(
      "custom",
      [](py::function program) {
        return compose::custom(paintProgram(program));
      },
      py::arg("program"));
  composition.def(
      "custom",
      [](const std::string& key, py::function program) {
        return compose::custom(key, paintProgram(program));
      },
      py::arg("key"), py::arg("program"));

  composition.def(
      "pen",
      [](const std::string& key, py::function program, compose::Cache cache) {
        return compose::pen(key, penProgram(program), cache);
      },
      py::arg("key"), py::arg("program"),
      py::arg("cache") = compose::Cache::None);
  composition.def(
      "pen",
      [](py::function program, compose::Cache cache) {
        return compose::pen(penProgram(program), cache);
      },
      py::arg("program"), py::arg("cache") = compose::Cache::None);
  composition.def(
      "graphics",
      [](const std::string& key, py::function program, compose::Cache cache) {
        return compose::graphics(key, penProgram(program), cache);
      },
      py::arg("key"), py::arg("program"),
      py::arg("cache") = compose::Cache::None);
  composition.def(
      "graphics",
      [](py::function program, compose::Cache cache) {
        return compose::graphics(penProgram(program), cache);
      },
      py::arg("program"), py::arg("cache") = compose::Cache::None);

  composition.def(
      "keyedShape",
      [](ShapeKeyObject key, py::function function) {
        return shapeKeyedBy(std::move(key), std::move(function));
      },
      py::arg("key"), py::arg("function"));
  extend<Element>(module, "compose.Element")
      .def(
          "shape",
          [](Element& self, ShapeKeyObject key,
             py::function function) -> Element& {
            return self.shape(
                shapeKeyedBy(std::move(key), std::move(function)));
          },
          py::arg("key"), py::arg("function"), fluent);

  composition.def(
      "image",
      [](std::shared_ptr<image::ImageAsset> asset) {
        return compose::image(std::move(asset));
      },
      py::arg("asset"));
  composition.def(
      "text",
      [](const ParagraphObject& paragraph,
         const ParagraphLayoutOptionsObject& options) {
        return compose::text(
            paragraph.cast<std::shared_ptr<weave::Paragraph>>(),
            options.is_none() ? weave::ParagraphLayoutOptions{}
                              : options.cast<weave::ParagraphLayoutOptions>());
      },
      py::arg("paragraph"), py::arg("options") = py::none());

  composition.def(
      "linearGradient",
      [](py::handle from, py::handle to, std::vector<SkColor4f> colors,
         std::vector<float> stops) {
        checkGradient(colors, stops);
        return compose::linearGradient(point(from), point(to),
                                       std::move(colors), std::move(stops));
      },
      py::arg("from_"), py::arg("to"), py::arg("colors"),
      py::arg("stops") = std::vector<float>{});
  composition.def(
      "radialGradient",
      [](py::handle center, float radius, std::vector<SkColor4f> colors,
         std::vector<float> stops) {
        checkGradient(colors, stops);
        return compose::radialGradient(point(center), radius, std::move(colors),
                                       std::move(stops));
      },
      py::arg("center"), py::arg("radius"), py::arg("colors"),
      py::arg("stops") = std::vector<float>{});
  extend<compose::Fill>(module, "compose.Fill")
      .def_static(
          "var",
          [](const std::string& name) {
            return compose::Fill::var(std::string_view{name});
          },
          py::arg("name"));
}

}  // namespace sigil::python
