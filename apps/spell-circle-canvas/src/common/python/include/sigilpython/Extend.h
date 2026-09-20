#pragma once

/** @file
 * Reaching a registration another file made: the submodule a package
 * publishes under, and the class a package extends without editing the
 * file that first registered it.
 */

#include <pybind11/pybind11.h>

#include <memory>
#include <string>

namespace sigil::python {

/** The submodule @p path names under @p root, created when it is not
 *  there yet. @p path is dotted and relative to @p root, so
 *  `submodule(module, "geometry.shapes")` answers `_sigil.geometry.shapes`
 *  whether or not another registration reached it first. Throws when a
 *  step of the path already names something that is not a module, since
 *  replacing it would unregister what another package put there. */
pybind11::module_ submodule(pybind11::module_& root, const char* path);

/** The registered type @p path names, checked, as a reference of the
 *  caller's own. `extend` is how a binding reaches it; this is the check
 *  underneath. */
pybind11::object registeredClass(pybind11::module_& root, const char* path);

/** The registered class @p path names under @p root, so a package adds
 *  verbs to a type another package registered instead of editing that
 *  package's file. @p path is dotted and ends in the class name, as in
 *  `extend<compose::Element>(module, "compose.Element")`. @p Holder is
 *  the holder the package that first registered the class gave it, and
 *  has to be named whenever that is not pybind11's own default: a
 *  constructor or a factory added through the wrong holder allocates one
 *  the rest of the bindings do not expect. Throws when nothing of that
 *  name is registered yet, which is what an author sees when the
 *  registration order puts this package first. */
template <class T, class Holder = std::unique_ptr<T>>
pybind11::class_<T, Holder> extend(pybind11::module_& root, const char* path) {
  return pybind11::reinterpret_borrow<pybind11::class_<T, Holder>>(
      registeredClass(root, path));
}

}  // namespace sigil::python
