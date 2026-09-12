#pragma once

/** @file
 * SigilGeometry pop — POP-style point combinators as VALUES. A Chain is
 * a description (nondestructive: edit a field, re-describe) over the
 * Cloud vocabulary in Points.h. The LANGUAGE and its host executor share
 * one scope: pop::on() opens a Chain and pop::cook() evaluates one (a
 * Cloud, ready for points::instance / panels / drawBillboards onto a
 * canvas), while a runtime that owns a device runs the same Chain as
 * compute dispatches over the same lanes.
 *
 * The two do not agree by inspection: for the operators that are
 * per-point arithmetic there is ONE formula, written in the kernel this
 * feature compiles to both a host build and a device one (Kernel.h),
 * and both ends seed and export through the same two functions. The
 * hashes, the spline and the variant order are what the rest rests on.
 *
 * The whole language in one include: the operator vocabulary, the
 * executor seam and its shared helpers, the sinks a cooked chain is
 * spent into, and the builder that spells them.
 */

#include "sigilgeometry/mesh/pop/Builder.h"
#include "sigilgeometry/mesh/pop/Operations.h"
#include "sigilgeometry/mesh/pop/Runtime.h"
#include "sigilgeometry/mesh/pop/Sinks.h"
