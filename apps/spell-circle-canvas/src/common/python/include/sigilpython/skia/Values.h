#pragma once

/** @file
 * What every binding that touches a Skia value is written in terms of:
 * the holder that lets a reference-counted Skia object cross into Python
 * and back, and the readings that take a point and a rectangle from the
 * looser shapes Python spells them as.
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
