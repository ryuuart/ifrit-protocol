#pragma once

/** @file
 * @ingroup sketch-core
 *
 * Where a process keeps what it writes to be read again by a later run.
 */

#include <filesystem>
#include <string_view>

namespace sigil::sketch {

/** THE STATE ROOT: one directory under which this process keeps
 *  everything it writes for a later run to read — the builds of the
 *  sketch files it compiled, and whatever a host keeps beside them, each
 *  kind in a directory of its own named for it.
 *
 *  Empty, which is how a process starts, leaves every kind of state in
 *  the platform's own location for it. A root is what makes a run
 *  hermetic: a test or a script that names one reads nothing an earlier
 *  run left and leaves nothing behind for a later one. Set once, before
 *  anything that keeps state has started. */
void setStateDirectory(const std::filesystem::path& directory);

/** The root this process keeps its state under; empty when none was set. */
[[nodiscard]] std::filesystem::path stateDirectory();

/** Where the state @p kind names stands under the root — `builds` for
 *  the compiled sketch files — or empty when no root was set, which
 *  sends the caller to the platform's own location for it. */
[[nodiscard]] std::filesystem::path stateLocation(std::string_view kind);

}  // namespace sigil::sketch
