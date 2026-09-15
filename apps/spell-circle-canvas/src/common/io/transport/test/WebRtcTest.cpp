/** @file
 * The WebRTC transport: the room a feed stands in, the introduction two
 * ends make over a websocket door and the channel they speak over
 * afterwards, the sender each arrival names, the one peer a named send
 * reaches instead of all of them, what a caller nobody answers is left
 * with, and the signalling socket a feed gives back when the last
 * holder lets go.
 *
 * A PEER STANDS IN A PROCESS OF ITS OWN, which is what this door is
 * for: two machines that cannot dial each other, with nothing between
 * them once they have met. A case that wants one starts this same
 * binary again on the case below it, which takes the room up, says what
 * it was told to say and echoes back whatever it hears. Both ends in
 * one process is no deployment anybody has, and one process is where
 * the two ends would share the single thread the library underneath
 * finds every end's routes on, so a case that held both would be timing
 * that thread rather than this transport.
 *
 * workaround: two ends sharing that thread also end the process. When
 * two of them come good within the same sweep of it, the second one's
 * encryption opens over a route the sweep has not finished installing,
 * and the error thrown for it crosses the C frame the sweep calls back
 * through — where no handler stands, so the process aborts rather than
 * the peer failing. Nothing above can catch what is thrown below such a
 * frame, and no call of this library's is on that stack; a peer in a
 * process of its own is what keeps two ends off one sweep.
 *
 * ONE CASE HOLDS BOTH ENDS ANYWAY, and times nothing: what it watches
 * is the process, which two ends coming good on one sweep have to leave
 * running.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Places.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern char** environ;

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
 *  they agree on and the stream they open through it, with a process to
 *  start before any of that — so the deadline is longer than the one a
 *  socket alone is given. */
constexpr std::chrono::seconds kPatience{10};

/** How long a case watches something that must NOT happen. A caller
 *  nobody is waiting for can never finish, so what this has to outlast
 *  is only the introduction it keeps making. */
constexpr std::chrono::seconds kQuiet{1};

/** WHAT A PEER IS TOLD, and how it says it. The room it is to take up
 *  stands in one, what it is to keep saying in the other; a binary run
 *  with neither is a binary nobody started for this, and the case below
 *  stands down. */
constexpr const char* kRoomVariable = "SIGIL_IO_WEBRTC_ROOM";
constexpr const char* kSayingVariable = "SIGIL_IO_WEBRTC_SAYING";

/** What a peer answers anything it hears with, in front of the words it
 *  heard: a case that sends knows its message arrived by being told it
 *  back. */
constexpr std::string_view kEcho = "echo ";

/** How long a peer keeps its room open before giving up on whoever
 *  started it. It outlives the deadline a case waits on, so a peer is
 *  ended by the case that started it and not by its own clock, and a
 *  case that died leaves nothing running for long. */
constexpr std::chrono::seconds kPeerLife{30};

/** How often a peer says its piece again. A message written before the
 *  channel is open goes nowhere and says nothing about it, so a peer
 *  keeps saying it until the case has heard it. */
constexpr std::chrono::milliseconds kPeerSays{20};

/** HOW MANY CONVERSATIONS ARE HELD ONE AFTER ANOTHER by the case that
 *  keeps both ends in this process. One pairing says two ends can meet;
 *  this many say that the sweep they share brings them good together
 *  often enough for the pairing to be judged on. */
constexpr int kConversations = 12;

/** How long one of those conversations is given to come good or to say
 *  why it did not. Both ends stand on this machine, so the routes they
 *  try are the ones that answer at once and the pair is made in a frame
 *  or two; a handshake is what is waited on here, never a network. */
constexpr std::chrono::seconds kPairing{5};

Bytes bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  Bytes out;
  out.bytes.assign(first, first + text.size());
  return out;
}

/** THE PEER, IN THE PROCESS A CASE STARTED FOR IT: it takes the room
 *  its environment names, keeps saying what it was told to say, and
 *  answers everything it hears with the same words behind `echo`.
 *
 *  It is a case of its own suite so that this binary can be run as one
 *  — a peer is this same test binary under another filter — and it
 *  stands down at once where nobody named a room for it, which is what
 *  happens when a sweep of the whole binary reaches it. */
TEST(IOWebRtcPeer, TheOneACaseStartsThisBinaryFor) {
  const char* const room = std::getenv(kRoomVariable);
  if (room == nullptr) {
    GTEST_SKIP() << "no room was named for a peer: this case is the one a "
                    "case that wants a peer starts this binary on";
  }
  const char* const saying = std::getenv(kSayingVariable);
  Hub hub;
  sigil::io::registerTransports(hub);
  const std::shared_ptr<Feed> caller = hub.feed(room);
  ASSERT_TRUE(caller->error().empty()) << caller->error();

  const auto deadline = std::chrono::steady_clock::now() + kPeerLife;
  auto next = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() < deadline) {
    // The frame, as any host makes it: what carries the introduction
    // this end is making is the same dispatch a scene would call.
    hub.dispatch();
    if (saying != nullptr && std::chrono::steady_clock::now() >= next) {
      next = std::chrono::steady_clock::now() + kPeerSays;
      caller->send(bytesOf(saying));
    }
    while (const std::optional<Arrival> arrival = caller->receive())
      caller->send(
          bytesOf(std::string(kEcho) + std::string(arrival->bytes->asText())));
    std::this_thread::sleep_for(1ms);
  }
}

/** A PEER THIS CASE STARTED: the process, and the way to end it.
 *
 *  Ending is a signal and then a wait: a peer that has been told to go
 *  is gone by the time this returns, so the case after it starts its
 *  own peer with none of this one's still standing. */
class StartedPeer {
 public:
  StartedPeer(const std::string& room, const std::string& saying) {
    const std::string binary = sigil::io::executablePath().string();
    const std::string filter =
        "--gtest_filter=IOWebRtcPeer.TheOneACaseStartsThisBinaryFor";
    const std::string named = std::string(kRoomVariable) + "=" + room;
    const std::string said = std::string(kSayingVariable) + "=" + saying;
    std::vector<char*> arguments{const_cast<char*>(binary.c_str()),
                                 const_cast<char*>(filter.c_str()), nullptr};
    std::vector<char*> environment;
    for (char** entry = environ; *entry != nullptr; ++entry)
      environment.push_back(*entry);
    environment.push_back(const_cast<char*>(named.c_str()));
    environment.push_back(const_cast<char*>(said.c_str()));
    environment.push_back(nullptr);
    if (posix_spawn(&m_process, binary.c_str(), nullptr, nullptr,
                    arguments.data(), environment.data()) != 0)
      m_process = -1;
  }

  ~StartedPeer() { end(); }

  StartedPeer(const StartedPeer&) = delete;
  StartedPeer& operator=(const StartedPeer&) = delete;

  bool started() const { return m_process > 0; }

  void end() {
    if (m_process <= 0) return;
    ::kill(m_process, SIGTERM);
    int how = 0;
    ::waitpid(m_process, &how, 0);
    m_process = -1;
  }

 private:
  pid_t m_process = -1;
};

/** WHAT A WEBRTC CASE NEEDS BEFORE IT CAN OPEN ANYTHING: a hub that has
 *  been taught the schemes, one Asio context of its own for picking a
 *  port with, and the peers it started, which are ended before the next
 *  case starts one. */
class IOWebRtc : public ::testing::Test {
 protected:
  IOWebRtc() { sigil::io::registerTransports(hub); }

  void TearDown() override {
    // The peers first — a peer whose case has let its door go is a
    // process calling a port that nobody holds — and then the doors.
    peers.clear();
    opened.clear();
  }

  /** Polls @p ready until it holds, or gives up. THE HUB IS DISPATCHED
   *  ON EVERY LOOK, because an introduction crosses on the frame: the
   *  call a host already makes once a frame is what reads the
   *  signalling door and answers it, so a case that never dispatched
   *  would wait for a handshake nothing was carrying. */
  bool waitUntil(const std::function<bool()>& ready,
                 std::chrono::seconds patience = kPatience) {
    const auto deadline = std::chrono::steady_clock::now() + patience;
    while (std::chrono::steady_clock::now() < deadline) {
      hub.dispatch();
      if (ready()) return true;
      std::this_thread::sleep_for(1ms);
    }
    hub.dispatch();
    return ready();
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

  /** The URI a door waiting at @p port stands on, and the one a peer
   *  reaches it by: one room, and the two shapes of the same signalling
   *  door. */
  std::string waitingAt(uint16_t port, std::string_view room = "room") {
    return "webrtc://" + std::string(room) +
           "?signal=ws://:" + std::to_string(port) + "/signal";
  }
  std::string callingInto(uint16_t port, std::string_view room = "room") {
    return "webrtc://" + std::string(room) +
           "?signal=ws://127.0.0.1:" + std::to_string(port) + "/signal";
  }

  /** Opens @p uri on the hub and keeps it, so teardown lets go of every
   *  door a case opened before the next case opens one. */
  std::shared_ptr<Feed> open(const std::string& uri) {
    std::shared_ptr<Feed> feed = hub.feed(uri);
    opened.push_back(feed);
    return feed;
  }

  /** Starts a peer that takes the room at @p port up and keeps saying
   *  @p saying, and answers the address its arrivals name it by — which
   *  is what says the channel between the two processes is open. */
  std::string peerSaying(uint16_t port, const std::string& saying,
                         const std::shared_ptr<Feed>& onto) {
    peers.push_back(std::make_unique<StartedPeer>(callingInto(port), saying));
    if (!peers.back()->started()) return {};
    std::string sender;
    waitUntil([&] {
      while (const std::optional<Arrival> arrival = onto->receive())
        if (arrival->bytes->asText() == saying) sender = arrival->from;
      return !sender.empty();
    });
    return sender;
  }

  /** Whether @p onto is told @p text back by the peer named @p from —
   *  every peer answers what it hears with the same words behind
   *  "echo", so a case that sends knows its message arrived. */
  bool echoedBy(const std::shared_ptr<Feed>& onto, const std::string& from,
                std::string_view text) {
    const std::string expected = std::string(kEcho) + std::string(text);
    bool heard = false;
    waitUntil([&] {
      while (const std::optional<Arrival> arrival = onto->receive())
        if (arrival->from == from && arrival->bytes->asText() == expected)
          heard = true;
      return heard;
    });
    return heard;
  }

  Hub hub;
  boost::asio::io_context context;
  std::vector<std::shared_ptr<Feed>> opened;
  std::vector<std::unique_ptr<StartedPeer>> peers;
};

TEST_F(IOWebRtc, AFeedNamesTheRoomItStandsInAndNotTheDoorItWasIntroducedOver) {
  const uint16_t port = freePort();
  const std::shared_ptr<Feed> waiting = open(waitingAt(port, "sky"));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();
  // The signal is this door's own arrangement and no part of what the
  // conversation is called, exactly as a listener's pages are no part
  // of the path its peers reach.
  EXPECT_EQ(waiting->address(), "webrtc://sky");
}

TEST_F(IOWebRtc, AUriThatNamesNoSignalOpensNothingAndSaysWhy) {
  const std::shared_ptr<Feed> feed = open("webrtc://sky");
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
  EXPECT_EQ(feed->latest(), nullptr);
}

TEST_F(IOWebRtc, AUriWhoseSignalIsNoWebsocketDoorOpensNothingAndSaysWhy) {
  const std::shared_ptr<Feed> feed = open("webrtc://sky?signal=udp://:27050");
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
}

TEST_F(IOWebRtc, APeersMessageArrivesOnTheOneWaitingNamingThePeerItCameFrom) {
  const uint16_t port = freePort();
  const std::shared_ptr<Feed> waiting = open(waitingAt(port));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();

  const std::string sender = peerSaying(port, "a phone speaks", waiting);
  ASSERT_FALSE(sender.empty()) << waiting->error();
  // A peer is named by the room it is in and the number it came in as,
  // which is an address the door can be asked to answer alone.
  EXPECT_TRUE(sender.starts_with("webrtc://room#")) << sender;
}

TEST_F(IOWebRtc, TheOneWaitingReachesThePeerWithOneSend) {
  const uint16_t port = freePort();
  const std::shared_ptr<Feed> waiting = open(waitingAt(port));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();

  // The peer speaks first because that is what makes the channel: until
  // one stands there is nothing for the door to broadcast over.
  const std::string sender = peerSaying(port, "a phone speaks", waiting);
  ASSERT_FALSE(sender.empty()) << waiting->error();

  EXPECT_TRUE(waiting->send(bytesOf("the sky as it stands")));
  EXPECT_TRUE(echoedBy(waiting, sender, "the sky as it stands"));
}

TEST_F(IOWebRtc, SendToReachesTheOnePeerItNamesAndNoOther) {
  const uint16_t port = freePort();
  const std::shared_ptr<Feed> waiting = open(waitingAt(port));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();

  // Each peer says which one it is, and the arrival it says it in names
  // the address that peer is answered by.
  const std::string first = peerSaying(port, "first", waiting);
  ASSERT_FALSE(first.empty()) << waiting->error();
  const std::string second = peerSaying(port, "second", waiting);
  ASSERT_FALSE(second.empty()) << waiting->error();
  ASSERT_NE(first, second);

  EXPECT_TRUE(waiting->sendTo(first, bytesOf("to you alone")));
  EXPECT_TRUE(echoedBy(waiting, first, "to you alone"));
  // The other peer echoes everything it hears, so what says the message
  // above reached one peer and not the room is that this one never
  // echoed it — while the broadcast after it comes back from both.
  EXPECT_TRUE(waiting->send(bytesOf("out to everyone")));
  EXPECT_TRUE(echoedBy(waiting, second, "out to everyone"));
  bool answeredTwice = false;
  while (const std::optional<Arrival> arrival = waiting->receive())
    if (arrival->from == second &&
        arrival->bytes->asText() == std::string(kEcho) + "to you alone")
      answeredTwice = true;
  EXPECT_FALSE(answeredTwice);
}

TEST_F(IOWebRtc, ASignalNobodyAnswersLeavesTheCallerOpenAndSilent) {
  const uint16_t port = freePort();
  // The signalling door stands and the caller's introduction crosses
  // it; what is not there is anybody waiting in the room to answer.
  const std::shared_ptr<Feed> door =
      open("ws://:" + std::to_string(port) + "/signal");
  ASSERT_TRUE(door->error().empty()) << door->error();
  const std::shared_ptr<Feed> caller = open(callingInto(port));
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
    // Held here and nowhere else — the fixture keeps no hold of this
    // one, since what this case watches is the last holder letting go.
    const std::shared_ptr<Feed> waiting = hub.feed(waitingAt(port));
    ASSERT_TRUE(waiting->error().empty()) << waiting->error();
    ASSERT_FALSE(peerSaying(port, "a phone speaks", waiting).empty());
    peers.clear();
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

/** BOTH ENDS OF ONE CONVERSATION IN THIS PROCESS, a dozen times over:
 *  the topology every other case here keeps apart, held together on
 *  purpose so that two ends come good within one sweep of the thread
 *  the library underneath finds routes on.
 *
 *  What is asserted after each pair is that this process is still
 *  running — a process that ended reaches this case as a case that
 *  never returned — and that each end either crossed a message or says
 *  why it could not. A pair that fails is one conversation's ending; a
 *  pair that ends the process is the library's. */
TEST_F(IOWebRtc,
       BothEndsOfAConversationInOneProcessPairAndPartWithoutEndingIt) {
  for (int conversation = 1; conversation <= kConversations; ++conversation) {
    // A room and a port of its own for every pair, so what the sweep
    // brings good is the two ends opened together and no leftover of
    // the pair before them.
    const std::string room = "sweep" + std::to_string(conversation);
    const uint16_t port = freePort();
    const std::shared_ptr<Feed> waiting = open(waitingAt(port, room));
    ASSERT_TRUE(waiting->error().empty()) << waiting->error();
    const std::shared_ptr<Feed> calling = open(callingInto(port, room));
    ASSERT_TRUE(calling->error().empty()) << calling->error();

    // The end that took the room up keeps saying its piece: a message
    // written before the channel is open goes nowhere and says nothing
    // about it.
    const std::string said = "conversation " + std::to_string(conversation);
    bool crossed = false;
    auto next = std::chrono::steady_clock::now();
    const auto settled = [&] {
      if (std::chrono::steady_clock::now() >= next) {
        next = std::chrono::steady_clock::now() + kPeerSays;
        calling->send(bytesOf(said));
      }
      while (const std::optional<Arrival> arrival = waiting->receive())
        if (arrival->bytes->asText() == said) crossed = true;
      return crossed || !waiting->error().empty() || !calling->error().empty();
    };
    waitUntil(settled, kPairing);

    EXPECT_TRUE(crossed || !waiting->error().empty() ||
                !calling->error().empty())
        << "conversation " << conversation
        << ": nothing crossed and neither end says why";

    // Both ends are let go before the next pair is made, so a teardown
    // and a handshake are on that thread together as well.
    calling->close();
    waiting->close();
  }
}

}  // namespace
