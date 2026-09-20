#pragma once

/** @file
 * The values the other bindings are written in terms of — a point,
 * a rectangle, the core and material records — and the readings that
 * take them from the looser shapes Python spells them as.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <pybind11/pybind11.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, sk_sp<T>, true);

namespace sigil::python {

/** A point read from @p value, which a two-number sequence describes. */
SkPoint point(pybind11::handle value);
/** A four-number sequence describes x, y, width and height. */
SkRect rect(pybind11::handle value);

}  // namespace sigil::python
