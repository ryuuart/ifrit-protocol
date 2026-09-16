/** @file
 * Native data snapshots, table transformations, axes and SQL query views.
 */

#include "DataBindings.h"

#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigildata/decode/Csv.h>
#include <sigildata/decode/Json.h>
#include <sigildata/query/Database.h>
#include <sigildata/scale/Scale.h>

#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_set>

#include "KitBindings.h"

namespace sigil::sketch::python {
namespace py = pybind11;
namespace {

struct DatabaseView {
  std::shared_ptr<const data::Database> owner;
};

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
      .def("__eq__", [](const T& a, const T& b) { return a == b; });
}

}  // namespace

py::object dataDatabase(std::shared_ptr<const data::Database> database) {
  return database ? py::cast(DatabaseView{std::move(database)}) : py::none();
}

void bindData(py::module_& root) {
  auto module = root.def_submodule("data");
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
          })
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
      .def("__getitem__",
           [](const data::Json& value, py::handle key) -> data::Json {
             if (py::isinstance<py::str>(key))
               return value[py::cast<std::string>(key)];
             if (!py::isinstance<py::int_>(key))
               throw py::type_error("A JSON index needs text or an integer.");
             const auto index = py::cast<py::ssize_t>(key);
             return index < 0 ? data::Json{}
                              : value[static_cast<size_t>(index)];
           })
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
      .def("__eq__",
           [](const data::Json& a, const data::Json& b) { return a == b; })
      .def("__repr__", [](const data::Json& value) {
        return "Json(" + data::encodeJson(value) + ")";
      });
  module.def("decodeJson", &data::decodeJson);
  module.def("encodeJson",
             [](py::handle value) { return data::encodeJson(json(value)); });
  module.def("tableFromJson",
             [](py::handle value) { return data::tableFromJson(json(value)); });

  py::class_<data::Instant>(module, "Instant")
      .def(py::init<double>(), py::arg("seconds") = 0)
      .def_readwrite("seconds", &data::Instant::seconds)
      .def("__float__", [](data::Instant value) { return value.seconds; })
      .def("__eq__", [](data::Instant a, data::Instant b) { return a == b; });
  py::class_<data::Flag>(module, "Flag")
      .def(py::init<bool>(), py::arg("set") = false)
      .def_readwrite("set", &data::Flag::set)
      .def("__bool__", [](data::Flag value) { return value.set; });
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
      .def("rename", &data::Column::rename)
      .def("type", &data::Column::type)
      .def("size", &data::Column::size)
      .def("__len__", &data::Column::size)
      .def("empty", &data::Column::empty)
      .def("at", [](const data::Column& value,
                    size_t row) { return cell(value.at(row)); })
      .def("__getitem__", &presentCell)
      .def("missing", &data::Column::missing)
      .def("markMissing", &data::Column::markMissing)
      .def("values",
           [](const data::Column& value) {
             py::list result;
             for (size_t row = 0; row < value.size(); ++row)
               result.append(presentCell(value, row));
             return result;
           })
      .def("take",
           [](const data::Column& value, const std::vector<size_t>& rows) {
             return value.take(rows);
           })
      .def("__eq__",
           [](const data::Column& a, const data::Column& b) { return a == b; });
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
      .def("column",
           [](const data::Table& value,
              std::string_view name) -> std::optional<data::Column> {
             if (const auto* found = value.column(name)) return *found;
             return {};
           })
      .def("has", &data::Table::has)
      .def("cell", [](const data::Table& value, std::string_view name,
                      size_t row) { return cell(value.cell(name, row)); })
      .def("row",
           [](const data::Table& value, size_t row) {
             py::dict result;
             for (const auto& column : value.columns())
               result[py::str(column.name())] = presentCell(column, row);
             return result;
           })
      .def("add", [](data::Table& value,
                     data::Column column) { value.add(std::move(column)); })
      .def(
          "add",
          [](data::Table& value, std::string name, py::iterable values,
             py::handle type) {
            value.add(column(std::move(name), values, type));
          },
          py::arg("name"), py::arg("values"), py::arg("type") = py::none())
      .def("remove", &data::Table::remove)
      .def("select",
           [](const data::Table& value, const std::vector<std::string>& names) {
             const std::vector<std::string_view> views(names.begin(),
                                                       names.end());
             return value.select(views);
           })
      .def("take",
           [](const data::Table& value, const std::vector<size_t>& rows) {
             return value.take(rows);
           })
      .def("filter",
           [](const data::Table& value, const py::function& keep) {
             return value.filter([&](size_t row) {
               const KitScopeBoundary boundary;
               return keep(row).cast<bool>();
             });
           })
      .def(
          "derive",
          [](data::Table& value, std::string name, const py::function& function,
             py::handle type) {
            py::list cells;
            for (size_t row = 0, count = value.size(); row < count; ++row) {
              const KitScopeBoundary boundary;
              cells.append(function(row));
            }
            value.add(column(std::move(name), cells, type));
          },
          py::arg("name"), py::arg("value"), py::arg("type") = py::none())
      .def("sort", &data::Table::sort, py::arg("name"),
           py::arg("order") = data::Order::Ascending)
      .def("group", &data::Table::group)
      .def("__eq__",
           [](const data::Table& a, const data::Table& b) { return a == b; });

  record<data::CsvOptions>(module, "CsvOptions")
      .def_readwrite("delimiter", &data::CsvOptions::delimiter)
      .def_readwrite("header", &data::CsvOptions::header)
      .def_readwrite("comment", &data::CsvOptions::comment);
  module.def("decodeCsv", &data::decodeCsv, py::arg("text"),
             py::arg("options") = data::CsvOptions{}, py::arg("name") = "");
  module.def("decodeInstant", &data::decodeInstant);

  py::class_<data::Interval>(module, "Interval")
      .def(py::init<double, double>(), py::arg("low") = 0, py::arg("high") = 1)
      .def_readwrite("low", &data::Interval::low)
      .def_readwrite("high", &data::Interval::high)
      .def("extent", &data::Interval::extent)
      .def("degenerate", &data::Interval::degenerate)
      .def("__eq__", [](data::Interval a, data::Interval b) { return a == b; });
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
      .def("apply", &data::Scale::apply)
      .def("__call__", &data::Scale::apply)
      .def("invert", &data::Scale::invert)
      .def("position", &data::Scale::position)
      .def("through",
           [](const data::Scale& value, double input,
              const py::function& interpolate) {
             const KitScopeBoundary boundary;
             return value.through(
                 input, [&](double position) { return interpolate(position); });
           })
      .def("slot", &data::Scale::slot)
      .def("bandwidth", &data::Scale::bandwidth)
      .def("stepWidth", &data::Scale::stepWidth)
      .def("ticks", &data::Scale::ticks, py::arg("count") = 10)
      .def("tickStep", &data::Scale::tickStep, py::arg("count") = 10)
      .def("nice", &data::Scale::nice, py::arg("count") = 10);

  py::enum_<data::Engine>(module, "Engine")
      .value("Sqlite", data::Engine::Sqlite)
      .value("Duck", data::Engine::Duck);
  py::class_<DatabaseView>(module, "Database")
      .def_static("open",
                  [](const std::filesystem::path& path) {
                    std::string why;
                    auto database = data::Database::open(path, &why);
                    if (!database) throw std::runtime_error(why);
                    return dataDatabase(
                        std::make_shared<data::Database>(std::move(*database)));
                  })
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
      .def("query", [](const DatabaseView& view, std::string_view sql) {
        std::string why;
        auto table = view.owner->query(sql, &why);
        if (!table) throw std::runtime_error(why);
        return std::move(*table);
      });
  module.def("engineOf", &data::engineOf);
}

}  // namespace sigil::sketch::python
