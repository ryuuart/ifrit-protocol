#pragma once

#include <pybind11/pybind11.h>

namespace sigil::python {
/** Gives every registered class, enumeration and function the module an
 *  author imports it from.
 *
 *  Call once, after the last registration: a class records its module when it
 *  is registered, so anything registered afterwards would carry the
 *  extension's own name. A public module named here must really export the
 *  value, because inspect.getmodule and pydoc import the module __module__
 *  names. The modules themselves keep the extension's naming, which is what
 *  a stub generator reads to find them; the package renames the ones it
 *  hands on where it publishes them. */
void namePublicly(pybind11::module_& module);
}  // namespace sigil::python
