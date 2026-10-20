/** @file
 * A Python session's live inputs: the feeds and connections it opens
 * through the hub close with it, whatever Python kept of them, and a
 * generation that fails or redeclares releases what it no longer holds.
 */

#include <gtest/gtest.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <sigilio/hub/Feed.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "PythonTestSupport.h"

namespace {

namespace sketch = sigil::sketch;
namespace fs = std::filesystem;
using sketch::test::fonts;
using sketch::test::PythonSource;

struct FeedCounts {
  std::map<std::string, int> opened;
  std::map<std::string, int> closed;
  std::map<std::string, sigil::io::Inlet> feeds;
};

struct FeedFixture {
  std::shared_ptr<FeedCounts> counts = std::make_shared<FeedCounts>();
  sketch::Assets services{fs::path{}};

  FeedFixture() {
    sigil::io::registerTransport(
        services.hub(), "fixture",
        [counts = counts](std::string_view uri, sigil::io::Inlet inlet) {
          const std::string key(uri);
          ++counts->opened[key];
          counts->feeds[key] = std::move(inlet);
          sigil::io::TransportEnd result;
          result.close = [counts, key] { ++counts->closed[key]; };
          result.send = [](const sigil::io::Bytes&) { return true; };
          return result;
        });
  }
};

TEST(SketchPython, ASessionClosesFeedsDespiteEscapedPythonWrappers) {
  PythonSource source("sigil_python_feed_escape");
  FeedFixture fixture;
  source.write("entry.py", R"PY(
import builtins
class Study:
    def setup(self, ctx):
        hub = ctx.assets.hub()
        builtins._sigil_escaped_feed = hub.listen('fixture://input')
        builtins._sigil_escaped_hub = hub
        hub.listen('fixture://unreferenced')
)PY");
  auto kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), fixture.services, true, "first");
  EXPECT_EQ(fixture.counts->closed["fixture://unreferenced"], 0);
  session.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://input"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://unreferenced"], 1);
  EXPECT_TRUE(fixture.counts->feeds["fixture://input"].expired());

  source.write("entry.py", R"PY(
import builtins
class Study:
    def setup(self, ctx):
        try:
            for action in (builtins._sigil_escaped_feed.latest,
                           lambda: builtins._sigil_escaped_hub.listen('fixture://input')):
                try:
                    action()
                except RuntimeError as error:
                    assert 'closed session' in str(error)
                else:
                    raise AssertionError('An expired session service remained usable')
        finally:
            del builtins._sigil_escaped_feed, builtins._sigil_escaped_hub
        self.feed = ctx.assets.hub().listen('fixture://input')
        assert self.feed.send(b'reopened')
)PY");
  kind = sketch::python::load(source.entry());
  session = kind->open(fonts(), fixture.services, true, "second");
  EXPECT_EQ(fixture.counts->opened["fixture://input"], 2);
  session.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://input"], 2);
}

TEST(SketchPython, ASessionClosesConnectionsDespiteEscapedPythonWrappers) {
  PythonSource source("sigil_python_connection_escape");
  FeedFixture fixture;
  source.write("entry.py", R"PY(
import builtins
from sigil import data
class Study:
    def setup(self, ctx):
        builtins._sigil_escaped_connection = data.connect(
            ctx.assets.hub(), 'fixture://connection')
)PY");
  auto kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), fixture.services, true, "first");
  EXPECT_EQ(fixture.counts->opened["fixture://connection"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://connection"], 0);
  EXPECT_FALSE(fixture.counts->feeds["fixture://connection"].expired());
  session.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://connection"], 1);
  EXPECT_TRUE(fixture.counts->feeds["fixture://connection"].expired());

  source.write("entry.py", R"PY(
import builtins
class Study:
    def setup(self, ctx):
        try:
            builtins._sigil_escaped_connection.state()
        except RuntimeError as error:
            assert 'closed session' in str(error)
        else:
            raise AssertionError('An escaped connection outlived its session')
        finally:
            del builtins._sigil_escaped_connection
)PY");
  kind = sketch::python::load(source.entry());
  session = kind->open(fonts(), fixture.services, true, "second");
  session.reset();
}

TEST(SketchPython, OverlappingAndFailedGenerationsShareTheWorkingFeed) {
  PythonSource source("sigil_python_feed_overlap");
  FeedFixture fixture;
  source.write("entry.py", R"PY(
class Study:
    def setup(self, ctx):
        self.feed = ctx.assets.hub().listen('fixture://shared')
    def update(self, elapsed, ctx):
        assert self.feed.send(b'working')
)PY");
  const auto good = sketch::python::load(source.entry());
  auto first = good->open(fonts(), fixture.services, true, "first");
  source.write("entry.py", R"PY(
class Study:
    def setup(self, ctx):
        ctx.assets.hub().listen('fixture://shared')
        ctx.assets.hub().listen('fixture://candidate')
        raise RuntimeError('candidate rejected')
)PY");
  const auto broken = sketch::python::load(source.entry());
  EXPECT_THROW((void)broken->open(fonts(), fixture.services, true, "broken"),
               std::runtime_error);
  EXPECT_EQ(fixture.counts->closed["fixture://candidate"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://shared"], 0);
  auto replacement = good->open(fonts(), fixture.services, true, "replacement");
  first.reset();
  EXPECT_EQ(fixture.counts->opened["fixture://shared"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://shared"], 0);
  SkNoDrawCanvas canvas(100, 100);
  EXPECT_NO_THROW(replacement->frame(canvas, 1.0 / 60.0));
  replacement.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://shared"], 1);
}

TEST(SketchPython, RedeclaringReleasesOmittedFeedsAndKeepsMatchingOnesOpen) {
  PythonSource source("sigil_python_feed_redeclare");
  FeedFixture fixture;
  source.write("entry.py", R"PY(
class Study:
    def setup(self, ctx):
        first = not hasattr(self, 'feed')
        self.feed = ctx.assets.hub().listen('fixture://retained')
        if first:
            ctx.assets.hub().listen('fixture://removed')
        assert self.feed.send(b'working')
)PY");
  const auto kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), fixture.services, true, "declared");
  session->redeclare();
  EXPECT_EQ(fixture.counts->opened["fixture://retained"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://retained"], 0);
  EXPECT_EQ(fixture.counts->closed["fixture://removed"], 1);
  session.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://retained"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://removed"], 1);
}

TEST(SketchPython, AFailedFrameReleasesTheSessionsLiveInputs) {
  PythonSource source("sigil_python_feed_failed_frame");
  FeedFixture fixture;
  source.write("entry.py", R"PY(
class Study:
    def setup(self, ctx):
        self.feed = ctx.assets.hub().listen('fixture://input')
    def update(self):
        raise RuntimeError('frame rejected')
)PY");
  const auto kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), fixture.services, true, "failed");
  SkNoDrawCanvas canvas(100, 100);
  EXPECT_THROW(session->frame(canvas, 1.0 / 60.0), std::runtime_error);
  EXPECT_EQ(fixture.counts->closed["fixture://input"], 1);
  session.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://input"], 1);
}

}  // namespace
