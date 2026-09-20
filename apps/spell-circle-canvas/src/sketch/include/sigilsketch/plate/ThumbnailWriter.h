#pragma once

/** @file
 * The encode and the write of a still already read back, off the thread
 * that drew it.
 */

#include <include/core/SkBitmap.h>

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace sigil::sketch {

/** THE ENCODE AND THE WRITE, OFF THE THREAD THAT DREW.
 *
 *  A still is read back where the device is, and nothing after that
 *  needs the device: the pixels are host memory nobody else can see. So
 *  the two expensive steps that follow — a PNG of a full-size frame,
 *  which for a still full of grain or scanlines defeats every row
 *  filter and deflates slowly, and the directory walk that removes the
 *  spent stills — belong on a worker. A frame that pays for them
 *  hitches on exactly the sketches worth looking at.
 *
 *  One worker, one write in flight, the asks waiting behind it, in the
 *  order they were made. It draws nothing and opens nothing: what it
 *  takes is pixels, so a case can assert what it writes without a
 *  session, and a host with a render thread hands over rather than
 *  waits.
 *
 *  The ask holds the bitmap it was given, so the caller's own may go the
 *  moment `write` returns. */
class ThumbnailWriter {
 public:
  /** Called ON THE WORKER once a still has landed, with the index the
   *  ask carried. Not called for an ask that could not be written. A
   *  host with a thread of its own marshals from there. */
  using Wrote = std::function<void(int index)>;

  explicit ThumbnailWriter(Wrote wrote = {});
  /** Finishes the write in flight and drops what is queued. A write is
   *  one encode over pixels that are already in hand, so finishing it
   *  is bounded in a way a render is not. */
  ~ThumbnailWriter();
  ThumbnailWriter(const ThumbnailWriter&) = delete;
  ThumbnailWriter& operator=(const ThumbnailWriter&) = delete;

  /** ONE STILL TO WRITE: @p still encoded as a PNG at @p outputPath,
   *  the spent stills and notes @p storeDirectory holds under @p stem
   *  removed, and @p index reported. The directories above the output
   *  are created. An ask carrying no pixels is dropped. */
  void write(int index, SkBitmap still, std::filesystem::path outputPath,
             std::filesystem::path storeDirectory, std::string stem);

  /** How many asks are waiting, the one in flight excluded. */
  [[nodiscard]] std::size_t pending() const;
  /** Whether a still is being written right now. */
  [[nodiscard]] bool writing() const;
  /** Blocks until the queue is empty and nothing is in flight — for a
   *  caller that must see the files, which is a test or a shutdown. */
  void drain();

 private:
  /** ONE ASK, holding everything its write needs: the worker reads no
   *  state of the caller's, so nothing the caller lets go of afterwards
   *  can be in the middle of being written. */
  struct Ask {
    int index = -1;
    SkBitmap still;
    std::filesystem::path outputPath;
    std::filesystem::path storeDirectory;
    std::string stem;
  };

  void loop();

  Wrote m_wrote;

  mutable std::mutex m_mutex;
  std::condition_variable m_wake;
  /** Raised when nothing is queued and nothing is in flight, for
   *  `drain` alone. */
  std::condition_variable m_idle;
  std::deque<Ask> m_pending;
  bool m_writing = false;
  bool m_stop = false;

  std::thread m_worker;
};

}  // namespace sigil::sketch
