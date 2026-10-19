#pragma once

/** @file
 * A sketch the author's own interpreter already holds, as a kind a host
 * opens with no file behind it.
 */

#include <pybind11/pybind11.h>
#include <sigilsketch/core/Kind.h>

namespace sigil::sketch::python {

/** A KIND WHOSE BODY IS MADE BY @p factory, a callable answering the
 *  instance each session draws: a `@sketch` class, or a function handing
 *  back an instance the caller keeps. Nothing is imported for it, so a
 *  reload has nothing to read again; `Host::restartSession` calls the
 *  factory again. @p scene3d says the body describes a world frame and
 *  @p needsDevice that a scene2d body bakes one, as the `@sketch`
 *  decorator and the body's own `needsDevice` declare them. */
[[nodiscard]] Kind held(pybind11::handle factory, bool scene3d,
                        bool needsDevice);

}  // namespace sigil::sketch::python
