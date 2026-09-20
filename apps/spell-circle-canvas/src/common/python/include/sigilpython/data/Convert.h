#pragma once

/** @file
 * Binding tabular data: the tables, the scales, and the database a
 * query view holds independently of the cache it was loaded from.
 */

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

/** A query view owns the native database independently of its asset cache. */
pybind11::object dataDatabase(std::shared_ptr<const data::Database> database);
/** The resource @p uri from @p hub, decoded into @p type — a table, a
 *  document or a database — and handed back as the Python value for
 *  it. */
pybind11::object loadData(sigil::io::Hub& hub, pybind11::handle type,
                          const std::string& uri);

}  // namespace sigil::python
