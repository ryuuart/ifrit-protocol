#pragma once

/** @file
 * The two places a process can only learn from the platform: where its
 * own binary stands, and where it may leave files nobody will miss.
 *
 * Both answer a path and touch no disk. Asking the platform where the
 * executable is takes a different call on every system, which is why it
 * is answered once here rather than at each site that wants to reach a
 * file staged beside the binary.
 */

#include <filesystem>
#include <string_view>

namespace sigil::io {

/** THE RUNNING BINARY'S OWN PATH, canonical. Empty when the platform
 *  will not say — a caller that stages resources beside its executable
 *  must have another way to find them. */
std::filesystem::path executablePath();

/** A DIRECTORY FOR THROWAWAY FILES, named for @p label and this process.
 *  The process id is in the name, so two runs of one binary side by side
 *  never write into one directory, and the label separates one purpose
 *  from another within a run.
 *
 *  Nothing is created: the caller decides whether a stale directory from
 *  a run that died is emptied or swept, which is not one answer. */
std::filesystem::path scratchDirectory(std::string_view label);

}  // namespace sigil::io
