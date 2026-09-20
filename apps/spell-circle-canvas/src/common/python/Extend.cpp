#include <sigilpython/Extend.h>

#include <stdexcept>
#include <string_view>

namespace sigil::python {
namespace py = pybind11;

namespace {

py::object step(const py::object& holder, const std::string& name) {
  if (!py::hasattr(holder, name.c_str())) return py::object();
  return holder.attr(name.c_str());
}

}  // namespace

py::module_ submodule(py::module_& root, const char* path) {
  py::module_ found = root;
  std::string_view rest(path);
  while (!rest.empty()) {
    const size_t stop = std::min(rest.find('.'), rest.size());
    const std::string name(rest.substr(0, stop));
    const py::object existing = step(found, name);
    found = existing && py::isinstance<py::module_>(existing)
                ? py::reinterpret_borrow<py::module_>(existing)
                : found.def_submodule(name.c_str());
    rest.remove_prefix(std::min(stop + 1, rest.size()));
  }
  return found;
}

py::handle registeredClass(py::module_& root, const char* path) {
  py::object found = root;
  std::string_view rest(path);
  while (!rest.empty()) {
    const size_t stop = std::min(rest.find('.'), rest.size());
    const std::string name(rest.substr(0, stop));
    found = step(found, name);
    if (!found)
      throw std::runtime_error(
          std::string("No registration named ") + path +
          " to extend. Register it before the binding that adds to it.");
    rest.remove_prefix(std::min(stop + 1, rest.size()));
  }
  if (!py::isinstance<py::type>(found))
    throw std::runtime_error(std::string(path) +
                             " is registered, but not as a class.");
  return found.release();
}

}  // namespace sigil::python
