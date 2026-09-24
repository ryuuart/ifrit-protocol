#pragma once

/** @file
 * @ingroup sketch-testing
 *
 * THE HARNESS: a GoogleTest fixture that drives a sketch host in the
 * test's own process through the protocol — `TEST_F(Harness, …)` —
 * with a state directory of its own per case and hooks run in a fixed
 * order, and whose failure prints first what the host says about
 * itself.
 */

#include <gtest/gtest.h>
#include <sigilsketch/testing/Comparison.h>
#include <sigilsketch/testing/InProcessHost.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace sigil::sketch::testing {

/** A CASE THAT DRIVES A SKETCH HOST, in the test's own process, through
 *  the protocol a script drives one through over a socket.
 *
 *  Each case gets a fresh state directory under the system's temporary
 *  one, named for the case, and a fresh host over it. The hooks run in
 *  this order, each a no-op until a case's fixture overrides it:
 *
 *  1. `SetUpStateDirectory` — the directory stands and is empty: seed it.
 *  2. `SetUpHostOptions` — the options the host is about to be made
 *     with: change what a case needs changed.
 *  3. `SetUpOnHost` — the host stands, with the registry, session and
 *     clock agents mounted and a client attached: prepare what every case
 *     of the fixture shares.
 *  4. the case's body.
 *  5. `TearDownOnHost` — the host still stands.
 *
 *  The directory is removed after a case that passed and kept after one
 *  that failed, so the still a failure names can be looked at.
 *
 *  A FAILING CASE PRINTS FIRST, before the failure itself, the host's
 *  own account: `host.describe`'s answer, the clock as it stands and the
 *  last still's path — so a red case is triaged by what the layer below
 *  says before the assertion above it is read.
 *
 *  The verbs are the protocol's commands, each answered before it
 *  returns; a case asserts on the answer:
 *
 *      TEST_F(Harness, TheBoxHasMovedAtOneSecond) {
 *        ASSERT_TRUE(host().pinDensity());  // a plate's grid, from frame one
 *        ASSERT_TRUE(open("marching_box"));
 *        ASSERT_TRUE(clock(protocol::clock::Policy_Advance));
 *        ASSERT_TRUE(step(1.0));
 *        const auto picture = still(2.0);
 *        ASSERT_TRUE(picture);
 *        EXPECT_TRUE(compare(picture.result(), expected).identical());
 *      } */
class Harness : public ::testing::Test {
 protected:
  Harness();
  ~Harness() override;

  /** 1. Seed the state directory, which stands and is empty. */
  virtual void SetUpStateDirectory(const std::filesystem::path& directory) {
    (void)directory;
  }
  /** 2. Change what the host is about to be made with. */
  virtual void SetUpHostOptions(InProcessHostOptions& options) {
    (void)options;
  }
  /** 3. Prepare what every case of the fixture shares; the host stands. */
  virtual void SetUpOnHost() {}
  /** 5. Anything the case left to undo; the host still stands. */
  virtual void TearDownOnHost() {}

  /** Opens @p sketch, a registry name or a path, and waits for it. */
  protocol::Answer<protocol::session::values::Summary> open(
      const std::string& sketch);
  /** Sets how the clock moves, with a budget of clock seconds or none. */
  protocol::Answer<protocol::values::Empty> clock(
      protocol::clock::Policy policy,
      std::optional<double> budgetSeconds = std::nullopt);
  /** Steps @p seconds in whole frames of one over @p rate. */
  protocol::Answer<protocol::clock::values::StepResult> step(
      double seconds, double rate = 60.0);
  /** A still at @p density pixels per canvas unit, under the state
   *  directory at @p path or at a name the host picks. */
  protocol::Answer<protocol::session::values::StillResult> still(
      double density = 1.0, const std::string& path = {});
  /** A still held against the picture at @p expected. */
  [[nodiscard]] Comparison compare(
      const protocol::session::values::StillResult& still,
      const std::filesystem::path& expected) const;

  /** The host, for everything no verb names: `host().caller()` is what a
   *  generated client speaks through. */
  [[nodiscard]] InProcessHost& host() { return *m_host; }
  /** This case's state directory. */
  [[nodiscard]] const std::filesystem::path& stateDirectory() const {
    return m_stateDirectory;
  }

  /** What a failing case prints first. */
  std::string readout();

  void SetUp() final;
  void TearDown() final;

 private:
  std::filesystem::path m_stateDirectory;
  std::unique_ptr<InProcessHost> m_host;
};

}  // namespace sigil::sketch::testing
