#pragma once

/** @file
 * THE CALLER-OWNED RASTER a material can sample: a bitmap one owner
 * writes into and publishes with `commit()`, so content changes without a
 * re-describe. `Paint::buffer()` is the door that turns one into a fill.
 */

#include <include/core/SkRefCnt.h>

#include <cstdint>
#include <memory>

class SkBitmap;
class SkCanvas;
class SkImage;

namespace sigil::material::skia {

/** The caller-owned raster behind `Paint::buffer()`: draw into
 *  `bitmap()`, or through `canvas()`, then `commit()` to publish.
 *
 *  The material's recipe carries (source, revision), so an identical
 *  re-describe between commits prunes and nothing repaints, and the first
 *  describe after a commit patches exactly once. `image()` snapshots
 *  lazily and caches per revision, so a describe that prunes copies no
 *  pixels at all.
 *
 *  Not thread-safe, deliberately: one owner, one writer. */
class PixelBuffer {
 public:
  PixelBuffer(int width, int height);
  ~PixelBuffer();
  PixelBuffer(const PixelBuffer&) = delete;
  PixelBuffer& operator=(const PixelBuffer&) = delete;

  /** The pixels, yours to write. N32 premul. */
  SkBitmap& bitmap();
  /** A raster canvas over the same pixels — the convenient writer. */
  SkCanvas& canvas();
  /** PUBLISH the edit: the next describe carries the new revision and
   *  the reconciler repaints the material's node exactly once. */
  void commit() { ++m_revision; }
  uint64_t revision() const { return m_revision; }
  /** The current snapshot (copied from the bitmap once per revision). */
  sk_sp<SkImage> image();

 private:
  struct State;  // SkBitmap + SkCanvas live out of line (heavy includes)
  std::unique_ptr<State> m_state;
  uint64_t m_revision = 0;
  uint64_t m_snapshotRevision = ~0ull;
  sk_sp<SkImage> m_snapshot;
};

}  // namespace sigil::material::skia
