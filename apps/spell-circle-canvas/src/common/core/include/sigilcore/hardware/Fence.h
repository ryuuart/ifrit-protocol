#pragma once

/** @file
 * THE TIMELINE TWO PROCESSORS AGREE ON: a value that only grows, raised
 * by the queue as it finishes work and waited on by the queue or by the
 * CPU. The value a signal issues is the value to wait for, so ordering
 * between the two sides is one number rather than a protocol.
 */

#include <chrono>
#include <cstdint>

namespace sigil::core::hardware {

/**
 * A fence is a timeline: a 64-bit value that only ever grows. Signalling
 * it from the GPU raises the value once the work queued before the
 * signal has finished; a wait — on the GPU, queued so later work holds,
 * or on the CPU, blocking — passes once the value has reached the one
 * asked for. Values are issued by the device, one per signal, so the
 * value a signal returns is the one to wait for. On Metal this is an
 * MTLSharedEvent; on Vulkan a timeline semaphore.
 */
using FenceValue = uint64_t;

/** The value every fence starts at; nothing has been signalled. */
constexpr FenceValue kFenceInitialValue = 0;

/** The wait a CPU-side wait uses when it is given none. */
constexpr std::chrono::milliseconds kFenceDefaultTimeout{1000};

/** What a CPU-side wait came back with. */
enum class FenceWait {
  /** The value was reached. */
  Reached,
  /** The timeout passed first; the fence may still reach it later. */
  TimedOut,
  /** The handle names no fence. */
  Invalid,
};

}  // namespace sigil::core::hardware
