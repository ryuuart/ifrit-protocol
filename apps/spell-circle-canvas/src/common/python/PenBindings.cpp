#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilpython/PenBindings.h>
#include <sigilpython/ValueBindings.h>
#include <src/core/SkScopeExit.h>

#include <bit>
#include <cstddef>
#include <cstring>
#include <exception>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
using draw::Pen;
using PenClass = py::class_<BorrowedPen, std::shared_ptr<BorrowedPen>>;

namespace {

void bindConstants(py::module_& module) {
  auto constant = py::enum_<draw::Constant>(module, "Constant");
#define SIGIL_PY_CONSTANT(name) constant.value(#name, draw::name)
  SIGIL_PY_CONSTANT(CORNER);
  SIGIL_PY_CONSTANT(CORNERS);
  SIGIL_PY_CONSTANT(CENTER);
  SIGIL_PY_CONSTANT(RADIUS);
  SIGIL_PY_CONSTANT(RADIANS);
  SIGIL_PY_CONSTANT(DEGREES);
  SIGIL_PY_CONSTANT(RGB);
  SIGIL_PY_CONSTANT(HSB);
  SIGIL_PY_CONSTANT(HSL);
  SIGIL_PY_CONSTANT(OPEN);
  SIGIL_PY_CONSTANT(CHORD);
  SIGIL_PY_CONSTANT(PIE);
  SIGIL_PY_CONSTANT(CLOSE);
  SIGIL_PY_CONSTANT(ROUND);
  SIGIL_PY_CONSTANT(SQUARE);
  SIGIL_PY_CONSTANT(PROJECT);
  SIGIL_PY_CONSTANT(MITER);
  SIGIL_PY_CONSTANT(BEVEL);
  SIGIL_PY_CONSTANT(LEFT);
  SIGIL_PY_CONSTANT(RIGHT);
  SIGIL_PY_CONSTANT(TOP);
  SIGIL_PY_CONSTANT(BOTTOM);
  SIGIL_PY_CONSTANT(BASELINE);
  SIGIL_PY_CONSTANT(NORMAL);
  SIGIL_PY_CONSTANT(ITALIC);
  SIGIL_PY_CONSTANT(BOLD);
  SIGIL_PY_CONSTANT(BOLDITALIC);
  SIGIL_PY_CONSTANT(POLYGON);
  SIGIL_PY_CONSTANT(POINTS);
  SIGIL_PY_CONSTANT(LINES);
  SIGIL_PY_CONSTANT(TRIANGLES);
  SIGIL_PY_CONSTANT(TRIANGLE_FAN);
  SIGIL_PY_CONSTANT(TRIANGLE_STRIP);
  SIGIL_PY_CONSTANT(QUADS);
  SIGIL_PY_CONSTANT(QUAD_STRIP);
  SIGIL_PY_CONSTANT(BLEND);
  SIGIL_PY_CONSTANT(ADD);
  SIGIL_PY_CONSTANT(MULTIPLY);
  SIGIL_PY_CONSTANT(SCREEN);
  SIGIL_PY_CONSTANT(REPLACE);
  SIGIL_PY_CONSTANT(REMOVE);
  SIGIL_PY_CONSTANT(DARKEST);
  SIGIL_PY_CONSTANT(LIGHTEST);
  SIGIL_PY_CONSTANT(DIFFERENCE);
  SIGIL_PY_CONSTANT(EXCLUSION);
  SIGIL_PY_CONSTANT(OVERLAY);
  SIGIL_PY_CONSTANT(HARD_LIGHT);
  SIGIL_PY_CONSTANT(SOFT_LIGHT);
  SIGIL_PY_CONSTANT(DODGE);
  SIGIL_PY_CONSTANT(BURN);
  SIGIL_PY_CONSTANT(SUBTRACT);
  SIGIL_PY_CONSTANT(CANVAS);
  SIGIL_PY_CONSTANT(SHAPE);
#undef SIGIL_PY_CONSTANT
  constant.export_values();
  module.attr("PI") = draw::PI;
  module.attr("TWO_PI") = draw::TWO_PI;
  module.attr("TAU") = draw::TAU;
  module.attr("HALF_PI") = draw::HALF_PI;
  module.attr("QUARTER_PI") = draw::QUARTER_PI;
}

std::vector<SkPoint> pointBatch(py::handle values) {
  if (PyObject_CheckBuffer(values.ptr())) {
    const auto source = py::reinterpret_borrow<py::buffer>(values).request();
    const bool nativeFloat =
        source.format == "f" || source.format == "@f" ||
        source.format == "=f" ||
        (std::endian::native == std::endian::little && source.format == "<f") ||
        (std::endian::native == std::endian::big && source.format == ">f");
    if (!nativeFloat || source.itemsize != sizeof(float))
      throw py::type_error(
          "Point buffers must contain native-endian float32 coordinates.");
    if (!((source.ndim == 1 && source.shape[0] % 2 == 0) ||
          (source.ndim == 2 && source.shape[1] == 2)))
      throw py::value_error(
          "Point buffers need shape (N, 2) or an even-length flat array.");
    if (!PyBuffer_IsContiguous(source.view(), 'C'))
      throw py::value_error("Point buffers must be C-contiguous.");

    // Buffer storage holds floats, not SkPoint objects. A bulk copy keeps
    // native object lifetime and alignment independent of the exporter.
    static_assert(std::is_trivially_copyable_v<SkPoint> &&
                  sizeof(SkPoint) == 2 * sizeof(float) &&
                  offsetof(SkPoint, fX) == 0 &&
                  offsetof(SkPoint, fY) == sizeof(float));
    std::vector<SkPoint> points(static_cast<size_t>(source.size) / 2);
    if (!points.empty())
      std::memcpy(points.data(), source.ptr, points.size() * sizeof(SkPoint));
    return points;
  }
  std::vector<SkPoint> points;
  if (PyList_Check(values.ptr()) || PyTuple_Check(values.ptr()))
    points.reserve(py::len(values));
  for (auto value : py::reinterpret_borrow<py::iterable>(values))
    points.push_back(point(value));
  return points;
}

draw::Slot callerSlot(Pen& pen, int index) {
  struct Names {
    std::set<std::string> files;
  };
  auto& names =
      pen.retained().get<Names>(draw::Slot::at(std::source_location::current()),
                                [] { return std::make_shared<Names>(); });
  PyFrameObject* frame = PyEval_GetFrame();
  if (!frame) return {"<python>", 0, 0, index};
  const py::object code = py::reinterpret_steal<py::object>(
      reinterpret_cast<PyObject*>(PyFrame_GetCode(frame)));
  const auto [name, inserted] =
      names.files.insert(py::str(code.attr("co_filename")));
  int line = PyFrame_GetLineNumber(frame), column = 0, endLine = 0,
      endColumn = 0;
  PyCode_Addr2Location(reinterpret_cast<PyCodeObject*>(code.ptr()),
                       PyFrame_GetLasti(frame), &line, &column, &endLine,
                       &endColumn);
  return {name->c_str(), static_cast<uint32_t>(std::max(line, 0)),
          static_cast<uint32_t>(std::max(column, 0)), index};
}

/** The canvas is borrowed through the pen that gave it, so retaining this
 *  wrapper cannot extend a native frame or bypass its thread check. */
class BorrowedCanvas {
 public:
  explicit BorrowedCanvas(std::shared_ptr<BorrowedPen> pen)
      : m_pen(std::move(pen)) {}
  SkCanvas& get() const {
    SkCanvas* canvas = m_pen->get().canvas();
    if (!canvas) throw std::runtime_error("The pen has no open canvas.");
    return *canvas;
  }

 private:
  std::shared_ptr<BorrowedPen> m_pen;
};

class Graphics : public std::enable_shared_from_this<Graphics> {
 public:
  Graphics(float width, float height) : m_graphics(width, height) {}
  std::shared_ptr<BorrowedPen> begin(const std::shared_ptr<BorrowedPen>& host) {
    if (!host) throw py::type_error("A graphics frame requires a host pen.");
    checkThread();
    if (m_pen)
      throw std::runtime_error(
          "This graphics buffer already has an open frame.");
    m_thread = std::this_thread::get_id();
    m_pen = std::make_shared<BorrowedPen>(m_graphics.begin(host->get()));
    host->whenClosed([self = shared_from_this(),
                      opened = std::weak_ptr<BorrowedPen>{m_pen}] {
      if (self->m_pen == opened.lock()) self->end();
    });
    return m_pen;
  }
  void end() {
    checkThread();
    if (!m_pen) return;
    m_pen->invalidate();
    m_graphics.end();
    m_pen.reset();
  }
  draw::Graphics& get() {
    checkThread();
    return m_graphics;
  }
  const draw::Graphics& get() const {
    checkThread();
    return m_graphics;
  }

 private:
  void checkThread() const {
    if (m_thread != std::thread::id{} && m_thread != std::this_thread::get_id())
      throw std::runtime_error(
          "A graphics buffer is used on its drawing thread.");
  }
  draw::Graphics m_graphics;
  std::shared_ptr<BorrowedPen> m_pen;
  std::thread::id m_thread;
};

void bindCanvas(py::module_& module) {
  auto mode = py::enum_<SkCanvas::PointMode>(module, "PointMode");
  mode.value("Points", SkCanvas::kPoints_PointMode)
      .value("Lines", SkCanvas::kLines_PointMode)
      .value("Polygon", SkCanvas::kPolygon_PointMode);
  py::class_<BorrowedCanvas>(module, "Canvas")
      .def("save", [](BorrowedCanvas& self) { return self.get().save(); })
      .def("restore", [](BorrowedCanvas& self) { self.get().restore(); })
      .def(
          "restoreToCount",
          [](BorrowedCanvas& self, int count) {
            self.get().restoreToCount(count);
          },
          py::arg("count"))
      .def("getSaveCount",
           [](BorrowedCanvas& self) { return self.get().getSaveCount(); })
      .def(
          "clear",
          [](BorrowedCanvas& self, py::object ink) {
            self.get().clear(color(ink));
          },
          py::arg("ink"))
      .def(
          "translate",
          [](BorrowedCanvas& self, float x, float y) {
            self.get().translate(x, y);
          },
          py::arg("x"), py::arg("y"))
      .def(
          "scale",
          [](BorrowedCanvas& self, float x, float y) {
            self.get().scale(x, y);
          },
          py::arg("x"), py::arg("y"))
      .def(
          "rotate",
          [](BorrowedCanvas& self, float degrees) {
            self.get().rotate(degrees);
          },
          py::arg("degrees"))
      .def("resetMatrix",
           [](BorrowedCanvas& self) { self.get().resetMatrix(); })
      .def(
          "clipRect",
          [](BorrowedCanvas& self, py::object box, bool invert) {
            self.get().clipRect(
                rect(box),
                invert ? SkClipOp::kDifference : SkClipOp::kIntersect, true);
          },
          py::arg("rect"), py::arg("invert") = false)
      .def(
          "clipPath",
          [](BorrowedCanvas& self, const SkPath& path, bool invert) {
            self.get().clipPath(
                path, invert ? SkClipOp::kDifference : SkClipOp::kIntersect,
                true);
          },
          py::arg("path"), py::arg("invert") = false)
      .def(
          "drawPath",
          [](BorrowedCanvas& self, const SkPath& path, const SkPaint& paint) {
            self.get().drawPath(path, paint);
          },
          py::arg("path"), py::arg("paint"))
      .def(
          "drawRect",
          [](BorrowedCanvas& self, py::object box, const SkPaint& paint) {
            self.get().drawRect(rect(box), paint);
          },
          py::arg("box"), py::arg("paint"))
      .def(
          "drawCircle",
          [](BorrowedCanvas& self, float x, float y, float radius,
             const SkPaint& paint) {
            self.get().drawCircle(x, y, radius, paint);
          },
          py::arg("x"), py::arg("y"), py::arg("radius"), py::arg("paint"))
      .def(
          "drawPoints",
          [](BorrowedCanvas& self, SkCanvas::PointMode mode, py::object values,
             const SkPaint& paint) {
            (void)self.get();
            const auto points = pointBatch(values);
            // Conversion can run Python iteration or buffer callbacks that
            // close this frame, so reacquire its checked canvas afterward.
            self.get().drawPoints(
                mode, SkSpan<const SkPoint>{points.data(), points.size()},
                paint);
          },
          py::arg("mode"), py::arg("points"), py::arg("paint"))
      .def(
          "drawVertices",
          [](BorrowedCanvas& self, const sk_sp<SkVertices>& vertices,
             SkBlendMode blend, const SkPaint& paint) {
            self.get().drawVertices(vertices, blend, paint);
          },
          py::arg("vertices"), py::arg("blend"), py::arg("paint"));
}

void bindGraphics(py::module_& module) {
  py::class_<Graphics, std::shared_ptr<Graphics>>(module, "Graphics")
      .def(py::init<float, float>(), py::arg("width"), py::arg("height"))
      .def("begin", &Graphics::begin, py::arg("host"))
      .def("end", &Graphics::end)
      .def(
          "draw",
          [](Graphics& self, const std::shared_ptr<BorrowedPen>& host,
             py::function function) {
            auto borrowed = self.begin(host);
            const SkScopeExit close([&] { self.end(); });
            try {
              function(borrowed);
            } catch (const py::error_already_set& error) {
              throw std::runtime_error(error.what());
            }
          },
          py::arg("host"), py::arg("program"))
      .def(
          "resize",
          [](Graphics& self, float width, float height) {
            self.get().resize(width, height);
          },
          py::arg("width"), py::arg("height"))
      .def(
          "setDensityFloor",
          [](Graphics& self, float density) {
            self.get().setDensityFloor(density);
          },
          py::arg("density"))
      .def("image", [](Graphics& self) { return self.get().image(); })
      .def("width", [](Graphics& self) { return self.get().width(); })
      .def("height", [](Graphics& self) { return self.get().height(); })
      .def("extent", [](Graphics& self) {
        const auto extent = self.get().extent();
        return py::make_tuple(extent.width(), extent.height());
      });
}

}  // namespace

template <typename R, typename... Args, typename... Extra>
void penMethod(PenClass& cls, const char* name, R (Pen::*method)(Args...),
               Extra&&... extra) {
  cls.def(
      name,
      [method](BorrowedPen& pen, Args... args) -> R {
        return (pen.get().*method)(std::forward<Args>(args)...);
      },
      std::forward<Extra>(extra)...);
}

template <typename R, typename... Args, typename... Extra>
void penMethod(PenClass& cls, const char* name, R (Pen::*method)(Args...) const,
               Extra&&... extra) {
  cls.def(
      name,
      [method](BorrowedPen& pen, Args... args) -> R {
        return (pen.get().*method)(std::forward<Args>(args)...);
      },
      std::forward<Extra>(extra)...);
}

template <typename T>
void penProperty(PenClass& cls, const char* name, T Pen::* member) {
  cls.def_property_readonly(
      name, [member](BorrowedPen& pen) { return pen.get().*member; });
}

SkColor4f penColor(Pen& pen, const py::args& args) {
  if (args.size() == 1 && !py::isinstance<py::float_>(args[0]) &&
      !py::isinstance<py::int_>(args[0]))
    return color(args[0]);
  switch (args.size()) {
    case 1:
      return pen.color(py::cast<float>(args[0]));
    case 2:
      return pen.color(py::cast<float>(args[0]), py::cast<float>(args[1]));
    case 3:
      return pen.color(py::cast<float>(args[0]), py::cast<float>(args[1]),
                       py::cast<float>(args[2]));
    case 4:
      return pen.color(py::cast<float>(args[0]), py::cast<float>(args[1]),
                       py::cast<float>(args[2]), py::cast<float>(args[3]));
    default:
      throw py::type_error(
          "A color takes a string, sequence, or one to four numbers.");
  }
}

void bindPen(py::module_& root) {
  auto module = root.def_submodule("draw");
  bindConstants(module);
  bindCanvas(module);
  PenClass cls(module, "Pen");
  penProperty(cls, "width", &Pen::width);
  penProperty(cls, "height", &Pen::height);
  penProperty(cls, "frameCount", &Pen::frameCount);
  penProperty(cls, "deltaTime", &Pen::deltaTime);
  penProperty(cls, "mouseX", &Pen::mouseX);
  penProperty(cls, "mouseY", &Pen::mouseY);
  penProperty(cls, "pmouseX", &Pen::pmouseX);
  penProperty(cls, "pmouseY", &Pen::pmouseY);
  penProperty(cls, "mouseIsPressed", &Pen::mouseIsPressed);
  penProperty(cls, "keyIsPressed", &Pen::keyIsPressed);
  penProperty(cls, "key", &Pen::key);
  penProperty(cls, "keyCode", &Pen::keyCode);
  cls.def("fill", [](BorrowedPen& borrowed, py::args args) {
    auto& pen = borrowed.get();
    if (args.size() == 1 && py::isinstance<material::Material>(args[0])) {
      pen.fill(py::cast<material::Material>(args[0]));
      return;
    }
    if (args.size() >= 1 && args.size() <= 2 &&
        py::isinstance<material::skia::Paint>(args[0])) {
      const auto paint = py::cast<material::skia::Paint>(args[0]);
      if (args.size() == 2)
        pen.fill(paint, py::cast<draw::Constant>(args[1]));
      else
        pen.fill(paint);
      return;
    }
    pen.fill(penColor(pen, args));
  });
  cls.def("stroke", [](BorrowedPen& borrowed, py::args args) {
    auto& pen = borrowed.get();
    if (args.size() == 1 && py::isinstance<material::Material>(args[0])) {
      pen.stroke(py::cast<material::Material>(args[0]));
      return;
    }
    if (args.size() >= 1 && args.size() <= 2 &&
        py::isinstance<material::skia::Paint>(args[0])) {
      const auto paint = py::cast<material::skia::Paint>(args[0]);
      if (args.size() == 2)
        pen.stroke(paint, py::cast<draw::Constant>(args[1]));
      else
        pen.stroke(paint);
      return;
    }
    pen.stroke(penColor(pen, args));
  });
  cls.def("background", [](BorrowedPen& borrowed, py::args args) {
    auto& pen = borrowed.get();
    if (args.size() == 1 && py::isinstance<material::Material>(args[0])) {
      pen.background(py::cast<material::Material>(args[0]));
      return;
    }
    if (args.size() == 1 && py::isinstance<material::skia::Paint>(args[0])) {
      pen.background(py::cast<material::skia::Paint>(args[0]));
      return;
    }
    pen.background(penColor(pen, args));
  });
  penMethod(cls, "clear", &Pen::clear);
  penMethod(cls, "noFill", &Pen::noFill);
  penMethod(cls, "noStroke", &Pen::noStroke);
  penMethod(cls, "strokeWeight", &Pen::strokeWeight, py::arg("weight"));
  penMethod(cls, "strokeCap", &Pen::strokeCap, py::arg("cap"));
  penMethod(cls, "strokeJoin", &Pen::strokeJoin, py::arg("join"));
  penMethod(cls, "smooth", &Pen::smooth);
  penMethod(cls, "noSmooth", &Pen::noSmooth);
  penMethod(cls, "blendMode", &Pen::blendMode, py::arg("mode"));
  penMethod(cls, "rectMode", &Pen::rectMode, py::arg("mode"));
  penMethod(cls, "ellipseMode", &Pen::ellipseMode, py::arg("mode"));
  penMethod(cls, "angleMode",
            py::overload_cast<draw::Constant>(&Pen::angleMode),
            py::arg("mode"));
  penMethod(cls, "colorMode",
            py::overload_cast<draw::Constant>(&Pen::colorMode),
            py::arg("mode"));
  penMethod(cls, "colorMode",
            py::overload_cast<draw::Constant, float>(&Pen::colorMode),
            py::arg("mode"), py::arg("max"));
  penMethod(cls, "point", py::overload_cast<float, float>(&Pen::point),
            py::arg("x"), py::arg("y"));
  penMethod(cls, "line",
            py::overload_cast<float, float, float, float>(&Pen::line),
            py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"));
  penMethod(cls, "rect",
            py::overload_cast<float, float, float, float>(&Pen::rect),
            py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"));
  penMethod(cls, "rect",
            py::overload_cast<float, float, float, float, float>(&Pen::rect),
            py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
            py::arg("radius"));
  penMethod(cls, "square", py::overload_cast<float, float, float>(&Pen::square),
            py::arg("x"), py::arg("y"), py::arg("size"));
  penMethod(cls, "ellipse",
            py::overload_cast<float, float, float, float>(&Pen::ellipse),
            py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"));
  penMethod(cls, "ellipse",
            py::overload_cast<float, float, float>(&Pen::ellipse), py::arg("x"),
            py::arg("y"), py::arg("width"));
  penMethod(cls, "circle", py::overload_cast<float, float, float>(&Pen::circle),
            py::arg("x"), py::arg("y"), py::arg("diameter"));
  penMethod(cls, "arc", &Pen::arc, py::arg("x"), py::arg("y"), py::arg("width"),
            py::arg("height"), py::arg("start"), py::arg("stop"),
            py::arg("mode") = draw::OPEN);
  penMethod(cls, "triangle", &Pen::triangle, py::arg("x1"), py::arg("y1"),
            py::arg("x2"), py::arg("y2"), py::arg("x3"), py::arg("y3"));
  penMethod(cls, "quad", &Pen::quad, py::arg("x1"), py::arg("y1"),
            py::arg("x2"), py::arg("y2"), py::arg("x3"), py::arg("y3"),
            py::arg("x4"), py::arg("y4"));
  penMethod(cls, "bezier", &Pen::bezier, py::arg("x1"), py::arg("y1"),
            py::arg("x2"), py::arg("y2"), py::arg("x3"), py::arg("y3"),
            py::arg("x4"), py::arg("y4"));
  penMethod(cls, "beginShape", &Pen::beginShape,
            py::arg("kind") = draw::POLYGON);
  penMethod(cls, "vertex", &Pen::vertex, py::arg("x"), py::arg("y"));
  penMethod(cls, "curveVertex", &Pen::curveVertex, py::arg("x"), py::arg("y"));
  penMethod(cls, "bezierVertex", &Pen::bezierVertex, py::arg("x2"),
            py::arg("y2"), py::arg("x3"), py::arg("y3"), py::arg("x4"),
            py::arg("y4"));
  penMethod(cls, "quadraticVertex", &Pen::quadraticVertex, py::arg("cx"),
            py::arg("cy"), py::arg("x3"), py::arg("y3"));
  penMethod(cls, "beginContour", &Pen::beginContour);
  penMethod(cls, "endContour", &Pen::endContour);
  penMethod(cls, "endShape", &Pen::endShape, py::arg("mode") = draw::OPEN);
  penMethod(cls, "textSize", &Pen::textSize, py::arg("size"));
  penMethod(cls, "textFont",
            py::overload_cast<std::string_view>(&Pen::textFont),
            py::arg("family"));
  penMethod(cls, "textFont",
            py::overload_cast<std::string_view, float>(&Pen::textFont),
            py::arg("family"), py::arg("size"));
  penMethod(cls, "textAlign",
            py::overload_cast<draw::Constant>(&Pen::textAlign),
            py::arg("horizontal"));
  penMethod(cls, "textAlign",
            py::overload_cast<draw::Constant, draw::Constant>(&Pen::textAlign),
            py::arg("horizontal"), py::arg("vertical"));
  penMethod(cls, "textLeading", py::overload_cast<float>(&Pen::textLeading),
            py::arg("leading"));
  penMethod(cls, "textStyle", &Pen::textStyle, py::arg("style"));
  penMethod(cls, "text",
            py::overload_cast<std::string_view, float, float>(&Pen::text),
            py::arg("text"), py::arg("x"), py::arg("y"));
  penMethod(cls, "text",
            py::overload_cast<std::string_view, float, float, float, float>(
                &Pen::text),
            py::arg("text"), py::arg("x"), py::arg("y"), py::arg("width"),
            py::arg("height"));
  penMethod(cls, "textWidth", &Pen::textWidth, py::arg("text"));
  penMethod(cls, "textAscent", &Pen::textAscent);
  penMethod(cls, "textDescent", &Pen::textDescent);
  penMethod(cls, "translate", &Pen::translate, py::arg("x"), py::arg("y"));
  penMethod(cls, "rotate", &Pen::rotate, py::arg("angle"));
  penMethod(cls, "scale", py::overload_cast<float>(&Pen::scale),
            py::arg("factor"));
  penMethod(cls, "scale", py::overload_cast<float, float>(&Pen::scale),
            py::arg("x"), py::arg("y"));
  penMethod(cls, "shearX", &Pen::shearX, py::arg("angle"));
  penMethod(cls, "shearY", &Pen::shearY, py::arg("angle"));
  penMethod(cls, "push", &Pen::push);
  penMethod(cls, "pop", &Pen::pop);
  penMethod(cls, "resetMatrix", &Pen::resetMatrix);
  penMethod(cls, "random", py::overload_cast<>(&Pen::random));
  penMethod(cls, "random", py::overload_cast<float>(&Pen::random),
            py::arg("max"));
  penMethod(cls, "random", py::overload_cast<float, float>(&Pen::random),
            py::arg("min"), py::arg("max"));
  penMethod(cls, "randomSeed", &Pen::randomSeed, py::arg("seed"));
  penMethod(cls, "randomGaussian", &Pen::randomGaussian, py::arg("mean") = 0.0f,
            py::arg("sd") = 1.0f);
  penMethod(cls, "noise", &Pen::noise, py::arg("x"), py::arg("y") = 0.0f,
            py::arg("z") = 0.0f);
  penMethod(cls, "noiseSeed", &Pen::noiseSeed, py::arg("seed"));
  penMethod(cls, "noiseDetail", &Pen::noiseDetail, py::arg("octaves"),
            py::arg("falloff"));
  penMethod(cls, "millis", &Pen::millis);
  penMethod(cls, "frameRate", py::overload_cast<>(&Pen::frameRate, py::const_));
  penMethod(cls, "frameRate", py::overload_cast<double>(&Pen::frameRate),
            py::arg("fps"));
  penMethod(cls, "noLoop", &Pen::noLoop);
  penMethod(cls, "loop", &Pen::loop);
  penMethod(cls, "redraw", &Pen::redraw);
  penMethod(cls, "keyIsDown", &Pen::keyIsDown, py::arg("code"));
  cls.def("keysDown", [](BorrowedPen& self) {
    const auto keys = self.get().keysDown();
    return std::vector<int>{keys.begin(), keys.end()};
  });
  penMethod(cls, "contentScale", &Pen::contentScale);
  penMethod(cls, "isLooping", &Pen::isLooping);
  penMethod(cls, "targetFrameRate", &Pen::targetFrameRate);
  penMethod(cls, "angleMode", py::overload_cast<>(&Pen::angleMode, py::const_));
  penMethod(cls, "imageMode", &Pen::imageMode, py::arg("mode"));
  penMethod(
      cls, "colorMode",
      py::overload_cast<draw::Constant, float, float, float>(&Pen::colorMode),
      py::arg("mode"), py::arg("max1"), py::arg("max2"), py::arg("max3"));
  penMethod(cls, "colorMode",
            py::overload_cast<draw::Constant, float, float, float, float>(
                &Pen::colorMode),
            py::arg("mode"), py::arg("max1"), py::arg("max2"), py::arg("max3"),
            py::arg("maxA"));
  cls.def("color", [](BorrowedPen& self, py::args values) {
    return penColor(self.get(), values);
  });
  cls.def_static(
      "lerpColor",
      [](py::object a, py::object b, float amount) {
        return Pen::lerpColor(color(a), color(b), amount);
      },
      py::arg("a"), py::arg("b"), py::arg("amount"));
  cls.def(
      "strokeDash",
      [](BorrowedPen& self, const std::vector<float>& intervals, float phase) {
        self.get().strokeDash(std::span<const float>{intervals}, phase);
      },
      py::arg("intervals"), py::arg("phase") = 0.0f);
  penMethod(cls, "noDash", &Pen::noDash);
  cls.def(
      "point",
      [](BorrowedPen& self, py::object at) { self.get().point(point(at)); },
      py::arg("at"));
  cls.def(
      "line",
      [](BorrowedPen& self, py::object from, py::object to) {
        self.get().line(point(from), point(to));
      },
      py::arg("start"), py::arg("end"));
  cls.def(
      "circle",
      [](BorrowedPen& self, py::object at, float diameter) {
        self.get().circle(point(at), diameter);
      },
      py::arg("at"), py::arg("diameter"));
  penMethod(
      cls, "rect",
      py::overload_cast<float, float, float, float, float, float, float, float>(
          &Pen::rect),
      py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
      py::arg("topLeft"), py::arg("topRight"), py::arg("bottomRight"),
      py::arg("bottomLeft"));
  penMethod(cls, "square",
            py::overload_cast<float, float, float, float>(&Pen::square),
            py::arg("x"), py::arg("y"), py::arg("size"), py::arg("radius"));
  penMethod(cls, "square",
            py::overload_cast<float, float, float, float, float, float, float>(
                &Pen::square),
            py::arg("x"), py::arg("y"), py::arg("size"), py::arg("topLeft"),
            py::arg("topRight"), py::arg("bottomRight"), py::arg("bottomLeft"));
  penMethod(cls, "curve", &Pen::curve, py::arg("x1"), py::arg("y1"),
            py::arg("x2"), py::arg("y2"), py::arg("x3"), py::arg("y3"),
            py::arg("x4"), py::arg("y4"));
  penMethod(cls, "curveTightness", &Pen::curveTightness, py::arg("amount"));
  penMethod(cls, "applyMatrix", &Pen::applyMatrix, py::arg("a"), py::arg("b"),
            py::arg("c"), py::arg("d"), py::arg("e"), py::arg("f"));
  penMethod(cls, "textLeading",
            py::overload_cast<>(&Pen::textLeading, py::const_));
  penMethod(cls, "text", py::overload_cast<double, float, float>(&Pen::text),
            py::arg("value"), py::arg("x"), py::arg("y"));
  penMethod(cls, "textFont",
            py::overload_cast<const weave::Type&>(&Pen::textFont),
            py::arg("type"));
  penMethod(cls, "textFont",
            py::overload_cast<sk_sp<SkTypeface>>(&Pen::textFont),
            py::arg("face"));
  cls.def("inheritedInk",
          [](BorrowedPen& self) { return self.get().inheritedInk(); });
  cls.def("inheritedFont",
          [](BorrowedPen& self) { return self.get().inheritedFont(); });
  cls.def(
      "inherit",
      [](BorrowedPen& self, py::object ink, const weave::Type& font) {
        self.get().inherit(color(ink), font);
      },
      py::arg("ink"), py::arg("font"));
  cls.def(
      "shape",
      [](BorrowedPen& self, const SkPath& path) { self.get().shape(path); },
      py::arg("path"));
  cls.def(
      "shape",
      [](BorrowedPen& self, py::object silhouette, float x, float y,
         float width, float height) {
        struct Shape {
          py::object object;
          /** The silhouette concept asks a value to compare, and for a
           *  wrapped Python object the honest answer is whether it
           *  wraps the same object — never the object's own `__eq__`,
           *  which may run code and may say two different drawings are
           *  one. */
          bool operator==(const Shape& other) const {
            return object.is(other.object);
          }
          SkPath path(SkSize size) const {
            return object
                .attr("path")(py::make_tuple(size.width(), size.height()))
                .cast<SkPath>();
          }
        } shape{std::move(silhouette)};
        self.get().shape(shape, x, y, width, height);
      },
      py::arg("silhouette"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"));
  penMethod(cls, "vertices", &Pen::vertices, py::arg("mesh"));
  cls.def(
      "clip",
      [](BorrowedPen& self, py::function shape, bool invert) {
        std::exception_ptr failure;
        self.get().clip(
            [&] {
              try {
                shape();
              } catch (...) {
                failure = std::current_exception();
              }
            },
            {.invert = invert});
        if (failure) std::rethrow_exception(failure);
      },
      py::arg("shape"), py::arg("invert") = false);
  cls.def("canvas", [](const std::shared_ptr<BorrowedPen>& self) {
    (void)self->get();
    return BorrowedCanvas{self};
  });
  cls.def("fillPaint", [](BorrowedPen& self) -> std::optional<SkPaint> {
    if (const auto* value = self.get().fillPaint()) return *value;
    return {};
  });
  cls.def("strokePaint", [](BorrowedPen& self) -> std::optional<SkPaint> {
    if (const auto* value = self.get().strokePaint()) return *value;
    return {};
  });
  cls.def(
      "element",
      [](BorrowedPen& self, const compose::Element& element, py::object box,
         int index) {
        auto& native = self.get();
        compose::paintRetained(native, element, rect(box),
                               callerSlot(native, index));
      },
      py::arg("element"), py::arg("box"), py::arg("index") = 0);
  penMethod(cls, "image",
            py::overload_cast<const sk_sp<SkImage>&, float, float>(&Pen::image),
            py::arg("image"), py::arg("x"), py::arg("y"));
  penMethod(
      cls, "image",
      py::overload_cast<const sk_sp<SkImage>&, float, float, float, float>(
          &Pen::image),
      py::arg("image"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"));
  penMethod(cls, "image",
            py::overload_cast<const sk_sp<SkImage>&, float, float, float, float,
                              float, float, float, float>(&Pen::image),
            py::arg("image"), py::arg("x"), py::arg("y"), py::arg("width"),
            py::arg("height"), py::arg("sourceX"), py::arg("sourceY"),
            py::arg("sourceWidth"), py::arg("sourceHeight"));
  cls.def(
      "image",
      [](BorrowedPen& self, Graphics& image, float x, float y) {
        self.get().image(image.get(), x, y);
      },
      py::arg("image"), py::arg("x"), py::arg("y"));
  cls.def(
      "image",
      [](BorrowedPen& self, Graphics& image, float x, float y, float width,
         float height) { self.get().image(image.get(), x, y, width, height); },
      py::arg("image"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"));
  bindGraphics(module);

  py::class_<draw::NoiseField>(module, "NoiseField")
      .def(py::init<uint32_t>(), py::arg("seed") = 0)
      .def("seed", py::overload_cast<uint32_t>(&draw::NoiseField::seed),
           py::arg("seed"))
      .def("seed", py::overload_cast<>(&draw::NoiseField::seed, py::const_))
      .def("detail", &draw::NoiseField::detail, py::arg("octaves"),
           py::arg("falloff"))
      .def("at", &draw::NoiseField::at, py::arg("x"), py::arg("y") = 0.0f,
           py::arg("z") = 0.0f)
      .def("octaves", &draw::NoiseField::octaves)
      .def("falloff", &draw::NoiseField::falloff)
      .def_static("corner", &draw::NoiseField::corner, py::arg("seed"),
                  py::arg("x"), py::arg("y"), py::arg("z"));

  module.def("map", &draw::map, py::arg("value"), py::arg("start1"),
             py::arg("stop1"), py::arg("start2"), py::arg("stop2"),
             py::arg("withinBounds") = false);
  module.def("lerp", &draw::lerp, py::arg("start"), py::arg("stop"),
             py::arg("amount"));
  module.def("constrain", &draw::constrain, py::arg("value"), py::arg("low"),
             py::arg("high"));
  module.def("dist", &draw::dist, py::arg("x1"), py::arg("y1"), py::arg("x2"),
             py::arg("y2"));
  module.def("mag", &draw::mag, py::arg("x"), py::arg("y"));
  module.def("norm", &draw::norm, py::arg("value"), py::arg("start"),
             py::arg("stop"));
  module.def("sq", &draw::sq, py::arg("value"));
  module.def("radians", &draw::radians, py::arg("degrees"));
  module.def("degrees", &draw::degrees, py::arg("radians"));
  module.def(
      "lerpColor",
      [](py::object a, py::object b, float amount) {
        return Pen::lerpColor(color(a), color(b), amount);
      },
      py::arg("a"), py::arg("b"), py::arg("amount"));
  module.def(
      "on",
      [](BorrowedCanvas& canvas, std::pair<float, float> size,
         py::function function) {
        const SkAutoCanvasRestore restore(&canvas.get(), true);
        draw::on(canvas.get(), {size.first, size.second},
                 [&](Pen& pen) { invokePen(function, pen); });
      },
      py::arg("canvas"), py::arg("size"), py::arg("program"));
}

}  // namespace sigil::python
