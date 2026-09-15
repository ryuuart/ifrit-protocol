/** @file
 * The wires: what a URI nobody can open leaves behind, what two wires on
 * the loopback carry between them, the sender a message is kept with,
 * the rate a tick reads off a wire, the file a recording writes and the
 * wire that plays it back, and the readings a message is shown through.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Recording.h>
#include <sigilseer/wire/Log.h>
#include <sigilseer/wire/Recorder.h>
#include <sigilseer/wire/Rendering.h>
#include <sigilseer/wire/Sender.h>
#include <sigilseer/wire/Wires.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

using sigil::io::Bytes;
using sigil::io::Feed;
using sigil::seer::Log;
using sigil::seer::Recorder;
using sigil::seer::Sender;
using sigil::seer::Vitals;
using sigil::seer::Wires;
using namespace std::chrono_literals;

namespace {

Bytes bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  Bytes out;
  out.bytes.assign(first, first + text.size());
  return out;
}

/** The port out of the address a feed reports. An IPv6 address is
 *  bracketed, so the port is always what follows the last colon. */
uint16_t portOf(const std::string& address) {
  const size_t colon = address.rfind(':');
  if (colon == std::string::npos) return 0;
  return static_cast<uint16_t>(std::stoul(address.substr(colon + 1)));
}

/** Polls @p ready until it holds, or gives up. A datagram travels on the
 *  transport's own thread, so there is nothing here to pump — only a
 *  moment to give it, and a deadline long enough that a loaded machine
 *  is not mistaken for a wire that carries nothing. */
bool waitUntil(const std::function<bool()>& ready) {
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (ready()) return true;
    std::this_thread::sleep_for(1ms);
  }
  return ready();
}

/** A directory of its own for the cases that write recordings, taken
 *  away again whatever the case did with it. */
class SeerScratch : public ::testing::Test {
 protected:
  void SetUp() override {
    directory =
        fs::temp_directory_path() / ("seer_test_" + std::to_string(::getpid()) +
                                     "_" + std::to_string(++counter));
    fs::create_directories(directory);
  }

  void TearDown() override {
    std::error_code ignored;
    fs::remove_all(directory, ignored);
  }

  fs::path directory;
  static inline int counter = 0;
};

class SeerRecorder : public SeerScratch {};

}  // namespace

TEST(SeerWires, AUriNoTransportOpensAnswersAWireThatSaysSo) {
  Wires wires;
  const std::shared_ptr<Feed> feed = wires.open("pigeon://the.desk");
  ASSERT_NE(feed, nullptr);
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
  EXPECT_EQ(feed->latest(), nullptr);
  // It is a wire all the same: a reader has to see the sentence beside
  // the URI they mistyped, which means the row has to be there.
  ASSERT_EQ(wires.feeds().size(), 1u);
  wires.tick(0.0);
  ASSERT_EQ(wires.vitals().size(), 1u);
  EXPECT_EQ(wires.vitals()[0].uri, "pigeon://the.desk");
  EXPECT_FALSE(wires.vitals()[0].error.empty());
}

TEST(SeerWires, OpeningAUriThatIsAlreadyOpenAnswersTheWireThatIsThere) {
  Wires wires;
  const std::shared_ptr<Feed> first = wires.open("pigeon://the.desk");
  const std::shared_ptr<Feed> again = wires.open("pigeon://the.desk");
  EXPECT_EQ(first, again);
  EXPECT_EQ(wires.feeds().size(), 1u);

  EXPECT_TRUE(wires.close("pigeon://the.desk"));
  EXPECT_TRUE(wires.feeds().empty());
  EXPECT_FALSE(wires.close("pigeon://the.desk"));
}

TEST(SeerWires, TwoWiresOnTheLoopbackCarryBytesAndTheLogDrainsThemInOrder) {
  Wires wires;
  const std::shared_ptr<Feed> listener = wires.open("udp://:0");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);

  Sender sender(wires);
  const std::shared_ptr<Feed> peer =
      sender.openPeer("udp://127.0.0.1:" + std::to_string(port));
  ASSERT_TRUE(peer->error().empty()) << peer->error();
  // The peer is a wire like any other, so both stand in the list in the
  // order they were opened.
  ASSERT_EQ(wires.feeds().size(), 2u);
  EXPECT_EQ(wires.feeds()[0], listener);
  EXPECT_EQ(wires.feeds()[1], peer);

  EXPECT_TRUE(sender.send(bytesOf("first")));
  ASSERT_TRUE(waitUntil([&] { return listener->generation() >= 1; }));
  EXPECT_TRUE(sender.send(bytesOf("second")));
  ASSERT_TRUE(waitUntil([&] { return listener->generation() >= 2; }));
  EXPECT_EQ(sender.sent(), 2u);

  Log log;
  EXPECT_EQ(log.drain(*listener), 2u);
  ASSERT_EQ(log.entries().size(), 2u);
  EXPECT_EQ(log.entries()[0].bytes->asText(), "first");
  EXPECT_EQ(log.entries()[0].generation, 1u);
  EXPECT_EQ(log.entries()[0].size, 5u);
  EXPECT_EQ(log.entries()[1].bytes->asText(), "second");
  EXPECT_EQ(log.entries()[1].generation, 2u);
  // Drained is handed out: a second drain of the same wire takes
  // nothing, and the log keeps what it already took.
  EXPECT_EQ(log.drain(*listener), 0u);
  EXPECT_EQ(log.entries().size(), 2u);
}

TEST(SeerLog, AnEntryCarriesTheSenderItWasDeliveredWith) {
  Wires wires;
  const std::shared_ptr<Feed> feed = wires.open("pigeon://the.desk");
  feed->deliver(bytesOf("from the field"),
                std::string("udp://127.0.0.1:52341"));
  feed->deliver(bytesOf("from nobody"));

  Log log;
  EXPECT_EQ(log.drain(*feed), 2u);
  ASSERT_EQ(log.entries().size(), 2u);
  // One wire carries messages from many senders, so the sender belongs
  // to the message: a reader looking down the log sees which of them
  // each line came from.
  EXPECT_EQ(log.entries()[0].from, "udp://127.0.0.1:52341");
  // A transport with no way of knowing, and a recording, name nobody.
  EXPECT_TRUE(log.entries()[1].from.empty());
}

TEST(SeerWires, AWireNamesTheSenderOfTheMessageLastTakenOffIt) {
  Wires wires;
  wires.open("pigeon://the.desk");
  wires.tick(0.0);
  // A wire nobody has taken a message off names nobody: the sender
  // travels with the arrival, and an arrival nobody took is one nobody
  // has read a sender out of.
  ASSERT_NE(wires.vitalsOf("pigeon://the.desk"), nullptr);
  EXPECT_TRUE(wires.vitalsOf("pigeon://the.desk")->lastFrom.empty());

  wires.rememberSender("pigeon://the.desk", "udp://127.0.0.1:52341");
  wires.tick(1.0);
  EXPECT_EQ(wires.vitalsOf("pigeon://the.desk")->lastFrom,
            "udp://127.0.0.1:52341");

  // A URI no wire is open on is passed over rather than remembered
  // against a wire that is not there.
  wires.rememberSender("pigeon://elsewhere", "udp://127.0.0.1:52342");
  wires.tick(2.0);
  EXPECT_EQ(wires.vitals().size(), 1u);
  EXPECT_EQ(wires.vitalsOf("pigeon://the.desk")->lastFrom,
            "udp://127.0.0.1:52341");
}

TEST(SeerLog, AFullLogLetsGoOfTheOldestAndCountsIt) {
  Wires wires;
  const std::shared_ptr<Feed> feed = wires.open("pigeon://the.desk");
  feed->deliver(bytesOf("one"));
  feed->deliver(bytesOf("two"));
  feed->deliver(bytesOf("three"));

  Log log(2);
  EXPECT_EQ(log.drain(*feed), 3u);
  ASSERT_EQ(log.entries().size(), 2u);
  EXPECT_EQ(log.entries()[0].bytes->asText(), "two");
  EXPECT_EQ(log.entries()[1].bytes->asText(), "three");
  EXPECT_EQ(log.forgotten(), 1u);

  log.clear();
  EXPECT_TRUE(log.entries().empty());
  EXPECT_EQ(log.forgotten(), 1u);
}

TEST(SeerWires, VitalsCountARateOverTheLastSecondOfTicks) {
  Wires wires;
  const std::shared_ptr<Feed> feed = wires.open("pigeon://the.desk");

  // Four messages over the second between these two ticks, which is
  // four a second whatever the wall clock did meanwhile.
  wires.tick(0.0);
  for (int number = 0; number != 4; ++number)
    feed->deliver(bytesOf(std::to_string(number)));
  wires.tick(1.0);

  const Vitals* vitals = wires.vitalsOf("pigeon://the.desk");
  ASSERT_NE(vitals, nullptr);
  EXPECT_EQ(vitals->generation, 4u);
  EXPECT_DOUBLE_EQ(vitals->arrivalsPerSecond, 4.0);
  ASSERT_NE(vitals->newest, nullptr);
  EXPECT_EQ(vitals->newest->asText(), "3");
  EXPECT_FALSE(vitals->closed);

  // A second of ticks with nothing on the wire reads as a wire with
  // nothing on it, rather than as the rate it last had.
  wires.tick(2.0);
  wires.tick(3.0);
  EXPECT_DOUBLE_EQ(wires.vitalsOf("pigeon://the.desk")->arrivalsPerSecond, 0.0);
}

TEST(SeerWires, ATickReadsAClosedWireAsClosed) {
  Wires wires;
  const std::shared_ptr<Feed> feed = wires.open("pigeon://the.desk");
  feed->deliver(bytesOf("last words"));
  feed->close();

  wires.tick(0.0);
  const Vitals* vitals = wires.vitalsOf("pigeon://the.desk");
  ASSERT_NE(vitals, nullptr);
  EXPECT_TRUE(vitals->closed);
  EXPECT_EQ(vitals->generation, 1u);
}

TEST_F(SeerRecorder, ARecordedWireIsAFileReadRecordingReadsBack) {
  Wires wires;
  const std::shared_ptr<Feed> feed = wires.open("pigeon://the.desk");
  Recorder recorder(wires);

  const fs::path file = directory / "desk.feed";
  ASSERT_TRUE(recorder.record(feed, file));
  EXPECT_TRUE(recorder.recording());
  EXPECT_EQ(recorder.path(), file);

  feed->deliver(bytesOf("first"), 0.25);
  feed->deliver(bytesOf("second"), 0.75);
  recorder.stop();
  EXPECT_FALSE(recorder.recording());

  const std::optional<std::vector<sigil::io::Arrival>> read =
      sigil::io::readRecording(file);
  ASSERT_TRUE(read.has_value());
  ASSERT_EQ(read->size(), 2u);
  EXPECT_EQ((*read)[0].bytes->asText(), "first");
  EXPECT_DOUBLE_EQ((*read)[0].at, 0.25);
  EXPECT_EQ((*read)[1].bytes->asText(), "second");
  EXPECT_DOUBLE_EQ((*read)[1].at, 0.75);
}

TEST_F(SeerRecorder, AReplayedWireDeliversTheRecordingAsTimeIsDispatched) {
  Wires wires;
  const fs::path file = directory / "desk.feed";
  {
    const std::shared_ptr<Feed> live = wires.open("pigeon://the.desk");
    Recorder recorder(wires);
    ASSERT_TRUE(recorder.record(live, file));
    live->deliver(bytesOf("first"), 0.25);
    live->deliver(bytesOf("second"), 0.75);
  }

  Recorder recorder(wires);
  const std::shared_ptr<Feed> replayed =
      recorder.replay("pigeon://the.desk", file);
  ASSERT_TRUE(replayed->error().empty()) << replayed->error();
  // The wire that was there is gone, and the one that took its place is
  // the file: one row, not two.
  ASSERT_EQ(wires.feeds().size(), 1u);
  EXPECT_EQ(wires.feeds()[0], replayed);

  // The first dispatch is where the recording starts, whatever the
  // caller's clock reads then: nothing is due at its own origin.
  wires.dispatch(10.0);
  EXPECT_EQ(replayed->generation(), 0u);
  wires.dispatch(10.3);
  EXPECT_EQ(replayed->generation(), 1u);
  EXPECT_EQ(replayed->latest()->asText(), "first");
  EXPECT_FALSE(replayed->closed());
  wires.dispatch(11.0);
  EXPECT_EQ(replayed->generation(), 2u);
  EXPECT_EQ(replayed->latest()->asText(), "second");
  // Nothing else is coming, and the wire says so rather than waiting on
  // a door that will not open again.
  EXPECT_TRUE(replayed->closed());
}

TEST_F(SeerRecorder, ReplayingAFileThatIsNoRecordingSaysSoOnTheWire) {
  Wires wires;
  const fs::path file = directory / "not.feed";
  {
    std::ofstream(file) << "a note to nobody";
  }

  Recorder recorder(wires);
  const std::shared_ptr<Feed> replayed =
      recorder.replay("pigeon://the.desk", file);
  EXPECT_FALSE(replayed->error().empty());
  wires.dispatch(0.0);
  EXPECT_EQ(replayed->generation(), 0u);
}

TEST(SeerSender, ARepeatSendsOnceEveryPeriodTheTicksPassThrough) {
  Wires wires;
  const std::shared_ptr<Feed> listener = wires.open("udp://:0");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);

  Sender sender(wires);
  ASSERT_TRUE(sender.openPeer("udp://127.0.0.1:" + std::to_string(port))
                  ->error()
                  .empty());
  sender.repeat(bytesOf("again"), 0.1);
  EXPECT_TRUE(sender.repeating());

  // The first tick after a repeat begins is the first send; a tick
  // before the next is due sends nothing.
  sender.tick(0.0);
  EXPECT_EQ(sender.sent(), 1u);
  sender.tick(0.05);
  EXPECT_EQ(sender.sent(), 1u);
  sender.tick(0.1);
  EXPECT_EQ(sender.sent(), 2u);

  sender.stopRepeating();
  sender.tick(0.2);
  EXPECT_EQ(sender.sent(), 2u);
  EXPECT_FALSE(sender.repeating());

  ASSERT_TRUE(waitUntil([&] { return listener->generation() >= 2; }));
  EXPECT_EQ(listener->latest()->asText(), "again");
}

TEST(SeerSender, AWireWithNoWayBackRefusesToSendAndCountsNothing) {
  Wires wires;
  Sender sender(wires);
  // A listener answers whoever writes to it and holds no peer of its
  // own, so there is no way out through it.
  ASSERT_TRUE(sender.openPeer("udp://:0")->error().empty());
  EXPECT_FALSE(sender.send(bytesOf("no way back")));
  EXPECT_EQ(sender.sent(), 0u);
}

TEST(SeerRendering, BytesAreShownAsHexadecimalPairsUpToTheLimit) {
  EXPECT_EQ(sigil::seer::hexadecimal(bytesOf("Hi\n")), "48 69 0a");
  EXPECT_EQ(sigil::seer::hexadecimal(bytesOf("Hi\n"), 2), "48 69 …");
  EXPECT_TRUE(sigil::seer::hexadecimal(bytesOf("")).empty());
}

TEST(SeerRendering, AnAddressIsShownWithoutTheSchemeTheWireAlreadySpells) {
  EXPECT_EQ(sigil::seer::hostAndPort("udp://127.0.0.1:52341"),
            "127.0.0.1:52341");
  EXPECT_EQ(sigil::seer::hostAndPort("ws://[::1]:27060"), "[::1]:27060");
  // An address with no scheme in front of it is already the end a
  // reader is being shown.
  EXPECT_EQ(sigil::seer::hostAndPort("the.desk"), "the.desk");
  EXPECT_TRUE(sigil::seer::hostAndPort("").empty());
}

TEST(SeerRendering, TextIsAnsweredOnlyWhenEveryByteIsPrintableUtf8) {
  EXPECT_EQ(sigil::seer::printableText(bytesOf("a scene\tarrives\n")),
            "a scene\tarrives\n");
  EXPECT_EQ(sigil::seer::printableText(bytesOf("ångström ☉")), "ångström ☉");
  // One byte that is not text makes the message not text: half a
  // sentence shown whole would read as a broken string rather than as
  // bytes.
  EXPECT_TRUE(sigil::seer::printableText(bytesOf("scene\x01"
                                                 "here"))
                  .empty());
  EXPECT_TRUE(sigil::seer::printableText(bytesOf("\xff\xfe")).empty());
}

TEST(SeerRendering, AJsonMessageIsShownIndentedAndAnythingElseIsNot) {
  EXPECT_EQ(sigil::seer::indentedJson(
                bytesOf(R"({"circles":[{"x":1,"y":2.5}],"live":true})")),
            "{\n"
            "  \"circles\": [\n"
            "    {\n"
            "      \"x\": 1,\n"
            "      \"y\": 2.5\n"
            "    }\n"
            "  ],\n"
            "  \"live\": true\n"
            "}");
  EXPECT_EQ(sigil::seer::indentedJson(bytesOf("[]")), "[]");
  EXPECT_TRUE(sigil::seer::indentedJson(bytesOf("a scene arrives")).empty());
  EXPECT_TRUE(sigil::seer::indentedJson(bytesOf("")).empty());
}
