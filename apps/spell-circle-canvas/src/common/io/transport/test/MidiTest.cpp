/** @file
 * The MIDI transport: the port a virtual URI makes and the port an
 * input opens back on it by name, the message that crosses between
 * them and the sender it names, what a name nobody answers to leaves on
 * its feed, the one way an input is, and the port a feed gives back
 * when the last holder lets go.
 *
 * BOTH ENDS OF A CABLE IN ONE BINARY: a case makes a port of its own
 * rather than waiting for a controller to be plugged in, and every case
 * that needs one is skipped with the reason where the system does not
 * offer ports made rather than found.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>

#include <chrono>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

using sigil::io::Arrival;
using sigil::io::Bytes;
using sigil::io::Feed;
using sigil::io::Hub;
using namespace std::chrono_literals;

/** Polls @p ready until it holds, or gives up. The work it waits for
 *  runs on the driver's own thread, so there is nothing here to pump —
 *  only a moment to give it, and a deadline long enough that a loaded
 *  machine is not mistaken for a cable that is not there. */
bool waitUntil(const std::function<bool()>& ready) {
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (ready()) return true;
    std::this_thread::sleep_for(1ms);
  }
  return ready();
}

Bytes bytesOf(std::initializer_list<int> message) {
  Bytes out;
  for (int one : message) out.bytes.push_back(static_cast<std::byte>(one));
  return out;
}

/** A name no other program on this machine is using: the case's own
 *  word and the number the system gave this run. Two copies of the test
 *  running at once would otherwise open each other's ports. */
std::string uniqueName(std::string_view what) {
#ifdef _WIN32
  const int pid = _getpid();
#else
  const int pid = static_cast<int>(::getpid());
#endif
  return "sigil-" + std::string(what) + "-" + std::to_string(pid);
}

/** WHAT A CASE NEEDS BEFORE IT CAN OPEN ANYTHING: a hub that has been
 *  taught the schemes. */
class IOMidi : public ::testing::Test {
 protected:
  IOMidi() { sigil::io::registerTransports(hub); }

  /** A port of this machine's own, made rather than found, for the
   *  cases that would otherwise need a controller plugged in. Nothing
   *  when the system refuses one, which is what a case skips on. */
  std::shared_ptr<Feed> makePort(const std::string& name) {
    return hub.feed("midi://out/virtual:" + name);
  }

  Hub hub;
};

TEST_F(IOMidi, AMessageCrossesFromAPortToTheInputThatOpenedItByName) {
  const std::string name = uniqueName("keys");
  const std::shared_ptr<Feed> keys = makePort(name);
  if (!keys->error().empty())
    GTEST_SKIP() << "this machine offers no port made rather than found: "
                 << keys->error();
  EXPECT_TRUE(keys->address().starts_with("midi://out/")) << keys->address();

  // A port takes a moment to appear to everything else on the machine,
  // so the input is opened until it opens rather than once.
  std::shared_ptr<Feed> pads;
  ASSERT_TRUE(waitUntil([&] {
    pads = hub.feed("midi://in/" + name);
    if (pads->error().empty()) return true;
    pads.reset();
    return false;
  })) << "no input port was made for "
      << name;

  // The input names the port by the whole of the port's own name, which
  // is the system's spelling of it and not the piece the URI asked for.
  EXPECT_TRUE(pads->address().starts_with("midi://in/")) << pads->address();
  EXPECT_NE(pads->address().find(name), std::string::npos) << pads->address();

  // A note on the middle C of the first channel, struck hard.
  EXPECT_TRUE(keys->send(bytesOf({0x90, 0x3C, 0x64})));
  ASSERT_TRUE(waitUntil([&] { return pads->latest() != nullptr; }));

  const std::optional<Arrival> arrival = pads->receive();
  ASSERT_TRUE(arrival.has_value());
  // The bytes are the wire's own, status byte first: what a message
  // MEANS is read by the library that owns the format.
  ASSERT_EQ(arrival->bytes->bytes.size(), 3u);
  EXPECT_EQ(arrival->bytes->bytes[0], static_cast<std::byte>(0x90));
  EXPECT_EQ(arrival->bytes->bytes[1], static_cast<std::byte>(0x3C));
  EXPECT_EQ(arrival->bytes->bytes[2], static_cast<std::byte>(0x64));
  // Every arrival names the port it came in at, spelled the way the URI
  // that opened it is.
  EXPECT_EQ(arrival->from, pads->address()) << arrival->from;
}

TEST_F(IOMidi, AnInputIsOneWay) {
  const std::string name = uniqueName("oneway");
  const std::shared_ptr<Feed> made = makePort(name);
  if (!made->error().empty())
    GTEST_SKIP() << "this machine offers no port made rather than found: "
                 << made->error();

  std::shared_ptr<Feed> pads;
  ASSERT_TRUE(waitUntil([&] {
    pads = hub.feed("midi://in/" + name);
    if (pads->error().empty()) return true;
    pads.reset();
    return false;
  })) << "no input port was made for "
      << name;

  // What comes back down a cable is the other cable, which is a door of
  // its own: there is nothing to send through an input and nobody it
  // could name to answer.
  EXPECT_FALSE(pads->send(bytesOf({0x90, 0x3C, 0x64})));
  EXPECT_FALSE(pads->sendTo(pads->address(), bytesOf({0x90, 0x3C, 0x64})));
}

TEST_F(IOMidi, ANameNoPortAnswersToOpensNothingAndSaysWhichPortsExist) {
  // A port MADE under a name of this run's own, so the sentence a case
  // reads has at least that one port in it wherever the machine is —
  // and waited for, because a sentence is written from the list as it
  // stands when the door is asked for.
  const std::string standing = uniqueName("standing");
  const std::shared_ptr<Feed> made = makePort(standing);
  bool listed = false;
  if (made->error().empty()) {
    std::shared_ptr<Feed> seen;
    listed = waitUntil([&] {
      seen = hub.feed("midi://in/" + standing);
      if (seen->error().empty()) return true;
      seen.reset();
      return false;
    });
  }

  const std::string missing = uniqueName("nobody-has-this");
  const std::shared_ptr<Feed> nowhere = hub.feed("midi://in/" + missing);
  EXPECT_FALSE(nowhere->error().empty());
  EXPECT_TRUE(nowhere->address().empty());
  EXPECT_EQ(nowhere->latest(), nullptr);
  // The sentence carries what was looked for, so a name typed from
  // memory is seen to be the thing that was wrong.
  EXPECT_NE(nowhere->error().find(missing), std::string::npos)
      << nowhere->error();
  // …and the ports that DO exist, so the name it should have been is
  // read off the same sentence. The port this case made is one of them,
  // however many others are plugged in.
  if (listed)
    EXPECT_NE(nowhere->error().find(standing), std::string::npos)
        << nowhere->error();
}

TEST_F(IOMidi, AUriThatNamesNoPortAtAllOpensNothingAndSaysWhy) {
  const std::shared_ptr<Feed> sideways = hub.feed("midi://sideways/keys");
  EXPECT_FALSE(sideways->error().empty());
  EXPECT_TRUE(sideways->address().empty());
  // A port to MAKE is a port to call something, so there is no first
  // port for it to fall back on.
  const std::shared_ptr<Feed> unnamed = hub.feed("midi://in/virtual:");
  EXPECT_FALSE(unnamed->error().empty());
  EXPECT_TRUE(unnamed->address().empty());
}

TEST_F(IOMidi, DroppingTheLastHolderOfAFeedGivesUpItsPort) {
  const std::string name = uniqueName("given-back");
  {
    const std::shared_ptr<Feed> made = makePort(name);
    if (!made->error().empty())
      GTEST_SKIP() << "this machine offers no port made rather than found: "
                   << made->error();
    std::shared_ptr<Feed> pads;
    ASSERT_TRUE(waitUntil([&] {
      pads = hub.feed("midi://in/" + name);
      if (pads->error().empty()) return true;
      pads.reset();
      return false;
    })) << "no input port was made for "
        << name;
  }

  // The port went with the feed that made it: an input naming it finds
  // nothing, where a moment ago it found a port. The system takes it
  // back a moment after the last holder lets go rather than within it,
  // so this is waited for.
  ASSERT_TRUE(waitUntil([&] {
    const std::shared_ptr<Feed> gone = hub.feed("midi://in/" + name);
    return !gone->error().empty();
  })) << "the port outlived the feed that made it";

  // And the name is free to be made again, which is the other half of
  // the same fact.
  const std::shared_ptr<Feed> again = makePort(name);
  EXPECT_TRUE(again->error().empty()) << again->error();
  EXPECT_FALSE(again->address().empty());
}

}  // namespace
