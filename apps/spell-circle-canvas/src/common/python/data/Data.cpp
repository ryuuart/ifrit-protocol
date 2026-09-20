/** @file
 * Native data snapshots, table transformations, axes and SQL query views.
 */

#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigildata/decode/ArtNet.h>
#include <sigildata/decode/Csv.h>
#include <sigildata/decode/Decoders.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Midi.h>
#include <sigildata/decode/Osc.h>
#include <sigildata/decode/Schema.h>
#include <sigildata/query/Database.h>
#include <sigildata/scale/Scale.h>
#include <sigilio/hub/Hub.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/data/Convert.h>
#include <sigilpython/data/Registration.h>
#include <sigilpython/io/Hub.h>

#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_set>

namespace sigil::python {
namespace py = pybind11;
namespace {

struct DatabaseView {
  std::shared_ptr<const data::Database> owner;
  std::shared_ptr<data::Database> writer;

  data::Database& writable() const {
    if (!writer) throw std::runtime_error("This database view is read-only.");
    return *writer;
  }
};

DatabaseView ownedDatabase(data::Database value) {
  auto owner = std::make_shared<data::Database>(std::move(value));
  return {owner, owner};
}

struct Recursion {
  inline static thread_local int depth = 0;
  Recursion() {
    // Python's C-stack guard and its configurable recursion limit are
    // independent. Respect both while converting nested native values.
    if (depth >= Py_GetRecursionLimit()) {
      PyErr_SetString(
          PyExc_RecursionError,
          "Maximum recursion depth exceeded while converting a JSON value.");
      throw py::error_already_set();
    }
    if (Py_EnterRecursiveCall(" while converting a JSON value"))
      throw py::error_already_set();
    ++depth;
  }
  ~Recursion() {
    --depth;
    Py_LeaveRecursiveCall();
  }
};

data::Json json(py::handle value, std::unordered_set<PyObject*>& active) {
  const Recursion recursion;
  if (value.is_none()) return {};
  if (py::isinstance<data::Json>(value)) return py::cast<data::Json>(value);
  if (py::isinstance<py::bool_>(value)) return py::cast<bool>(value);
  if (py::isinstance<py::float_>(value) || py::isinstance<py::int_>(value))
    return py::cast<double>(value);
  if (py::isinstance<py::str>(value)) return py::cast<std::string>(value);
  const bool object = py::isinstance<py::dict>(value);
  if (!object && !py::isinstance<py::list>(value) &&
      !py::isinstance<py::tuple>(value))
    throw py::type_error(
        "A JSON value needs null, a boolean, a number, text, a list or a "
        "dict.");
  if (!active.insert(value.ptr()).second)
    throw py::value_error(
        "A JSON value cannot contain a recursive collection.");
  struct Release {
    std::unordered_set<PyObject*>& active;
    PyObject* value;
    ~Release() { active.erase(value); }
  } release{active, value.ptr()};
  if (object) {
    data::Json::Object result;
    for (auto item : py::reinterpret_borrow<py::dict>(value)) {
      if (!py::isinstance<py::str>(item.first))
        throw py::type_error("A JSON object key must be text.");
      result.emplace_back(py::cast<std::string>(item.first),
                          json(item.second, active));
    }
    return result;
  }
  data::Json::Array result;
  for (auto item : py::reinterpret_borrow<py::iterable>(value))
    result.push_back(json(item, active));
  return result;
}

data::Json json(py::handle value) {
  std::unordered_set<PyObject*> active;
  return json(value, active);
}

py::object pythonJson(const data::Json& value) {
  const Recursion recursion;
  return std::visit(
      [](const auto& held) -> py::object {
        using T = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>)
          return py::none();
        else if constexpr (std::is_same_v<T, data::Json::Array>) {
          py::list result;
          for (const auto& item : held) result.append(pythonJson(item));
          return result;
        } else if constexpr (std::is_same_v<T, data::Json::Object>) {
          py::dict result;
          for (const auto& [key, item] : held)
            if (!result.contains(py::str(key)))
              result[py::str(key)] = pythonJson(item);
          return result;
        } else
          return py::cast(held);
      },
      value.held());
}

py::object cell(const data::Value& value) {
  return std::visit(
      [](const auto& held) -> py::object {
        if constexpr (std::is_same_v<std::decay_t<decltype(held)>, data::Flag>)
          return py::bool_(held.set);
        else
          return py::cast(held);
      },
      value);
}

py::object presentCell(const data::Column& column, size_t row) {
  return column.missing(row) ? py::none() : cell(column.at(row));
}

/** The flag a Python value stands for, if it stands for one. A native
 *  `Flag` converts to and from `bool`, and a boolean cell read out of a
 *  table arrives as a Python `bool`, so both answer a flag here. */
std::optional<data::Flag> flagFrom(py::handle value) {
  if (py::isinstance<data::Flag>(value)) return py::cast<data::Flag>(value);
  if (py::isinstance<py::bool_>(value))
    return data::Flag(py::cast<bool>(value));
  return std::nullopt;
}

/** One `Flag` comparison against an arbitrary Python operand: the answer
 *  where the operand is a flag, and `NotImplemented` where it is not, so
 *  Python falls back to its own rule rather than raising from here. */
template <class Comparison>
py::object flagComparison(data::Flag value, py::handle other,
                          Comparison comparison) {
  const std::optional<data::Flag> compared = flagFrom(other);
  if (!compared) return py::reinterpret_borrow<py::object>(Py_NotImplemented);
  return py::cast(comparison(value, *compared));
}

data::ColumnType columnType(const py::list& values, py::handle explicitType) {
  if (!explicitType.is_none()) return py::cast<data::ColumnType>(explicitType);
  for (auto value : values) {
    if (value.is_none()) continue;
    if (py::isinstance<py::bool_>(value) || py::isinstance<data::Flag>(value))
      return data::ColumnType::Boolean;
    if (py::isinstance<data::Instant>(value)) return data::ColumnType::Time;
    if (py::isinstance<py::str>(value)) return data::ColumnType::Text;
    if (py::isinstance<py::float_>(value) || py::isinstance<py::int_>(value))
      return data::ColumnType::Number;
    throw py::type_error(
        "A table cell needs a number, text, a boolean, an Instant or None.");
  }
  return data::ColumnType::Number;
}

template <class T>
data::Column typedColumn(std::string name, const py::list& values) {
  std::vector<T> cells;
  cells.reserve(values.size());
  for (auto value : values) {
    if (value.is_none())
      cells.emplace_back();
    else if constexpr (std::is_same_v<T, data::Flag>) {
      if (py::isinstance<data::Flag>(value))
        cells.push_back(py::cast<data::Flag>(value));
      else if (py::isinstance<py::bool_>(value))
        cells.emplace_back(py::cast<bool>(value));
      else
        throw py::type_error("A boolean column accepts bool, Flag and None.");
    } else
      cells.push_back(py::cast<T>(value));
  }
  data::Column result(std::move(name), std::move(cells));
  for (size_t index = 0; index < values.size(); ++index)
    if (values[index].is_none()) result.markMissing(index);
  return result;
}

data::Column column(std::string name, py::iterable input, py::handle type) {
  const py::list values(input);
  switch (columnType(values, type)) {
    case data::ColumnType::Number:
      return typedColumn<double>(std::move(name), values);
    case data::ColumnType::Text:
      return typedColumn<std::string>(std::move(name), values);
    case data::ColumnType::Boolean:
      return typedColumn<data::Flag>(std::move(name), values);
    case data::ColumnType::Time:
      return typedColumn<data::Instant>(std::move(name), values);
  }
  throw py::type_error("Unknown column type.");
}

data::Interval interval(py::handle value) {
  if (py::isinstance<data::Interval>(value))
    return py::cast<data::Interval>(value);
  const auto pair = py::cast<py::sequence>(value);
  if (pair.size() != 2)
    throw py::value_error("An interval needs two endpoints.");
  return {py::cast<double>(pair[0]), py::cast<double>(pair[1])};
}

template <class T>
py::class_<T> record(py::module_& module, const char* name) {
  return py::class_<T>(module, name)
      .def(py::init([](py::kwargs fields) {
        py::object value = py::cast(T{});
        for (auto item : fields) {
          const auto name = py::cast<std::string>(item.first);
          if (!py::hasattr(value, name.c_str()))
            throw py::type_error("Unknown data property: " + name);
          py::setattr(value, name.c_str(), item.second);
        }
        return value.cast<T>();
      }))
      .def("copy", [](const T& value) { return value; })
      .def(
          "__eq__", [](const T& a, const T& b) { return a == b; },
          py::arg("other"));
}

}  // namespace

py::object dataDatabase(std::shared_ptr<const data::Database> database) {
  return database ? py::cast(DatabaseView{std::move(database)}) : py::none();
}

py::object loadData(io::Hub& hub, py::handle type, const std::string& uri) {
  if (type.is(py::type::of<DatabaseView>())) {
    std::shared_ptr<const data::Database> value;
    {
      const py::gil_scoped_release release;
      value = hub.load<data::Database>(uri);
    }
    return dataDatabase(std::move(value));
  }
  if (type.is(py::type::of<data::Json>())) {
    std::shared_ptr<const data::Json> value;
    {
      const py::gil_scoped_release release;
      value = hub.load<data::Json>(uri);
    }
    return value ? py::cast(*value) : py::none();
  }
  if (type.is(py::type::of<data::Table>())) {
    std::shared_ptr<const data::Table> value;
    {
      const py::gil_scoped_release release;
      value = hub.load<data::Table>(uri);
    }
    return value ? py::cast(*value) : py::none();
  }
  throw py::type_error(
      "This hub loader accepts Json, Table, Database or ImageAsset.");
}

void bindData(py::module_& root) {
  auto module = root.def_submodule("data");
  module.def(
      "registerDecoders",
      [](const HubHandle& value) {
        auto& hub = value.get();
        const py::gil_scoped_release release;
        data::registerDecoders(hub);
        // One Python module stands over every feature of this library, so
        // the call that puts its decoders on a hub puts the database
        // decoder there too: an owned hub loads what a session hub loads.
        hub.registerDecoder<data::Database>(data::DatabaseDecoder{});
      },
      py::arg("hub"));
  py::enum_<data::Json::Kind>(module, "JsonKind")
      .value("Null", data::Json::Kind::Null)
      .value("Boolean", data::Json::Kind::Boolean)
      .value("Number", data::Json::Kind::Number)
      .value("Text", data::Json::Kind::Text)
      .value("List", data::Json::Kind::List)
      .value("Record", data::Json::Kind::Record);
  py::class_<data::Json>(module, "Json")
      .def(py::init([](py::handle value) { return json(value); }),
           py::arg("value") = py::none())
      .def_static(
          "object",
          [](py::iterable fields) {
            data::Json::Object result;
            for (auto item : fields) {
              const auto pair = py::cast<py::sequence>(item);
              if (pair.size() != 2)
                throw py::value_error("An object field needs a key and value.");
              result.emplace_back(py::cast<std::string>(pair[0]),
                                  json(pair[1]));
            }
            return data::Json(std::move(result));
          },
          py::arg("items"))
      .def("copy", [](const data::Json& value) { return value; })
      .def("kind", &data::Json::kind)
      .def("null", &data::Json::null)
      .def("boolean", &data::Json::boolean, py::arg("fallback") = false)
      .def("number", &data::Json::number, py::arg("fallback") = 0)
      .def(
          "text",
          [](const data::Json& value, std::string_view fallback) {
            return std::string(value.text(fallback));
          },
          py::arg("fallback") = "")
      .def("size", &data::Json::size)
      .def("__len__", &data::Json::size)
      .def(
          "__getitem__",
          [](const data::Json& value, py::handle key) -> data::Json {
            if (py::isinstance<py::str>(key))
              return value[py::cast<std::string>(key)];
            if (!py::isinstance<py::int_>(key))
              throw py::type_error("A JSON index needs text or an integer.");
            const auto index = py::cast<py::ssize_t>(key);
            return index < 0 ? data::Json{} : value[static_cast<size_t>(index)];
          },
          py::arg("key"))
      .def("items",
           [](const data::Json& value) {
             return std::vector<data::Json>(value.items().begin(),
                                            value.items().end());
           })
      .def("fields",
           [](const data::Json& value) {
             return data::Json::Object(value.fields().begin(),
                                       value.fields().end());
           })
      .def("to_python", &pythonJson,
           "A detached Python value; duplicate object keys keep the first "
           "member.")
      .def(
          "__eq__",
          [](const data::Json& a, const data::Json& b) { return a == b; },
          py::arg("other"))
      .def("__repr__", [](const data::Json& value) {
        return "Json(" + data::encodeJson(value) + ")";
      });
  module.def("decodeJson", &data::decodeJson, py::arg("text"));
  module.def(
      "encodeJson",
      [](py::handle value) { return data::encodeJson(json(value)); },
      py::arg("value"));
  module.def(
      "tableFromJson",
      [](py::handle value) { return data::tableFromJson(json(value)); },
      py::arg("value"));

  // Both cell values order natively, so both order here: a column hands
  // them to Python code that sorts and compares them like any other value.
  // Each comparison is an explicit lambda with a named input, because the
  // operator shorthand leaves the input unnamed in the declarations. An
  // Instant compares only with an Instant, having no native conversion
  // from a number, and answers NotImplemented for anything else rather
  // than raising; a Flag converts from bool natively, so it compares with
  // a flag or a boolean, which is what a table's boolean cell reads as.
  // Defining a comparison drops the hash a Python object is born with, so
  // each value hashes by what it holds: a time cell is a group's key, and
  // a grouping is read into a dictionary.
  py::class_<data::Instant>(module, "Instant")
      .def(py::init<double>(), py::arg("seconds") = 0)
      .def_readwrite("seconds", &data::Instant::seconds)
      .def("__float__", [](data::Instant value) { return value.seconds; })
      .def("__hash__",
           [](data::Instant value) {
             return py::hash(py::float_(value.seconds));
           })
      .def(
          "__eq__",
          [](data::Instant value, data::Instant other) {
            return value == other;
          },
          py::arg("other"), py::is_operator())
      .def(
          "__lt__",
          [](data::Instant value, data::Instant other) {
            return value < other;
          },
          py::arg("other"), py::is_operator())
      .def(
          "__le__",
          [](data::Instant value, data::Instant other) {
            return value <= other;
          },
          py::arg("other"), py::is_operator())
      .def(
          "__gt__",
          [](data::Instant value, data::Instant other) {
            return value > other;
          },
          py::arg("other"), py::is_operator())
      .def(
          "__ge__",
          [](data::Instant value, data::Instant other) {
            return value >= other;
          },
          py::arg("other"), py::is_operator());
  py::class_<data::Flag>(module, "Flag")
      .def(py::init<bool>(), py::arg("set") = false)
      .def_readwrite("set", &data::Flag::set)
      .def("__bool__", [](data::Flag value) { return value.set; })
      .def("__hash__",
           [](data::Flag value) { return py::hash(py::bool_(value.set)); })
      .def(
          "__eq__",
          [](data::Flag value, py::handle other) {
            return flagComparison(value, other, std::equal_to<>());
          },
          py::arg("other"))
      .def(
          "__lt__",
          [](data::Flag value, py::handle other) {
            return flagComparison(value, other, std::less<>());
          },
          py::arg("other"))
      .def(
          "__le__",
          [](data::Flag value, py::handle other) {
            return flagComparison(value, other, std::less_equal<>());
          },
          py::arg("other"))
      .def(
          "__gt__",
          [](data::Flag value, py::handle other) {
            return flagComparison(value, other, std::greater<>());
          },
          py::arg("other"))
      .def(
          "__ge__",
          [](data::Flag value, py::handle other) {
            return flagComparison(value, other, std::greater_equal<>());
          },
          py::arg("other"));
  py::enum_<data::ColumnType>(module, "ColumnType")
      .value("Number", data::ColumnType::Number)
      .value("Text", data::ColumnType::Text)
      .value("Boolean", data::ColumnType::Boolean)
      .value("Time", data::ColumnType::Time);
  py::enum_<data::Order>(module, "Order")
      .value("Ascending", data::Order::Ascending)
      .value("Descending", data::Order::Descending);
  py::class_<data::Column>(module, "Column")
      .def(py::init(&column), py::arg("name"), py::arg("values"),
           py::arg("type") = py::none())
      .def("copy", [](const data::Column& value) { return value; })
      .def("name", &data::Column::name)
      .def("rename", &data::Column::rename, py::arg("name"))
      .def("type", &data::Column::type)
      .def("size", &data::Column::size)
      .def("__len__", &data::Column::size)
      .def("empty", &data::Column::empty)
      .def(
          "at",
          [](const data::Column& value, size_t row) {
            return cell(value.at(row));
          },
          py::arg("row"))
      .def("__getitem__", &presentCell, py::arg("row"))
      .def("missing", &data::Column::missing, py::arg("row"))
      .def("markMissing", &data::Column::markMissing, py::arg("row"))
      .def("values",
           [](const data::Column& value) {
             py::list result;
             for (size_t row = 0; row < value.size(); ++row)
               result.append(presentCell(value, row));
             return result;
           })
      .def(
          "take",
          [](const data::Column& value, const std::vector<size_t>& rows) {
            return value.take(rows);
          },
          py::arg("rows"))
      .def(
          "__eq__",
          [](const data::Column& a, const data::Column& b) { return a == b; },
          py::arg("other"));
  py::class_<data::Table::Group>(module, "Group")
      .def_property_readonly(
          "key",
          [](const data::Table::Group& group) { return cell(group.key); })
      .def_property_readonly(
          "rows", [](const data::Table::Group& group) { return group.rows; });
  py::class_<data::Table>(module, "Table")
      .def(py::init([](const std::vector<data::Column>& columns) {
             data::Table result;
             for (auto& column : columns) result.add(column);
             return result;
           }),
           py::arg("columns") = std::vector<data::Column>{})
      .def("copy", [](const data::Table& value) { return value; })
      .def("size", &data::Table::size)
      .def("__len__", &data::Table::size)
      .def("empty", &data::Table::empty)
      .def("columns",
           [](const data::Table& value) {
             return std::vector<data::Column>(value.columns().begin(),
                                              value.columns().end());
           })
      .def(
          "column",
          [](const data::Table& value,
             std::string_view name) -> std::optional<data::Column> {
            if (const auto* found = value.column(name)) return *found;
            return {};
          },
          py::arg("name"))
      .def("has", &data::Table::has, py::arg("name"))
      .def(
          "cell",
          [](const data::Table& value, std::string_view name, size_t row) {
            return cell(value.cell(name, row));
          },
          py::arg("name"), py::arg("row"))
      .def(
          "row",
          [](const data::Table& value, size_t row) {
            py::dict result;
            for (const auto& column : value.columns())
              result[py::str(column.name())] = presentCell(column, row);
            return result;
          },
          py::arg("row"))
      .def(
          "add",
          [](data::Table& value, data::Column column) {
            value.add(std::move(column));
          },
          py::arg("column"))
      .def(
          "add",
          [](data::Table& value, std::string name, py::iterable values,
             py::handle type) {
            value.add(column(std::move(name), values, type));
          },
          py::arg("name"), py::arg("values"), py::arg("type") = py::none())
      .def("remove", &data::Table::remove, py::arg("name"))
      .def(
          "select",
          [](const data::Table& value, const std::vector<std::string>& names) {
            const std::vector<std::string_view> views(names.begin(),
                                                      names.end());
            return value.select(views);
          },
          py::arg("names"))
      .def(
          "take",
          [](const data::Table& value, const std::vector<size_t>& rows) {
            return value.take(rows);
          },
          py::arg("rows"))
      .def(
          "filter",
          [](const data::Table& value, const py::function& keep) {
            return value.filter([&](size_t row) {
              const CallbackBoundary boundary;
              return keep(row).cast<bool>();
            });
          },
          py::arg("predicate"))
      .def(
          "derive",
          [](data::Table& value, std::string name, const py::function& function,
             py::handle type) {
            py::list cells;
            for (size_t row = 0, count = value.size(); row < count; ++row) {
              const CallbackBoundary boundary;
              cells.append(function(row));
            }
            value.add(column(std::move(name), cells, type));
          },
          py::arg("name"), py::arg("value"), py::arg("type") = py::none())
      .def("sort", &data::Table::sort, py::arg("name"),
           py::arg("order") = data::Order::Ascending)
      .def("group", &data::Table::group, py::arg("name"))
      .def(
          "__eq__",
          [](const data::Table& a, const data::Table& b) { return a == b; },
          py::arg("other"));

  record<data::CsvOptions>(module, "CsvOptions")
      .def_readwrite("delimiter", &data::CsvOptions::delimiter)
      .def_readwrite("header", &data::CsvOptions::header)
      .def_readwrite("comment", &data::CsvOptions::comment);
  module.def("decodeCsv", &data::decodeCsv, py::arg("text"),
             py::arg("options") = data::CsvOptions{}, py::arg("name") = "");
  module.def("decodeInstant", &data::decodeInstant, py::arg("text"));

  py::class_<data::Interval>(module, "Interval")
      .def(py::init<double, double>(), py::arg("low") = 0, py::arg("high") = 1)
      .def_readwrite("low", &data::Interval::low)
      .def_readwrite("high", &data::Interval::high)
      .def("extent", &data::Interval::extent)
      .def("degenerate", &data::Interval::degenerate)
      .def(
          "__eq__", [](data::Interval a, data::Interval b) { return a == b; },
          py::arg("other"));
  py::enum_<data::Transform>(module, "Transform")
      .value("Linear", data::Transform::Linear)
      .value("Log", data::Transform::Log)
      .value("Pow", data::Transform::Pow)
      .value("Sqrt", data::Transform::Sqrt)
      .value("Symlog", data::Transform::Symlog)
      .value("Time", data::Transform::Time)
      .value("Quantize", data::Transform::Quantize)
      .value("Threshold", data::Transform::Threshold)
      .value("Ordinal", data::Transform::Ordinal)
      .value("Band", data::Transform::Band)
      .value("Point", data::Transform::Point);
  py::enum_<data::Overflow>(module, "Overflow")
      .value("Extend", data::Overflow::Extend)
      .value("Clamp", data::Overflow::Clamp)
      .value("Wrap", data::Overflow::Wrap)
      .value("PingPong", data::Overflow::PingPong);
  record<data::Scale>(module, "Scale")
      .def_property(
          "domain",
          [](data::Scale& value) -> data::Interval& { return value.domain; },
          [](data::Scale& value, py::handle input) {
            value.domain = interval(input);
          },
          py::return_value_policy::reference_internal)
      .def_property(
          "range",
          [](data::Scale& value) -> data::Interval& { return value.range; },
          [](data::Scale& value, py::handle input) {
            value.range = interval(input);
          },
          py::return_value_policy::reference_internal)
      .def_readwrite("transform", &data::Scale::transform)
      .def_readwrite("overflow", &data::Scale::overflow)
      .def_readwrite("base", &data::Scale::base)
      .def_readwrite("exponent", &data::Scale::exponent)
      .def_readwrite("threshold", &data::Scale::threshold)
      .def_readwrite("steps", &data::Scale::steps)
      .def_readwrite("padding", &data::Scale::padding)
      .def_readwrite("outerPadding", &data::Scale::outerPadding)
      .def_readwrite("thresholds", &data::Scale::thresholds)
      .def("apply", &data::Scale::apply, py::arg("value"))
      .def("__call__", &data::Scale::apply, py::arg("value"))
      .def("invert", &data::Scale::invert, py::arg("value"))
      .def("position", &data::Scale::position, py::arg("index"))
      .def(
          "through",
          [](const data::Scale& value, double input,
             const py::function& interpolate) {
            const CallbackBoundary boundary;
            return value.through(
                input, [&](double position) { return interpolate(position); });
          },
          py::arg("input"), py::arg("interpolate"))
      .def("slot", &data::Scale::slot, py::arg("value"))
      .def("bandwidth", &data::Scale::bandwidth)
      .def("stepWidth", &data::Scale::stepWidth)
      .def("ticks", &data::Scale::ticks, py::arg("count") = 10)
      .def("tickStep", &data::Scale::tickStep, py::arg("count") = 10)
      .def("nice", &data::Scale::nice, py::arg("count") = 10);

  py::enum_<data::Engine>(module, "Engine")
      .value("Sqlite", data::Engine::Sqlite)
      .value("Duck", data::Engine::Duck);
  py::class_<DatabaseView>(module, "Database")
      .def_static(
          "memory",
          [](data::Engine engine) {
            std::string why;
            auto database = data::Database::memory(engine, &why);
            if (!database) throw std::runtime_error(why);
            return ownedDatabase(std::move(*database));
          },
          py::arg("engine") = data::Engine::Sqlite)
      .def_static(
          "open",
          [](const std::filesystem::path& path) {
            std::string why;
            auto database = data::Database::open(path, &why);
            if (!database) throw std::runtime_error(why);
            return ownedDatabase(std::move(*database));
          },
          py::arg("path"))
      .def_static(
          "fromBytes",
          [](py::bytes bytes, std::string hint) {
            const auto text = bytes.cast<std::string>();
            io::Bytes source;
            source.bytes.resize(text.size());
            if (!text.empty())
              std::memcpy(source.bytes.data(), text.data(), text.size());
            std::string why;
            auto database = data::Database::fromBytes(source, hint, &why);
            if (!database) throw std::runtime_error(why);
            return dataDatabase(
                std::make_shared<data::Database>(std::move(*database)));
          },
          py::arg("bytes"), py::arg("hint") = "data.sqlite")
      .def("engine",
           [](const DatabaseView& view) { return view.owner->engine(); })
      .def("file", [](const DatabaseView& view) { return view.owner->file(); })
      .def(
          "execute",
          [](const DatabaseView& view, std::string_view sql) {
            std::string why;
            if (!view.writable().execute(sql, &why))
              throw std::runtime_error(why);
          },
          py::arg("sql"))
      .def(
          "insert",
          [](const DatabaseView& view, std::string_view name,
             const data::Table& rows) {
            std::string why;
            if (!view.writable().insert(name, rows, &why))
              throw std::runtime_error(why);
          },
          py::arg("name"), py::arg("rows"))
      .def(
          "query",
          [](const DatabaseView& view, std::string_view sql) {
            std::string why;
            // A view that refuses writes refuses them through every
            // method, so a writing statement is named before it is run
            // and raises what execute() and insert() raise.
            if (!view.writer) {
              const auto writes = view.owner->writes(sql, &why);
              if (!writes) throw std::runtime_error(why);
              if (*writes)
                throw std::runtime_error("This database view is read-only.");
            }
            auto table = view.owner->query(sql, &why);
            if (!table) throw std::runtime_error(why);
            return std::move(*table);
          },
          py::arg("sql"));
  module.def("engineOf", &data::engineOf, py::arg("uri"));

  auto decodePacket = [](auto decoder) {
    return [decoder](py::bytes input) {
      const auto source = input.cast<std::string>();
      return decoder(std::as_bytes(std::span(source)));
    };
  };
  auto encodePacket = [](auto encoder) {
    return [encoder](py::handle message) {
      const auto result = encoder(json(message));
      return py::bytes(reinterpret_cast<const char*>(result.data()),
                       result.size());
    };
  };
  module.def("decodeOsc", decodePacket(data::decodeOsc), py::arg("packet"));
  module.def("decodeMidi", decodePacket(data::decodeMidi), py::arg("message"));
  module.def("decodeArtNet", decodePacket(data::decodeArtNet),
             py::arg("packet"));
  module.def(
      "encodeOsc",
      encodePacket(static_cast<std::vector<std::byte> (*)(const data::Json&)>(
          &data::encodeOsc)),
      py::arg("message"));
  module.def(
      "encodeOsc",
      [](std::string_view address, py::handle arguments) {
        const auto result = data::encodeOsc(address, json(arguments));
        return py::bytes(reinterpret_cast<const char*>(result.data()),
                         result.size());
      },
      py::arg("address"), py::arg("arguments"));
  module.def("encodeMidi", encodePacket(data::encodeMidi), py::arg("message"));
  module.def("encodeArtNet", encodePacket(data::encodeArtNet),
             py::arg("message"));
  module.attr("maxOscPacketBytes") = data::maxOscPacketBytes;
  py::class_<data::Schema>(module, "Schema")
      .def(py::init<>())
      .def_static(
          "fromBinarySchema",
          [](py::bytes bytes) {
            const auto source = bytes.cast<std::string>();
            std::string why;
            auto schema = data::Schema::fromBinarySchema(
                std::as_bytes(std::span(source)), &why);
            if (!schema) throw py::value_error(why);
            return schema;
          },
          py::arg("bytes"))
      .def("__bool__", [](const data::Schema& value) { return bool(value); })
      .def("rootName", &data::Schema::rootName)
      .def(
          "text",
          [](const data::Schema& value, py::bytes bytes) {
            const auto source = bytes.cast<std::string>();
            std::string why;
            auto result = value.text(std::as_bytes(std::span(source)), &why);
            if (!result) throw py::value_error(why);
            return *result;
          },
          py::arg("binary"))
      .def(
          "binary",
          [](const data::Schema& value, std::string_view json) {
            std::string why;
            auto result = value.binary(json, &why);
            if (!result) throw py::value_error(why);
            return py::bytes(reinterpret_cast<const char*>(result->data()),
                             result->size());
          },
          py::arg("json"));
}

}  // namespace sigil::python
