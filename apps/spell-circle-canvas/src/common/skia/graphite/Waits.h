#pragma once

/** @file
 * What this library notes about a recorder it brought up: the GPU waits
 * its next inserted recording carries — what a texture another queue
 * writes needs before a draw here samples it — and how many recordings
 * this library has closed on it. Every insert this library makes takes
 * the waits right after its snap, so whichever submit carries the
 * sampling draw carries its wait, and the count rises with every snap.
 */

#include <include/gpu/graphite/BackendSemaphore.h>
#include <include/gpu/graphite/GraphiteTypes.h>

#include <cstdint>
#include <vector>

namespace skgpu::graphite {
class Recorder;
}  // namespace skgpu::graphite

namespace sigil::skia {

/** Forgets everything noted about @p recorder, which is going. */
void forgetRecorder(const skgpu::graphite::Recorder* recorder);

/** How many recordings this library has closed on @p recorder: every
 *  `PendingWaits` taken for it counts one, since each follows a snap. */
uint32_t recordingsClosed(const skgpu::graphite::Recorder* recorder);

/** THE WAITS ONE INSERT CARRIES, taken off the recorder's list: kept
 *  alive here for as long as the insert reads them. */
class PendingWaits {
 public:
  /** Takes every wait registered for @p recorder, leaving its list
   *  empty, and counts the recording just closed on it. */
  explicit PendingWaits(const skgpu::graphite::Recorder* recorder);

  /** Puts the waits taken on @p insert. */
  void attach(skgpu::graphite::InsertRecordingInfo& insert);

 private:
  std::vector<skgpu::graphite::BackendSemaphore> m_semaphores;
};

}  // namespace sigil::skia
