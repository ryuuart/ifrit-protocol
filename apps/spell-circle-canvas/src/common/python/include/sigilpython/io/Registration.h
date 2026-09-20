#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: resource access, feeds and publication.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers resource access on @p module. */
void bindIO(pybind11::module_& module);
/** Registers hub::onDispatch, Python decoders and transports,
 *  image/channel/probe views, Feed::receivedAt on @p module. */
void bindIOHubGrowth(pybind11::module_& module);
/** Registers sigilIOPublish: publications, publishers and subscriptions
 *  on @p module. */
void bindIOPublish(pybind11::module_& module);
/** Registers writeBytes, places, archives, AnyByteSource, TextCatalog
 *  and the network cache on @p module. */
void bindIOSources(pybind11::module_& module);

}  // namespace sigil::python
