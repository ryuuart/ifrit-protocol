#pragma once

/** @file
 * The GPU device this process brought up, if it brought one up.
 */

namespace sigil::geometry::device {
class Device;
}  // namespace sigil::geometry::device

namespace sigil::sketch {

/** THE DEVICE THIS PROCESS HOLDS, or null.
 *
 *  A sketch draws through a runtime and never asks which one — that is
 *  what `sketch::runtime()` and `sketch::painterRuntime()` are for, and
 *  both answer on a machine with no device. This is the third answer,
 *  for the one thing a runtime cannot stand in for: a call that takes
 *  the device ITSELF, because what it does is give the device a handle
 *  over something the graphics API already holds. A foreign texture
 *  entering a material slot is the case — the pixels stand on one
 *  device and on no other, so there is nothing for a CPU executor to
 *  answer with.
 *
 *  NULL IS THE CPU TIER, and it is the answer a byte-identity plate is
 *  taken on, so a sketch that reaches for the device states what it
 *  draws without one:
 *
 *      if (auto* on = sketch::device())
 *        slot = world::diligent::importNative(*on, native);
 *
 *  A sketch that cannot draw its subject at all without one says so
 *  through `unavailable(...)` rather than drawing an empty set.
 *
 *  The device's own words are SigilGeometry's, from
 *  `<sigilgeometry/device/Device.h>`; a sketch walking through this door
 *  includes that header and whichever library's door takes a device.
 *
 *  A host that brought one up says so ONCE, beside the two runtimes it
 *  installs from the same device, because a device is a property of the
 *  process and not of a sketch. It borrows: the host owns the device and
 *  must say `useDevice(nullptr)` before letting it go. */
void useDevice(geometry::device::Device* device);
[[nodiscard]] geometry::device::Device* device();

}  // namespace sigil::sketch
