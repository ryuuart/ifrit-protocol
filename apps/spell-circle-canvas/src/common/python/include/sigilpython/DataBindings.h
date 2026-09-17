#pragma once

#include <pybind11/pybind11.h>

#include <memory>
#include <string>

namespace sigil::data {
class Database;
}
namespace sigil::io {
class Hub;
}

namespace sigil::python {

void bindData(pybind11::module_& module);
/** A query view owns the native database independently of its asset cache. */
pybind11::object dataDatabase(std::shared_ptr<const data::Database> database);
pybind11::object loadData(sigil::io::Hub& hub, pybind11::handle type,
                          const std::string& uri);

}  // namespace sigil::python
