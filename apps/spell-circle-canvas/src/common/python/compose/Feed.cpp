#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Feed.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>
#include <sigilweave/layout/StyleSheet.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace {
namespace feed = compose::feed;
using compose::Element;

/** A RING VALUE THAT IS PYTHON'S OWN: whatever the author appended,
 *  retained so that closing the host lifetime releases it while the ring
 *  itself may still be held. Two are equal when Python says the values
 *  are, asked under the interpreter lock; one whose lifetime has closed
 *  equals nothing but itself, so a comparison made from native code
 *  never raises for that reason. */
struct PythonRow {
  std::shared_ptr<PythonValue> value;

  bool operator==(const PythonRow& other) const {
    if (value == other.value) return true;
    if (!value || !other.value) return false;
    const py::gil_scoped_acquire lock;
    py::object left;
    py::object right;
    try {
      left = value->get();
      right = other.value->get();
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
};

using PythonRing = feed::Ring<PythonRow>;

/** ONE RING ROW AS PYTHON READS IT: the sequence id and the value under
 *  it, copied out, so nothing handed to Python aliases storage the next
 *  append may drop. The native row is a template over the value; Python
 *  has one class for every ring, and the value is whatever that ring
 *  holds. Equal when the ids are and Python says the values are. */
struct RowCopy {
  std::uint64_t sequence = 0;
  py::object value = py::none();

  bool operator==(const RowCopy& other) const {
    if (sequence != other.sequence) return false;
    const int equal =
        PyObject_RichCompareBool(value.ptr(), other.value.ptr(), Py_EQ);
    if (equal < 0) throw py::error_already_set();
    return equal != 0;
  }
};

/** A text row as a Python object of its own. */
py::object valueOf(const feed::TextRow& row) {
  return py::cast(row, py::return_value_policy::copy);
}

/** The Python value a row retains. Throws once the lifetime it was
 *  retained against has closed. */
py::object valueOf(const PythonRow& row) {
  if (!row.value) return py::none();
  return row.value->get();
}

template <class Value>
RowCopy copied(const feed::Row<Value>& row) {
  return {row.sequence, valueOf(row.value)};
}

/** Every row of @p ring, oldest first. */
template <class Value>
std::vector<RowCopy> copiedRows(const feed::Ring<Value>& ring) {
  std::vector<RowCopy> rows;
  rows.reserve(ring.size());
  for (const feed::Row<Value>& row : ring.rows()) rows.push_back(copied(row));
  return rows;
}

/** The row at @p index counted from the oldest, or from the newest when
 *  it is negative, as a Python sequence is indexed. */
template <class Value>
RowCopy rowAt(const feed::Ring<Value>& ring, py::ssize_t index) {
  const auto count = static_cast<py::ssize_t>(ring.size());
  if (index < 0) index += count;
  if (index < 0 || index >= count)
    throw py::index_error("A ring is indexed by the rows it holds.");
  return copied(ring.rows()[static_cast<std::size_t>(index)]);
}

/** Registers the ring over @p Value as @p name. @p append is the one
 *  verb that differs between rings, because it is where a Python value
 *  becomes the value the ring holds. The sequence protocol reads the
 *  same copies `rows` answers, so iterating a ring while appending to it
 *  walks the rows that stood when the iteration began. */
template <class Value, class Append>
void bindRing(py::module_& module, const char* name, Append append) {
  using Ring = feed::Ring<Value>;
  py::class_<Ring> ring(module, name);
  ring.def(py::init<std::size_t>(), py::arg("capacity") = 512)
      .def("append", append, py::arg("value"))
      .def("clear", &Ring::clear)
      .def("rows", [](const Ring& self) { return copiedRows(self); })
      .def("size", &Ring::size)
      .def("empty", &Ring::empty)
      .def("capacity", &Ring::capacity)
      .def("nextSequence", &Ring::nextSequence)
      .def("copy", [](const Ring& self) { return self; })
      .def("__len__", &Ring::size)
      .def(
          "__getitem__",
          [](const Ring& self, py::ssize_t index) { return rowAt(self, index); },
          py::arg("index"))
      .def("__iter__", [](const Ring& self) {
        return py::iter(py::cast(copiedRows(self)));
      });
  copyProtocol(ring);
}

/** What @p row answers for @p value, which has to be an element. The
 *  call happens on the thread and under the interpreter lock of the
 *  Python call that asked for the feed, and native scopes it opened and
 *  left open are closed before it returns. What it raises travels out
 *  through the native column as the Python error it is. */
Element rowElement(const py::function& row, py::object value) {
  const CallbackBoundary boundary;
  const py::object built = row(std::move(value));
  if (!py::isinstance<Element>(built))
    throw py::type_error("A feed row function returns an Element.");
  return built.cast<Element>();
}

/** The feed of @p ring with every visible row built by the Python
 *  function @p row. The function is called once per visible row before
 *  this returns and is not kept. */
template <class Value>
Element feedOf(const feed::Ring<Value>& ring, const feed::Options& options,
               const py::function& row) {
  return feed::feed(ring, options, [&row](const Value& value) {
    return rowElement(row, valueOf(value));
  });
}

void bindRecords(py::module_& module) {
  auto options =
      bindRecord<feed::Options>(module, "Options", "Unknown Options field: ");
  options.def_readwrite("visible", &feed::Options::visible)
      .def_readwrite("gap", &feed::Options::gap)
      .def(py::self == py::self);
  // The entrance is written in the schedule vocabulary, which another
  // file registers ahead of this one. A property's signature is written
  // when it is registered, so the field is offered once that value has a
  // Python name to be read back under.
  if (py::detail::get_type_info(typeid(motion::Spread)))
    options.def_readwrite("entrance", &feed::Options::entrance);

  auto textRow =
      bindRecord<feed::TextRow>(module, "TextRow", "Unknown TextRow field: ");
  // The line is held as the bytes the shaping vocabulary takes; Python
  // reads and writes it as a string.
  textRow
      .def_property(
          "text",
          [](const feed::TextRow& row) {
            const std::u8string& bytes = row.text.bytes();
            return std::string(bytes.begin(), bytes.end());
          },
          [](feed::TextRow& row, const std::string& text) { row.text = text; })
      .def_readwrite("style", &feed::TextRow::style)
      .def(py::self == py::self);

  // The window and the styles are read as references into the record, so
  // `options.window.gap = 0` and `options.styles.set(...)` write through.
  auto textOptions = bindRecord<feed::TextOptions>(
      module, "TextOptions", "Unknown TextOptions field: ");
  textOptions.def_readwrite("window", &feed::TextOptions::window)
      .def_readwrite("styles", &feed::TextOptions::styles)
      .def(py::self == py::self);

  auto row = bindRecord<RowCopy>(module, "Row", "Unknown Row field: ");
  row.def_readwrite("sequence", &RowCopy::sequence)
      .def_readwrite("value", &RowCopy::value)
      .def(py::self == py::self);
}

void bindRings(py::module_& module) {
  bindRing<feed::TextRow>(module, "TextRing",
                          [](feed::TextRing& self, feed::TextRow value) {
                            return self.append(std::move(value));
                          });
  // The ring of Python's own values: a row is whatever was appended, and
  // a row function is handed it back.
  bindRing<PythonRow>(module, "Ring", [](PythonRing& self, py::object value) {
    return self.append(PythonRow{retainValue(std::move(value))});
  });
}

void bindColumns(py::module_& module) {
  module.def("rowKey", &feed::rowKey, py::arg("sequence"));
  module.def("textRow", &feed::textRow, py::arg("row"), py::arg("styles"));
  module.def(
      "feed",
      [](const feed::TextRing& ring, const feed::TextOptions& options) {
        return feed::feed(ring, options);
      },
      py::arg("ring"), py::arg("options"));
  // Styled by whichever native scope provides the text options on this
  // thread, and by default-constructed options where none does.
  module.def(
      "feed", [](const feed::TextRing& ring) { return feed::feed(ring); },
      py::arg("ring"));
  module.def(
      "feed",
      [](const feed::TextRing& ring, const feed::Options& options,
         const py::function& row) { return feedOf(ring, options, row); },
      py::arg("ring"), py::arg("options"), py::arg("row"));
  module.def(
      "feed",
      [](const PythonRing& ring, const feed::Options& options,
         const py::function& row) { return feedOf(ring, options, row); },
      py::arg("ring"), py::arg("options"), py::arg("row"));
}
}  // namespace

void bindComposeFeed(py::module_& root) {
  auto module = submodule(root, "compose.feed");
  bindRecords(module);
  bindRings(module);
  bindColumns(module);
}

}  // namespace sigil::python
