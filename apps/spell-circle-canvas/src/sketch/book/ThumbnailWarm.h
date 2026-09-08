#pragma once

/** @file
 * The app's own thumbnail store: where it stands, and the command that
 * fills what is missing or stale in it.
 */

#include <chrono>
#include <filesystem>
#include <string>

namespace sigil::sketch {
class Assets;
}

namespace sigil::weave {
class FontContext;
}

/** WHERE SKETCHBOOK KEEPS ITS THUMBNAILS. The command line names one; an
 *  environment variable names one for a test; otherwise the platform
 *  cache location, under this app's own name. The store is the app's
 *  alone: no ledger and no sweep writes into it. */
std::filesystem::path thumbnailStoreDir(const std::string& override);

/** THE WARM COMMAND: render every selected sketch's MISSING OR STALE
 *  thumbnail through the same CPU path the window's own fill takes, and
 *  exit non-zero naming the ones that could not be drawn.
 *
 *  It answers to the same budget the window's fill does: a still that
 *  runs past it is abandoned and NOTED, so the note stands in for the
 *  thumbnail and neither this command nor the window spends the budget
 *  on that sketch again while its source stays put. A sketch
 *  this machine cannot run is stood down by name rather than failed, and
 *  a sketch whose thumbnail or note is already fresh is left alone. */
int runThumbnails(int only, const std::string& kind,
                  const std::filesystem::path& dir,
                  std::chrono::milliseconds budget, bool heavy,
                  sigil::weave::FontContext& fonts,
                  sigil::sketch::Assets& store);
