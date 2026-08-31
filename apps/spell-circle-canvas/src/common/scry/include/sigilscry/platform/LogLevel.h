#pragma once

/** @file
 * The severity of a message the engine reports — Ultralight's own log,
 * a page's console output, and the library's diagnostics all arrive
 * through one callback tagged with it.
 */

namespace sigil::scry {

/** Severity of an engine log message, mirroring ultralight::LogLevel. */
enum class LogLevel { Error, Warning, Info };

}  // namespace sigil::scry
