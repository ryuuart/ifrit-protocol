#pragma once

/** @file
 * What this library notes about a recorder it brought up: the GPU waits
 * its next inserted recording carries — what a texture another queue
 * writes needs before a draw here samples it. Every insert this library
 * makes takes the waits right after its snap, so whichever submit carries
 * the sampling draw carries its wait.
 */

#include <include/gpu/graphite/BackendSemaphore.h>
#include <include/gpu/graphite/GraphiteTypes.h>

#include <chrono>
#include <memory>
#include <vector>

namespace skgpu::graphite {
class Recorder;
}  // namespace skgpu::graphite

namespace sigil::skia {

/** Forgets everything noted about @p recorder, which is going. */
void forgetRecorder(const skgpu::graphite::Recorder* recorder);

/** THE WAITS ONE INSERT CARRIES, taken off the recorder's list: kept
 *  alive here for as long as the insert reads them. */
class PendingWaits {
 public:
  /** Takes every wait registered for @p recorder, leaving its list
   *  empty. */
  explicit PendingWaits(const skgpu::graphite::Recorder* recorder);

  /** Puts the waits taken on @p insert. */
  void attach(skgpu::graphite::InsertRecordingInfo& insert);

 private:
  std::vector<skgpu::graphite::BackendSemaphore> m_semaphores;
};

/** A SIGNAL THE DEVICE RAISES when it reaches it, and a host wait on it
 *  that holds nothing: carried by the last insert of a submit, it is
 *  raised once everything that submit carries has run, so a reader waits
 *  exactly as long as the device takes rather than asking the context
 *  over and over. On Metal it is a shared event the queue raises; on
 *  Vulkan the submit signals a semaphore, and a submit of its own that
 *  waits on it raises a fence the host waits on. Empty where the backend
 *  offers neither, which the caller answers by asking the context
 *  instead. */
class DeviceSignal {
 public:
  /** A signal on the device @p recorder draws on, or an empty one. */
  explicit DeviceSignal(const skgpu::graphite::Recorder& recorder);
  ~DeviceSignal();
  DeviceSignal(const DeviceSignal&) = delete;
  DeviceSignal& operator=(const DeviceSignal&) = delete;

  explicit operator bool() const { return m_form != nullptr; }
  /** Puts the signal on @p insert, which must outlive the insert. */
  void attach(skgpu::graphite::InsertRecordingInfo& insert);
  /** Hands the queue what raises the signal for the host, once the
   *  submit carrying the insert has gone: called right after that submit,
   *  under the same locks, since it may submit to the same queue. Nothing
   *  to do on Metal, whose event the host reads itself. */
  void afterSubmit();
  /** Waits until the device raises the signal or @p deadline passes, and
   *  answers whether it was raised. */
  bool wait(std::chrono::steady_clock::time_point deadline) const;

  /** THE SIGNAL ON ONE BACKEND, made by that backend's unit. */
  class Form {
   public:
    virtual ~Form() = default;
    virtual void attach(skgpu::graphite::InsertRecordingInfo& insert) = 0;
    virtual void afterSubmit() = 0;
    virtual bool wait(std::chrono::steady_clock::time_point deadline) const = 0;
  };

 private:
  std::unique_ptr<Form> m_form;
};

/** The signal on Metal, a shared event on the device @p recorder draws
 *  on; null where that device makes none. Defined on Apple alone. */
std::unique_ptr<DeviceSignal::Form> metalSignal(
    const skgpu::graphite::Recorder& recorder);

/** The signal on Vulkan: a semaphore the submit raises and a fence a
 *  submit of its own raises after it, on @p device and @p queue, whose
 *  entry points @p getDeviceProcAddr — a PFN_vkGetDeviceProcAddr bridged
 *  to void* — answers; null where any of them is missing or this build's
 *  Skia carries no Vulkan backend. */
std::unique_ptr<DeviceSignal::Form> vulkanSignal(void* device, void* queue,
                                                 void* getDeviceProcAddr);

}  // namespace sigil::skia
