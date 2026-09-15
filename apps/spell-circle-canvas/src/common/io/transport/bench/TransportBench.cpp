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
 * region of this machine's memory, a pseudo-terminal pair, a MIDI port
 * made rather than found. Nothing is plugged in and nothing is reached
 * over a network. A machine that will not give this process the door an
 * arm asks for skips that arm with the reason; a door that opened and
 * then did not carry its batch inside the deadline ends its arm with the
 * error, which is a regression rather than a machine.
 *
 * Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>

#include "ScratchDir.h"

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
  Bytes out;
  out.bytes.assign(size, static_cast<std::byte>('x'));
  return out;
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
 *  has taken that many more messages. What counts them is generation(),
 *  which counts every message a feed took whether or not a reader
 *  drained it, so nothing here has to hold a batch in the feed's queue
 *  to count it and the feed keeps what its policy comes with.
 *
 *  ONE DEADLINE COVERS THE WHOLE BATCH: a door that stopped taking
 *  messages and one that stopped delivering them both end the arm
 *  rather than hanging it. The wait is spun rather than slept, so a
 *  message is counted when it lands rather than at the next tick of a
 *  sleep. */
bool carried(const std::shared_ptr<Feed>& into, const SendOne& sendOne) {
  const Moment until = std::chrono::steady_clock::now() + kDeadline;
  const uint64_t wanted = into->generation() + kBatch;
  for (size_t message = 0; message != kBatch; ++message) {
    while (!sendOne(until)) {
      if (std::chrono::steady_clock::now() > until) return false;
      std::this_thread::yield();
    }
  }
  while (into->generation() < wanted) {
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
  sigil::io::registerUdp(hub);
  const std::shared_ptr<Feed> listener = hub.feed("udp://:0");
  if (!listener->error().empty()) {
    state.SkipWithError(listener->error());
    return;
  }
  const std::shared_ptr<Feed> sender = hub.feed(
      "udp://127.0.0.1:" + std::to_string(portOf(listener->address())));
  if (!sender->error().empty()) {
    state.SkipWithError(sender->error());
    return;
  }

  const Bytes message = payloadOf(size);
  const SendOne sendOne = [&](Moment) { return sender->send(message); };
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
  sigil::io::registerWebSocket(hub);
  sigil::io::registerWebSocketClient(hub);
  const std::shared_ptr<Feed> server = hub.feed("ws://:0/bench");
  if (!server->error().empty()) {
    state.SkipWithError(server->error());
    return;
  }
  const std::shared_ptr<Feed> client = hub.feed(
      "ws://127.0.0.1:" + std::to_string(portOf(server->address())) + "/bench");
  if (!client->error().empty()) {
    state.SkipWithError(client->error());
    return;
  }

  const Bytes message = payloadOf(size);
  // The handshake runs on the client's own thread, so what says the
  // session is up at this end is the first send that goes out over it,
  // and what says the listener has the peer to publish to is that
  // message arriving there.
  const uint64_t standing = server->generation() + 1;
  if (!waitUntil([&] { return client->send(message); }) ||
      !waitUntil([&] { return server->generation() >= standing; })) {
    state.SkipWithError("the session never stood: " + client->error());
    return;
  }

  const SendOne sendOne = [&](Moment) { return server->send(message); };
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
bool carriedByRegion(SharedMemoryWriter& writer,
                     const std::shared_ptr<Feed>& into,
                     std::span<const std::byte> message) {
  const Moment until = std::chrono::steady_clock::now() + kDeadline;
  for (size_t written = 0; written != kBatch; ++written) {
    const uint64_t wanted = into->generation() + 1;
    if (!writer.write(message)) return false;
    while (into->generation() < wanted) {
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
  sigil::io::registerSharedMemory(hub);
  const std::shared_ptr<Feed> region = hub.feed("shm://" + name + kLookRate);
  if (!region->error().empty()) {
    state.SkipWithError(region->error());
    return;
  }

  const Bytes message = payloadOf(size);
  const std::span<const std::byte> written(message.bytes);
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
  sigil::io::registerMidi(hub);
  const std::string name = uniqueName("midi");
  const std::shared_ptr<Feed> keys = hub.feed("midi://out/virtual:" + name);
  if (!keys->error().empty()) {
    state.SkipWithMessage("this machine made no midi port of its own: " +
                          keys->error());
    return;
  }
  // A port takes a moment to appear to everything else on the machine,
  // so the input is opened until it opens rather than once.
  std::shared_ptr<Feed> pads;
  const bool opened = waitUntil([&] {
    pads = hub.feed("midi://in/" + name);
    if (pads->error().empty()) return true;
    pads.reset();
    return false;
  });
  if (!opened) {
    state.SkipWithMessage("no input port was made for " + name);
    return;
  }

  // A note on the middle C of the first channel, struck hard.
  Bytes message;
  for (int one : {0x90, 0x3C, 0x64})
    message.bytes.push_back(static_cast<std::byte>(one));
  const SendOne sendOne = [&](Moment) { return keys->send(message); };
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

// --------------------------------------------------------------- serial

/** A PSEUDO-TERMINAL PAIR STANDING IN FOR A BOARD: the slave is a port
 *  with a path, which is the whole of what the serial transport asks of
 *  a device, and the master is the end this process writes the readings
 *  into. What an arm on it times is the reading, the line the transport
 *  cuts out of the bytes and the feed it delivers into, with a kernel
 *  buffer where the wire would be. */
struct PseudoTerminal {
  PseudoTerminal() {
    master = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master < 0) return;
    if (::grantpt(master) != 0 || ::unlockpt(master) != 0) return;
    const char* const named = ::ptsname(master);
    if (named != nullptr) device = named;
  }
  ~PseudoTerminal() {
    if (master >= 0) ::close(master);
  }
  PseudoTerminal(const PseudoTerminal&) = delete;
  PseudoTerminal& operator=(const PseudoTerminal&) = delete;

  /** Raw mode between the two ends, which is what makes the bytes
   *  written here the bytes the port reads: a discipline left as it
   *  comes echoes them back and turns a newline into a pair. A pair
   *  answers no such call until its slave is open, so this is asked once
   *  the feed has opened it. */
  bool raw() const {
    termios line{};
    if (::tcgetattr(master, &line) != 0) return false;
    ::cfmakeraw(&line);
    return ::tcsetattr(master, TCSANOW, &line) == 0;
  }

  /** One line onto the wire, whole: a master with no room now takes the
   *  rest of it once the reader has drained some, and gives up at @p
   *  until rather than writing half a line forever. */
  bool writeLine(std::string_view line, Moment until) const {
    size_t written = 0;
    while (written != line.size()) {
      const ssize_t count =
          ::write(master, line.data() + written, line.size() - written);
      if (count > 0) {
        written += static_cast<size_t>(count);
        continue;
      }
      if (std::chrono::steady_clock::now() > until) return false;
      std::this_thread::yield();
    }
    return true;
  }

  int master = -1;
  std::string device;
};

void BM_Serial(benchmark::State& state) {
  const size_t size = static_cast<size_t>(state.range(0));
  PseudoTerminal pair;
  if (pair.device.empty()) {
    state.SkipWithMessage("this machine offered no pseudo-terminal pair");
    return;
  }
  Hub hub;
  sigil::io::registerSerial(hub);
  const std::shared_ptr<Feed> board =
      hub.feed("serial://" + pair.device + "?baud=115200");
  if (!board->error().empty()) {
    state.SkipWithError(board->error());
    return;
  }
  if (!pair.raw()) {
    state.SkipWithMessage("this machine would not put the pair in raw mode");
    return;
  }

  // A message is a line, so the newline is written after the payload and
  // is no part of the message the feed takes.
  const std::string line = std::string(size, 'x') + "\n";
  const SendOne sendOne = [&](Moment until) {
    return pair.writeLine(line, until);
  };
  if (!carried(board, sendOne)) {
    state.SkipWithError(kNotCarried);
    return;
  }
  for ([[maybe_unused]] auto iteration : state) {
    if (!carried(board, sendOne)) {
      state.SkipWithError(kNotCarried);
      break;
    }
  }
  countMessages(state, size);
}
BENCHMARK(BM_Serial)->Arg(64)->Arg(1400)->Unit(benchmark::kMicrosecond);

// ----------------------------------------------------------------- grpc

/** A call on one generic method, both ends of it in this binary: what is
 *  timed is the queue a message is written from, the stream underneath
 *  it, and the thread of gRPC's own the server's feed is delivered
 *  from. */
void BM_Grpc(benchmark::State& state) {
  const size_t size = static_cast<size_t>(state.range(0));
  Hub hub;
  sigil::io::registerGrpc(hub);
  const std::shared_ptr<Feed> server = hub.feed("grpc://:0/Sky/Watch");
  if (!server->error().empty()) {
    state.SkipWithError(server->error());
    return;
  }
  const std::shared_ptr<Feed> caller =
      hub.feed("grpc://127.0.0.1:" + std::to_string(portOf(server->address())) +
               "/Sky/Watch");
  if (!caller->error().empty()) {
    state.SkipWithError(caller->error());
    return;
  }

  const Bytes message = payloadOf(size);
  const SendOne sendOne = [&](Moment) { return caller->send(message); };
  // The first batch is what the call is opened by: a message handed over
  // before the stream is up waits on it rather than going nowhere, and
  // an arm measures the stream standing rather than the call reaching.
  if (!carried(server, sendOne)) {
    state.SkipWithError(kNotCarried);
    return;
  }
  for ([[maybe_unused]] auto iteration : state) {
    if (!carried(server, sendOne)) {
      state.SkipWithError(kNotCarried);
      break;
    }
  }
  countMessages(state, size);
}
BENCHMARK(BM_Grpc)->Arg(64)->Arg(1400)->Unit(benchmark::kMicrosecond);

// ----------------------------------------------------------------- quic

/** A SELF-SIGNED PAIR FOR ONE ARM TO ANSWER WITH, written into @p
 *  scratch as cert.pem and key.pem.
 *
 *  A QUIC port answers for itself or it answers nobody, and nobody signs
 *  for a machine an arm makes up, so the pair is made here: an
 *  elliptic-curve key on the P-256 curve and a certificate naming
 *  localhost that vouches for itself. False where the pair could not be
 *  made or written, which is an arm that cannot run rather than a door
 *  that regressed. */
bool standCredentials(const sigil::test::ScratchDir& scratch) {
  EVP_PKEY* key = EVP_EC_gen("P-256");
  if (!key) return false;
  X509* certificate = X509_new();
  if (!certificate) {
    EVP_PKEY_free(key);
    return false;
  }
  bool made = false;
  do {
    // Version 3, which is what the number 2 spells here.
    if (X509_set_version(certificate, 2) != 1) break;
    if (ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1) != 1) break;
    if (!X509_gmtime_adj(X509_getm_notBefore(certificate), 0)) break;
    if (!X509_gmtime_adj(X509_getm_notAfter(certificate), 60 * 60)) break;
    if (X509_set_pubkey(certificate, key) != 1) break;
    X509_NAME* const named = X509_get_subject_name(certificate);
    if (X509_NAME_add_entry_by_txt(
            named, "CN", MBSTRING_ASC,
            reinterpret_cast<const unsigned char*>("localhost"), -1, -1,
            0) != 1)
      break;
    // Signed by itself, which is what makes it self-signed.
    if (X509_set_issuer_name(certificate, named) != 1) break;
    if (X509_sign(certificate, key, EVP_sha256()) == 0) break;

    BIO* out = BIO_new_file((scratch.path / "cert.pem").string().c_str(), "wb");
    if (!out) break;
    const int wroteCertificate = PEM_write_bio_X509(out, certificate);
    BIO_free(out);
    if (wroteCertificate != 1) break;
    out = BIO_new_file((scratch.path / "key.pem").string().c_str(), "wb");
    if (!out) break;
    const int wroteKey = PEM_write_bio_PrivateKey(out, key, nullptr, nullptr, 0,
                                                  nullptr, nullptr);
    BIO_free(out);
    if (wroteKey != 1) break;
    made = true;
  } while (false);
  X509_free(certificate);
  EVP_PKEY_free(key);
  return made;
}

/** ONE CONNECTION, CARRYING ITS MESSAGES THE WAY THE URI ASKED: a
 *  unidirectional stream apiece, or a datagram apiece where @p
 *  asDatagrams. What is timed either way is the send, the encryption and
 *  the worker of the library's own the listening feed is delivered
 *  from. */
void measureQuic(benchmark::State& state, bool asDatagrams) {
  const size_t size = static_cast<size_t>(state.range(0));
  const sigil::test::ScratchDir scratch(asDatagrams ? "sigilio_quic_datagram"
                                                    : "sigilio_quic");
  if (!standCredentials(scratch)) {
    state.SkipWithMessage("no certificate could be made on this machine");
    return;
  }
  Hub hub;
  sigil::io::registerQuic(hub);
  std::string listening =
      "quic://:0?cert=" + (scratch.path / "cert.pem").string() +
      "&key=" + (scratch.path / "key.pem").string();
  std::string calling = "quic://127.0.0.1:";
  if (asDatagrams) listening += "&datagrams=1";
  const std::shared_ptr<Feed> listener = hub.feed(listening);
  if (!listener->error().empty()) {
    state.SkipWithError(listener->error());
    return;
  }
  // The self-signed pair is reached the way a machine on a stage is: the
  // certificate is not checked, and everything crossing is encrypted all
  // the same.
  calling += std::to_string(portOf(listener->address())) + "?insecure=1";
  if (asDatagrams) calling += "&datagrams=1";
  const std::shared_ptr<Feed> caller = hub.feed(calling);
  if (!caller->error().empty()) {
    state.SkipWithError(caller->error());
    return;
  }

  // THE SMALLEST MESSAGE FIRST. A send is taken as soon as there is a
  // connection to carry it, and a datagram only once the path has said
  // how large one may be, so what says both stand is the door taking a
  // message nothing could refuse for its size.
  const Bytes smallest = payloadOf(1);
  if (!waitUntil([&] { return caller->send(smallest); })) {
    state.SkipWithError("the connection never stood: " + caller->error());
    return;
  }
  const Bytes message = payloadOf(size);
  if (!waitUntil([&] { return caller->send(message); })) {
    state.SkipWithMessage("this connection carries no message of that size");
    return;
  }

  const SendOne sendOne = [&](Moment) { return caller->send(message); };
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

void BM_Quic(benchmark::State& state) {
  measureQuic(state, /*asDatagrams=*/false);
}
BENCHMARK(BM_Quic)->Arg(64)->Arg(1400)->Unit(benchmark::kMicrosecond);

void BM_QuicDatagrams(benchmark::State& state) {
  measureQuic(state, /*asDatagrams=*/true);
}
BENCHMARK(BM_QuicDatagrams)->Arg(64)->Arg(1400)->Unit(benchmark::kMicrosecond);

/** NO WEBRTC ARM STANDS HERE. Both ends of one conversation inside a
 *  single process is the arrangement the library underneath that door
 *  can end the process from — which is why its cases run the far end as
 *  a process of their own — and an arm whose two ends were two
 *  processes would time the pipe between them beside the door it meant
 *  to measure. That door is proven by its suite and left out of the
 *  ledger. */

}  // namespace
