#pragma once

/** @file
 * The program Sketchbook answers `host.describe` as.
 */

#include <sigilprotocol/dispatch/Program.h>

#include <filesystem>

/** Sketchbook, at the version it was built as, keeping its state under
 *  @p stateRoot. */
sigil::protocol::Program sketchbookProgram(std::filesystem::path stateRoot);
