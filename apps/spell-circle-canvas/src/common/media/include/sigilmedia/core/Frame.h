#pragma once

/** @file
 * @ingroup media-core
 * The picture at a moment and the clock it is read against: `Frame`, one
 * decoded picture and where it stands in its document; `Timing` and
 * `Loop`, how a caller's elapsed time becomes a document's; and
 * `HardwarePreference`, whether a codec may take the platform's video
 * device.
 */

/** @defgroup media-core Frames, documents and sources
 *  The values every consumer of pictures holds: one frame, the time it
 *  is asked at, a decoded image document, what a document says about
 *  itself, and the one seam every picture crosses into another library.
 *  @{ */
/** @} */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <chrono>
#include <cstdint>

#include "sigilmedia/advanced/Device.h"

/** What pictures MEAN, still and moving, for a caller that already has
 *  their bytes: an image document decoded whole, a video opened as a
 *  seekable clip whose frames decode around a playhead, the frame either
 *  answers for a time, the metadata read without a decode, the encoders
 *  that write pixels and frames back out, the one pixel source every
 *  other library takes, and the coverage, distance and difference
 *  answers a silhouette or a baseline needs. Where the bytes come from —
 *  a path, a URI, a mount, a cache — belongs to whoever fetched them;
 *  nothing here opens a file. */
namespace sigil::media {

/** Whether a codec may use a platform video device. */
enum class HardwarePreference {
  Disabled,   ///< Decode and encode on the CPU whatever the platform offers.
  Preferred,  ///< Take the device when it opens, fall back to the CPU.
  Required,   ///< Fail rather than fall back to the CPU.
};

/** What happens past a document's end. */
enum class Loop {
  /** As the document says: an animated image repeats as many times as
   *  its file states and then holds its last frame; a video repeats
   *  forever. */
  AsEncoded,
  Forever,  ///< Repeat whatever the document says.
  Once,     ///< Play once and hold the last frame.
};

/** HOW A CALLER'S ELAPSED TIME BECOMES A DOCUMENT'S: where in the
 *  document time zero stands, how fast it runs, and what happens past
 *  its end. Left alone, a document plays from its beginning at its own
 *  rate and loops as it says. */
struct Timing {
  /** Where in the document time zero stands. */
  std::chrono::duration<double> start{};
  /** Document seconds per caller second. */
  double rate = 1.0;
  Loop loop = Loop::AsEncoded;

  /** The document time to show at @p elapsed, for a document @p length
   *  long that repeats @p repetitions times as encoded (negative for
   *  forever): inside `[0, length)` while it plays, just short of
   *  `length` once a finite play is over, and never negative. A document
   *  with no length answers the elapsed time itself, from zero. */
  std::chrono::duration<double> documentTime(
      std::chrono::duration<double> elapsed,
      std::chrono::duration<double> length, int repetitions = -1) const;

  bool operator==(const Timing&) const = default;
};

/** ONE DECODED PICTURE AND WHERE IT STANDS IN ITS DOCUMENT: a still, one
 *  frame of an animation, one frame of a video, the newest frame of a
 *  stream. `image` is what every drawer takes.
 *  @trap A FRAME THAT STANDS ON A DEVICE CARRIES NO `image`: a hardware
 *  video frame, a texture another renderer painted. `deviceImage(frame,
 *  recorder)` binds it for the recorder that will draw it, or reads it
 *  back with none — which is what a leaf, a pen and a texture call. */
struct Frame {
  /** The picture, premultiplied and immutable; null for a frame that
   *  stands on a device and has not been bound. */
  sk_sp<SkImage> image;
  /** Where the frame begins on its document's clock. */
  std::chrono::duration<double> time{};
  /** How long it stands before the next one begins; zero for a still. */
  std::chrono::duration<double> duration{};
  /** Its position in its document, from zero. */
  int64_t index = 0;
  /** Where its pixels stand when they stand on a device. */
  DeviceFrame device;

  /** Whether anything is here: an image or a device surface. */
  explicit operator bool() const {
    return image != nullptr || static_cast<bool>(device);
  }
};

}  // namespace sigil::media
