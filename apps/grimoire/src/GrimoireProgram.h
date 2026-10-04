#pragma once

/** @file
 * The program Grimoire answers `host.describe` as.
 */

#include <sigilprotocol/dispatch/Program.h>

#include <filesystem>

/** Grimoire, at the version it was built as, keeping its state under
 *  @p stateRoot. */
sigil::protocol::Program grimoireProgram(std::filesystem::path stateRoot);
