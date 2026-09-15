/** @file
 * The WebRTC transport: the room a feed stands in, the introduction two
 * ends make over a websocket door and the channel they speak over
 * afterwards, the sender each arrival names, the one peer a named send
 * reaches instead of all of them, what a caller nobody answers is left
 * with, and the signalling socket a feed gives back when the last
 * holder lets go.
 *
 * Both ends stand in this one binary and on hubs of their own, which is
 * what two machines are here: the room's hub waits at a port, and each
 * caller's hub reaches it over the loopback.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace {

using boost::asio::ip::tcp;
using sigil::io::Arrival;
using sigil::io::Bytes;
using sigil::io::Feed;
using sigil::io::Hub;
using namespace std::chrono_literals;

/** How long a case gives something that must happen. A WHOLE HANDSHAKE
 *  STANDS BEHIND EVERY WAIT HERE — the offer and the answer over the
 *  signalling door, the routes each end finds and tries, the encryption
 *  they agree on and the stream they open through it — so the deadline
 *  is longer than the one a socket alone is given. */
constexpr std::chrono::seconds kPatience{5};

/** How long a case watches something that must NOT happen. A caller
 *  nobody is waiting for can never finish, so what this has to outlast
 *  is only the introduction it keeps making. */
constexpr std::chrono::seconds kQuiet{1};

Bytes bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  Bytes out;
  out.bytes.assign(first, first + text.size());
  return out;
}

/** WHAT A WEBRTC CASE NEEDS BEFORE IT CAN OPEN ANYTHING: one hub per
 *  machine — the room is waited in on `hub`, and `away` and `elsewhere`
 *  are the two that call into it — and one Asio context of the test's
 *  own for picking a port with. */
class IOWebRtc : public ::testing::Test {
 protected:
  IOWebRtc() {
    sigil::io::registerTransports(hub);
    sigil::io::registerTransports(away);
    sigil::io::registerTransports(elsewhere);
  }

  /** Polls @p ready until it holds, or gives up. EVERY HUB IS
   *  DISPATCHED ON EVERY LOOK, because an introduction crosses on the
   *  frame: the call a host already makes once a frame is what reads
   *  the signalling door and answers it, so a case that never
   *  dispatched would wait for a handshake nothing was carrying. */
  bool waitUntil(const std::function<bool()>& ready,
                 std::chrono::seconds patience = kPatience) {
    const auto deadline = std::chrono::steady_clock::now() + patience;
    while (std::chrono::steady_clock::now() < deadline) {
      frame();
      if (ready()) return true;
      std::this_thread::sleep_for(1ms);
    }
    frame();
    return ready();
  }

  void frame() {
    hub.dispatch();
    away.dispatch();
    elsewhere.dispatch();
  }

  /** A port nothing holds: one taken and given straight back, which is
   *  how a case names a port in a URI it has to spell twice — once for
   *  the end that holds it and once for the end that reaches it. */
  uint16_t freePort() {
    tcp::acceptor probe(context, tcp::endpoint(tcp::v6(), 0));
    const uint16_t port = probe.local_endpoint().port();
    probe.close();
    return port;
  }

  /** The URI a door waiting at @p port stands on, and the one a caller
   *  reaches it by: one room, and the two shapes of the same signalling
   *  door. */
  std::string waitingAt(uint16_t port, std::string_view room = "room") {
    return "webrtc://" + std::string(room) +
           "?signal=ws://:" + std::to_string(port) + "/signal";
  }
  std::string callingInto(uint16_t port, std::string_view room = "room",
                          std::string_view host = "127.0.0.1") {
    return "webrtc://" + std::string(room) + "?signal=ws://" +
           std::string(host) + ":" + std::to_string(port) + "/signal";
  }

  /** Keeps saying @p text on @p from until it lands on @p onto, and
   *  answers the address the arrival that landed names its sender by.
   *
   *  A MESSAGE WRITTEN BEFORE A CHANNEL IS OPEN GOES NOWHERE and says
   *  nothing about it, a feed's send answering whether the door stands
   *  rather than whether a peer heard — so what a case waits for is the
   *  first one the channel was open for. */
  std::string speakUntilHeard(const std::shared_ptr<Feed>& from,
                              const std::shared_ptr<Feed>& onto,
                              std::string_view text) {
    std::string sender;
    waitUntil([&] {
      from->send(bytesOf(text));
      while (const std::optional<Arrival> arrival = onto->receive())
        if (arrival->bytes->asText() == text) sender = arrival->from;
      return !sender.empty();
    });
    return sender;
  }

  Hub hub;
  Hub away;
  Hub elsewhere;
  boost::asio::io_context context;
};

TEST_F(IOWebRtc, AFeedNamesTheRoomItStandsInAndNotTheDoorItWasIntroducedOver) {
  const uint16_t port = freePort();
  const std::shared_ptr<Feed> waiting = hub.feed(waitingAt(port, "sky"));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();
  // The signal is this door's own arrangement and no part of what the
  // conversation is called, exactly as a listener's pages are no part
  // of the path its peers reach.
  EXPECT_EQ(waiting->address(), "webrtc://sky");
}

TEST_F(IOWebRtc, AUriThatNamesNoSignalOpensNothingAndSaysWhy) {
  const std::shared_ptr<Feed> feed = hub.feed("webrtc://sky");
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
  EXPECT_EQ(feed->latest(), nullptr);
}

TEST_F(IOWebRtc, AUriWhoseSignalIsNoWebsocketDoorOpensNothingAndSaysWhy) {
  const std::shared_ptr<Feed> feed =
      hub.feed("webrtc://sky?signal=udp://:27050");
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
}

TEST_F(IOWebRtc, ACallersMessageArrivesOnTheOneWaitingNamingThePeerItCameFrom) {
  const uint16_t port = freePort();
  const std::shared_ptr<Feed> waiting = hub.feed(waitingAt(port));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();
  const std::shared_ptr<Feed> caller = away.feed(callingInto(port));
  ASSERT_TRUE(caller->error().empty()) << caller->error();

  const std::string sender = speakUntilHeard(caller, waiting, "a phone speaks");
  ASSERT_FALSE(sender.empty()) << waiting->error();
  // A peer is named by the room it is in and the number it came in as,
  // which is an address the door can be asked to answer alone.
  EXPECT_TRUE(sender.starts_with("webrtc://room#")) << sender;
}

TEST_F(IOWebRtc, TheOneWaitingReachesTheCallerWithOneSend) {
  const uint16_t port = freePort();
  const std::shared_ptr<Feed> waiting = hub.feed(waitingAt(port));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();
  const std::shared_ptr<Feed> caller = away.feed(callingInto(port));
  ASSERT_TRUE(caller->error().empty()) << caller->error();

  // The caller speaks first because that is what makes the channel:
  // until one stands there is nothing for the door to broadcast over.
  ASSERT_FALSE(speakUntilHeard(caller, waiting, "a phone speaks").empty());
  EXPECT_FALSE(
      speakUntilHeard(waiting, caller, "the sky as it stands").empty());
}

TEST_F(IOWebRtc, SendToReachesTheOnePeerItNamesAndNoOther) {
  const uint16_t port = freePort();
  const std::shared_ptr<Feed> waiting = hub.feed(waitingAt(port));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();
  const std::shared_ptr<Feed> first = away.feed(callingInto(port));
  ASSERT_TRUE(first->error().empty()) << first->error();
  const std::shared_ptr<Feed> second = elsewhere.feed(callingInto(port));
  ASSERT_TRUE(second->error().empty()) << second->error();

  // Each caller says which one it is, and the arrival it says it in
  // names the address that caller is answered by.
  const std::string answering = speakUntilHeard(first, waiting, "first");
  ASSERT_FALSE(answering.empty());
  ASSERT_FALSE(speakUntilHeard(second, waiting, "second").empty());

  ASSERT_TRUE(waitUntil([&] {
    return waiting->sendTo(answering, bytesOf("to you alone")) &&
           first->latest() != nullptr;
  }));
  EXPECT_EQ(first->latest()->asText(), "to you alone");

  // What the other caller reads first is the broadcast that came after,
  // which is what says the message before it went to one peer and not
  // to the room.
  ASSERT_TRUE(waitUntil([&] {
    waiting->send(bytesOf("out to everyone"));
    return second->latest() != nullptr;
  }));
  const std::optional<Arrival> arrival = second->receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->bytes->asText(), "out to everyone");
}

TEST_F(IOWebRtc, ASignalNobodyAnswersLeavesTheCallerOpenAndSilent) {
  const uint16_t port = freePort();
  // The signalling door stands and the caller's introduction crosses
  // it; what is not there is anybody waiting in the room to answer.
  const std::shared_ptr<Feed> door =
      hub.feed("ws://:" + std::to_string(port) + "/signal");
  ASSERT_TRUE(door->error().empty()) << door->error();
  const std::shared_ptr<Feed> caller = away.feed(callingInto(port));
  ASSERT_TRUE(caller->error().empty()) << caller->error();

  EXPECT_FALSE(waitUntil([&] { return caller->latest() != nullptr; }, kQuiet));
  // A conversation nobody has taken up is not a door that failed: the
  // feed stands, with nothing on it and nothing to explain, for as long
  // as somebody may still answer.
  EXPECT_TRUE(caller->error().empty()) << caller->error();
  EXPECT_FALSE(caller->closed());
  EXPECT_EQ(caller->generation(), 0u);
}

TEST_F(IOWebRtc, DroppingTheLastHolderOfAFeedGivesUpItsSignallingPort) {
  const uint16_t port = freePort();
  {
    const std::shared_ptr<Feed> waiting = hub.feed(waitingAt(port));
    ASSERT_TRUE(waiting->error().empty()) << waiting->error();
    const std::shared_ptr<Feed> caller = away.feed(callingInto(port));
    ASSERT_TRUE(caller->error().empty()) << caller->error();
    ASSERT_FALSE(speakUntilHeard(caller, waiting, "a phone speaks").empty());
  }

  // The signalling door was the transport's own, so the last
  // conversation crossing it letting go is what gives its port back —
  // a moment after, the close travelling to the listener's own loop.
  const std::string again = "ws://:" + std::to_string(port) + "/signal";
  std::shared_ptr<Feed> taken;
  ASSERT_TRUE(waitUntil([&] {
    taken = hub.feed(again);
    if (taken->error().empty()) return true;
    taken.reset();
    return false;
  }));
}

}  // namespace
