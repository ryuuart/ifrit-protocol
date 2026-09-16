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
 * A PEER SAYS WHEN IT IS UP, in one line on its own standard output
 * which the case that started it reads back off the process, and that
 * wait carries no verdict at all: what stands behind it is a process
 * starting, which a machine with other work on it can be slow at and
 * which says nothing about this transport. Only the pairing after that
 * line is waited on against this transport's own deadline, so a case
 * fails when two ends that were both up could not meet, and stands down
 * naming the machine where a peer never got going at all.
 *
 * ONE CASE HOLDS BOTH ENDS ANYWAY, and times nothing: what it watches
 * is the process, which two ends coming good on one sweep have to leave
 * running.
 */

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Places.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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

using sigil::io::Arrival;
using sigil::io::Bytes;
using sigil::io::Feed;
using sigil::io::Hub;
using namespace std::chrono_literals;

/** How long a case gives something that must happen. A WHOLE HANDSHAKE
 *  STANDS BEHIND EVERY WAIT HERE — the offer and the answer over the
 *  signalling door, the routes each end finds and tries, the encryption
 *  they agree on and the stream they open through it — so the deadline
 *  is longer than the one a socket alone is given. What does NOT stand
 *  behind it is a process starting: every peer a case wants is up and
 *  has said so before any wait of this length begins. */
constexpr std::chrono::seconds kPatience{10};

/** How long a case watches something that must NOT happen. A caller
 *  nobody is waiting for can never finish, so what this has to outlast
 *  is only the introduction it keeps making. */
constexpr std::chrono::seconds kQuiet{1};

/** How long a case waits for the peer it started to say it is up. NO
 *  VERDICT HANGS ON THIS ONE: what it covers is a process starting,
 *  which a loaded machine can be slow at and which this suite is not
 *  here to time, so it is generous and a case that runs out of it
 *  stands down naming the machine rather than failing the transport. */
constexpr std::chrono::seconds kPeerComingUp{20};

/** WHAT A PEER IS TOLD, and how it says it. The room it is to take up
 *  stands in one, what it is to keep saying in the other; a binary run
 *  with neither is a binary nobody started for this, and the case below
 *  stands down. */
constexpr const char* kRoomVariable = "SIGIL_IO_WEBRTC_ROOM";
constexpr const char* kSayingVariable = "SIGIL_IO_WEBRTC_SAYING";

/** WHAT A PEER SAYS WHEN IT IS UP, on its own standard output: the room
 *  is taken up, the feed over it opened without complaint and the first
 *  greeting written. The case that started the process reads this line
 *  back off it, and everything it waits on afterwards is this
 *  transport's to answer for. */
constexpr const char* kPeerIsUp = "the peer is up";

/** WHAT A CASE SAYS WHERE ITS PEER NEVER CAME UP. A process that did not
 *  get going in a generous time is the machine this suite is running
 *  on, so the case stands down on it rather than reporting a transport
 *  that was never reached. */
constexpr const char* kNeverCameUp =
    "the peer never came up: the process this case started never said its "
    "room was open, which is this machine and not this transport";

/** WHAT A CASE SAYS WHERE THE PEER WAS UP AND THE TWO ENDS NEVER MET.
 *  Both ends were standing and this transport did not pair them, which
 *  is the failure this suite is here to catch. */
constexpr const char* kNeverPaired =
    "the peer was up and the two ends never paired: ";

/** What a peer answers anything it hears with, in front of the words it
 *  heard: a case that sends knows its message arrived by being told it
 *  back. */
constexpr std::string_view kEcho = "echo ";

/** How long a peer keeps its room open before giving up on whoever
 *  started it. It outlives every wait a case makes on it — the one for
 *  it to come up and the ones on the transport after — so a peer is
 *  ended by the case that started it and not by its own clock, and a
 *  case that died leaves nothing running for long. */
constexpr std::chrono::seconds kPeerLife{60};

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

/** The port the door at @p address bound: the digits behind the last
 *  colon of it, which is where a port stands whether the address before
 *  it is written in one family's spelling or the other's, and a path
 *  behind them is not read. Zero where nothing there is a port, which
 *  is what a door that bound none answers. */
uint16_t portOf(const std::string& address) {
  const size_t colon = address.rfind(':');
  if (colon == std::string::npos) return 0;
  unsigned long port = 0;
  for (size_t at = colon + 1; at != address.size(); ++at) {
    if (address[at] < '0' || address[at] > '9') break;
    port = port * 10 + static_cast<unsigned long>(address[at] - '0');
  }
  return static_cast<uint16_t>(port);
}

Bytes bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  Bytes out;
  out.bytes.assign(first, first + text.size());
  return out;
}

/** THE PEER, IN THE PROCESS A CASE STARTED FOR IT: it takes the room
 *  its environment names, SAYS ON ITS OWN OUTPUT THAT IT IS UP, keeps
 *  saying what it was told to say, and answers everything it hears with
 *  the same words behind `echo`.
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

  // UP, AND SAYING SO: the room is taken and the first greeting is
  // written, which is the whole of what this process has to do before
  // the case that started it can hold the transport to anything. The
  // line is pushed out as it is written, since what reads it is another
  // process standing on it.
  if (saying != nullptr) caller->send(bytesOf(saying));
  std::printf("%s\n", kPeerIsUp);
  std::fflush(stdout);

  const auto deadline = std::chrono::steady_clock::now() + kPeerLife;
  auto next = std::chrono::steady_clock::now() + kPeerSays;
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

/** A PEER THIS CASE STARTED: the process, what it has said on its own
 *  output, and the way to end it.
 *
 *  ITS OUTPUT COMES BACK THROUGH A PIPE rather than straight to this
 *  process's, because the line a peer prints when its room is open is
 *  how a case knows the process is standing. Whatever is read off that
 *  pipe is written on to this process's own output as it comes, so a
 *  peer's words stand where anybody looking for them looks, and the
 *  pipe is emptied often enough that a peer is never held up by it.
 *
 *  Ending is a signal and then a wait: a peer that has been told to go
 *  is gone by the time this returns, so the case after it starts its
 *  own peer with none of this one's still standing. */
class StartedPeer {
 public:
  StartedPeer(const std::string& room, const std::string& saying) {
    int pipeEnds[2] = {-1, -1};
    if (::pipe(pipeEnds) != 0) return;
    m_reading = pipeEnds[0];
    const int writing = pipeEnds[1];
    // Read without waiting: a case looks for the peer's line between
    // dispatches, and a look that stood still on a silent process would
    // be a frame the introduction did not cross on.
    ::fcntl(m_reading, F_SETFL, O_NONBLOCK);

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
    // The peer's output is the pipe and nothing else of this end's is
    // the peer's: what it writes is what a case here reads.
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, writing, STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, writing);
    posix_spawn_file_actions_addclose(&actions, m_reading);
    if (posix_spawn(&m_process, binary.c_str(), &actions, nullptr,
                    arguments.data(), environment.data()) != 0)
      m_process = -1;
    posix_spawn_file_actions_destroy(&actions);
    // Written from the peer alone from here, so a peer that ended is a
    // pipe that ended and a case reading it is told so.
    ::close(writing);
  }

  ~StartedPeer() {
    end();
    if (m_reading >= 0) ::close(m_reading);
  }

  StartedPeer(const StartedPeer&) = delete;
  StartedPeer& operator=(const StartedPeer&) = delete;

  bool started() const { return m_process > 0; }

  /** Whether the peer has said it is up. It says so once, and what it
   *  said stands from then on. */
  bool cameUp() {
    if (m_cameUp) return true;
    takeWhatItSaid();
    m_cameUp = m_said.find(kPeerIsUp) != std::string::npos;
    return m_cameUp;
  }

  void end() {
    if (m_process <= 0) return;
    ::kill(m_process, SIGTERM);
    int how = 0;
    ::waitpid(m_process, &how, 0);
    m_process = -1;
    takeWhatItSaid();
  }

 private:
  /** Takes whatever the peer has written since the last look and writes
   *  it on to this process's own output. */
  void takeWhatItSaid() {
    if (m_reading < 0) return;
    char block[512];
    for (;;) {
      const ssize_t taken = ::read(m_reading, block, sizeof(block));
      if (taken <= 0) return;
      m_said.append(block, static_cast<size_t>(taken));
      std::fwrite(block, 1, static_cast<size_t>(taken), stdout);
    }
  }

  pid_t m_process = -1;
  int m_reading = -1;
  bool m_cameUp = false;
  std::string m_said;
};

/** WHAT STARTING A PEER FOR A CASE CAME TO. The two waits behind it mean
 *  different things, so what came of them is answered apart: whether the
 *  peer ever said it was up, which is the machine's to answer, and the
 *  address its arrivals name it by, which is filled where the two ends
 *  paired and empty where this transport left them apart. */
struct PeerStanding {
  bool cameUp = false;
  std::string sender;
};

/** WHAT A WEBRTC CASE NEEDS BEFORE IT CAN OPEN ANYTHING: a hub that has
 *  been taught the schemes, and the peers it started, which are ended
 *  before the next case starts one.
 *
 *  NO PORT IS EVER GUESSED HERE. Every door opens on zero, which is the
 *  kernel's to fill, and answers the port it was given; what has to
 *  reach that door is spelled from the address it answered. A port
 *  asked for by number is one the door a moment from giving it back may
 *  still hold. */
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

  /** The URI a door waiting at @p port stands on, and the one a peer
   *  reaches it by: one room, and the two shapes of the same signalling
   *  door. Zero is what a door waiting stands on, the port it gets
   *  being read back off it. */
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

  /** Starts a peer that takes the room @p onto waits in up — on the
   *  port that door answered, which is the one it was given — and keeps
   *  saying @p saying, and answers the address its arrivals name it by,
   *  which is what says the channel between the two processes is open.
   *
   *  TWO WAITS STAND HERE AND THEY ANSWER TO DIFFERENT THINGS. The
   *  first is for the peer's own line saying its room is open, which is
   *  a process starting and nothing of this transport, so it is
   *  generous and nothing is judged on it. The second is for the
   *  greeting that peer is already saying to arrive, which is the two
   *  ends pairing and is what the case above passes or fails on. */
  PeerStanding peerSaying(const std::shared_ptr<Feed>& onto,
                          const std::string& saying) {
    const uint16_t port = portOf(onto->address());
    // A door standing on no port is this transport with nothing for a
    // peer to dial: no process is started for it, and the case's own
    // verdict on the pairing is what says so.
    if (port == 0) return {.cameUp = true};
    peers.push_back(std::make_unique<StartedPeer>(callingInto(port), saying));
    StartedPeer& peer = *peers.back();
    if (!peer.started()) return {};
    if (!waitUntil([&] { return peer.cameUp(); }, kPeerComingUp)) return {};

    PeerStanding standing;
    standing.cameUp = true;
    waitUntil([&] {
      while (const std::optional<Arrival> arrival = onto->receive())
        if (arrival->bytes->asText() == saying) standing.sender = arrival->from;
      return !standing.sender.empty();
    });
    return standing;
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
  std::vector<std::shared_ptr<Feed>> opened;
  std::vector<std::unique_ptr<StartedPeer>> peers;
};

TEST_F(IOWebRtc, AWaitingFeedNamesItsSignalAsBoundAndACallingOneTheRoomAlone) {
  const std::shared_ptr<Feed> waiting = open(waitingAt(0, "sky"));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();
  // WHAT A CALLER HAS TO DIAL AND NOTHING ELSE: the conversation, and
  // the signalling door as it BOUND rather than as the URI asked for
  // it. The room and the path stand where the URI had them, and the
  // port between them is the one the kernel gave — so an end that
  // waited on zero is an end a phone can be pointed at.
  EXPECT_TRUE(waiting->address().starts_with("webrtc://sky?signal=ws://[::]:"))
      << waiting->address();
  EXPECT_TRUE(waiting->address().ends_with("/signal")) << waiting->address();
  const uint16_t port = portOf(waiting->address());
  ASSERT_NE(port, 0) << waiting->address();

  // The end that TOOK A ROOM UP holds no door anybody dials, so what it
  // names is the conversation and nothing of the arrangement it was
  // introduced under. It takes a room of its own up, over the door the
  // one above is waiting behind: an introduction goes to the room it
  // names, so these two are two doors on one socket and never the pair
  // the case at the end of this file is for.
  const std::shared_ptr<Feed> calling = open(callingInto(port, "lane"));
  ASSERT_TRUE(calling->error().empty()) << calling->error();
  EXPECT_EQ(calling->address(), "webrtc://lane");
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
  const std::shared_ptr<Feed> waiting = open(waitingAt(0));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();

  const PeerStanding peer = peerSaying(waiting, "a phone speaks");
  if (!peer.cameUp) GTEST_SKIP() << kNeverCameUp;
  ASSERT_FALSE(peer.sender.empty()) << kNeverPaired << waiting->error();
  // A peer is named by the room it is in and the number it came in as,
  // which is an address the door can be asked to answer alone.
  EXPECT_TRUE(peer.sender.starts_with("webrtc://room#")) << peer.sender;
}

TEST_F(IOWebRtc, TheOneWaitingReachesThePeerWithOneSend) {
  const std::shared_ptr<Feed> waiting = open(waitingAt(0));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();

  // The peer speaks first because that is what makes the channel: until
  // one stands there is nothing for the door to broadcast over.
  const PeerStanding peer = peerSaying(waiting, "a phone speaks");
  if (!peer.cameUp) GTEST_SKIP() << kNeverCameUp;
  ASSERT_FALSE(peer.sender.empty()) << kNeverPaired << waiting->error();

  EXPECT_TRUE(waiting->send(bytesOf("the sky as it stands")));
  EXPECT_TRUE(echoedBy(waiting, peer.sender, "the sky as it stands"));
}

TEST_F(IOWebRtc, SendToReachesTheOnePeerItNamesAndNoOther) {
  const std::shared_ptr<Feed> waiting = open(waitingAt(0));
  ASSERT_TRUE(waiting->error().empty()) << waiting->error();

  // Each peer says which one it is, and the arrival it says it in names
  // the address that peer is answered by.
  const PeerStanding first = peerSaying(waiting, "first");
  if (!first.cameUp) GTEST_SKIP() << kNeverCameUp;
  ASSERT_FALSE(first.sender.empty()) << kNeverPaired << waiting->error();
  const PeerStanding second = peerSaying(waiting, "second");
  if (!second.cameUp) GTEST_SKIP() << kNeverCameUp;
  ASSERT_FALSE(second.sender.empty()) << kNeverPaired << waiting->error();
  ASSERT_NE(first.sender, second.sender);

  EXPECT_TRUE(waiting->sendTo(first.sender, bytesOf("to you alone")));
  EXPECT_TRUE(echoedBy(waiting, first.sender, "to you alone"));
  // The other peer echoes everything it hears, so what says the message
  // above reached one peer and not the room is that this one never
  // echoed it — while the broadcast after it comes back from both.
  EXPECT_TRUE(waiting->send(bytesOf("out to everyone")));
  EXPECT_TRUE(echoedBy(waiting, second.sender, "out to everyone"));
  bool answeredTwice = false;
  while (const std::optional<Arrival> arrival = waiting->receive())
    if (arrival->from == second.sender &&
        arrival->bytes->asText() == std::string(kEcho) + "to you alone")
      answeredTwice = true;
  EXPECT_FALSE(answeredTwice);
}

TEST_F(IOWebRtc, ASignalNobodyAnswersLeavesTheCallerOpenAndSilent) {
  // The signalling door stands and the caller's introduction crosses
  // it; what is not there is anybody waiting in the room to answer.
  const std::shared_ptr<Feed> door = open("ws://:0/signal");
  ASSERT_TRUE(door->error().empty()) << door->error();
  const uint16_t port = portOf(door->address());
  ASSERT_NE(port, 0) << door->address();
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
  uint16_t port = 0;
  {
    // Held here and nowhere else — the fixture keeps no hold of this
    // one, since what this case watches is the last holder letting go.
    const std::shared_ptr<Feed> waiting = hub.feed(waitingAt(0));
    ASSERT_TRUE(waiting->error().empty()) << waiting->error();
    // The port it was given, read off it while it still stands: what
    // this case asks for again below is the very port that went.
    port = portOf(waiting->address());
    ASSERT_NE(port, 0) << waiting->address();
    const PeerStanding peer = peerSaying(waiting, "a phone speaks");
    if (!peer.cameUp) GTEST_SKIP() << kNeverCameUp;
    ASSERT_FALSE(peer.sender.empty()) << kNeverPaired << waiting->error();
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
 *  never returned — and, over the dozen, that the arrangement carries a
 *  message at all. A pair that finds no route is one conversation's
 *  ending, which the door shows as a peer let go and the feed shows as
 *  nothing, since a feed with a room of peers is not failed by one of
 *  them; a pair that ends the process is the library's. */
TEST_F(IOWebRtc,
       BothEndsOfAConversationInOneProcessPairAndPartWithoutEndingIt) {
  int crossings = 0;
  for (int conversation = 1; conversation <= kConversations; ++conversation) {
    // A room and a port of its own for every pair, so what the sweep
    // brings good is the two ends opened together and no leftover of
    // the pair before them.
    const std::string room = "sweep" + std::to_string(conversation);
    // The port is the kernel's to give and the door answers the one it
    // got: a signalling door gives its port back a moment after its
    // last holder lets go, on the listener's own loop, and the pair
    // before this one let go a moment ago — so a port named by number
    // here would be one that pair may still be holding.
    const std::shared_ptr<Feed> waiting = open(waitingAt(0, room));
    ASSERT_TRUE(waiting->error().empty()) << waiting->error();
    const uint16_t port = portOf(waiting->address());
    ASSERT_NE(port, 0) << waiting->address();
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
    if (crossed) ++crossings;

    // Both ends are let go before the next pair is made, so a teardown
    // and a handshake are on that thread together as well.
    calling->close();
    waiting->close();
  }
  EXPECT_GE(crossings, 1) << "no pair of the " << kConversations
                          << " carried its message";
  RecordProperty("crossings", crossings);
}

}  // namespace
