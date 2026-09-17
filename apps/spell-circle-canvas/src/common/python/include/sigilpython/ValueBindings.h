#pragma once

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <pybind11/pybind11.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, sk_sp<T>, true);

namespace sigil::python {

SkPoint point(pybind11::handle value);
/** A four-number sequence describes x, y, width and height. */
SkRect rect(pybind11::handle value);
void bindCore(pybind11::module_& module);
void bindValues(pybind11::module_& module);
void bindMaterial(pybind11::module_& module);

}  // namespace sigil::python
