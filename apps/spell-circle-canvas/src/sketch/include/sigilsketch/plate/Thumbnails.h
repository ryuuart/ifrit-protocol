#pragma once

/** @file
 * The app's own thumbnail store: where a sketch's still is kept, when it
 * is stale, and the CPU render that fills it.
 *
 * The browser owns its thumbnails. They are not the plate ledger's to
 * write: a still a browser shows is a convenience, not a verdict, so it
 * is rendered on demand into a cache beside the binary rather than copied
 * out of a sweep. One PNG per sketch stem, its filename carrying a KEY —
 * a hash of the sketch's source folded with the running host's build
 * identity — so a thumbnail whose key no longer matches is stale and is
 * re-rendered. The render is the same capture the CPU plate tier takes,
 * scaled down, and it never touches a device.
 */

#include <sigilsketch/core/Registry.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch {

class Assets;

/** How wide a stored thumbnail is rendered — one file per stem serves
 *  the list row, the gallery card and the inspector alike, so it is taken
 *  large enough for the widest of them and each scales it down from the
 *  image decoder's cache. */
inline constexpr int kThumbnailWidth = 640;

/** THE STALENESS KEY for the sketch whose entry file is @p entrySource.
 *
 *  It hashes the sketch's source — the entry file, or, for a sketch that
 *  is a directory, every `.cpp`/`.h` standing beside the entry — by size
 *  and modification time, and folds in the running host's build identity
 *  so that a rebuilt host regenerates every still. A thumbnail file whose
 *  name carries a different key is stale. Cheap enough to compute on the
 *  UI thread: it stats files rather than reading them. */
[[nodiscard]] std::string thumbnailKey(const std::filesystem::path& entrySource);

/** Where a fresh thumbnail for @p stem at @p key lands under @p dir. The
 *  key is in the filename so a changed source is a new URL — which is
 *  what makes a cached image decoder reload it. */
[[nodiscard]] std::filesystem::path thumbnailFile(
    const std::filesystem::path& dir, std::string_view stem,
    std::string_view key);

/** The fresh thumbnail already on disk for @p stem at @p key, or empty
 *  when none is — a missing file, or one carrying a different key. */
[[nodiscard]] std::filesystem::path freshThumbnail(
    const std::filesystem::path& dir, std::string_view stem,
    std::string_view key);

/** RENDERS ONE REGISTRY SKETCH'S STILL and writes it to @p out.
 *
 *  It opens the sketch's kind, steps it from zero at the sweep's own
 *  fixed rate to its declared moment (or the sweep's derived default when
 *  it names none), takes the still the plate tier takes, and scales it so
 *  its larger side is @p maxDimension pixels before encoding a PNG. CPU
 *  only — it allocates a raster surface and never a device one, so it can
 *  run on a worker that shares no graphics context. Any older thumbnail
 *  for the same stem under @p out's directory is removed. False when the
 *  sketch has no kind, declares an empty canvas, or the write fails. */
[[nodiscard]] bool renderThumbnail(const Entry& entry, weave::FontContext& fonts,
                                   Assets& assets,
                                   const std::filesystem::path& out,
                                   int maxDimension);

}  // namespace sigil::sketch
