/** @file
 * The instanced leaf: the pool an author fills and the lanes it is filled
 * through, the flights it steps, the cell sheet its sprites are baked
 * into, the pick that answers which sprite stands under a point, and the
 * one element that stamps the pool.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSize.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Instances.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace {

namespace instancing = compose::instancing;
using compose::Element;
using instancing::CellSheet;
using instancing::Pool;
using Flight = Pool::Flight;
using PoolClass = py::class_<Pool, std::shared_ptr<Pool>>;

constexpr auto fluent = py::return_value_policy::reference_internal;

/** A size read from @p value: a size, or a width and a height. None
 *  loads as a null size that no conversion reports, so it is refused
 *  here in the words of the slot that asked. */
SkSize sizeFrom(py::handle value, const char* refusal) {
  if (value.is_none()) throw py::type_error(refusal);
  try {
    return py::cast<SkSize>(value);
  } catch (const py::cast_error&) {
    throw py::type_error(refusal);
  }
}

// ---------------------------------------------------------------------------
// Lanes

/** HOW ONE KIND OF LANE CROSSES INTO PYTHON: the class it is registered
 *  as, how many numbers stand for one item in a buffer, the type a single
 *  assignment is declared with, and the two readings between an item and
 *  its row of numbers. A rectangle's row is x, y, width and height, the
 *  way every rectangle Python passes is written, rather than the four
 *  edges Skia stores; a flight's row is its fields in declaration
 *  order. */
template <class Item>
struct LaneTraits;

template <>
struct LaneTraits<SkPoint> {
  using Scalar = float;
  using Input = py::handle;
  static constexpr const char* name = "PointLane";
  static constexpr const char* refusal =
      "A lane of points holds a Point, or an x and a y.";
  static constexpr py::ssize_t width = 2;
  static SkPoint read(py::handle value) { return point(value); }
  static void write(const SkPoint& item, float* row) {
    row[0] = item.fX;
    row[1] = item.fY;
  }
  static SkPoint item(const float* row) { return {row[0], row[1]}; }
};

template <>
struct LaneTraits<float> {
  using Scalar = float;
  using Input = float;
  static constexpr const char* name = "NumberLane";
  static constexpr const char* refusal = "A lane of numbers holds numbers.";
  static constexpr py::ssize_t width = 1;
  static float read(py::handle value) { return py::cast<float>(value); }
  static void write(const float& item, float* row) { row[0] = item; }
  static float item(const float* row) { return row[0]; }
};

template <>
struct LaneTraits<SkColor4f> {
  using Scalar = float;
  using Input = SkColor4f;
  static constexpr const char* name = "ColorLane";
  static constexpr const char* refusal =
      "A lane of colours holds a Color, a CSS string, or three or four "
      "channels.";
  static constexpr py::ssize_t width = 4;
  static SkColor4f read(py::handle value) { return color(value); }
  static void write(const SkColor4f& item, float* row) {
    row[0] = item.fR;
    row[1] = item.fG;
    row[2] = item.fB;
    row[3] = item.fA;
  }
  static SkColor4f item(const float* row) {
    return {row[0], row[1], row[2], row[3]};
  }
};

template <>
struct LaneTraits<int> {
  using Scalar = int;
  using Input = int;
  static constexpr const char* name = "FrameLane";
  static constexpr const char* refusal =
      "A lane of frames holds whole numbers.";
  static constexpr py::ssize_t width = 1;
  static int read(py::handle value) { return py::cast<int>(value); }
  static void write(const int& item, int* row) { row[0] = item; }
  static int item(const int* row) { return row[0]; }
};

template <>
struct LaneTraits<SkSize> {
  using Scalar = float;
  using Input = py::handle;
  static constexpr const char* name = "SizeLane";
  static constexpr const char* refusal =
      "A lane of sizes holds a Size, or a width and a height.";
  static constexpr py::ssize_t width = 2;
  static SkSize read(py::handle value) { return sizeFrom(value, refusal); }
  static void write(const SkSize& item, float* row) {
    row[0] = item.width();
    row[1] = item.height();
  }
  static SkSize item(const float* row) { return {row[0], row[1]}; }
};

template <>
struct LaneTraits<SkRect> {
  using Scalar = float;
  using Input = py::handle;
  static constexpr const char* name = "RectLane";
  static constexpr const char* refusal =
      "A lane of rectangles holds a Rect, or an x, a y, a width and a "
      "height.";
  static constexpr py::ssize_t width = 4;
  static SkRect read(py::handle value) { return rect(value); }
  static void write(const SkRect& item, float* row) {
    row[0] = item.x();
    row[1] = item.y();
    row[2] = item.width();
    row[3] = item.height();
  }
  static SkRect item(const float* row) {
    return SkRect::MakeXYWH(row[0], row[1], row[2], row[3]);
  }
};

template <>
struct LaneTraits<Flight> {
  using Scalar = float;
  using Input = Flight;
  static constexpr const char* name = "FlightLane";
  static constexpr const char* refusal = "A lane of flights holds Flights.";
  static constexpr py::ssize_t width = 12;
  static Flight read(py::handle value) { return py::cast<Flight>(value); }
  static void write(const Flight& item, float* row) {
    row[0] = item.from.fX;
    row[1] = item.from.fY;
    row[2] = item.to.fX;
    row[3] = item.to.fY;
    row[4] = item.rotateFrom;
    row[5] = item.rotateTo;
    row[6] = item.scaleFrom;
    row[7] = item.scaleTo;
    row[8] = item.alphaFrom;
    row[9] = item.alphaTo;
    row[10] = item.start;
    row[11] = item.duration;
  }
  static Flight item(const float* row) {
    return {.from = {row[0], row[1]},
            .to = {row[2], row[3]},
            .rotateFrom = row[4],
            .rotateTo = row[5],
            .scaleFrom = row[6],
            .scaleTo = row[7],
            .alphaFrom = row[8],
            .alphaTo = row[9],
            .start = row[10],
            .duration = row[11]};
  }
};

/** ONE LANE OF A POOL, as Python holds it. It keeps the pool and the
 *  accessor that reads the lane, and asks the pool again at every use: the
 *  native span is a view into storage the pool lets go of whenever its
 *  length changes, so nothing here keeps one past the call that took it.
 *  A lane held across an `add`, a `resize` or a `clear` therefore reads
 *  the pool as it then stands. */
template <class Item>
class Lane {
 public:
  using Access = std::span<Item> (*)(Pool&);

  Lane(std::shared_ptr<Pool> pool, Access access)
      : m_pool(std::move(pool)), m_access(access) {}

  /** The lane as it stands now. An opt-in lane is materialised by the
   *  asking, as the native accessor materialises it. */
  std::span<Item> items() const { return m_access(*m_pool); }

 private:
  std::shared_ptr<Pool> m_pool;
  Access m_access;
};

/** @p value as the item a lane of @p Item holds, refused in the lane's
 *  own words when it does not read as one. */
template <class Item>
Item laneItem(py::handle value) {
  try {
    return LaneTraits<Item>::read(value);
  } catch (const py::cast_error&) {
    throw py::type_error(LaneTraits<Item>::refusal);
  }
}

/** What a single assignment was handed, as the item it stands for. A
 *  lane whose item Python can name is declared with that type and is
 *  handed the item already read. */
template <class Item>
Item laneInput(const typename LaneTraits<Item>::Input& value) {
  if constexpr (std::is_same_v<typename LaneTraits<Item>::Input, py::handle>)
    return laneItem<Item>(value);
  else
    return value;
}

/** Where @p index falls in a lane of @p size items, counting from the end
 *  when it is negative. */
std::size_t laneIndex(py::ssize_t index, std::size_t size) {
  if (index < 0) index += static_cast<py::ssize_t>(size);
  if (index < 0 || static_cast<std::size_t>(index) >= size)
    throw py::index_error("Lane index out of range.");
  return static_cast<std::size_t>(index);
}

/** The items a slice names, as where it starts, how it steps and how many
 *  it reaches in a lane of @p size items. */
struct LaneSlice {
  py::ssize_t start = 0;
  py::ssize_t step = 1;
  py::ssize_t count = 0;
};

LaneSlice laneSlice(const py::slice& index, std::size_t size) {
  LaneSlice named;
  py::ssize_t stop = 0;
  if (!index.compute(static_cast<py::ssize_t>(size), &named.start, &stop,
                     &named.step, &named.count))
    throw py::error_already_set();
  return named;
}

/** Which numbers a buffer holds, read from its format and item size:
 *  `f` and `d` for floats of four and eight bytes, `i` and `q` for whole
 *  numbers of four and eight, and zero for anything else. A byte-order
 *  mark that names this machine's own order is read past. */
char scalarKind(const py::buffer_info& info) {
  std::string_view format = info.format;
  if (!format.empty() &&
      (format.front() == '@' || format.front() == '=' ||
       (format.front() == '<' && std::endian::native == std::endian::little) ||
       (format.front() == '>' && std::endian::native == std::endian::big)))
    format.remove_prefix(1);
  if (format.size() != 1) return 0;
  switch (format.front()) {
    case 'f':
      return info.itemsize == 4 ? 'f' : 0;
    case 'd':
      return info.itemsize == 8 ? 'd' : 0;
    case 'i':
    case 'l':
    case 'q':
      return info.itemsize == 4 ? 'i' : info.itemsize == 8 ? 'q' : 0;
    default:
      return 0;
  }
}

/** The number stored at @p address as @p Stored, copied out because a
 *  buffer promises nothing about how its items are aligned. */
template <class Stored>
Stored storedAt(const char* address) {
  Stored value;
  std::memcpy(&value, address, sizeof(Stored));
  return value;
}

/** The number at @p address, stored as @p kind names it, as the number a
 *  lane of @p Scalar holds. */
template <class Scalar>
Scalar scalarAt(const char* address, char kind) {
  switch (kind) {
    case 'f':
      return static_cast<Scalar>(storedAt<float>(address));
    case 'd':
      return static_cast<Scalar>(storedAt<double>(address));
    case 'i':
      return static_cast<Scalar>(storedAt<std::int32_t>(address));
    default:
      return static_cast<Scalar>(storedAt<std::int64_t>(address));
  }
}

/** The numbers @p info holds, a row after a row, for @p rows items of
 *  @p width numbers each. The buffer is one row per item, or the same
 *  numbers flat; its strides are followed, so a view over part of an
 *  array reads as the array it is a view of. */
template <class Scalar>
std::vector<Scalar> bufferScalars(const py::buffer_info& info, py::ssize_t rows,
                                  py::ssize_t width) {
  const char kind = scalarKind(info);
  const bool whole = kind == 'i' || kind == 'q';
  if (kind == 0 || whole != std::is_integral_v<Scalar>)
    throw py::type_error(
        std::is_integral_v<Scalar>
            ? "A lane of frames is assigned from a buffer of whole numbers "
              "of four or eight bytes."
            : "A lane of floats is assigned from a buffer of floats of four "
              "or eight bytes.");
  const bool flat = info.ndim == 1 && info.shape[0] == rows * width;
  const bool rowed =
      info.ndim == 2 && info.shape[0] == rows && info.shape[1] == width;
  if (!flat && !rowed)
    throw py::value_error("A slice of " + std::to_string(rows) +
                          " items is assigned from " + std::to_string(rows) +
                          " rows of " + std::to_string(width) +
                          " numbers, or from the same numbers flat.");
  std::vector<Scalar> numbers;
  numbers.reserve(static_cast<std::size_t>(rows * width));
  const char* base = static_cast<const char*>(info.ptr);
  for (py::ssize_t row = 0; row < rows; ++row)
    for (py::ssize_t column = 0; column < width; ++column) {
      const py::ssize_t offset =
          flat ? (row * width + column) * info.strides[0]
               : row * info.strides[0] + column * info.strides[1];
      numbers.push_back(scalarAt<Scalar>(base + offset, kind));
    }
  return numbers;
}

/** Writes @p values over the items @p index names. Everything Python
 *  hands over is read before the lane is asked for, because reading runs
 *  Python — an iterator, a number's own conversion — and Python may change
 *  the pool's length while it runs. A slice never changes that length, so
 *  the count has to match. */
template <class Item>
void assignSlice(const Lane<Item>& lane, const py::slice& index,
                 const py::object& values) {
  using Traits = LaneTraits<Item>;
  using Scalar = typename Traits::Scalar;
  if (PyObject_CheckBuffer(values.ptr())) {
    const py::buffer_info info =
        py::reinterpret_borrow<py::buffer>(values).request();
    const std::span<Item> items = lane.items();
    const LaneSlice named = laneSlice(index, items.size());
    const std::vector<Scalar> numbers =
        bufferScalars<Scalar>(info, named.count, Traits::width);
    for (py::ssize_t row = 0; row < named.count; ++row)
      items[static_cast<std::size_t>(named.start + row * named.step)] =
          Traits::item(numbers.data() + row * Traits::width);
    return;
  }
  std::vector<Item> read;
  for (const py::handle value : py::reinterpret_borrow<py::iterable>(values))
    read.push_back(laneItem<Item>(value));
  const std::span<Item> items = lane.items();
  const LaneSlice named = laneSlice(index, items.size());
  if (static_cast<py::ssize_t>(read.size()) != named.count)
    throw py::value_error("A slice of " + std::to_string(named.count) +
                          " items is assigned " + std::to_string(read.size()) +
                          "; a lane changes length only through its pool.");
  for (py::ssize_t row = 0; row < named.count; ++row)
    items[static_cast<std::size_t>(named.start + row * named.step)] =
        read[static_cast<std::size_t>(row)];
}

/** WHAT ONE BUFFER REQUEST IS HANDED: the lane as it stood when it was
 *  asked, with the shape and strides the view points at. It belongs to
 *  the view and is freed when the consumer releases it, so an array kept
 *  for a whole session never points into a pool. */
template <class Scalar>
struct LaneSnapshot {
  std::vector<Scalar> numbers;
  std::array<Py_ssize_t, 2> shape{};
  std::array<Py_ssize_t, 2> strides{};
};

/** The buffer protocol's reading slot for a lane of @p Item: a read-only
 *  copy, one row per item. A writable view is refused, because the only
 *  storage there is to write into is the copy. */
template <class Item>
int laneGetBuffer(PyObject* object, Py_buffer* view, int flags) {
  using Traits = LaneTraits<Item>;
  using Scalar = typename Traits::Scalar;
  static constexpr char kFloatFormat[] = "f";
  static constexpr char kWholeFormat[] = "i";
  if (view == nullptr) {
    PyErr_SetString(PyExc_BufferError, "A lane needs a view to fill.");
    return -1;
  }
  view->obj = nullptr;
  if ((flags & PyBUF_WRITABLE) == PyBUF_WRITABLE) {
    PyErr_SetString(PyExc_BufferError,
                    "A lane's buffer is a copy of the lane. Write through "
                    "the lane itself: lane[index] = value, or lane[:] = "
                    "values.");
    return -1;
  }
  std::unique_ptr<LaneSnapshot<Scalar>> snapshot;
  try {
    const auto& lane = py::cast<const Lane<Item>&>(py::handle(object));
    const std::span<Item> items = lane.items();
    snapshot = std::make_unique<LaneSnapshot<Scalar>>();
    // One spare number, so an empty lane still has an address to give.
    snapshot->numbers.resize(items.size() * Traits::width + 1);
    for (std::size_t index = 0; index < items.size(); ++index)
      Traits::write(items[index],
                    snapshot->numbers.data() + index * Traits::width);
    snapshot->shape = {static_cast<Py_ssize_t>(items.size()), Traits::width};
    snapshot->strides = {
        static_cast<Py_ssize_t>(Traits::width * sizeof(Scalar)),
        static_cast<Py_ssize_t>(sizeof(Scalar))};
  } catch (const std::exception& error) {
    PyErr_SetString(PyExc_BufferError, error.what());
    return -1;
  }
  const bool rowed = Traits::width > 1;
  view->buf = snapshot->numbers.data();
  view->len = snapshot->shape[0] * Traits::width *
              static_cast<Py_ssize_t>(sizeof(Scalar));
  view->itemsize = sizeof(Scalar);
  view->readonly = 1;
  view->ndim = 1;
  view->format = nullptr;
  view->shape = nullptr;
  view->strides = nullptr;
  view->suboffsets = nullptr;
  if ((flags & PyBUF_FORMAT) == PyBUF_FORMAT)
    view->format = const_cast<char*>(std::is_integral_v<Scalar> ? kWholeFormat
                                                                : kFloatFormat);
  if ((flags & PyBUF_ND) == PyBUF_ND) {
    view->ndim = rowed ? 2 : 1;
    view->shape = snapshot->shape.data();
  }
  if ((flags & PyBUF_STRIDES) == PyBUF_STRIDES)
    // A lane of single numbers has one axis, stepped a number at a time.
    view->strides =
        rowed ? snapshot->strides.data() : snapshot->strides.data() + 1;
  // Rows first is the only order there is, so a consumer that asks for
  // columns first is told so unless the two orders coincide.
  if ((flags & PyBUF_F_CONTIGUOUS) == PyBUF_F_CONTIGUOUS && rowed &&
      snapshot->shape[0] > 1) {
    PyErr_SetString(PyExc_BufferError,
                    "A lane's buffer is laid out a row after a row.");
    return -1;
  }
  view->internal = snapshot.release();
  view->obj = object;
  Py_INCREF(object);
  return 0;
}

/** The buffer protocol's release slot: the copy the view was handed goes
 *  with the view. */
template <class Item>
void laneReleaseBuffer(PyObject*, Py_buffer* view) {
  delete static_cast<LaneSnapshot<typename LaneTraits<Item>::Scalar>*>(
      view->internal);
  view->internal = nullptr;
}

/** Registers the lane of @p Item on @p module: a sequence of copies that
 *  is written by index and by slice, and a read-only buffer. */
template <class Item>
void bindLane(py::module_& module, const char* documentation) {
  using Traits = LaneTraits<Item>;
  py::class_<Lane<Item>>(module, Traits::name,
                         // pybind11's own buffer protocol frees nothing but its
                         // description of the buffer, and a lane's view owns
                         // the copy it reads, so the two slots are this file's.
                         py::custom_type_setup([](PyHeapTypeObject* type) {
                           type->ht_type.tp_as_buffer = &type->as_buffer;
                           type->as_buffer.bf_getbuffer = &laneGetBuffer<Item>;
                           type->as_buffer.bf_releasebuffer =
                               &laneReleaseBuffer<Item>;
                         }),
                         documentation)
      .def("__len__",
           [](const Lane<Item>& lane) { return lane.items().size(); })
      .def(
          "__getitem__",
          [](const Lane<Item>& lane, py::ssize_t index) -> Item {
            const std::span<Item> items = lane.items();
            return items[laneIndex(index, items.size())];
          },
          py::arg("index"), "One item, copied.")
      .def(
          "__getitem__",
          [](const Lane<Item>& lane, const py::slice& index) {
            const std::span<Item> items = lane.items();
            const LaneSlice named = laneSlice(index, items.size());
            std::vector<Item> copied;
            copied.reserve(static_cast<std::size_t>(named.count));
            for (py::ssize_t row = 0; row < named.count; ++row)
              copied.push_back(items[static_cast<std::size_t>(
                  named.start + row * named.step)]);
            return copied;
          },
          py::arg("index"), "The items a slice names, copied.")
      .def(
          "__iter__",
          [](const Lane<Item>& lane) {
            const std::span<Item> items = lane.items();
            return py::iter(
                py::cast(std::vector<Item>(items.begin(), items.end())));
          },
          "Iterates over a copy of the lane as it stands now.")
      .def(
          "__setitem__",
          [](const Lane<Item>& lane, py::ssize_t index,
             const typename Traits::Input& value) {
            // The item is read first: reading may run Python, and Python
            // may change the pool's length while it runs.
            const Item read = laneInput<Item>(value);
            const std::span<Item> items = lane.items();
            items[laneIndex(index, items.size())] = read;
          },
          py::arg("index"), py::arg("value"),
          "Writes one item. The pool publishes nothing until `commit()`.")
      .def(
          "__setitem__",
          [](const Lane<Item>& lane, const py::slice& index,
             const py::object& values) { assignSlice(lane, index, values); },
          py::arg("index"), py::arg("values"),
          "Writes the items a slice names from as many values: an iterable "
          "of items, or a buffer of numbers, one row per item or the same "
          "numbers flat. `lane[:] = array` is the bulk door. The pool "
          "publishes nothing until `commit()`.");
}

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
      .def_readwrite("rotateFrom", &Flight::rotateFrom)
      .def_readwrite("rotateTo", &Flight::rotateTo)
      .def_readwrite("scaleFrom", &Flight::scaleFrom)
      .def_readwrite("scaleTo", &Flight::scaleTo)
      .def_readwrite("alphaFrom", &Flight::alphaFrom)
      .def_readwrite("alphaTo", &Flight::alphaTo)
      .def_readwrite("start", &Flight::start)
      .def_readwrite("duration", &Flight::duration)
      .def(
          "__eq__",
          [](const Flight& value, const Flight& other) {
            return value == other;
          },
          py::is_operator(), py::arg("other"));
  // `from` is a Python keyword, so the field answers to `from_` as well:
  // the native name reaches it through keywords and `getattr`, and the
  // other one through attribute syntax.
  for (const char* name : {"from", "from_"})
    flight.def_property(
        name, [](const Flight& value) { return value.from; },
        [](Flight& value, py::handle position) {
          value.from = point(position);
        });
  flight.def_property(
      "to", [](const Flight& value) { return value.to; },
      [](Flight& value, py::handle position) { value.to = point(position); });
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
  bindLane<Flight>(
      module,
      "A pool's lane of flights, on the same terms as a lane of points; "
      "as a buffer it is one row of twelve floats per instance, a "
      "flight's fields in the order it declares them.");
  pool.def(py::init<>())
      .def(
          "add",
          [](Pool& self, py::handle position, int frame, float rotateRadians,
             float scale, SkColor4f tint) {
            return self.add(point(position), frame, rotateRadians, scale, tint);
          },
          py::arg("position"), py::arg("frame") = 0,
          py::arg("rotateRadians") = 0.0f, py::arg("scale") = 1.0f,
          py::arg("tint") = SkColors::kWhite,
          "Appends one instance and answers its index.")
      .def("clear", &Pool::clear,
           "Drops every instance, and every opt-in lane with them.")
      .def("resize", &Pool::resize, py::arg("count"),
           "Makes the pool `count` instances long. An appended instance "
           "stands at the origin, unrotated, at unit scale, untinted, on "
           "frame zero.")
      .def("size", &Pool::size)
      .def("__len__", &Pool::size)
      .def("hasTexWindows", &Pool::hasTexWindows)
      .def("hasSizes", &Pool::hasSizes)
      .def("hasAlphas", &Pool::hasAlphas)
      .def("hasFlights", &Pool::hasFlights)
      .def(
          "fly",
          [](Pool& self, float seconds, py::handle ease) {
            flyPool(self, seconds, ease);
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
      .def("revision", &Pool::revision);
  poolLane<SkPoint>(
      pool, "positions", +[](Pool& self) { return self.positions(); },
      "Where each instance's cell is centred.");
  poolLane<float>(
      pool, "rotations", +[](Pool& self) { return self.rotations(); },
      "Each instance's rotation about its centre, in radians.");
  poolLane<float>(
      pool, "scales", +[](Pool& self) { return self.scales(); },
      "Each instance's uniform scale.");
  poolLane<SkColor4f>(
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

// ---------------------------------------------------------------------------
// The cell sheet

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
    const CallbackBoundary boundary;
    const py::object answer = named == 0 ? make() : make(index);
    if (py::isinstance<Element>(answer)) {
      if (!shared)
        throw py::type_error(
            "A variant recipe given no shared size answers an Element "
            "and its size.");
      return {answer.cast<Element>(), *shared};
    }
    if ((py::isinstance<py::tuple>(answer) ||
         py::isinstance<py::list>(answer)) &&
        py::len(answer) == 2) {
      const auto pair = py::reinterpret_borrow<py::sequence>(answer);
      const py::object tree = pair[0];
      if (py::isinstance<Element>(tree))
        return {tree.cast<Element>(),
                sizeFrom(pair[1],
                         "A variant's size is a Size, or a width and a "
                         "height.")};
    }
    throw py::type_error(
        "A variant recipe answers an Element, or an Element and its "
        "size.");
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
      .def("filter", py::overload_cast<>(&CellSheet::filter, py::const_))
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
      .def("frameCount", &CellSheet::frameCount)
      .def("frameSize", &CellSheet::frameSize, py::arg("frame"),
           "The logical size of `frame`, or an empty size for a frame "
           "the sheet does not have.")
      .def("frameTex", &CellSheet::frameTex, py::arg("frame"),
           "Where `frame` was baked, in the sheet's own pixels; empty "
           "until the sheet has baked.")
      .def("oversample", &CellSheet::oversample)
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
  bindLane<SkPoint>(
      module,
      "A pool's lane of points. It reads and writes the pool it came from "
      "at every use, so it is never stale; as a buffer it is a read-only "
      "copy, one row of x and y per instance.");
  bindLane<float>(
      module,
      "A pool's lane of numbers, on the same terms as a lane of points; "
      "as a buffer it is one float per instance.");
  bindLane<SkColor4f>(
      module,
      "A pool's lane of colours, on the same terms as a lane of points; "
      "as a buffer it is one row of red, green, blue and alpha per "
      "instance.");
  bindLane<int>(
      module,
      "A pool's lane of frame indices, on the same terms as a lane of "
      "points; as a buffer it is one whole number of four bytes per "
      "instance.");
  bindLane<SkSize>(
      module,
      "A pool's lane of sizes, on the same terms as a lane of points; as "
      "a buffer it is one row of width and height per instance.");
  bindLane<SkRect>(
      module,
      "A pool's lane of rectangles, on the same terms as a lane of "
      "points; as a buffer it is one row of x, y, width and height per "
      "instance.");
  bindPool(module);
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
         instancing::Mode mode, SkBlendMode blend) {
        return instancing::instances(std::move(atlas), std::move(pool), mode,
                                     blend);
      },
      py::arg("atlas").none(false), py::arg("pool").none(false),
      py::arg("mode") = instancing::Mode::Data,
      py::arg("blend") = SkBlendMode::kSrcOver,
      "The single-draw stamping leaf. It FILLS ITS PARENT, so wrap it in a "
      "sized or positioned box and the pool's positions are that box's "
      "local pixels. `blend` is per sprite, which is what lets "
      "overlapping sprites accumulate; `Element.blend` on the leaf would "
      "flatten the whole field into one layer first. The element keeps the "
      "sheet and the pool it names.");
  module.def(
      "pick",
      [](const Pool& pool, const CellSheet& atlas, py::handle position) {
        return instancing::pick(pool, atlas, point(position));
      },
      py::arg("pool"), py::arg("atlas"), py::arg("point"),
      "The index of the topmost instance whose drawn quad contains "
      "`point`, in the same local pixels the pool's positions are in, or "
      "None. A hit test cannot see inside the leaf, so this is the only "
      "way to ask which sprite is under a point. A fully faded instance "
      "still picks.");
}

}  // namespace sigil::python
