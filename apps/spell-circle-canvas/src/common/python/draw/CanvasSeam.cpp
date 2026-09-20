#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkSpan.h>
#include <include/core/SkVertices.h>
#include <sigildraw/Pen.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/draw/Canvas.h>
#include <sigilpython/draw/Registration.h>
#include <sigilpython/skia/Values.h>

#include <bit>
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

namespace {

/** The pen is asked for its canvas at every verb, so a pen that has left
 *  its drawing callback, or that another thread holds, refuses here, and
 *  a pen between frames has nothing open to lend. */
class PenCanvasSource final : public CanvasSource {
 public:
  explicit PenCanvasSource(std::shared_ptr<BorrowedPen> pen)
      : m_pen(std::move(pen)) {}

  SkCanvas& canvas() const override {
    SkCanvas* open = m_pen->get().canvas();
    if (!open) throw std::runtime_error("The pen has no open canvas.");
    return *open;
  }

 private:
  std::shared_ptr<BorrowedPen> m_pen;
};

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

}  // namespace

CanvasSource::~CanvasSource() = default;

BorrowedCanvas::BorrowedCanvas(std::shared_ptr<CanvasSource> source)
    : m_source(std::move(source)) {}

SkCanvas& BorrowedCanvas::get() const {
  if (!m_source) throw std::runtime_error("This canvas loan has ended.");
  return m_source->canvas();
}

void BorrowedCanvas::invalidate() { m_source.reset(); }

std::shared_ptr<CanvasSource> penCanvasSource(
    std::shared_ptr<BorrowedPen> pen) {
  if (!pen) throw py::type_error("A canvas is lent by a pen.");
  return std::make_shared<PenCanvasSource>(std::move(pen));
}

py::object borrowedCanvas(std::shared_ptr<CanvasSource> source) {
  return py::cast(BorrowedCanvas{std::move(source)});
}

SkCanvas& canvas(py::handle value) {
  if (!py::isinstance<BorrowedCanvas>(value))
    throw py::type_error("Drawing requires a canvas.");
  return py::cast<BorrowedCanvas&>(value).get();
}

void invalidateCanvas(py::handle value) {
  if (!py::isinstance<BorrowedCanvas>(value))
    throw py::type_error("Ending a loan requires a canvas.");
  py::cast<BorrowedCanvas&>(value).invalidate();
}

void bindDrawCanvasSeam(py::module_& root) {
  auto module = submodule(root, "draw");
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

}  // namespace sigil::python
