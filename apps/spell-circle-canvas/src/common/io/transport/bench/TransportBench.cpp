/** @file
 * io_transport_bench — THE WIRES' OWN ARMS: one per door, with both
 * ends of it standing in this one process, measuring the messages a
 * second that door carries end to end. A batch goes out through one
 * feed and the arm spins until the feed at the other end says it has
 * taken that many, so what is timed is the whole round through the
 * door — the send, the stack underneath it, the thread or the driver
 * callback the far end is delivered from, and the feed that takes the
 * message — and never the socket alone.
 *
 * Every arm opens what it measures for itself: a port on the loopback, a
 * region of this machine's memory, a MIDI port made rather than found.
 * Nothing is plugged in and nothing is reached over a network. A machine that
 * will not give this process the door an arm asks for skips that arm with the
 * reason; a door that opened and then did not carry its batch inside the
 * deadline ends its arm with the error, which is a regression rather than a
 * machine.
 *
 * Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <sigilio/advanced/Time.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <thread>

namespace {

using sigil::io::Bytes;
using sigil::io::Feed;
using sigil::io::Hub;
using sigil::io::SharedMemoryWriter;
using namespace std::chrono_literals;

using Moment = std::chrono::steady_clock::time_point;

/** HOW MANY MESSAGES ONE ITERATION SENDS. Enough of them that the wait
 *  for the last is a small part of what the iteration costs, and few
 *  enough that no door's buffer between the two ends fills while the
 *  receiving end drains it — a door that drops under a burst measures
 *  the burst rather than the door. */
constexpr size_t kBatch = 64;

/** The longest one batch is given. It is long enough that a loaded
 *  machine is never mistaken for a door that stopped carrying, and
 *  short enough that an arm ends rather than hangs. */
constexpr auto kDeadline = 10s;

/** The longest a door is given to stand before an arm is timed: a port
 *  to appear to the rest of the machine, a session to finish its
 *  handshake, a path to say how large a datagram may be. */
constexpr auto kReach = 2s;

/** What an arm ends with where a door that opened stopped carrying: not
 *  a machine without such a door, but a door that took its batch before
 *  and does not now. */
constexpr const char* kNotCarried =
    "the door did not carry its batch before the deadline";

/** A payload of @p size bytes: one printable letter over and over, so
 *  the same payload is a whole line on the door that carries lines and
 *  bytes on every other. */
Bytes payloadOf(size_t size) {
  return Bytes(std::vector<std::byte>(size, static_cast<std::byte>('x')));
}

/** The port out of an address a feed reports. The authority is what
 *  stands before the first slash behind the scheme, an IPv6 address in
 *  it is bracketed, and the port follows that authority's last colon —
 *  so one reading serves every door here, whether or not a path stands
 *  behind the port. */
uint16_t portOf(const std::string& address) {
  const size_t scheme = address.find("://");
  const size_t start = scheme == std::string::npos ? 0 : scheme + 3;
  const size_t path = address.find('/', start);
  const std::string authority = address.substr(
      start, path == std::string::npos ? std::string::npos : path - start);
  const size_t colon = authority.rfind(':');
  if (colon == std::string::npos) return 0;
  return static_cast<uint16_t>(std::stoul(authority.substr(colon + 1)));
}

/** Polls @p ready with a moment between looks, or gives up. For the
 *  waits an arm makes before it is timed, which ask the system for
 *  something rather than for a message: asking again in a moment is what
 *  such a wait costs least. */
bool waitUntil(const std::function<bool()>& ready) {
  const Moment until = std::chrono::steady_clock::now() + kReach;
  while (std::chrono::steady_clock::now() < until) {
    if (ready()) return true;
    std::this_thread::sleep_for(1ms);
  }
  return ready();
}

/** HANDING ONE MESSAGE TO A DOOR, given the moment the batch it belongs
 *  to gives up at. Most doors take one through a feed; the cable takes
 *  one by the file descriptor at the other end of it. */
using SendOne = std::function<bool(Moment until)>;

/** ONE BATCH THROUGH A DOOR: @p sendOne is offered until it is taken,
 *  kBatch times over, and the feed at the far end is spun on until it
 *  has taken that many more messages. What counts them is revision(),
 *  which counts every message a feed took whether or not a reader
 *  drained it, so nothing here has to hold a batch in the feed's queue
 *  to count it and the feed keeps what its policy comes with.
 *
 *  ONE DEADLINE COVERS THE WHOLE BATCH: a door that stopped taking
 *  messages and one that stopped delivering them both end the arm
 *  rather than hanging it. The wait is spun rather than slept, so a
 *  message is counted when it lands rather than at the next tick of a
 *  sleep. */
bool carried(const Feed& into, const SendOne& sendOne) {
  const Moment until = std::chrono::steady_clock::now() + kDeadline;
  const uint64_t wanted = into.state().revision + kBatch;
  for (size_t message = 0; message != kBatch; ++message) {
    while (!sendOne(until)) {
      if (std::chrono::steady_clock::now() > until) return false;
      std::this_thread::yield();
    }
  }
  while (into.state().revision < wanted) {
    if (std::chrono::steady_clock::now() > until) return false;
    std::this_thread::yield();
  }
  return true;
}

/** WHAT EVERY ARM REPORTS: the messages it carried and the bytes they
 *  came to. Both are divided by the time the iterations took, so a row
 *  says the messages a second the door carried and the bytes a second
 *  they were made of. */
void countMessages(benchmark::State& state, size_t size) {
  const int64_t messages =
      static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(kBatch);
  state.SetItemsProcessed(messages);
  state.SetBytesProcessed(messages * static_cast<int64_t>(size));
}

/** A name no other program on this machine is using: the door's own word
 *  and the number the system gave this run. */
std::string uniqueName(std::string_view what) {
  return "sigil-" + std::string(what) + "-" +
         std::to_string(static_cast<int>(::getpid()));
}

// ------------------------------------------------------------------ udp

/** The datagrams one socket on the loopback sends to another: what is
 *  timed is the send, the receiving socket's own thread and the feed it
 *  delivers into. */
void BM_Udp(benchmark::State& state) {
  const size_t size = static_cast<size_t>(state.range(0));
  Hub hub;
  sigil::io::registerTransports(hub, {"udp"});
  const Feed listener = hub.listen("udp://:0");
  if (!listener.state().error.empty()) {
    state.SkipWithError(listener.state().error);
    return;
  }
  const Feed sender =
      hub.listen("udp://127.0.0.1:" +
                 std::to_string(portOf(listener.state().localAddress)));
  if (!sender.state().error.empty()) {
    state.SkipWithError(sender.state().error);
    return;
  }

  const Bytes message = payloadOf(size);
  const SendOne sendOne = [&](Moment) { return sender.send(message); };
  // One batch before anything is timed: a socket that has just been
  // bound pays for the first datagram through it, and an arm measures
  // the door standing rather than the door opening.
  if (!carried(listener, sendOne)) {
    state.SkipWithError(kNotCarried);
    return;
  }
  for ([[maybe_unused]] auto iteration : state) {
    if (!carried(listener, sendOne)) {
      state.SkipWithError(kNotCarried);
      break;
    }
  }
  countMessages(state, size);
}
BENCHMARK(BM_Udp)->Arg(64)->Arg(1400)->Unit(benchmark::kMicrosecond);

// ------------------------------------------------------------ websocket

/** A listener's messages to the one peer that called it: what is timed
 *  is the send deferred onto the listener's loop, the frames it writes,
 *  and the session thread the client's feed is delivered from. */
void BM_WebSocket(benchmark::State& state) {
  const size_t size = static_cast<size_t>(state.range(0));
  Hub hub;
  // The listener is taught first and the client stands in front of it:
  // one scheme, and the shape of the URI is what says which end a feed
  // is.
  sigil::io::registerTransports(hub, {"ws"});
  const Feed server = hub.listen("ws://:0/bench");
  if (!server.state().error.empty()) {
    state.SkipWithError(server.state().error);
    return;
  }
  const Feed client = hub.listen(
      "ws://127.0.0.1:" + std::to_string(portOf(server.state().localAddress)) +
      "/bench");
  if (!client.state().error.empty()) {
    state.SkipWithError(client.state().error);
    return;
  }

  const Bytes message = payloadOf(size);
  // The client reads Open once its handshake is through, and what says
  // the listener has the peer to publish to is the first message sent
  // after that arriving there.
  const uint64_t standing = server.state().revision + 1;
  if (!waitUntil([&] { return client.state().isOpen(); }) ||
      !client.send(message) ||
      !waitUntil([&] { return server.state().revision >= standing; })) {
    state.SkipWithError("the session never stood: " + client.state().error);
    return;
  }

  const SendOne sendOne = [&](Moment) { return server.send(message); };
  if (!carried(client, sendOne)) {
    state.SkipWithError(kNotCarried);
    return;
  }
  for ([[maybe_unused]] auto iteration : state) {
    if (!carried(client, sendOne)) {
      state.SkipWithError(kNotCarried);
      break;
    }
  }
  countMessages(state, size);
}
BENCHMARK(BM_WebSocket)->Arg(64)->Arg(1400)->Unit(benchmark::kMicrosecond);

// -------------------------------------------------------- shared memory

/** What a region is made to hold, which is more than any message an arm
 *  writes into it. */
constexpr size_t kRegionRoom = 4096;

/** How often the reader looks at the region. A look takes the message
 *  standing there and at most one, so the rate is what bounds this
 *  door and the arm asks for a fast one rather than the one a scene
 *  drawing on the frame is served by. */
constexpr const char* kLookRate = "?rate=20000";

/** A BATCH INTO A REGION, one message at a time: a region holds the
 *  message standing NOW, so each is waited for before the next is
 *  written — a batch written without pause would be written over itself
 *  and arrive as one message. What this arm times is therefore the
 *  round from a write to the look that takes it, at the rate the URI
 *  asked the reader to look. */
bool carriedByRegion(SharedMemoryWriter& writer, const Feed& into,
                     std::span<const std::byte> message) {
  const Moment until = std::chrono::steady_clock::now() + kDeadline;
  for (size_t written = 0; written != kBatch; ++written) {
    const uint64_t wanted = into.state().revision + 1;
    if (!writer.write(message)) return false;
    while (into.state().revision < wanted) {
      if (std::chrono::steady_clock::now() > until) return false;
      std::this_thread::yield();
    }
  }
  return true;
}

void BM_SharedMemory(benchmark::State& state) {
  const size_t size = static_cast<size_t>(state.range(0));
  const std::string name = uniqueName("shm");
  SharedMemoryWriter writer(name, kRegionRoom);
  if (!writer.open()) {
    state.SkipWithMessage("this machine made no region of that name");
    return;
  }
  Hub hub;
  sigil::io::registerTransports(hub, {"shm"});
  const Feed region = hub.listen("shm://" + name + kLookRate);
  if (!region.state().error.empty()) {
    state.SkipWithError(region.state().error);
    return;
  }

  const Bytes message = payloadOf(size);
  const std::span<const std::byte> written = message.span();
  if (!carriedByRegion(writer, region, written)) {
    state.SkipWithError(kNotCarried);
    return;
  }
  for ([[maybe_unused]] auto iteration : state) {
    if (!carriedByRegion(writer, region, written)) {
      state.SkipWithError(kNotCarried);
      break;
    }
  }
  countMessages(state, size);
}
BENCHMARK(BM_SharedMemory)->Arg(64)->Arg(1400)->Unit(benchmark::kMicrosecond);

// ----------------------------------------------------------------- midi

/** How many bytes a message on this door is: a status byte and the two
 *  a note takes. There is no size to sweep — what a controller sends is
 *  this long and the arm carries what a controller sends. */
constexpr size_t kNoteBytes = 3;

/** A port of this machine's own, made rather than found, and the input
 *  opened back on it by name: both ends of a cable in one process with
 *  no controller plugged in. What is timed is the write into the
 *  driver and the callback of its own it delivers the message on. */
void BM_Midi(benchmark::State& state) {
  Hub hub;
  sigil::io::registerTransports(hub, {"midi"});
  const std::string name = uniqueName("midi");
  const Feed keys = hub.listen("midi://out/virtual:" + name);
  if (!keys.state().error.empty()) {
    state.SkipWithMessage("this machine made no midi port of its own: " +
                          keys.state().error);
    return;
  }
  // A port takes a moment to appear to everything else on the machine,
  // so the input is opened until it opens rather than once.
  Feed pads;
  const bool opened = waitUntil([&] {
    pads = hub.listen("midi://in/" + name);
    if (pads.state().error.empty()) return true;
    pads = {};
    return false;
  });
  if (!opened) {
    state.SkipWithMessage("no input port was made for " + name);
    return;
  }

  // A note on the middle C of the first channel, struck hard.
  const Bytes message(std::vector<std::byte>{std::byte{0x90}, std::byte{0x3C},
                                             std::byte{0x64}});
  const SendOne sendOne = [&](Moment) { return keys.send(message); };
  if (!carried(pads, sendOne)) {
    state.SkipWithError(kNotCarried);
    return;
  }
  for ([[maybe_unused]] auto iteration : state) {
    if (!carried(pads, sendOne)) {
      state.SkipWithError(kNotCarried);
      break;
    }
  }
  countMessages(state, kNoteBytes);
}
BENCHMARK(BM_Midi)->Unit(benchmark::kMicrosecond);

}  // namespace
