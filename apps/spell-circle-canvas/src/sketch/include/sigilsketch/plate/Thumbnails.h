#pragma once

/** @file
 * The app's own thumbnail store: where a sketch's still is kept, when it
 * is stale, and the CPU render that fills it.
 *
 * The browser owns its thumbnails. They are not the plate ledger's to
 * write: a still a browser shows is a convenience, not a verdict, so it
 * is rendered on demand into a cache beside the binary rather than copied
 * out of a sweep. One PNG per sketch stem, its filename carrying a KEY —
 * a hash of the sketch's SOURCE and nothing else — so a thumbnail whose
 * key no longer matches is stale and is re-rendered. The render is the
 * same capture the CPU plate tier takes, scaled down, and it never
 * touches a device.
 */

#include <sigilsketch/core/Registry.h>

#include <atomic>
#include <chrono>
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

/** HOW LONG ONE STILL MAY TAKE before the sketch is left with its
 *  placeholder.
 *
 *  A still is a walk from zero to the sketch's declared moment at a fixed
 *  rate, so its cost is the sketch's own frame cost times a number the
 *  sketch chose — and a sketch that names a late moment can be walking
 *  for minutes. The budget is what keeps one such sketch from being the
 *  whole of a fill: it is abandoned, noted, and the fill moves on. */
inline constexpr std::chrono::milliseconds kThumbnailBudget{8000};

/** THE STALENESS KEY for the sketch whose entry file is @p entrySource.
 *
 *  It hashes the sketch's source — the entry file, or, for a sketch that
 *  is a directory, every `.cpp`/`.h` standing beside the entry — by size
 *  and modification time, and NOTHING ELSE. A thumbnail file whose name
 *  carries a different key is stale. Cheap enough to compute on the UI
 *  thread: it stats files rather than reading them.
 *
 *  THE HOST IS NOT IN IT. A library edit that changes what a sketch draws
 *  leaves every still on disk claiming to be fresh, and that is the right
 *  trade: the alternative throws all of them away on every rebuild, and
 *  the refresh on opening writes back the frame that was just presented,
 *  so a still a rebuild made wrong heals the moment it is looked at. */
[[nodiscard]] std::string thumbnailKey(
    const std::filesystem::path& entrySource);

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

/** LEAVES ONE LINE SAYING WHY THIS SKETCH HAS NO STILL, beside where the
 *  still would have gone and under the same key.
 *
 *  A sketch that ran past the budget, or that declared itself a plate,
 *  costs its whole budget to find out — every launch, forever, if the
 *  finding is not written down. The note carries the same key the still
 *  would have, so editing the sketch asks the question again and nothing
 *  else does. */
bool noteThumbnail(const std::filesystem::path& dir, std::string_view stem,
                   std::string_view key, std::string_view why);

/** The note left for @p stem at @p key, or empty when there is none. */
[[nodiscard]] std::string thumbnailNote(const std::filesystem::path& dir,
                                        std::string_view stem,
                                        std::string_view key);

/** Removes everything @p dir holds for @p stem except @p keep — the
 *  stills and the notes of every other key, so a sketch that changed
 *  leaves no spent answer behind. */
void pruneThumbnails(const std::filesystem::path& dir, std::string_view stem,
                     const std::filesystem::path& keep);

/** HOW ONE RENDER ENDED. */
enum class ThumbnailOutcome {
  /** The still is on disk under the key it was asked for. */
  Wrote,
  /** The caller's stop was raised and the walk was abandoned. Nothing
   *  is on disk and nothing is known about the sketch: a stopped render
   *  is not a sketch that cannot be drawn. */
  Stopped,
  /** The sketch declared itself a plate and the run was not asked to
   *  walk those. Setup ran; the walk did not. */
  Heavy,
  /** The walk ran past the run's budget and was abandoned. */
  OverBudget,
  /** No kind, an empty canvas, no surface, or the write failed. */
  Failed,
};

/** ONE RENDER: where the still goes, how large, how long it may take and
 *  what stops it. */
struct ThumbnailRun {
  std::filesystem::path out;
  /** The still's larger side in pixels. */
  int maxDimension = kThumbnailWidth;
  /** How long the frame walk may take. Zero waits for the walk however
   *  long it is. */
  std::chrono::milliseconds budget = kThumbnailBudget;
  /** WALK A SKETCH THAT DECLARED ITSELF A PLATE. Such a sketch states
   *  that its subject is the sheet it draws rather than a scene it holds
   *  a frame rate at, which is the sketch most likely to spend a whole
   *  budget and answer nothing; it is stood down by that declaration
   *  unless this says otherwise. */
  bool heavy = false;
  /** READ BETWEEN FRAMES, and raising it abandons the walk at the next
   *  one. A still is a walk from zero to the sketch's moment, which for
   *  a sketch that names a late one is thousands of frames — long enough
   *  that whoever asked for it can be gone before it lands, and long
   *  enough that a caller waiting for the walk to end is a caller that
   *  has stopped answering. Null never stops. */
  const std::atomic_bool* stop = nullptr;
};

/** RENDERS ONE REGISTRY SKETCH'S STILL and writes it to `run.out`.
 *
 *  It opens the sketch's kind, steps it from zero at the sweep's own
 *  fixed rate to its declared moment (or the sweep's derived default when
 *  it names none), takes the still the plate tier takes, and scales it so
 *  its larger side is `run.maxDimension` pixels before encoding a PNG.
 *  CPU only — it allocates a raster surface and never a device one, so it
 *  can run on a worker that shares no graphics context. Any older
 *  thumbnail for the same stem under the output's directory is
 *  removed. */
[[nodiscard]] ThumbnailOutcome renderThumbnail(const Entry& entry,
                                               weave::FontContext& fonts,
                                               Assets& assets,
                                               const ThumbnailRun& run);

}  // namespace sigil::sketch
