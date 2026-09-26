#pragma once

/** @file
 * @ingroup media-core
 * THE FILENAME QUESTION: which format a name's extension says, and the
 * extension each format is written with. The one place a name is allowed
 * to decide a format — decoding sniffs bytes and encoding is told — so
 * a caller that wants a name to choose asks here, as a hub's save does.
 */

#include <filesystem>
#include <optional>

#include "sigilmedia/core/Format.h"

namespace sigil::media {

/** The format a path's extension names, case-insensitively — ".jpg" and
 *  ".jpeg" are both JPEG — and nothing when it names none. Meaning only:
 *  the path is never opened. */
std::optional<Format> formatForPath(const std::filesystem::path& path);

/** The conventional extension for @p format, leading dot included. */
const char* extensionFor(Format format);

}  // namespace sigil::media
