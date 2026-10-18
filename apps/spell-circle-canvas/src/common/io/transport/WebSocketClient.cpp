/** @file
 * The WebSocket client transport: the server a feed's URI names, the
 * handshake run on a thread of its own, the frames that thread puts
 * back together into whole messages, and the message a send writes over
 * the same connection.
 *
 * ws:// and wss:// are one scheme in two spellings, and each is opened
 * two ways. A URI that names a host is a server to CALL and is opened
 * here; a URI that names none is a port to HOLD and is opened by
 * whatever listening transport this one was installed in front of. The
 * shape of the URI is what decides, in one place, so a reader of a feed
 * sees one scheme and not two.
 */

#include <curl/curl.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "Registration.h"
#include "sigilio/advanced/Transport.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/source/Source.h"
#include "sigilio/transport/Transport.h"

namespace sigil::io {
namespace {

/** The largest message this transport puts back together out of a
 *  server's frames. A feed's message is one whole thing rather than a
 *  stream, so a server sending more than this ends the session instead
 *  of the client holding a growing fragment for it. */
constexpr size_t kMessageCeiling = 16u * 1024u * 1024u;

/** How much of a message one read takes off the connection. A message
 *  larger than this arrives in as many reads as it needs. */
constexpr size_t kReadCeiling = 64u * 1024u;

/** How long the handshake is given before the feed is told the server
 *  was not reached. */
constexpr long kConnectMilliseconds = 10000;

/** The longest a reader sleeps on its socket before asking again. A
 *  message wakes it as it arrives and a close wakes it at once; this
 *  bounds only a wake the socket never gives. */
constexpr int kReadWaitMilliseconds = 1000;

/** The longest a send blocked on a full connection sleeps on its socket
 *  before looking again at whether the session closed. */
constexpr int kSendWaitMilliseconds = 100;

/** How long a send keeps trying for room on a connection that is not
 *  draining before it gives the message up. A send answers whether it
 *  went, and a server that stopped reading makes that answer false
 *  rather than holding the caller. */
constexpr std::chrono::seconds kSendCeiling{5};

/** libcurl's one-time start, run before the first handle is made and no
 *  more than once however many feeds are opened. */
bool networkStarted() {
  static const CURLcode started = curl_global_init(CURL_GLOBAL_DEFAULT);
  return started == CURLE_OK;
}

/** The authority @p uri names — what stands between the scheme and the
 *  path — when it names a HOST, and nothing when it names a port to
 *  hold instead. `ws://:8848/scene` holds a port; `ws://desk.local:8848/
 *  scene` names a server to call, and so does a bracketed IPv6 literal,
 *  which is what a host written as an address looks like. */
std::optional<std::string_view> namedServer(std::string_view uri) {
  const size_t scheme = uri.find("://");
  if (scheme == std::string_view::npos) return std::nullopt;
  std::string_view authority = uri.substr(scheme + 3);
  // The path is taken off the back first, so what remains is an
  // authority and a slash inside a path is never read as the one that
  // ends it.
  if (const size_t slash = authority.find('/'); slash != std::string_view::npos)
    authority = authority.substr(0, slash);
  if (authority.empty() || authority.starts_with(':')) return std::nullopt;
  return authority;
}

/** Whether @p authority ends in a port: a colon, then decimal digits and
 *  nothing else. A host written as an IPv6 literal is bracketed, so
 *  every colon inside it stands in front of the bracket and only a
 *  colon after that bracket carries a port. */
bool namesPort(std::string_view authority) {
  const size_t colon = authority.rfind(':');
  if (colon == std::string_view::npos) return false;
  if (const size_t bracket = authority.rfind(']');
      bracket != std::string_view::npos && bracket > colon)
    return false;
  const std::string_view digits = authority.substr(colon + 1);
  unsigned int port = 0;
  const char* const end = digits.data() + digits.size();
  const std::from_chars_result read = std::from_chars(digits.data(), end, port);
  return read.ec == std::errc() && read.ptr == end && port <= 65535;
}

/** THE SESSION ONE FEED HOLDS: the handle every call is made on, the
 *  lock those calls are made under, the feed the messages go to, and the
 *  flags the reading thread and whoever closes it read each other
 *  through.
 *
 *  The reading thread holds this as well as the door does, so a handle a
 *  thread is still calling into is cleaned up after that call rather
 *  than under it. The feed itself is held weakly: when it cannot be
 *  locked there is nobody left to deliver to, and the session ends
 *  there. */
struct Session {
  ~Session() {
    if (waiter) curl_multi_cleanup(waiter);
    if (handle) curl_easy_cleanup(handle);
  }

  /** A libcurl handle takes one caller at a time, so every call on it —
   *  the read, the send and the close frame — is made under this. It is
   *  never held across a wait: a reader with nothing to read must not
   *  stop a writer from sending. */
  std::mutex gate;
  CURL* handle = nullptr;
  /** What the reading thread sleeps in until its socket has something:
   *  libcurl's wait over that one socket, which a close interrupts from
   *  any thread. It holds no transfer of its own. Only the reading
   *  thread waits in it. */
  CURLM* waiter = nullptr;
  /** The connection's socket, read once the handshake is through. */
  curl_socket_t socket = CURL_SOCKET_BAD;
  /** Raised once the handshake is through, before the feed is handed
   *  the end it sends through. Nothing calls the handle until it is: the
   *  thread running the handshake has it to itself. */
  std::atomic<bool> connected{false};
  /** Raised before the close frame goes out, so a read racing it ends
   *  instead of speaking to a session that is being taken down. */
  std::atomic<bool> closed{false};
  Inlet inlet;
  /** The URL that was called: the address the feed reports, and the
   *  sender every arrival names, a client having the one peer it
   *  dialled. */
  std::string url;
};

/** What libcurl asks while it is connecting: whether the connect is
 *  still wanted, answered no once the session is closed. Without it a
 *  close would wait for a handshake nobody wants any more to run out its
 *  patience, and the thread it joins is inside that handshake. */
int stillWanted(void* held, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  const auto* const session = static_cast<const Session*>(held);
  return session->closed.load(std::memory_order_acquire) ? 1 : 0;
}

/** Writes @p message over @p session as one whole binary message,
 *  answering whether it went. */
bool sendOver(Session& session, const Bytes& message);

/** Runs one session: the handshake, then every message the server sends
 *  until the feed is let go, the session is closed, or the server ends
 *  it.
 *
 *  A message is one or more frames and a frame arrives in one or more
 *  reads, so what is read is gathered until a piece arrives with nothing
 *  left of its frame and no continuation behind it. Control frames stand
 *  between the pieces of a message and are not part of it: libcurl
 *  answers a ping itself, and a close says there is nothing more to
 *  read. */
void carry(const std::shared_ptr<Session>& session) {
  char trouble[CURL_ERROR_SIZE] = {};
  curl_easy_setopt(session->handle, CURLOPT_ERRORBUFFER, trouble);
  const CURLcode reached = curl_easy_perform(session->handle);
  // Taken back before anything else: the buffer stands on this stack,
  // and a later call on the handle would write into it after this
  // function has returned.
  curl_easy_setopt(session->handle, CURLOPT_ERRORBUFFER, nullptr);
  if (reached != CURLE_OK) {
    // A session closed before its handshake finished was given up on
    // purpose, and there is nobody it has to be explained to. Any other
    // ending is the reason, and then an end with nothing in it: the feed
    // never opened, and the next ask for its URI calls again.
    if (!session->closed.load(std::memory_order_acquire)) {
      session->inlet.fail("could not reach " + session->url + ": " +
                          (trouble[0] != '\0' ? std::string(trouble)
                                              : curl_easy_strerror(reached)));
      session->inlet.open(TransportEnd{});
    }
    return;
  }
  curl_easy_getinfo(session->handle, CURLINFO_ACTIVESOCKET, &session->socket);
  session->connected.store(true, std::memory_order_release);
  // THE DOOR STANDS NOW, and not before: the server has switched
  // protocols and holds this client as a peer, so the first send after
  // the feed reads Open goes out. The close and the address are the ones
  // the feed was answered with.
  TransportEnd standing;
  standing.send = [session](const Bytes& message) {
    return sendOver(*session, message);
  };
  session->inlet.open(std::move(standing));

  std::vector<std::byte> message;
  std::vector<std::byte> piece(kReadCeiling);
  while (!session->closed.load(std::memory_order_acquire)) {
    size_t taken = 0;
    int flags = 0;
    curl_off_t left = 0;
    CURLcode read = CURLE_OK;
    {
      const std::lock_guard<std::mutex> lock(session->gate);
      const struct curl_ws_frame* frame = nullptr;
      read = curl_ws_recv(session->handle, piece.data(), piece.size(), &taken,
                          &frame);
      // What the frame says is read here and not after the lock: the
      // struct belongs to libcurl and holds only until the next call on
      // the handle, which a send may make as soon as this is released.
      if (read == CURLE_OK && frame != nullptr) {
        flags = frame->flags;
        left = frame->bytesleft;
      }
    }
    if (read == CURLE_AGAIN) {
      // Nothing is there yet: sleep on the socket until it has something
      // or a close interrupts. The wait is outside the lock, so a send is
      // not kept behind a message that has not come.
      curl_waitfd readable{session->socket, CURL_WAIT_POLLIN, 0};
      curl_multi_poll(session->waiter, &readable, 1, kReadWaitMilliseconds,
                      nullptr);
      continue;
    }
    if (read != CURLE_OK) {
      // A session closed here ended as it was meant to; any other
      // ending is what the feed is told, in libcurl's own words.
      if (!session->closed.load(std::memory_order_acquire))
        session->inlet.fail(session->url +
                            " ended: " + curl_easy_strerror(read));
      return;
    }
    if ((flags & CURLWS_CLOSE) != 0) {
      // The server is done. The feed takes nothing more, and closing it
      // is what sends the answering close frame and ends this thread.
      session->inlet.close();
      return;
    }
    if ((flags & (CURLWS_PING | CURLWS_PONG)) != 0) continue;

    if (message.size() + taken > kMessageCeiling) {
      session->inlet.fail(session->url +
                          " sent a message larger than this feed takes");
      session->inlet.close();
      return;
    }
    message.insert(message.end(), piece.begin(),
                   piece.begin() + static_cast<std::ptrdiff_t>(taken));
    // Whole when this piece finishes its frame and that frame is not one
    // of several the server split the message into.
    if (left != 0 || (flags & CURLWS_CONT) != 0) continue;
    Bytes whole;
    whole = Bytes(std::move(message));
    message.clear();
    if (session->inlet.expired()) return;
    session->inlet.deliver(std::move(whole), session->url);
  }
}

/** THE TRANSPORT'S END OF ONE FEED: the session, and the thread reading
 *  it.
 *
 *  One session per feed and one thread per session: a feed is one
 *  conversation with one server, so closing it ends that conversation
 *  and reaches no other feed. */
struct Door {
  ~Door() { close(); }

  void close();

  std::shared_ptr<Session> session = std::make_shared<Session>();
  std::thread thread;
};

void Door::close() {
  if (session->closed.exchange(true)) return;
  // A reader asleep on its socket is woken to find the flag raised.
  if (session->waiter) curl_multi_wakeup(session->waiter);
  // The server is told the conversation is over rather than left to
  // find a socket that stopped answering. There is nothing to tell
  // where the handshake never finished.
  if (session->connected.load(std::memory_order_acquire)) {
    const std::lock_guard<std::mutex> lock(session->gate);
    size_t sent = 0;
    curl_ws_send(session->handle, "", 0, &sent, 0, CURLWS_CLOSE);
  }
  if (!thread.joinable()) return;
  // A message in flight holds the feed while it delivers, so the last
  // holder of a feed can be this very thread. It cannot wait for
  // itself: the flag is raised, and the loop ends on the way out of the
  // delivery this is inside.
  if (thread.get_id() == std::this_thread::get_id()) {
    thread.detach();
    return;
  }
  thread.join();
}

bool sendOver(Session& session, const Bytes& message) {
  if (session.closed.load(std::memory_order_acquire)) return false;
  if (!session.connected.load(std::memory_order_acquire)) return false;
  const std::byte* remaining = message.data();
  size_t left = message.size();
  // One whole message in one frame, which is what a feed's message is,
  // and bytes rather than text, which is what a feed carries. libcurl
  // takes as much of it as the connection has room for and says how
  // much that was; what is left goes out in the calls after this one as
  // the same message continued, never as a fragment of its own.
  const auto deadline = std::chrono::steady_clock::now() + kSendCeiling;
  const auto release = [](CURLM* waiter) { curl_multi_cleanup(waiter); };
  std::unique_ptr<CURLM, decltype(release)> roomWaiter(nullptr, release);
  while (true) {
    size_t taken = 0;
    CURLcode wrote = CURLE_OK;
    {
      const std::lock_guard<std::mutex> lock(session.gate);
      wrote = curl_ws_send(session.handle, remaining, left, &taken, 0,
                           CURLWS_BINARY);
    }
    if (wrote != CURLE_OK && wrote != CURLE_AGAIN) return false;
    const size_t moved = wrote == CURLE_OK ? taken : 0;
    remaining += moved;
    left -= moved;
    if (wrote == CURLE_OK && left == 0) return true;
    if (moved != 0) continue;
    // Nothing moved: the connection has no room. Sleep on the socket
    // until it can take more, outside the lock, so a message arriving is
    // not kept behind one going out. A blocked send makes a socket wait
    // of its own, since the reader's admits one sleeper.
    if (std::chrono::steady_clock::now() >= deadline) return false;
    if (session.closed.load(std::memory_order_acquire)) return false;
    if (!roomWaiter) roomWaiter.reset(curl_multi_init());
    if (!roomWaiter) return false;
    curl_waitfd writable{session.socket, CURL_WAIT_POLLOUT, 0};
    curl_multi_poll(roomWaiter.get(), &writable, 1, kSendWaitMilliseconds,
                    nullptr);
  }
}

/** A feed whose transport could not open: the reason stands on the feed,
 *  and there is no door to close or to send through. */
TransportEnd refuse(const Inlet& into, std::string why) {
  into.fail(std::move(why));
  return {};
}

/** Opens one feed's session: the handle set up for the URL it names, and
 *  the thread that runs the handshake and then reads.
 *
 *  The handshake is left to that thread rather than waited for here,
 *  because a server that answers slowly, or is not there at all, would
 *  otherwise hold whoever asked for the feed for as long as connecting
 *  takes. So the answer is an end that does not stand yet — the close
 *  that gives the handshake up, and the server being called — and the
 *  feed reads Connecting until that thread hands over the end that
 *  sends, or the sentence `state().error` answers. */
TransportEnd openFeed(std::string_view uri, const Inlet& into) {
  const std::optional<std::string_view> server = namedServer(uri);
  if (!server || !namesPort(*server))
    return refuse(into, std::string(uri) +
                            " is not a websocket server to call: a feed "
                            "reaches one at ws://host:port/path, and at "
                            "wss://host:port/path for a session over TLS");
  if (!networkStarted())
    return refuse(into, "could not reach " + std::string(uri) +
                            ": the network library could not be started");
  CURL* const handle = curl_easy_init();
  if (handle == nullptr)
    return refuse(into, "could not reach " + std::string(uri) +
                            ": no connection could be made");

  CURLM* const waiter = curl_multi_init();
  if (waiter == nullptr) {
    curl_easy_cleanup(handle);
    return refuse(into, "could not reach " + std::string(uri) +
                            ": no connection could be made");
  }

  const auto door = std::make_shared<Door>();
  door->session->handle = handle;
  door->session->waiter = waiter;
  door->session->inlet = into;
  door->session->url = std::string(uri);
  curl_easy_setopt(handle, CURLOPT_URL, door->session->url.c_str());
  // The handshake and nothing after it: libcurl hands the connection
  // back once the server has switched protocols, and every message over
  // it is read and written through the calls this file makes.
  curl_easy_setopt(handle, CURLOPT_CONNECT_ONLY, 2L);
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, kConnectMilliseconds);
  // A session runs on a thread of its own, and a signal belongs to the
  // whole process rather than to one thread, so the library times its
  // own work out without raising any.
  curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(handle, CURLOPT_USERAGENT, "SigilIO/1.0");
  // Asked while the handshake runs, so closing a feed gives the connect
  // up instead of waiting for it. The session outlives the handle it
  // holds, so what this points at stands for as long as libcurl can ask.
  curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, &stillWanted);
  curl_easy_setopt(handle, CURLOPT_XFERINFODATA, door->session.get());
  door->thread = std::thread([session = door->session] { carry(session); });

  TransportEnd opened;
  // The address a client has is the server it called: the port its own
  // socket took is the system's to choose and nothing anybody could
  // reach it at.
  opened.localAddress = door->session->url;
  opened.close = [door] { door->close(); };
  opened.standing = false;
  return opened;
}

/** ONE SCHEME OUT OF TWO OPENERS, split by the shape of the URI: a URI
 *  that names a server is called through @p calling, and a URI that
 *  names a port to hold is opened through @p listening, which is
 *  whatever was registered for the scheme before this stood in front of
 *  it. A scheme with nothing behind it says so, rather than a URI meant
 *  for a door that is not there opening a call to nowhere. */
Transport openWebSocket(Transport listening, Transport calling) {
  return [listening = std::move(listening), calling = std::move(calling)](
             std::string_view uri, Inlet into) -> TransportEnd {
    if (namedServer(uri)) return calling(uri, std::move(into));
    if (listening) return listening(uri, std::move(into));
    return refuse(into, std::string(uri) +
                            " names a port to listen on, and no websocket "
                            "listener is registered for its scheme: a feed "
                            "calls a server at ws://host:port/path");
  };
}

}  // namespace

void detail::registerWebSocketClient(Hub& hub) {
  const Transport calling = [](std::string_view uri, Inlet into) {
    return openFeed(uri, into);
  };
  // Each spelling stands in front of whatever was registered for it, and
  // is read before it is replaced: a URI naming a server is called here,
  // and a URI naming a port goes to the listener that was already there.
  // Nothing listens for wss:// unless something was registered for it,
  // the sockets a listener is built on carrying no TLS.
  Transport listening = io::transport(hub, "ws");
  io::registerTransport(hub, "ws",
                        openWebSocket(std::move(listening), calling));
  Transport listeningSecurely = io::transport(hub, "wss");
  io::registerTransport(hub, "wss",
                        openWebSocket(std::move(listeningSecurely), calling));
}

}  // namespace sigil::io
