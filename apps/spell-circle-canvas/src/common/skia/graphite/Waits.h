#pragma once

/** @file
 * What this library notes about a recorder it brought up: the Metal
 * device it draws on, and the GPU waits its next inserted recording
 * carries — what a texture another queue writes needs before a draw here
 * samples it. Every insert this library makes attaches the waits, so
 * whichever submit carries the sampling draw carries its wait.
 */

#include <include/gpu/graphite/BackendSemaphore.h>
#include <include/gpu/graphite/GraphiteTypes.h>

#include <vector>

namespace skgpu::graphite {
class Recorder;
}  // namespace skgpu::graphite

namespace sigil::skia {

/** THE WAITS ONE INSERT CARRIES, taken off the recorder's list: kept
 *  alive here for as long as the insert reads them. */
/** Notes that @p recorder draws on the Metal device @p mtlDevice. */
void noteMetalDevice(const skgpu::graphite::Recorder* recorder,
                     void* mtlDevice);
/** The Metal device noted for @p recorder; null when none was. */
void* metalDeviceOf(const skgpu::graphite::Recorder* recorder);
/** Forgets everything noted about @p recorder, which is going. */
void forgetRecorder(const skgpu::graphite::Recorder* recorder);

class PendingWaits {
 public:
  /** Takes every wait registered for @p recorder; the list is empty
   *  afterwards. */
  explicit PendingWaits(const skgpu::graphite::Recorder* recorder);

  /** Puts the waits taken on @p insert. */
  void attach(skgpu::graphite::InsertRecordingInfo& insert);

 private:
  std::vector<skgpu::graphite::BackendSemaphore> m_semaphores;
};

}  // namespace sigil::skia
