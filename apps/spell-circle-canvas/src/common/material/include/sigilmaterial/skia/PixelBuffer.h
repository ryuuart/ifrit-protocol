#pragma once

/** @file
 * @ingroup material-skia
 *
 * The caller-owned raster a material can sample. Write its pixels, call
 * `commit()`, then construct a fresh `skia::buffer()` paint and re-describe
 * its consumer to publish the edit.
 */

#include <include/core/SkRefCnt.h>

#include <cstdint>
#include <memory>

class SkBitmap;
class SkCanvas;
class SkImage;

namespace sigil::material::skia {

/** The caller-owned raster behind `skia::buffer()`: draw into `bitmap()`,
 *  or through `canvas()`, then call `commit()` and construct a fresh paint.
 *
 *  Each paint captures (source, revision) and that revision's image.
 *  Re-describing with a fresh paint between commits compares equal; a
 *  fresh paint after a commit carries the new pixels and compares unequal.
 *  Existing paints keep their captured pixels, including while fitted or
 *  panned. `image()` copies on the first request after each commit and
 *  reuses that snapshot on subsequent requests.
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
  /** Advances the revision. A fresh `skia::buffer()` paint captures the
   *  edited pixels; existing paints retain their previous snapshot. */
  void commit() { ++m_revision; }
  uint64_t revision() const { return m_revision; }
  /** The current snapshot, copied on the first request after a commit. */
  sk_sp<SkImage> image();

 private:
  struct State;  // SkBitmap + SkCanvas live out of line (heavy includes)
  std::unique_ptr<State> m_state;
  uint64_t m_revision = 0;
  uint64_t m_snapshotRevision = ~0ull;
  sk_sp<SkImage> m_snapshot;
};

}  // namespace sigil::material::skia
