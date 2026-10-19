#include <pybind11/stl.h>
#include <sigilcompose/kit/Rows.h>
#include <sigilpython/compose/Kit.h>
#include <sigilpython/compose/Registration.h>

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace composeKit = compose::kit;
using kit::field;
using kit::record;

namespace {

/** A table as Python holds it: the native record and the runs its row
 *  patches and keys stand in, so the record's views into them stay valid
 *  however the value is copied. */
struct HeldTable : composeKit::Table {
  std::vector<compose::Fill> swatchRun;
  std::vector<std::string> keyRun;

  HeldTable() = default;
  explicit HeldTable(const composeKit::Table& table)
      : composeKit::Table(table),
        swatchRun(table.swatches.begin(), table.swatches.end()),
        keyRun(table.keys.begin(), table.keys.end()) {
    point();
  }
  HeldTable(const HeldTable& other)
      : composeKit::Table(other),
        swatchRun(other.swatchRun),
        keyRun(other.keyRun) {
    point();
  }
  HeldTable& operator=(const HeldTable& other) {
    composeKit::Table::operator=(other);
    swatchRun = other.swatchRun;
    keyRun = other.keyRun;
    point();
    return *this;
  }
  void point() {
    swatches = swatchRun;
    keys = keyRun;
  }
};

/** Bars as Python holds them: the native record and the run of row inks
 *  it views. */
struct HeldBars : composeKit::Bars {
  std::vector<material::Color> inkRun;

  HeldBars() = default;
  explicit HeldBars(const composeKit::Bars& bars)
      : composeKit::Bars(bars), inkRun(bars.inks.begin(), bars.inks.end()) {
    point();
  }
  HeldBars(const HeldBars& other)
      : composeKit::Bars(other), inkRun(other.inkRun) {
    point();
  }
  HeldBars& operator=(const HeldBars& other) {
    composeKit::Bars::operator=(other);
    inkRun = other.inkRun;
    point();
    return *this;
  }
  void point() { inks = inkRun; }
};

/** @p member of the native record, as a member of the record Python
 *  holds in its place. */
template <class Held, class Member, class Native>
Member Held::* heldMember(Member Native::* member) {
  return static_cast<Member Held::*>(member);
}

/** What Python is handed for one value a part is offered. */
template <class Value>
py::object offered(const Value& value) {
  if constexpr (std::is_same_v<Value, compose::Utf8>) {
    const auto& bytes = value.bytes();
    return py::str(std::string(bytes.begin(), bytes.end()));
  } else if constexpr (std::is_same_v<Value, std::span<const compose::Utf8>>) {
    py::list words;
    for (const compose::Utf8& each : value) words.append(offered(each));
    return words;
  } else if constexpr (std::is_same_v<Value, composeKit::Table>) {
    return py::cast(HeldTable(value));
  } else if constexpr (std::is_same_v<Value, composeKit::Bars>) {
    return py::cast(HeldBars(value));
  } else {
    return py::cast(value);
  }
}

/** How a value a part is offered is read back from Python and held while
 *  a native part is called with it. */
template <class Value>
struct Taken {
  using Held = Value;
  static Held read(py::handle value) { return kit::converted<Value>(value); }
  static const Value& view(const Held& held) { return held; }
};
template <>
struct Taken<std::span<const compose::Utf8>> {
  using Held = std::vector<compose::Utf8>;
  static Held read(py::handle value) { return kit::converted<Held>(value); }
  static std::span<const compose::Utf8> view(const Held& held) {
    return held;
  }
};
template <>
struct Taken<composeKit::Table> {
  using Held = HeldTable;
  static Held read(py::handle value) { return py::cast<HeldTable>(value); }
  static const composeKit::Table& view(const Held& held) { return held; }
};
template <>
struct Taken<composeKit::Bars> {
  using Held = HeldBars;
  static Held read(py::handle value) { return py::cast<HeldBars>(value); }
  static const composeKit::Bars& view(const Held& held) { return held; }
};

/** Registers @p member, a part, as the property @p name on @p type: read
 *  as a function taking every value the part is offered, or None where
 *  the part is empty, and written as a Python callable taking as many of
 *  those values, in order, as it names, or None for the default. */
template <class Record, class Native, class... Offered>
void partField(py::class_<Record>& type, const char* name,
               composeKit::Part<Offered...> Native::* member) {
  type.def_property(
      name,
      [member](const Record& self) -> py::object {
        const composeKit::Part<Offered...> draw = self.*member;
        if (!draw) return py::none();
        return py::cpp_function([draw](py::args arguments) {
          if (py::len(arguments) != sizeof...(Offered))
            throw py::type_error("A kit part is called with every value it "
                                 "is offered.");
          return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            std::tuple<typename Taken<Offered>::Held...> held{
                Taken<Offered>::read(arguments[Index])...};
            return draw(Taken<Offered>::view(std::get<Index>(held))...);
          }(std::index_sequence_for<Offered...>{});
        });
      },
      [member](Record& self, py::object callable) {
        if (callable.is_none()) {
          self.*member = {};
          return;
        }
        if (!PyCallable_Check(callable.ptr()))
          throw py::type_error("A kit part must be callable or None.");
        const int arity = py::module_::import("sigil._callbacks")
                              .attr("arity")(callable, sizeof...(Offered))
                              .template cast<int>();
        auto held =
            retainCallback(py::reinterpret_borrow<py::function>(callable));
        self.*member = [held, arity](const Offered&... values) {
          const py::gil_scoped_acquire lock;
          const CallbackBoundary scopes;
          try {
            const py::object all[] = {offered(values)...};
            py::tuple taken(arity);
            for (int index = 0; index < arity; ++index)
              taken[index] = all[index];
            return node(held->get()(*taken));
          } catch (const py::error_already_set& error) {
            throw std::runtime_error(error.what());
          }
        };
      });
}

/** The cells of @p rows as owned words: a run of rows, each a run of
 *  words, or one row-major run of words. */
std::vector<std::vector<compose::Utf8>> rowsOf(py::handle rows,
                                               bool& rowMajor) {
  std::vector<std::vector<compose::Utf8>> cells;
  rowMajor = true;
  for (py::handle row : py::iter(rows))
    if (!py::isinstance<py::str>(row)) rowMajor = false;
  if (rowMajor) {
    cells.push_back(kit::converted<std::vector<compose::Utf8>>(rows));
    return cells;
  }
  for (py::handle row : py::iter(rows))
    cells.push_back(kit::converted<std::vector<compose::Utf8>>(row));
  return cells;
}

}  // namespace

void bindComposeKitRows(py::module_& root) {
  auto module = root.attr("compose").attr("kit").cast<py::module_>();
  auto column = record<composeKit::Column>(module, "Column");
  field(column, "head", &composeKit::Column::head);
  field(column, "width", &composeKit::Column::width);
  field(column, "figure", &composeKit::Column::figure);

  auto reading = record<composeKit::Reading>(module, "Reading");
  reading.doc() =
      "One reading: what was measured, what it came to, and what that "
      "means where the figure alone would not say, with a swatch before "
      "the name for a row that is also a key.";
  field(reading, "name", &composeKit::Reading::name);
  field(reading, "value", &composeKit::Reading::value);
  field(reading, "note", &composeKit::Reading::note);
  field(reading, "swatch", &composeKit::Reading::swatch);
  field(reading, "ink", &composeKit::Reading::ink);
  auto rows = record<composeKit::Rows>(module, "Rows");
  rows.doc() =
      "How a row is set: the measure it ranges across, the name column's "
      "width, the air between rows and between a name and its figure, the "
      "divider and the swatch. No type: the lines are set by the sheet in "
      "force.";
  field(rows, "measure", &composeKit::Rows::measure);
  field(rows, "nameMeasure", &composeKit::Rows::nameMeasure);
  field(rows, "gap", &composeKit::Rows::gap);
  field(rows, "labelGap", &composeKit::Rows::labelGap);
  field(rows, "divider", &composeKit::Rows::divider);
  field(rows, "dividerWidth", &composeKit::Rows::dividerWidth);
  field(rows, "swatchSide", &composeKit::Rows::swatchSide);
  field(rows, "swatchCorners", &composeKit::Rows::swatchCorners);
  module.def("reading", &composeKit::reading, py::arg("one"),
             py::arg("how") = composeKit::Rows{});
  module.def(
      "readout",
      [](const std::vector<composeKit::Reading>& readings,
         const composeKit::Rows& how) {
        return composeKit::readout(readings, how);
      },
      py::arg("rows"), py::arg("how") = composeKit::Rows{});

  py::class_<HeldTable> table(module, "Table");
  table.doc() =
      "How a table is set: its columns, the air between columns and rows, "
      "the rules, a patch and a key per row, and the head and body cells "
      "as parts.";
  table
      .def(py::init([](py::kwargs fields) {
        return keywordValue<HeldTable>(fields, "Unknown kit property: ");
      }))
      .def("copy", [](const HeldTable& value) { return value; });
  copyProtocol(table);
  field(table, "columns",
        heldMember<HeldTable>(&composeKit::Table::columns));
  field(table, "gap",
        heldMember<HeldTable>(&composeKit::Table::gap));
  field(table, "rowGap",
        heldMember<HeldTable>(&composeKit::Table::rowGap));
  field(table, "divider",
        heldMember<HeldTable>(&composeKit::Table::divider));
  field(table, "dividerWidth",
        heldMember<HeldTable>(&composeKit::Table::dividerWidth));
  field(table, "headRuled",
        heldMember<HeldTable>(&composeKit::Table::headRuled));
  table.def_property(
      "swatches",
      [](const HeldTable& self) { return self.swatchRun; },
      [](HeldTable& self, py::handle value) {
        self.swatchRun = kit::converted<std::vector<compose::Fill>>(value);
        self.point();
      },
      "One patch per row, in row order, before that row's first column.");
  field(table, "swatchSide",
        heldMember<HeldTable>(&composeKit::Table::swatchSide));
  field(table, "swatchCorners",
        heldMember<HeldTable>(&composeKit::Table::swatchCorners));
  table.def_property(
      "keys", [](const HeldTable& self) { return self.keyRun; },
      [](HeldTable& self, std::vector<std::string> value) {
        self.keyRun = std::move(value);
        self.point();
      },
      "One key per row, in row order; an empty name keys nothing.");
  partField(table, "headLine", &composeKit::Table::headLine);
  partField(table, "cellLine", &composeKit::Table::cellLine);
  module.def(
      "table",
      [](py::handle rows, const HeldTable& how) {
        bool rowMajor = false;
        const auto cells = rowsOf(rows, rowMajor);
        if (rowMajor)
          return composeKit::table(std::span<const compose::Utf8>(cells[0]),
                                   how);
        std::vector<std::span<const compose::Utf8>> views(cells.begin(),
                                                          cells.end());
        return composeKit::table(
            std::span<const std::span<const compose::Utf8>>(views), how);
      },
      py::arg("rows"), py::arg("how"),
      "The table: rows in the columns `how` states, each row its own run "
      "of cells, or one row-major run of words, as many to a row as there "
      "are columns.");

  py::class_<HeldBars> bars(module, "Bars");
  bars.doc() =
      "How a column of bars is drawn: the label at the left, the bar "
      "against the largest value, and the figure after it.";
  bars.def(py::init([](py::kwargs fields) {
        return keywordValue<HeldBars>(fields, "Unknown kit property: ");
      }))
      .def("copy", [](const HeldBars& value) { return value; });
  copyProtocol(bars);
  field(bars, "length",
        heldMember<HeldBars>(&composeKit::Bars::length));
  field(bars, "largest",
        heldMember<HeldBars>(&composeKit::Bars::largest));
  field(bars, "labelMeasure",
        heldMember<HeldBars>(&composeKit::Bars::labelMeasure));
  field(bars, "barHeight",
        heldMember<HeldBars>(&composeKit::Bars::barHeight));
  field(bars, "gap",
        heldMember<HeldBars>(&composeKit::Bars::gap));
  field(bars, "rowGap",
        heldMember<HeldBars>(&composeKit::Bars::rowGap));
  field(bars, "bar",
        heldMember<HeldBars>(&composeKit::Bars::bar));
  field(bars, "rest",
        heldMember<HeldBars>(&composeKit::Bars::rest));
  bars.def_property(
      "inks", [](const HeldBars& self) { return self.inkRun; },
      [](HeldBars& self, py::handle value) {
        self.inkRun = kit::converted<std::vector<material::Color>>(value);
        self.point();
      },
      "One ink per row, in the values' own order, over the bar and its two "
      "lines.");
  partField(bars, "labelLine", &composeKit::Bars::labelLine);
  partField(bars, "figureLine", &composeKit::Bars::figureLine);
  module.def(
      "bars",
      [](const std::vector<std::string>& labels,
         const std::vector<double>& values, const HeldBars& how) {
        std::vector<compose::Utf8> words(labels.begin(), labels.end());
        return composeKit::bars(words, values, how);
      },
      py::arg("labels"), py::arg("values"), py::arg("how") = HeldBars{},
      "The bars: one row per value, the labels read in the same order.");
}

}  // namespace sigil::python
