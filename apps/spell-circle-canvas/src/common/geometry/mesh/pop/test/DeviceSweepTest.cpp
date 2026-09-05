/** @file
 * THE SWEEP'S CONFORMANCE: every rail and profile swept both ways and
 * compared BIT FOR BIT.
 *
 * Not a distance and not a tolerance. The ring arithmetic is one piece
 * of Slang compiled twice — to the C++ the host executor calls and to
 * the SPIR-V this device dispatches — under a float model pinned at both
 * ends, so the two answers are the same bits or one of the pins has come
 * loose.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/mesh/pop/Sweep.h>

#include <bit>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace sigil;
namespace gm = sigil::geometry::mesh;
namespace curve = sigil::geometry::mesh::curve;
namespace pop = sigil::geometry::mesh::pop;

namespace {

/** A DEVICE AND THE SWEEP RUNTIME ON IT, or the reason there is neither.
 *  Every test that needs one SKIPS rather than fails without a Vulkan
 *  runtime, so a machine with no GPU stays green. */
struct OnDevice {
  std::unique_ptr<geometry::device::Device> device;
  pop::SweepRuntime sweep;
  std::string error;
  explicit operator bool() const { return (bool)device; }
};

OnDevice onDevice() {
  OnDevice out;
  const geometry::device::DeviceConfig config;
  out.device = geometry::device::Device::create(config, &out.error);
  if (out.device) out.sweep = pop::sweepDeviceRuntime(*out.device);
  return out;
}

/** A closed loop that turns in all three axes, so no ring's frame is
 *  axis-aligned and every component of the arithmetic is exercised. */
curve::Spline3 loop() {
  curve::Spline3 spline;
  spline.points = {{-120, 0, -60},
                   {-40, 55, 90},
                   {70, -30, 60},
                   {130, 25, -50},
                   {30, -60, -110}};
  spline.closed = true;
  return spline;
}

/** …and an open one, which is what a cap has ends to close. */
curve::Spline3 arc() {
  curve::Spline3 spline;
  spline.points = {{-90, -20, 0}, {-20, 60, 35}, {50, 10, -25}, {110, 70, 40}};
  return spline;
}

/** Bit-for-bit over a mesh's every lane. Compared as the BITS of each
 *  float rather than as floats, so two values that differ in their last
 *  place are two values and not one — and so that the comparison says
 *  what it means about a NaN and about the two zeroes, which compare
 *  equal as numbers and are different answers. */
::testing::AssertionResult identical(const gm::Mesh& host,
                                     const gm::Mesh& device) {
  const auto bits = [](float value) { return std::bit_cast<uint32_t>(value); };
  if (host.positions.size() != device.positions.size())
    return ::testing::AssertionFailure()
           << "vertex counts differ: " << host.positions.size() << " and "
           << device.positions.size();
  size_t differing = 0;
  const auto same3 = [&](glm::vec3 a, glm::vec3 b) {
    return bits(a.x) == bits(b.x) && bits(a.y) == bits(b.y) &&
           bits(a.z) == bits(b.z);
  };
  for (size_t i = 0; i < host.positions.size(); ++i) {
    if (!same3(host.positions[i], device.positions[i])) ++differing;
    if (i < host.normals.size() && i < device.normals.size() &&
        !same3(host.normals[i], device.normals[i]))
      ++differing;
    if (i < host.uvs.size() && i < device.uvs.size() &&
        (bits(host.uvs[i].x) != bits(device.uvs[i].x) ||
         bits(host.uvs[i].y) != bits(device.uvs[i].y)))
      ++differing;
  }
  if (differing != 0)
    return ::testing::AssertionFailure()
           << differing << " values differ in their bits";
  if (host.indices != device.indices)
    return ::testing::AssertionFailure() << "the topology differs";
  return ::testing::AssertionSuccess();
}

const char* nameOf(pop::SweepOptions::Normals rule) {
  switch (rule) {
    case pop::SweepOptions::Normals::Radial:
      return "Radial";
    case pop::SweepOptions::Normals::Frame:
      return "Frame";
    case pop::SweepOptions::Normals::Geometric:
      return "Geometric";
  }
  return "?";
}

}  // namespace

TEST(DeviceSweep, TheRuntimeIsAValue) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;
  EXPECT_TRUE((bool)on.sweep);
  EXPECT_EQ(on.sweep->name(), "diligent");
  EXPECT_EQ(on.sweep, pop::SweepRuntime(on.sweep));
  EXPECT_NE(on.sweep, pop::SweepRuntime::cpu());
  EXPECT_NE(on.sweep, pop::sweepDeviceRuntime(*on.device));
}

TEST(DeviceSweep, EveryNormalRuleIsBitIdenticalToTheHost) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  for (pop::SweepOptions::Normals rule :
       {pop::SweepOptions::Normals::Radial, pop::SweepOptions::Normals::Frame,
        pop::SweepOptions::Normals::Geometric}) {
    for (bool closed : {true, false}) {
      const curve::Spline3 spline = closed ? loop() : arc();
      const std::vector<curve::Frame3> rail = curve::frames(spline, 64);
      for (const auto& contour :
           {pop::profile::circle(12), pop::profile::line()}) {
        pop::SweepOptions options;
        options.normals = rule;
        options.scale = 14.0f;
        options.caps = !closed;
        const gm::Mesh host = pop::sweep(rail, contour, options);
        options.runtime = on.sweep;
        const gm::Mesh device = pop::sweep(rail, contour, options);
        EXPECT_TRUE(identical(host, device))
            << nameOf(rule) << (closed ? " on a loop" : " on an arc")
            << " with a " << contour.points.size() << "-point profile";
      }
    }
  }
}

TEST(DeviceSweep, ATaperReachesTheDeviceAsTheSizeItResolvedTo) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  // A taper is an arbitrary host function; what crosses is the number it
  // answered, once per ring, so the two tiers scale by the same bits.
  pop::SweepOptions options;
  options.scale = 20.0f;
  options.taper = [](float t) { return 0.2f + 0.9f * t * t; };
  const std::vector<curve::Frame3> rail = curve::frames(loop(), 48);
  const sigil::geometry::path::Polyline contour = pop::profile::circle(9);
  const gm::Mesh host = pop::sweep(rail, contour, options);
  options.runtime = on.sweep;
  EXPECT_TRUE(identical(host, pop::sweep(rail, contour, options)));
}

TEST(DeviceSweep, ASplineSweepsTheSameOnEitherRuntime) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  // The spline overload builds the rail first and then hands it over, so
  // the runtime reaches the rings through the options exactly as the
  // rail overload does.
  pop::SweepOptions options;
  options.segments = 128;
  options.scale = 9.0f;
  const gm::Mesh host = pop::sweep(loop(), pop::profile::circle(16), options);
  options.runtime = on.sweep;
  EXPECT_TRUE(
      identical(host, pop::sweep(loop(), pop::profile::circle(16), options)));
}
