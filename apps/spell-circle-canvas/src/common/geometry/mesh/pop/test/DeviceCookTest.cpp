/** @file
 * CONFORMANCE: every chain the device runtime says it can cook, cooked
 * both ways and compared BIT FOR BIT.
 *
 * Not a distance, and not a tolerance. The operators these chains are
 * made of are one piece of arithmetic compiled twice — to the C++ the
 * host executor calls and to the SPIR-V dispatched here — under a float
 * model pinned at both ends, so the two answers are either the same bits
 * or there is a defect to name. A tolerance here would hide exactly the
 * thing this test exists to see.
 */

#include <Primitives/interface/DebugOutput.h>
#include <gtest/gtest.h>
#include <sigilgeometry/mesh/pop/Kernel.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/mesh/pop/Pop.h>

#include <bit>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace sigil;
using sigil::geometry::mesh::Cloud;
using sigil::geometry::mesh::pop;

namespace {

/** A DEVICE AND THE POP RUNTIME ON IT, or the reason there is neither.
 *  Every test here SKIPS rather than fails without a Vulkan runtime, so
 *  a machine with no GPU stays green. */
struct OnDevice {
  std::unique_ptr<geometry::device::Device> device;
  pop::Runtime runtime;
  std::string error;
  explicit operator bool() const { return (bool)device; }
};

OnDevice onDevice() {
  OnDevice out;
  const geometry::device::DeviceConfig config;
  out.device = geometry::device::Device::create(config, &out.error);
  if (out.device) out.runtime = pop::deviceRuntime(*out.device);
  return out;
}

/** The closed loop every chain below is scattered along. */
std::vector<glm::vec3> loop() {
  return {
      {-120, 0, 0}, {-40, 70, 30}, {60, 20, -40}, {110, -50, 10}, {0, -80, 20}};
}

/** HOW FAR TWO COOKS STAND APART, which is meant to be nowhere: the
 *  number of float values whose BITS differ, and the first one's name
 *  and index. */
struct Difference {
  size_t values = 0;
  size_t differing = 0;
  std::string first;
  size_t at = 0;
  float host = 0;
  float device = 0;
};

void note(Difference* out, const std::string& lane, size_t index, float a,
          float b) {
  ++out->values;
  // Compared as BITS and not as numbers: two NaNs out of one formula are
  // the agreement they are, and a zero of either sign is not the same
  // answer as the other.
  if (std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b)) return;
  if (out->differing == 0) {
    out->first = lane;
    out->at = index;
    out->host = a;
    out->device = b;
  }
  ++out->differing;
}

Difference compare(const Cloud& host, const Cloud& device) {
  Difference out;
  if (host.positions.size() != device.positions.size()) {
    out.differing = 1;
    out.first = "positions (a different count)";
    return out;
  }
  for (size_t i = 0; i < host.positions.size(); ++i)
    for (int k = 0; k < 3; ++k)
      note(&out, "P", i, host.positions[i][k], device.positions[i][k]);
  for (const auto& [name, values] : host.scalars) {
    const std::vector<float>* other = device.scalarIf(name);
    if (!other || other->size() != values.size()) {
      out.differing += values.size();
      if (out.first.empty()) out.first = name + " (missing on the device)";
      continue;
    }
    for (size_t i = 0; i < values.size(); ++i)
      note(&out, name, i, values[i], (*other)[i]);
  }
  for (const auto& [name, values] : host.vectors) {
    const std::vector<glm::vec3>* other = device.vectorIf(name);
    if (!other || other->size() != values.size()) {
      out.differing += values.size();
      if (out.first.empty()) out.first = name + " (missing on the device)";
      continue;
    }
    for (size_t i = 0; i < values.size(); ++i)
      for (int k = 0; k < 3; ++k)
        note(&out, name, i, values[i][k], (*other)[i][k]);
  }
  for (const auto& [name, values] : host.colors) {
    const std::vector<glm::vec4>* other = device.colorIf(name);
    if (!other || other->size() != values.size()) {
      out.differing += values.size();
      if (out.first.empty()) out.first = name + " (missing on the device)";
      continue;
    }
    for (size_t i = 0; i < values.size(); ++i)
      for (int k = 0; k < 4; ++k)
        note(&out, name, i, values[i][k], (*other)[i][k]);
  }
  return out;
}

/** ONE CHAIN AND WHAT IT IS FOR, so a failure names the operator rather
 *  than an index into a list. */
struct Case {
  std::string what;
  pop::Chain chain;
};

/** Every operator the device runtime says it can cook, one chain each,
 *  and two that put several of them in a row — because an operator that
 *  agrees alone and disagrees after another has found a barrier and not
 *  a formula. */
std::vector<Case> everySupportedChain() {
  const int n = 4000;
  const glm::mat4 place =
      glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3{12, -7, 3}), 0.7f,
                  glm::vec3{0.3f, 0.9f, 0.2f});
  std::vector<Case> cases;
  const auto add = [&](std::string what, pop::Chain chain) {
    cases.push_back({std::move(what), std::move(chain)});
  };
  add("Jitter", pop::on(loop()).count(n).spread(9.0f).jitter(14.0f));
  add("Ramp", pop::on(loop()).count(n).fade({0.9f, 0.2f, 0.1f, 1.0f},
                                            {0.1f, 0.4f, 1.0f, 0.25f}));
  add("Vary", pop::on(loop()).count(n).vary(0.65f, 1.4f));
  add("LookAt", pop::on(loop()).count(n).lookAt({30, -90, 55}));
  add("Math", pop::on(loop()).count(n).move({13.5f, -2.25f, 7.125f}));
  add("Fill", pop::on(loop()).count(n).fill("heat", {0.3f, 0.6f, 0.9f, 1.0f}));
  add("Atlas", pop::on(loop()).count(n).atlas(3, 5));
  add("Lookup",
      pop::on(loop()).count(n).rampBy(
          geometry::mesh::pop::Lane::P, 1,
          {{0, 0, 0, 1}, {1, 0.5f, 0, 1}, {1, 1, 1, 1}}, -80.0f, 80.0f));
  add("Select and the mask it writes",
      pop::on(loop())
          .count(n)
          .select("core", {0, 0, 0}, 70.0f, 0.4f)
          .jitter(20.0f)
          .masked("core"));
  add("Select, box and inverted",
      pop::on(loop())
          .count(n)
          .select("slab", pop::Select::Shape::Box, {0, 0, 0}, {50, 200, 200},
                  0.0f, pop::Select::Combine::Replace, true)
          .move({0, 40, 0})
          .masked("slab"));
  add("Normal, unit and outward",
      pop::on(loop()).count(n).fill(pop::Lane::Dir, {3, 0, 0, 0}).normal(
          1.0f, {0, 0, 0}));
  add("Affine, as a placement", pop::on(loop()).count(n).affine(place));
  add("Affine, as a direction", pop::on(loop()).count(n).orient(place));
  add("Peak", pop::on(loop()).count(n).peak(18.0f));
  add("Mix, by a constant", pop::on(loop())
                                .count(n)
                                .fill("warm", {1, 0.4f, 0, 1})
                                .mix(geometry::mesh::pop::Lane::Color, "warm",
                                     geometry::mesh::pop::Lane::Color, 0.35f));
  add("Mix, by a lane", pop::on(loop())
                            .count(n)
                            .select("core", {0, 0, 0}, 70.0f, 0.5f)
                            .fill("warm", {1, 0.4f, 0, 1})
                            .mixBy(geometry::mesh::pop::Lane::Color, "warm",
                                   geometry::mesh::pop::Lane::Color, "core"));
  // …and the two that run several in a row.
  add("A whole chain", pop::on(loop())
                           .count(n)
                           .spread(11.0f)
                           .select("core", {20, 0, 0}, 90.0f, 0.35f)
                           .jitter(16.0f)
                           .masked("core")
                           .vary(0.5f)
                           .fade({1, 0.9f, 0.4f, 1}, {0.1f, 0.2f, 0.8f, 0.4f})
                           .atlas(2, 2)
                           .peak(6.0f)
                           .affine(place)
                           .lookAt({0, 200, 0}));
  Cloud given;
  given.positions = {{0, 0, 0}, {10, 4, -2}, {-6, 9, 3}, {2, -8, 11}};
  given.scalar("t") = {0.0f, 0.25f, 0.75f, 1.0f};
  given.scalar("heat") = {0.1f, 0.9f, 0.4f, 0.6f};
  add("A given point set, filtered",
      pop::on(given).jitter(3.0f).fade({1, 0, 0, 1}, {0, 0, 1, 1}).peak(2.0f));
  return cases;
}

/** WHAT THE BACKEND COMPLAINED ABOUT while a cook ran. Diligent reports
 *  a wrong barrier, a wrong binding or a wrong state through this
 *  callback and then carries on drawing the right picture, so a defect
 *  in the plumbing is invisible to a comparison of the answers — which
 *  is why it is read here rather than trusted to show up as a wrong
 *  number. The callback is a plain function pointer, so what it collects
 *  into is a plain global. */
std::vector<std::string>& complaints() {
  static std::vector<std::string> held;
  return held;
}

void DILIGENT_CALL_TYPE collect(Diligent::DEBUG_MESSAGE_SEVERITY severity,
                                const char* message, const char* function,
                                const char* file, int line) {
  (void)function;
  (void)file;
  (void)line;
  // Errors and worse only: a warning is Diligent telling the caller
  // about the machine, and this test is about the calls.
  if (severity < Diligent::DEBUG_MESSAGE_SEVERITY_ERROR) return;
  complaints().emplace_back(message ? message : "");
}

}  // namespace

TEST(DevicePop, ACookThatReadsBackAndCooksAgainDrawsNoComplaint) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;

  // A lane reaches a dispatch from three places, and only a SECOND cook
  // over held buffers visits all of them: the first cook creates the
  // lanes and dispatches over them, its readback leaves every one of
  // them a copy source, and the second cook re-uploads the seeded ones,
  // which leaves those a copy destination. A barrier naming one state
  // for all three is right for one of them.
  const pop::Chain chain = pop::on(loop())
                               .count(2048)
                               .spread(11.0f)
                               .select("core", {20, 0, 0}, 90.0f, 0.35f)
                               .jitter(16.0f)
                               .masked("core")
                               .fade({1, 0.9f, 0.4f, 1}, {0.1f, 0.2f, 0.8f, 0.4f});

  complaints().clear();
  const Diligent::DebugMessageCallbackType before =
      Diligent::DebugMessageCallback;
  Diligent::SetDebugMessageCallback(&collect);
  const Cloud once = pop::cook(chain, on.runtime);
  const Cloud twice = pop::cook(chain, on.runtime);
  Diligent::SetDebugMessageCallback(before);

  ASSERT_FALSE(once.positions.empty());
  EXPECT_EQ(once.positions.size(), twice.positions.size());
  EXPECT_TRUE(complaints().empty())
      << complaints().size() << " complaint(s), first: " << complaints().front();
  // …and the second cook is still the answer, so nothing was fixed by
  // barriering the lanes into silence.
  EXPECT_EQ(compare(pop::cook(chain, pop::Runtime::cpu()), twice).differing,
            0u);
}

TEST(DevicePop, EverySupportedChainCooksToTheSameBits) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;

  for (const Case& one : everySupportedChain()) {
    for (const pop::Op& op : one.chain)
      ASSERT_TRUE(on.runtime->supports(op))
          << one.what << ": the device declined an operator this test "
          << "expected it to run";
    const Cloud host = pop::cook(one.chain, pop::Runtime::cpu());
    const Cloud device = pop::cook(one.chain, on.runtime);
    ASSERT_FALSE(host.positions.empty()) << one.what;
    const Difference difference = compare(host, device);
    EXPECT_EQ(difference.differing, 0u)
        << one.what << ": " << difference.differing << " of "
        << difference.values << " values differ, first in \""
        << difference.first << "\" at " << difference.at << " — host "
        << difference.host << ", device " << difference.device;
  }
}

TEST(DevicePop, AnOperatorWithNoKernelIsDeclinedByName) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;

  // Each of these is a boundary the runtime states rather than a gap:
  // a neighbourhood read, a permutation, the primitive class, and two
  // operators defined in terms of a library sine.
  const pop::Chain relax = pop::on(loop()).count(64).smooth();
  const pop::Chain sorted = pop::on(loop()).count(64).order();
  const pop::Chain promoted =
      pop::on(loop()).count(64).promote(geometry::mesh::pop::Lane::Color, "c");
  const pop::Chain noised = pop::on(loop()).count(64).noise(4.0f);
  const pop::Chain twisted = pop::on(loop()).count(64).twist(45.0f);
  for (const pop::Chain& chain : {relax, sorted, promoted, noised, twisted}) {
    EXPECT_THROW(pop::cook(chain, on.runtime), std::runtime_error);
    // …and the host runs every one of them, so what the device declines
    // is a chain that still has an answer.
    EXPECT_FALSE(pop::cook(chain, pop::Runtime::cpu()).positions.empty());
  }
}

TEST(DevicePop, TwoRuntimesFromOneDeviceAreNotOneValue) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  const pop::Runtime again = pop::deviceRuntime(*on.device);
  // Each call holds its own pipeline and its own buffers, so a
  // reconciler comparing two descriptions sees two runtimes; a copy of
  // one is the one it was copied from.
  EXPECT_FALSE(on.runtime == again);
  const pop::Runtime copy = on.runtime;
  EXPECT_TRUE(copy == on.runtime);
  EXPECT_FALSE(on.runtime == pop::Runtime::cpu());
}

TEST(DevicePop, TheKernelAndTheRuntimeAgreeOnWhatHasOne) {
  // Whether an operator has a kernel is asked in ONE place, so a
  // runtime cannot come to hold a list of its own that drifts from it.
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  namespace kernel = geometry::mesh::kernel;
  const pop::Op jitter = pop::Jitter{};
  const pop::Op relax = pop::Relax{};
  EXPECT_TRUE(kernel::has(jitter));
  EXPECT_FALSE(kernel::has(relax));
  EXPECT_EQ(on.runtime->supports(jitter), kernel::has(jitter));
  EXPECT_EQ(on.runtime->supports(relax), kernel::has(relax));
}
