#pragma once

#include <pybind11/pybind11.h>

#include <memory>

namespace sigil::data {
class Database;
}

namespace sigil::sketch::python {

void bindData(pybind11::module_& module);
/** A query view owns the native database independently of its asset cache. */
pybind11::object dataDatabase(std::shared_ptr<const data::Database> database);

}  // namespace sigil::sketch::python
