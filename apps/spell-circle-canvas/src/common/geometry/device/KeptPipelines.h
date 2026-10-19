#pragma once

/** @file
 * The driver's pipeline cache as it is kept between runs: framed on disk
 * so that only bytes a driver handed back are ever handed to one.
 */

#include <filesystem>
#include <span>
#include <vector>

namespace sigil::geometry::device {

/** WHAT THE LAST RUN KEPT IN @p file, or nothing where the file is
 *  missing or its frame does not hold: torn by two processes writing at
 *  once, cut short, or written by anything else. */
[[nodiscard]] std::vector<char> readKeptPipelines(
    const std::filesystem::path& file);

/** KEEPS @p bytes IN @p file, framed, written beside it under a name no
 *  other process or call writes and moved over it — so a run that stops
 *  halfway through leaves the last whole set, and two runs closing at
 *  once leave one of their two whole sets rather than a mixture of both.
 *  Returns whether the set landed. */
bool writeKeptPipelines(const std::filesystem::path& file,
                        std::span<const char> bytes);

}  // namespace sigil::geometry::device
