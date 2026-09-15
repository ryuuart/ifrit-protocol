/** @file
 * The gRPC transport: the method a feed's URI names, the server that
 * holds it or the call that reaches it, the calls that arrive as peers,
 * and the bytes that go each way over one bidirectional stream.
 *
 * NOTHING HERE KNOWS WHAT A MESSAGE IS. The method is generic at both
 * ends — every message is a buffer gRPC carries from one side to the
 * other and never a structure it parses — so a feed's FlatBuffers, its
 * JSON and whatever else a sender writes cross a gRPC method with no
 * generated stub anywhere in this file. What a message MEANS is the
 * business of the library that owns the format, exactly as it is on
 * every other door.
 *
 * One scheme, two shapes, and the shape of the URI is what says which
 * end a feed is: grpc://:PORT/Service/Method holds a port and every
 * call that reaches it is a peer, grpc://HOST:PORT/Service/Method calls
 * one and holds the single stream it opened.
 *
 * INSECURE AT BOTH ENDS. A server takes callers without TLS and a call
 * is made without it, so this door belongs on a machine or a network
 * somebody already trusts; a scheme carrying credentials is the door
 * beside this one and is not built yet.
 *
 * NO THREAD IS STARTED FOR A FEED. gRPC runs threads of its own and
 * calls back onto them, so a server's callers and a client's stream are
 * carried without this library holding a loop, an executor or a
 * completion queue. The one thread this file ever makes is the one a
 * door let go from inside a callback hands its shutdown to, below.
 */

#include <grpc/impl/channel_arg_names.h>
#include <grpc/impl/connectivity_state.h>
#include <grpcpp/alarm.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/generic/callback_generic_service.h>
#include <grpcpp/generic/generic_stub_callback.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/byte_buffer.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/support/slice.h>
#include <grpcpp/support/status.h>
#include <grpcpp/support/stub_options.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/source/Source.h"
#include "sigilio/transport/Transport.h"

namespace sigil::io {
namespace {

/** How long a call's channel is given to connect before the feed is
 *  told the server was not reached. A channel that is refused says so
 *  at once and never waits this out; what this bounds is the other
 *  case, a host that takes the connection nowhere, which without a
 *  bound would leave a feed waiting for as long as anybody held it. */
constexpr std::chrono::seconds kReachWithin{10};

/** WHAT A grpc:// URI NAMES: the host that makes it a server to call
 *  rather than a port to hold, the port, and the method both ends name,
 *  which is a leading slash, a service and a method. */
struct Address {
  std::string host;
  std::string port;
  std::string method;
};

/** Whether @p path is a method both ends can name: a slash, a service,
 *  a slash and a method, with nothing empty and nothing after. It is
 *  the shape gRPC itself puts on the wire, so a URI that does not carry
 *  one names no method at all rather than half of one. */
bool namesMethod(std::string_view path) {
  if (!path.starts_with('/')) return false;
  const std::string_view rest = path.substr(1);
  const size_t slash = rest.find('/');
  if (slash == std::string_view::npos || slash == 0) return false;
  const std::string_view method = rest.substr(slash + 1);
  return !method.empty() && method.find('/') == std::string_view::npos;
}

/** The address @p uri names, or nothing when it names none: a host and
 *  a port, then the method behind them. The host is bracketed when it
 *  is an IPv6 literal and left out altogether to hold a port on every
 *  interface; the port is decimal digits and nothing else. */
std::optional<Address> parseAddress(std::string_view uri) {
  constexpr std::string_view kScheme = "grpc://";
  if (!uri.starts_with(kScheme)) return std::nullopt;
  std::string_view rest = uri.substr(kScheme.size());
  Address address;
  // The method is taken off the back first, so what remains is an
  // authority the same shape as any other and the slash inside a method
  // is never mistaken for the one that ends it.
  const size_t slash = rest.find('/');
  if (slash == std::string_view::npos) return std::nullopt;
  const std::string_view named = rest.substr(slash);
  if (!namesMethod(named)) return std::nullopt;
  address.method = std::string(named);
  rest = rest.substr(0, slash);

  if (rest.starts_with('[')) {
    const size_t bracket = rest.find(']');
    if (bracket == std::string_view::npos || bracket == 1) return std::nullopt;
    address.host = std::string(rest.substr(1, bracket - 1));
    rest = rest.substr(bracket + 1);
  } else {
    // An unbracketed host ends at the first colon, which leaves an IPv6
    // literal written without its brackets to fail on its own digits.
    const size_t colon = rest.find(':');
    if (colon == std::string_view::npos) return std::nullopt;
    address.host = std::string(rest.substr(0, colon));
    rest = rest.substr(colon);
  }
  if (!rest.starts_with(':')) return std::nullopt;
  const std::string_view digits = rest.substr(1);
  unsigned int port = 0;
  const char* const end = digits.data() + digits.size();
  const std::from_chars_result read = std::from_chars(digits.data(), end, port);
  if (read.ec != std::errc() || read.ptr != end || port > 65535)
    return std::nullopt;
  address.port = std::string(digits);
  return address;
}

/** @p message as one buffer for gRPC to carry. The bytes are copied
 *  into a slice of gRPC's own, so the caller's Bytes are its own again
 *  as soon as this returns. */
grpc::ByteBuffer bufferOf(const Bytes& message) {
  const grpc::Slice slice(message.bytes.data(), message.bytes.size());
  return grpc::ByteBuffer(&slice, 1);
}

/** Every byte of @p buffer, gathered out of however many slices it was
 *  carried in; nothing when it cannot be read. A message reaches a feed
 *  whole or not at all, which is what a feed's message is. */
std::optional<Bytes> bytesOf(const grpc::ByteBuffer& buffer) {
  std::vector<grpc::Slice> slices;
  if (!buffer.Dump(&slices).ok()) return std::nullopt;
  Bytes out;
  out.bytes.reserve(buffer.Length());
  for (const grpc::Slice& slice : slices) {
    const auto* const first = reinterpret_cast<const std::byte*>(slice.begin());
    out.bytes.insert(out.bytes.end(), first, first + slice.size());
  }
  return out;
}

/** One caller's address with the call it arrived on behind it, spelled
 *  the way a URI of this scheme is: `grpc://127.0.0.1:52341#3`.
 *
 *  gRPC names a peer by the transport it came over — `ipv4:127.0.0.1:
 *  52341` — and what a reader wants is the address alone, so that word
 *  is taken off the front; an IPv6 peer keeps its brackets, so what
 *  follows the last colon is always the port.
 *
 *  ONE CALLER MAY HOLD SEVERAL CALLS at once, and each of them is a
 *  stream of its own with its own answers, so the address alone would
 *  not tell two of them apart. The number is what does, and it counts
 *  the calls one server feed has taken. */
std::string callerAddress(std::string_view peer, uint64_t call) {
  const size_t transport = peer.find(':');
  const std::string_view named =
      transport == std::string_view::npos ? peer : peer.substr(transport + 1);
  return "grpc://" + std::string(named) + "#" + std::to_string(call);
}

/** Whether this thread is inside one of gRPC's own callbacks.
 *
 *  A server is shut down by waiting for every call's callbacks to end,
 *  and a callback cannot wait for itself. The last holder of a feed can
 *  be the very delivery arriving on it, so letting that delivery go is
 *  one of the ways a door is closed — and a door closed that way hands
 *  its shutdown to a thread nothing is waiting for. */
thread_local bool insideCallback = false;

/** Raises that flag for as long as one callback runs. */
struct InCallback {
  InCallback() : before(insideCallback) { insideCallback = true; }
  ~InCallback() { insideCallback = before; }

  InCallback(const InCallback&) = delete;
  InCallback& operator=(const InCallback&) = delete;

  bool before;
};

class ServerCall;

/** WHAT A SERVER FEED'S CALLBACKS SHARE: the feed every caller delivers
 *  into, the one method this door answers to, and the calls standing on
 *  it now.
 *
 *  Held by the door and by every call alike, so a callback still
 *  running when the feed is let go keeps everything it reads. The feed
 *  itself is held weakly: when it cannot be locked there is nobody left
 *  to deliver to, and the call ends there. */
struct ServerSession {
  std::weak_ptr<Feed> feed;
  std::string method;
  /** Guards the two below. It is taken for the moves that add a call,
   *  find one and take one out, never across a write: a caller waits
   *  for another caller and never for a stream. */
  std::mutex gate;
  /** How many calls this feed has taken, which is what numbers them. */
  uint64_t taken = 0;
  /** EVERY CALL STANDING, under the address its messages are named by,
   *  which is what an answer to one of them is found through. */
  std::unordered_map<std::string, std::shared_ptr<ServerCall>> calls;
};

/** ONE CALL A CALLER HOLDS: the stream it reads and writes, the
 *  messages waiting to go out on it, and the name it is answered by.
 *
 *  IT HOLDS ITSELF UNTIL IT IS DONE. gRPC may call back onto a reactor
 *  until OnDone, so the reference the session's table keeps is not what
 *  its life stands on: the call keeps one of its own and lets that one
 *  go in OnDone, which is the one place a reactor may be freed. */
class ServerCall final : public grpc::ServerGenericBidiReactor {
 public:
  ServerCall(std::shared_ptr<ServerSession> session, std::string named)
      : m_session(std::move(session)), m_named(std::move(named)) {}

  /** Puts this call in its session's table and takes the first message
   *  off the caller. The table comes first, so a message cannot arrive
   *  naming a sender nobody can answer. */
  void begin(const std::shared_ptr<ServerCall>& self) {
    m_self = self;
    {
      const std::lock_guard<std::mutex> lock(m_session->gate);
      m_session->calls[m_named] = self;
    }
    StartRead(&m_incoming);
  }

  /** Puts @p message in this call's queue and writes it when nothing
   *  else is being written; the rest go out in the order they were
   *  given as each write finishes. False once the call is ending, there
   *  being nobody left on it to write to. */
  bool write(const Bytes& message) {
    {
      const std::lock_guard<std::mutex> lock(m_gate);
      if (m_ending) return false;
      m_outgoing.push_back(bufferOf(message));
      if (m_writing) return true;
      m_writing = true;
    }
    // Outside the lock: a write may finish on this very thread, and the
    // callback that finishes it takes the same lock.
    StartWrite(&m_outgoing.front());
    return true;
  }

  void OnReadDone(bool ok) override {
    const InCallback callback;
    // A read that is not ok is the caller saying it has written its
    // last message, which is how a peer leaves.
    if (!ok) return ending();
    const std::shared_ptr<Feed> feed = m_session->feed.lock();
    if (!feed) return ending();
    if (std::optional<Bytes> message = bytesOf(m_incoming))
      feed->deliver(std::move(*message), m_named);
    m_incoming.Clear();
    StartRead(&m_incoming);
  }

  void OnWriteDone(bool ok) override {
    const InCallback callback;
    grpc::ByteBuffer* next = nullptr;
    bool done = false;
    {
      const std::lock_guard<std::mutex> lock(m_gate);
      if (!m_outgoing.empty()) m_outgoing.pop_front();
      if (!ok) {
        // Nothing more goes out on a stream that refused a message, and
        // what was queued behind it goes nowhere either.
        m_outgoing.clear();
        m_ending = true;
      }
      if (!m_outgoing.empty()) {
        next = &m_outgoing.front();
      } else {
        m_writing = false;
        done = m_ending;
      }
    }
    if (next) return StartWrite(next);
    if (done) conclude();
  }

  void OnCancel() override {
    const InCallback callback;
    ending();
  }

  void OnDone() override {
    const InCallback callback;
    forget();
    // The last reference goes here and nowhere earlier: this is the one
    // place a reactor may be freed, and it is freed on the way out of
    // this function with nothing left to touch.
    const std::shared_ptr<ServerCall> last = std::move(m_self);
  }

 private:
  /** NO MORE ARRIVES AND NO MORE IS TAKEN. What is already queued still
   *  goes out — a message handed to a door that was open is not dropped
   *  because the caller stopped talking — and the status follows the
   *  last of it. */
  void ending() {
    bool done = false;
    {
      const std::lock_guard<std::mutex> lock(m_gate);
      if (m_ending) return;
      m_ending = true;
      done = !m_writing;
    }
    if (done) conclude();
  }

  /** Ends the call: out of the table first, so no send reaches a stream
   *  that is finishing, and then the status the caller reads. */
  void conclude() {
    {
      const std::lock_guard<std::mutex> lock(m_gate);
      if (m_concluded) return;
      m_concluded = true;
    }
    forget();
    Finish(grpc::Status::OK);
  }

  /** Takes this call out of its session's table. The reference that
   *  comes out is let go after the lock, since dropping it under one
   *  would be a caller freed while another caller waits for the table.
   */
  void forget() {
    std::shared_ptr<ServerCall> kept;
    const std::lock_guard<std::mutex> lock(m_session->gate);
    const auto found = m_session->calls.find(m_named);
    if (found == m_session->calls.end()) return;
    if (found->second.get() != this) return;
    kept = std::move(found->second);
    m_session->calls.erase(found);
  }

  const std::shared_ptr<ServerSession> m_session;
  /** The address every message of this call's arrives under, written
   *  once so the sender a message names, the key an answer is found
   *  under, and the key that is cleared cannot drift apart. */
  const std::string m_named;
  /** This call's own reference to itself, let go in OnDone. */
  std::shared_ptr<ServerCall> m_self;
  grpc::ByteBuffer m_incoming;

  /** Guards everything below: one write is in flight at a time, and the
   *  queue's front is what that write points at, so it may not move
   *  while anybody is looking at it. */
  std::mutex m_gate;
  std::deque<grpc::ByteBuffer> m_outgoing;
  bool m_writing = false;
  bool m_ending = false;
  bool m_concluded = false;
};

/** A call asking for a method this door does not answer to. A generic
 *  service is handed EVERY method a caller names, there being no
 *  generated stub to have refused one first, so the refusal is written
 *  here. */
class Unanswered final : public grpc::ServerGenericBidiReactor {
 public:
  explicit Unanswered(const std::string& method) {
    Finish(grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                        "this feed answers to " + method + " alone"));
  }

  void OnCancel() override {}
  void OnDone() override { delete this; }
};

/** THE ONE METHOD A SERVER FEED SERVES. Every call that arrives is
 *  looked at by the name it asked for: the one this feed's URI named
 *  becomes a peer, and any other is told nothing here answers to it. */
class ServedMethod final : public grpc::CallbackGenericService {
 public:
  explicit ServedMethod(std::shared_ptr<ServerSession> session)
      : m_session(std::move(session)) {}

  grpc::ServerGenericBidiReactor* CreateReactor(
      grpc::GenericCallbackServerContext* context) override {
    const InCallback callback;
    if (context->method() != m_session->method)
      return new Unanswered(m_session->method);
    uint64_t number = 0;
    {
      const std::lock_guard<std::mutex> lock(m_session->gate);
      number = ++m_session->taken;
    }
    const auto call = std::make_shared<ServerCall>(
        m_session, callerAddress(context->peer(), number));
    call->begin(call);
    return call.get();
  }

 private:
  const std::shared_ptr<ServerSession> m_session;
};

/** THE TRANSPORT'S END OF ONE SERVER FEED: the server holding the port,
 *  the service answering on it, and the calls standing on it now.
 *
 *  One server per feed. Closing a feed then gives back a port of its
 *  own and ends callers of its own, and reaches no other feed; one
 *  server shared by several methods would have to outlive whichever of
 *  its feeds closed first, and could give no port back at all while
 *  another feed still stood on it. */
struct ServerDoor {
  ~ServerDoor() { close(); }

  void close();
  bool send(const Bytes& message);
  bool sendTo(std::string_view to, const Bytes& message);

  /** Every call standing now, for a send that reaches all of them. */
  std::vector<std::shared_ptr<ServerCall>> standing() const;

  std::shared_ptr<ServerSession> session = std::make_shared<ServerSession>();
  std::unique_ptr<ServedMethod> service;
  std::unique_ptr<grpc::Server> server;
  /** Raised before the shutdown begins, so a send racing it stops
   *  rather than handing bytes to a server that is ending. */
  std::atomic<bool> closed{false};
};

void ServerDoor::close() {
  if (closed.exchange(true)) return;
  if (!server) return;
  // The service and the session outlive the server here because the
  // shutdown drives every call to its end, and a call's last callbacks
  // read both.
  auto shut = [held = std::move(server), service = std::move(service),
               session = session]() mutable {
    // A deadline already past cancels every call standing rather than
    // waiting for callers to finish talking: a feed that was let go is
    // not a conversation anybody is still having.
    held->Shutdown(std::chrono::system_clock::now());
    // In that order and no other: the server goes, then the service its
    // calls were made through, then the session both of them read.
    held.reset();
    service.reset();
    session.reset();
  };
  if (insideCallback) {
    // The shutdown waits for this very callback to end, so it cannot be
    // waited for from inside it. The port comes back a moment after the
    // last holder let go rather than within it.
    std::thread(std::move(shut)).detach();
    return;
  }
  shut();
}

std::vector<std::shared_ptr<ServerCall>> ServerDoor::standing() const {
  std::vector<std::shared_ptr<ServerCall>> reached;
  const std::lock_guard<std::mutex> lock(session->gate);
  reached.reserve(session->calls.size());
  for (const auto& [named, call] : session->calls) reached.push_back(call);
  return reached;
}

bool ServerDoor::send(const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  // The calls are taken out from under the lock and written to outside
  // it: a write on one caller's stream must not hold the table every
  // other caller is found through.
  bool went = false;
  for (const std::shared_ptr<ServerCall>& call : standing())
    if (call->write(message)) went = true;
  // A door that holds its callers can say a broadcast reached none of
  // them, which is what "nobody is listening" looks like from here.
  return went;
}

bool ServerDoor::sendTo(std::string_view to, const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  std::shared_ptr<ServerCall> call;
  {
    const std::lock_guard<std::mutex> lock(session->gate);
    const auto found = session->calls.find(std::string(to));
    if (found == session->calls.end()) return false;
    call = found->second;
  }
  return call->write(message);
}

/** THE TRANSPORT'S END OF ONE CLIENT FEED: the channel, the stub over
 *  it and the one call it opened.
 *
 *  It holds itself until it is done, as a server's call does, because
 *  gRPC may call back onto a reactor until OnDone. A HOLD stands on the
 *  stream for as long as this feed does: a send comes from whatever
 *  thread asked for it rather than from inside a callback, and a hold
 *  is what keeps the call from being taken down between two of them. */
class ClientCall final
    : public grpc::ClientBidiReactor<grpc::ByteBuffer, grpc::ByteBuffer> {
 public:
  ClientCall(std::shared_ptr<grpc::Channel> channel, std::string url,
             std::string method, std::weak_ptr<Feed> feed)
      : m_channel(std::move(channel)),
        m_stub(m_channel),
        m_url(std::move(url)),
        m_method(std::move(method)),
        m_feed(std::move(feed)) {}

  /** Opens the call and starts reading it. */
  void begin(const std::shared_ptr<ClientCall>& self) {
    m_self = self;
    m_stub.PrepareBidiStreamingCall(&m_context, m_method, grpc::StubOptions(),
                                    this);
    AddHold();
    StartRead(&m_incoming);
    // ARMED BEFORE THE CALL IS STARTED, so what arms the bound and what
    // the bound looks at cannot cross. It is held weakly: a call that
    // is over before the bound passes is a call nothing has to be told
    // about.
    const std::weak_ptr<ClientCall> watching = self;
    m_bound.Set(std::chrono::system_clock::now() + kReachWithin,
                [watching](bool fired) {
                  if (!fired) return;
                  if (const std::shared_ptr<ClientCall> call = watching.lock())
                    call->atTheBound();
                });
    StartCall();
  }

  /** Puts @p message in the queue and writes it when nothing else is
   *  being written. A message handed over before the server has been
   *  reached waits on the stream rather than going nowhere: gRPC holds
   *  it and writes it as the call opens. */
  bool write(const Bytes& message) {
    {
      const std::lock_guard<std::mutex> lock(m_gate);
      if (m_ending) return false;
      m_outgoing.push_back(bufferOf(message));
      if (m_writing) return true;
      m_writing = true;
    }
    StartWrite(&m_outgoing.front());
    return true;
  }

  /** Gives the call up. The server is told the conversation is over
   *  rather than left to find a stream that stopped answering, and the
   *  hold is taken off so gRPC may finish the call. It waits for
   *  nothing: a cancel is asked for and the answer arrives in OnDone,
   *  on a thread of gRPC's. A call that is already over takes the
   *  cancel as the nothing it is, which is how closing a feed from
   *  inside its own OnDone reaches here and stops. */
  void stop() {
    if (m_stopped.exchange(true)) return;
    {
      const std::lock_guard<std::mutex> lock(m_gate);
      m_ending = true;
    }
    m_context.TryCancel();
    release();
  }

  void OnReadInitialMetadataDone(bool ok) override {
    const InCallback callback;
    if (ok) m_reached.store(true, std::memory_order_release);
  }

  void OnReadDone(bool ok) override {
    const InCallback callback;
    if (!ok) {
      // Nothing more will be read on this stream, which is the moment a
      // hold taken for the writing may come off: with one still on, the
      // call could never reach OnDone and the feed would never be told
      // how it ended.
      {
        const std::lock_guard<std::mutex> lock(m_gate);
        m_ending = true;
      }
      release();
      return;
    }
    m_reached.store(true, std::memory_order_release);
    const std::shared_ptr<Feed> feed = m_feed.lock();
    if (!feed) return;
    if (std::optional<Bytes> message = bytesOf(m_incoming))
      // A client has the one peer it called, and every message it takes
      // is named for it.
      feed->deliver(std::move(*message), m_url);
    m_incoming.Clear();
    StartRead(&m_incoming);
  }

  void OnWriteDone(bool ok) override {
    const InCallback callback;
    // A message that went out went out over a stream that stands, which
    // is one of the ways a call proves it reached its server.
    if (ok) m_reached.store(true, std::memory_order_release);
    grpc::ByteBuffer* next = nullptr;
    {
      const std::lock_guard<std::mutex> lock(m_gate);
      if (!m_outgoing.empty()) m_outgoing.pop_front();
      if (!ok) {
        m_outgoing.clear();
        m_ending = true;
      }
      if (!m_outgoing.empty())
        next = &m_outgoing.front();
      else
        m_writing = false;
    }
    if (next) StartWrite(next);
  }

  void OnDone(const grpc::Status& status) override {
    const InCallback callback;
    const std::shared_ptr<Feed> feed = m_feed.lock();
    if (feed) {
      // A call given up on purpose ended as it was meant to and there is
      // nobody it has to be explained to; any other ending is what the
      // feed is told, in gRPC's own words — and WHICH ending it was is
      // the difference between a server that was never there and one
      // that went away mid-conversation, which a reader of the sentence
      // has to be able to tell apart.
      if (!status.ok()) {
        if (!m_stopped.load(std::memory_order_acquire))
          feed->fail(m_reached.load(std::memory_order_acquire)
                         ? m_url + " ended: " + status.error_message()
                         : "could not reach " + m_url + ": " +
                               status.error_message());
      } else {
        // The server ended the call. Nothing more arrives, so the feed
        // takes nothing more.
        feed->close();
      }
    }
    const std::shared_ptr<ClientCall> last = std::move(m_self);
  }

 private:
  /** The bound has passed. A channel that is READY connected, whether
   *  or not the server has said anything yet — a stream nobody has
   *  written on is a stream, not a failure. Anything else is a server
   *  that was not reached, and a call left standing on it is one nobody
   *  will ever answer. */
  void atTheBound() {
    if (m_reached.load(std::memory_order_acquire)) return;
    if (m_channel->GetState(false) == GRPC_CHANNEL_READY) return;
    if (m_stopped.load(std::memory_order_acquire)) return;
    if (const std::shared_ptr<Feed> feed = m_feed.lock())
      feed->fail("could not reach " + m_url + " within ten seconds");
    m_context.TryCancel();
  }

  /** Takes the hold off the stream, once however many ways this is
   *  reached: one hold was put on, so one comes off. */
  void release() {
    if (m_held.exchange(false)) RemoveHold();
  }

  const std::shared_ptr<grpc::Channel> m_channel;
  grpc::GenericStubCallback m_stub;
  grpc::ClientContext m_context;
  /** The URL that was called: the address the feed reports, and the
   *  sender every arrival names. */
  const std::string m_url;
  const std::string m_method;
  const std::weak_ptr<Feed> m_feed;
  std::shared_ptr<ClientCall> m_self;
  grpc::ByteBuffer m_incoming;
  /** The bound on reaching the server, which fires on a thread of
   *  gRPC's and needs none of this library's. */
  grpc::Alarm m_bound;

  std::mutex m_gate;
  std::deque<grpc::ByteBuffer> m_outgoing;
  bool m_writing = false;
  bool m_ending = false;
  std::atomic<bool> m_held{true};
  std::atomic<bool> m_stopped{false};
  /** Whether this call ever reached its server: the headers came back,
   *  or a message crossed in either direction. It is what a failure is
   *  worded from, and it is raised once and never lowered — a server
   *  that answered and then went away is not a server that was never
   *  there. */
  std::atomic<bool> m_reached{false};
};

/** THE DOOR A CLIENT FEED HOLDS: one call, given up when the feed is. */
struct ClientDoor {
  ~ClientDoor() { close(); }

  void close() {
    if (call) call->stop();
  }

  std::shared_ptr<ClientCall> call;
};

/** A feed whose transport could not open: the reason stands on the
 *  feed, and there is no door to close or to send through. */
OpenedFeed refuse(const std::weak_ptr<Feed>& into, std::string why) {
  if (const std::shared_ptr<Feed> feed = into.lock())
    feed->fail(std::move(why));
  return {};
}

/** Opens one feed's server: the port the URI names, and the one method
 *  it answers to.
 *
 *  The bind is done here rather than left to the background, so a feed
 *  that could not take its port says so by the time it is answered. */
OpenedFeed hold(std::string_view uri, const Address& place,
                const std::weak_ptr<Feed>& into) {
  const auto door = std::make_shared<ServerDoor>();
  door->session->feed = into;
  door->session->method = place.method;
  door->service = std::make_unique<ServedMethod>(door->session);

  grpc::ServerBuilder builder;
  int bound = 0;
  // Every interface of both families is one dual-stack listener, which
  // is an IPv6 address with nothing in it.
  builder.AddListeningPort("[::]:" + place.port,
                           grpc::InsecureServerCredentials(), &bound);
  // ONE SERVER PER PORT. Left to itself gRPC lets several sockets share
  // one port, which would make two feeds on one port both succeed and
  // split the callers between them — and would leave a port that was
  // never given back looking as free as one that was.
  builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);
  builder.RegisterCallbackGenericService(door->service.get());
  door->server = builder.BuildAndStart();
  if (!door->server || bound == 0)
    return refuse(into, "could not listen on " + std::string(uri) +
                            ": the port could not be taken");

  OpenedFeed opened;
  // It is the address a CALLER reaches, so it carries the method: a
  // port with no method named is a port nothing here answers on.
  opened.address =
      "grpc://[::]:" + std::to_string(bound) + door->session->method;
  opened.close = [door] { door->close(); };
  opened.send = [door](const Bytes& message) { return door->send(message); };
  opened.sendTo = [door](std::string_view to, const Bytes& message) {
    return door->sendTo(to, message);
  };
  return opened;
}

/** Opens one feed's call: a channel to the host and port the URI names,
 *  and one bidirectional stream on the method behind them.
 *
 *  The connecting is left to gRPC rather than waited for here, because a
 *  server that answers slowly, or is not there at all, would otherwise
 *  hold whoever asked for the feed for as long as connecting takes.
 *  What it decided reaches the feed either way: as arrivals, or as the
 *  sentence error() answers. */
OpenedFeed reach(std::string_view uri, const Address& place,
                 const std::weak_ptr<Feed>& into) {
  // The authority as the URI wrote it is what gRPC takes as its target,
  // a bracketed IPv6 literal included.
  const std::string target = place.host.find(':') == std::string::npos
                                 ? place.host + ":" + place.port
                                 : "[" + place.host + "]:" + place.port;
  const std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
  if (!channel)
    return refuse(into, "could not reach " + std::string(uri) +
                            ": no channel could be made");

  const auto door = std::make_shared<ClientDoor>();
  door->call = std::make_shared<ClientCall>(channel, std::string(uri),
                                            place.method, into);
  door->call->begin(door->call);

  OpenedFeed opened;
  // The address a client has is the server it called: the port its own
  // socket took is the system's to choose and nothing anybody could
  // reach it at.
  opened.address = std::string(uri);
  opened.close = [door] { door->close(); };
  opened.send = [door](const Bytes& message) {
    return door->call->write(message);
  };
  return opened;
}

/** ONE SCHEME, TWO SHAPES, split by the shape of the URI: a URI naming
 *  a host is a server to call, and a URI naming none is a port to
 *  hold. */
OpenedFeed openFeed(std::string_view uri, const std::weak_ptr<Feed>& into) {
  const std::optional<Address> address = parseAddress(uri);
  if (!address)
    return refuse(into, std::string(uri) +
                            " is not a grpc address: a feed holds a port at "
                            "grpc://:port/Service/Method and calls one at "
                            "grpc://host:port/Service/Method, the method being "
                            "what both ends name");
  if (address->host.empty()) return hold(uri, *address, into);
  return reach(uri, *address, into);
}

}  // namespace

void registerGrpc(Hub& hub) {
  hub.setFeedTransport("grpc",
                       [](std::string_view uri, std::weak_ptr<Feed> into) {
                         return openFeed(uri, into);
                       });
}

}  // namespace sigil::io
