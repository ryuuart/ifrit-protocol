#pragma once

/** @file
 * Reaching a registration another file made: the submodule a package
 * publishes under, and the class a package extends without editing the
 * file that first registered it.
 */

#include <pybind11/pybind11.h>

#include <string>

namespace sigil::python {

/** The submodule @p path names under @p root, created when it is not
 *  there yet. @p path is dotted and relative to @p root, so
 *  `submodule(module, "geometry.shapes")` answers `_sigil.geometry.shapes`
 *  whether or not another registration reached it first. */
pybind11::module_ submodule(pybind11::module_& root, const char* path);

/** The registered type @p path names, checked. `extend` is how a binding
 *  reaches it; this is the check underneath. */
pybind11::handle registeredClass(pybind11::module_& root, const char* path);

/** The registered class @p path names under @p root, so a package adds
 *  verbs to a type another package registered instead of editing that
 *  package's file. @p path is dotted and ends in the class name, as in
 *  `extend<compose::Element>(module, "compose.Element")`. Throws when
 *  nothing of that name is registered yet, which is what an author sees
 *  when the registration order puts this package first. */
template <class T>
pybind11::class_<T> extend(pybind11::module_& root, const char* path) {
  return pybind11::reinterpret_borrow<pybind11::class_<T>>(
      registeredClass(root, path));
}

}  // namespace sigil::python
