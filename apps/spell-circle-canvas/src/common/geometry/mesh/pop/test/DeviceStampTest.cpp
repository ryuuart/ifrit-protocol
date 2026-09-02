/** @file
 * THE STAMPING'S CONFORMANCE: one cloud stamped both ways and compared
 * BIT FOR BIT.
 *
 * Not a distance and not a tolerance. The vertex arithmetic is one piece
 * of Slang compiled twice — to the C++ the host executor calls and to
 * the SPIR-V this device dispatches — under a float model pinned at both
 * ends, so the two answers are the same bits or one of the pins has come
 * loose.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>

#include <bit>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace sigil;
namespace gm = sigil::geometry::mesh;
namespace points = sigil::geometry::mesh::points;

namespace {

/** A DEVICE AND THE STAMP RUNTIME ON IT, or the reason there is neither.
 *  Every test that needs one SKIPS rather than fails without a Vulkan
 *  runtime, so a machine with no GPU stays green. */
struct OnDevice {
  std::unique_ptr<geometry::device::Device> device;
  points::StampRuntime stamp;
  std::string error;
  explicit operator bool() const { return (bool)device; }
};

OnDevice onDevice() {
  OnDevice out;
  const geometry::device::DeviceConfig config;
  out.device = geometry::device::Device::create(config, &out.error);
  if (out.device) out.stamp = points::deviceRuntime(*out.device);
  return out;
}

/** A cloud with every conventional lane written, so no lane of the
 *  dispatch is the filled-in default: a direction that is not axis
 *  aligned, a size that varies, a tint that varies, and a texture window
 *  per point. */
gm::Cloud cloud(int count) {
  gm::pop::SplineScatter scatter;
  scatter.loop = {{-120, 0, -60},
                  {-40, 55, 90},
                  {70, -30, 60},
                  {130, 25, -50},
                  {30, -60, -110}};
  scatter.count = count;
  scatter.radius = 12.0f;
  return gm::pop::cook(gm::pop::Chain(gm::pop::on(scatter.loop)
                                          .count(count)
                                          .spread(12.0f)
                                          .jitter(9.0f)
                                          .vary(0.6f, 1.3f)
                                          .fade({0.9f, 0.2f, 0.1f, 1.0f},
                                                {0.1f, 0.4f, 1.0f, 0.25f})
                                          .atlas(3, 5)
                                          .lookAt({40, 90, -30})));
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
  if (host.normals.size() != device.normals.size() ||
      host.uvs.size() != device.uvs.size() ||
      host.colors.size() != device.colors.size())
    return ::testing::AssertionFailure() << "the lanes present differ";
  size_t differing = 0;
  const auto same3 = [&](glm::vec3 a, glm::vec3 b) {
    return bits(a.x) == bits(b.x) && bits(a.y) == bits(b.y) &&
           bits(a.z) == bits(b.z);
  };
  for (size_t i = 0; i < host.positions.size(); ++i) {
    if (!same3(host.positions[i], device.positions[i])) ++differing;
    if (i < host.normals.size() && !same3(host.normals[i], device.normals[i]))
      ++differing;
    if (i < host.uvs.size() && (bits(host.uvs[i].x) != bits(device.uvs[i].x) ||
                                bits(host.uvs[i].y) != bits(device.uvs[i].y)))
      ++differing;
    if (i < host.colors.size() &&
        (bits(host.colors[i].r) != bits(device.colors[i].r) ||
         bits(host.colors[i].g) != bits(device.colors[i].g) ||
         bits(host.colors[i].b) != bits(device.colors[i].b) ||
         bits(host.colors[i].a) != bits(device.colors[i].a)))
      ++differing;
  }
  if (differing != 0)
    return ::testing::AssertionFailure()
           << differing << " values differ in their bits";
  if (host.indices != device.indices)
    return ::testing::AssertionFailure() << "the topology differs";
  return ::testing::AssertionSuccess();
}

}  // namespace

TEST(DeviceStamp, TheRuntimeIsAValue) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;
  EXPECT_TRUE((bool)on.stamp);
  EXPECT_EQ(on.stamp->name(), "diligent");
  EXPECT_EQ(on.stamp, points::StampRuntime(on.stamp));
  EXPECT_NE(on.stamp, points::StampRuntime::cpu());
  EXPECT_NE(on.stamp, points::deviceRuntime(*on.device));
}

TEST(DeviceStamp, EveryStampIsBitIdenticalToTheHost) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  const gm::Cloud points_ = cloud(3000);
  // Three stamps, each carrying a different set of the optional lanes: a
  // solid with normals and uvs, a flat panel, and a bare triangle with
  // neither — because which lanes the result carries is the stamp's
  // answer and the kernel fills its own either way.
  gm::Mesh bare;
  bare.positions = {{-4, -4, 0}, {4, -4, 0}, {0, 5, 0}};
  bare.indices = {0, 1, 2};
  const gm::Mesh stamps[3] = {gm::superellipsoid({6, 6, 6}, 1, 8, 6),
                              gm::quad(7, 5), bare};

  for (const gm::Mesh& stamp : stamps) {
    points::InstanceOptions options = points::stampOptions(points_);
    const gm::Mesh host = points::instance(points_, stamp, options);
    options.runtime = on.stamp;
    EXPECT_TRUE(identical(host, points::instance(points_, stamp, options)))
        << "a stamp of " << stamp.vertexCount() << " vertices";
  }
}

TEST(DeviceStamp, AChainsStampedSinkIsTheSameOnEitherRuntime) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  // A cloud with no direction lane keeps the stamp's own axes, which is
  // the branch the oriented case does not take.
  gm::Cloud flat;
  for (int i = 0; i < 500; ++i)
    flat.positions.emplace_back((float)i, (float)(i % 7) * 3.0f,
                                (float)(i % 11) * -2.0f);
  const gm::Mesh stamp = gm::quad(3, 3);
  points::InstanceOptions options = points::stampOptions(flat);
  const gm::Mesh host = points::instance(flat, stamp, options);
  options.runtime = on.stamp;
  EXPECT_TRUE(identical(host, points::instance(flat, stamp, options)));
}
