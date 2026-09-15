/** @file
 * The QUIC transport: the port a feed's URI holds or the host it calls,
 * the connections that cross it, the stream each message is, and the
 * datagrams a door sends instead when its URI asks for them.
 *
 * ONE CONNECTION CARRIES EVERY MESSAGE, AND A MESSAGE IS ONE STREAM. A
 * send opens a unidirectional stream, writes the bytes and ends it; the
 * arrival is delivered when the end of that stream arrives. So a message
 * keeps its boundary, which a byte stream has none of, and no message
 * waits behind another: a stream that lost a packet holds up itself and
 * nothing else. The price is that two messages sent one after the other
 * may land in the other order, each stream being carried on its own.
 *
 * A DATAGRAM IS THE OTHER WAY ACROSS THE SAME CONNECTION. `datagrams=1`
 * in a URI's query sends every message as a QUIC datagram instead:
 * unreliable, unordered, and no larger than what one packet on the path
 * carries, so a send of more than that answers false. Datagrams are
 * always TAKEN, whatever a door sends, so a peer that writes one reaches
 * a door whose own URI asked for nothing.
 *
 * TLS IS NOT OPTIONAL. A QUIC connection is encrypted or it is not a
 * connection, so a feed that holds a port names the certificate and the
 * private key it answers with, and a feed that calls one either trusts
 * what it is shown or says plainly that it will not check. Both ends
 * name the same protocol, and an end speaking another is refused at the
 * handshake rather than left to send bytes nobody reads.
 *
 * NO THREAD IS STARTED FOR A FEED. The library underneath runs workers
 * of its own and calls back onto them, so the packets, the encryption,
 * the streams and the timers are carried without this library holding a
 * loop, an executor or a socket. The one thread this file ever makes is
 * the one a door let go from inside such a callback hands its shutdown
 * to, below.
 */

#include <arpa/inet.h>
#include <msquic.h>
#include <netinet/in.h>

#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
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

/** THE PROTOCOL BOTH ENDS NAME. A QUIC handshake carries the names of
 *  the protocols each end speaks and fails when they share none, so this
 *  is what keeps a feed's door from being taken up by software that
 *  speaks something else over the same port. */
constexpr std::string_view kProtocol = "sigil-feed/1";

/** The largest message a peer may send. A feed's message is one whole
 *  thing rather than a stream, so a stream that grows past this is
 *  abandoned instead of the door holding a growing fragment for it. */
constexpr size_t kMessageCeiling = 16u * 1024u * 1024u;

/** How long a call is given to reach the end it named before the feed is
 *  told it was not reached. It is the handshake's own bound, and what it
 *  answers for is a host that TAKES THE CONNECTION NOWHERE: a machine
 *  that answers at once that nothing is listening on that port ends the
 *  call there and never waits this out, while without a bound the silent
 *  case would leave a feed waiting for as long as anybody held it. */
constexpr uint32_t kReachWithinMs = 10000;

/** How long a connection with nothing crossing it stands, and how often
 *  nothing is said on it. A door is open for as long as a scene holds
 *  it, so the quiet is kept alive on purpose; an end that goes away
 *  rather than falling quiet is noticed within the first of these. */
constexpr uint32_t kIdleMs = 30000;
constexpr uint32_t kKeepAliveMs = 10000;

/** How many of a peer's messages may be in flight at once. Every message
 *  is a stream, so a peer may open this many before it waits for the
 *  ones it opened to finish. */
constexpr uint16_t kMessagesInFlight = 1024;

/** What a door says as it ends a connection or a stream: nothing in
 *  particular. The number crosses the wire, so the two ends would have
 *  to agree on what it means, and there is nothing here for them to
 *  agree about. */
constexpr QUIC_UINT62 kNothingToSay = 0;

/** THE LIBRARY UNDERNEATH: its function table, and the registration its
 *  workers run under. Both are made on the first quic:// feed and stand
 *  for the life of the process.
 *
 *  GIVING EITHER BACK WAITS FOR EVERY WORKER TO END, and a worker cannot
 *  wait for itself — while the last holder of a feed may well be a
 *  delivery running on one of those workers. So what a door owns and
 *  gives back is a listener, a connection and a stream, each of which
 *  can be given back from where it is reached, and the library beneath
 *  them stays. */
struct Library {
  const QUIC_API_TABLE* api = nullptr;
  HQUIC registration = nullptr;
};

const Library& library() {
  static const Library one = [] {
    Library made;
    if (QUIC_FAILED(MsQuicOpen2(&made.api))) return Library{};
    const QUIC_REGISTRATION_CONFIG config{"sigil-feed",
                                          QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    if (QUIC_FAILED(made.api->RegistrationOpen(&config, &made.registration))) {
      MsQuicClose(made.api);
      return Library{};
    }
    return made;
  }();
  return one;
}

/** The protocol as the handshake carries it. A function-local string
 *  rather than a literal because the call takes a writable buffer, and
 *  nothing here writes through it. */
QUIC_BUFFER protocol() {
  static std::string held{kProtocol};
  QUIC_BUFFER named;
  named.Length = static_cast<uint32_t>(held.size());
  named.Buffer = reinterpret_cast<uint8_t*>(held.data());
  return named;
}

/** What went wrong, in words rather than in a number. The ones named
 *  here are the ones a reader of a feed's error can act on; anything
 *  else is handed over as the library's own number, which says more than
 *  a sentence that fits everything. */
std::string reasonOf(QUIC_STATUS status) {
  if (status == QUIC_STATUS_CONNECTION_REFUSED) return "the port refused it";
  if (status == QUIC_STATUS_CONNECTION_TIMEOUT) return "nothing answered";
  if (status == QUIC_STATUS_CONNECTION_IDLE)
    return "nothing crossed it for long enough that it was let go";
  if (status == QUIC_STATUS_UNREACHABLE)
    return "nothing is listening there, or the host cannot be reached";
  if (status == QUIC_STATUS_ALPN_NEG_FAILURE)
    return "the other end does not speak " + std::string(kProtocol);
  if (status == QUIC_STATUS_ADDRESS_IN_USE) return "the port is already held";
  if (status == QUIC_STATUS_ADDRESS_NOT_AVAILABLE)
    return "no interface of this machine has that address";
  if (status == QUIC_STATUS_INVALID_ADDRESS)
    return "that is not an address this machine can bind";
  if (status == QUIC_STATUS_ABORTED) return "it was cut short";
  if (status == QUIC_STATUS_USER_CANCELED) return "the other end gave it up";
  if (status == QUIC_STATUS_HANDSHAKE_FAILURE)
    return "the two ends could not finish a handshake";
  if (status == QUIC_STATUS_TLS_ERROR)
    return "the certificate or the key was refused";
  if (status == QUIC_STATUS_BAD_CERTIFICATE)
    return "the certificate was not accepted";
  if (status == QUIC_STATUS_EXPIRED_CERTIFICATE)
    return "the certificate has expired";
  if (status == QUIC_STATUS_UNKNOWN_CERTIFICATE ||
      status == QUIC_STATUS_CERT_UNTRUSTED_ROOT)
    return "nobody this machine trusts signed that certificate, which a "
           "self-signed one is reached past with insecure=1";
  if (status == QUIC_STATUS_INVALID_PARAMETER)
    return "the library refused what it was given";
  if (status == QUIC_STATUS_NOT_SUPPORTED)
    return "the quic library on this machine does not carry that";
  return "quic status " + std::to_string(static_cast<unsigned>(status));
}

/** One end's address, spelled the way a URI of this scheme is,
 *  "quic://127.0.0.1:52341". An IPv6 address is bracketed, so what
 *  follows the last colon is always the port; a peer that reached a
 *  dual-stack listener over IPv4 arrives as its address mapped into
 *  IPv6, and is named by the IPv4 address it can be written back to
 *  rather than by the mapping. It is the same spelling the datagram
 *  transport beside this one gives an endpoint, so one reader reads the
 *  sender of an arrival off either. */
std::string peerAddress(const QUIC_ADDR& address) {
  char printed[INET6_ADDRSTRLEN] = {};
  const std::string port = std::to_string(QuicAddrGetPort(&address));
  if (QuicAddrGetFamily(&address) == QUIC_ADDRESS_FAMILY_INET) {
    if (!inet_ntop(AF_INET, &address.Ipv4.sin_addr, printed, sizeof(printed)))
      return {};
    return "quic://" + std::string(printed) + ":" + port;
  }
  const in6_addr& six = address.Ipv6.sin6_addr;
  if (IN6_IS_ADDR_V4MAPPED(&six)) {
    in_addr four{};
    std::memcpy(&four, six.s6_addr + 12, sizeof(four));
    if (!inet_ntop(AF_INET, &four, printed, sizeof(printed))) return {};
    return "quic://" + std::string(printed) + ":" + port;
  }
  if (!inet_ntop(AF_INET6, &six, printed, sizeof(printed))) return {};
  return "quic://[" + std::string(printed) + "]:" + port;
}

/** Whether this thread is inside one of the library's own callbacks.
 *
 *  A listener is given back by waiting for it to stop indicating, and a
 *  callback cannot wait for itself. The last holder of a feed can be the
 *  very delivery arriving on it, so letting that delivery go is one of
 *  the ways a door is closed — and a door closed that way hands its
 *  shutdown to a thread nothing is waiting for. */
thread_local bool insideCallback = false;

/** Raises that flag for as long as one callback runs. */
struct InCallback {
  InCallback() : before(insideCallback) { insideCallback = true; }
  ~InCallback() { insideCallback = before; }

  InCallback(const InCallback&) = delete;
  InCallback& operator=(const InCallback&) = delete;

  bool before;
};

/** WHAT A quic:// URI NAMES: the host that makes it an end to call
 *  rather than a port to hold, the port, the certificate and key a port
 *  answers with, whether a call checks what it is shown, and whether a
 *  send goes as a datagram rather than a stream. */
struct Address {
  std::string host;
  std::string port;
  std::string certificate;
  std::string key;
  bool insecure = false;
  bool datagrams = false;
};

/** The value @p name carries in @p query, or nothing where the query
 *  carries none. A query is ampersand-separated pairs, so a door that
 *  grows a second setting spells it beside these and neither has to know
 *  about the other; a value may hold anything but an ampersand, which is
 *  what lets one of them be a URI. */
std::string_view valueNamed(std::string_view query, std::string_view name) {
  while (!query.empty()) {
    const size_t next = query.find('&');
    const std::string_view pair = query.substr(0, next);
    if (pair.size() > name.size() && pair.starts_with(name) &&
        pair[name.size()] == '=')
      return pair.substr(name.size() + 1);
    if (next == std::string_view::npos) break;
    query = query.substr(next + 1);
  }
  return {};
}

/** The address @p uri names, or nothing when it names none: a host and a
 *  port, with the query behind them. The host is bracketed when it is an
 *  IPv6 literal and left out altogether to hold a port on every
 *  interface; the port is decimal digits and nothing else. */
std::optional<Address> parseAddress(std::string_view uri) {
  constexpr std::string_view kScheme = "quic://";
  if (!uri.starts_with(kScheme)) return std::nullopt;
  std::string_view rest = uri.substr(kScheme.size());
  Address address;
  // THE QUERY IS THE DOOR'S OWN ARRANGEMENT and no part of the address
  // anybody reaches, so it comes off first: what the socket stands on,
  // what the feed reports and what an arrival names are the authority
  // alone.
  if (const size_t question = rest.find('?');
      question != std::string_view::npos) {
    const std::string_view query = rest.substr(question + 1);
    address.certificate = std::string(valueNamed(query, "cert"));
    address.key = std::string(valueNamed(query, "key"));
    address.insecure = valueNamed(query, "insecure") == "1";
    address.datagrams = valueNamed(query, "datagrams") == "1";
    rest = rest.substr(0, question);
  }

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

/** The port @p place named, as a number. It parsed as one already, which
 *  is what made the address an address. */
uint16_t portOf(const Address& place) {
  unsigned int port = 0;
  std::from_chars(place.port.data(), place.port.data() + place.port.size(),
                  port);
  return static_cast<uint16_t>(port);
}

/** The authority as a caller writes it: the host, bracketed where it is
 *  an IPv6 literal, and the port behind it. */
std::string authorityOf(const Address& place) {
  return place.host.find(':') == std::string::npos
             ? place.host + ":" + place.port
             : "[" + place.host + "]:" + place.port;
}

/** The file @p named stands at: the hub's mount table first, and the
 *  name as a plain path where no mount matches it. Empty where nothing
 *  of that name is a file, which is what a door that cannot answer for
 *  itself says. */
std::filesystem::path fileAt(const Hub& hub, const std::string& named) {
  if (named.empty()) return {};
  std::filesystem::path path = hub.resolve(named);
  if (path.empty()) path = named;
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec) return {};
  return path;
}

/** HOW EVERY CONNECTION OF EVERY FEED IS ARRANGED. The quiet is kept
 *  alive rather than let go, a peer may hold as many messages in flight
 *  as there are streams for, and a datagram is always taken — a sender
 *  that writes one reaches a door whose own URI asked for nothing. */
QUIC_SETTINGS feedSettings() {
  QUIC_SETTINGS settings{};
  settings.IdleTimeoutMs = kIdleMs;
  settings.IsSet.IdleTimeoutMs = 1;
  settings.HandshakeIdleTimeoutMs = kReachWithinMs;
  settings.IsSet.HandshakeIdleTimeoutMs = 1;
  settings.KeepAliveIntervalMs = kKeepAliveMs;
  settings.IsSet.KeepAliveIntervalMs = 1;
  settings.PeerUnidiStreamCount = kMessagesInFlight;
  settings.IsSet.PeerUnidiStreamCount = 1;
  settings.DatagramReceiveEnabled = 1;
  settings.IsSet.DatagramReceiveEnabled = 1;
  return settings;
}

/** The arrangement one door's connections stand on, with @p credential
 *  loaded into it; null where it could not be made, and @p why is then
 *  the library's word for that. */
HQUIC configurationFor(const QUIC_CREDENTIAL_CONFIG& credential,
                       QUIC_STATUS& why) {
  const Library& lib = library();
  QUIC_SETTINGS settings = feedSettings();
  const QUIC_BUFFER named = protocol();
  HQUIC configuration = nullptr;
  why = lib.api->ConfigurationOpen(lib.registration, &named, 1, &settings,
                                   sizeof(settings), nullptr, &configuration);
  if (QUIC_FAILED(why)) return nullptr;
  why = lib.api->ConfigurationLoadCredential(configuration, &credential);
  if (QUIC_FAILED(why)) {
    lib.api->ConfigurationClose(configuration);
    return nullptr;
  }
  return configuration;
}

class Peer;

/** WHAT A FEED'S CALLBACKS SHARE: the feed everything is delivered into,
 *  the arrangement its connections stand on, what a send goes out as,
 *  and the connections standing now.
 *
 *  Held by the door and by every connection alike, so a callback still
 *  running when the feed is let go keeps everything it reads. The feed
 *  itself is held weakly: where it cannot be locked there is nobody left
 *  to deliver to. */
struct Session : std::enable_shared_from_this<Session> {
  ~Session() { giveBack(); }

  /** Gives the arrangement back, once. The door does it as it closes,
   *  which is never from inside one of the library's callbacks, and the
   *  destructor is only what catches a session no door closed. A
   *  connection still ending does not lose what it stands on: the
   *  library holds a reference of its own for every connection that took
   *  this arrangement. */
  void giveBack() {
    if (!configuration) return;
    library().api->ConfigurationClose(configuration);
    configuration = nullptr;
  }

  std::weak_ptr<Feed> feed;
  HQUIC configuration = nullptr;
  bool datagrams = false;
  /** What a CALL reports and what every arrival on one names; empty on a
   *  door that holds a port, whose arrivals are named for the connection
   *  they came in on. */
  std::string called;

  /** Guards the two below. It is taken for the moves that add a
   *  connection, find one and take one out, never across a write: a peer
   *  waits for another peer and never for a stream. */
  std::mutex gate;
  /** How many connections this feed has taken, which is what numbers
   *  them. */
  uint64_t taken = 0;
  /** EVERY CONNECTION STANDING, under the address its messages are named
   *  by, which is what an answer to one of them is found through. */
  std::unordered_map<std::string, std::shared_ptr<Peer>> peers;
};

/** Delivers one message into @p session's feed, naming @p from. */
void deliver(const Session& session, const std::string& from, Bytes message) {
  if (const std::shared_ptr<Feed> feed = session.feed.lock())
    feed->deliver(std::move(message), from);
}

/** ONE MESSAGE ON ITS WAY OUT: the bytes, and the buffer that points at
 *  them. It outlives the call that handed it over — the library writes
 *  FROM these bytes rather than from a copy of its own — and is let go
 *  when the stream it went on has ended, or when the datagram it was has
 *  reached a state it will not leave. */
struct Sending {
  explicit Sending(const Bytes& message) : bytes(message.bytes) {
    buffer.Length = static_cast<uint32_t>(bytes.size());
    buffer.Buffer = reinterpret_cast<uint8_t*>(bytes.data());
  }

  std::vector<std::byte> bytes;
  QUIC_BUFFER buffer{};
};

/** ONE MESSAGE ON ITS WAY IN: the session it will be delivered into, the
 *  name it will arrive under, and the bytes gathered so far. A peer's
 *  stream is one message, so what arrives in however many pieces is one
 *  arrival when the end of the stream comes — and a stream that grows
 *  past what a message may be is abandoned, its bytes dropped and
 *  nothing delivered. */
struct Incoming {
  std::shared_ptr<Session> session;
  std::string named;
  Bytes message;
  bool abandoned = false;
  bool delivered = false;
};

QUIC_STATUS QUIC_API onConnection(HQUIC connection, void* context,
                                  QUIC_CONNECTION_EVENT* event);
QUIC_STATUS QUIC_API onSentStream(HQUIC stream, void* context,
                                  QUIC_STREAM_EVENT* event);
QUIC_STATUS QUIC_API onReceivedStream(HQUIC stream, void* context,
                                      QUIC_STREAM_EVENT* event);

/** ONE CONNECTION: the handle, the name its messages arrive under, and
 *  how much of a datagram the path will take.
 *
 *  IT HOLDS ITSELF UNTIL THE CONNECTION IS DONE. The library may call
 *  back onto it until the shutdown is complete, so the reference the
 *  session's table keeps is not what its life stands on: the connection
 *  keeps one of its own and lets that one go where the handle is given
 *  back, which is the one place it may be freed. */
class Peer {
 public:
  Peer(std::shared_ptr<Session> session, std::string named, bool dialled)
      : m_session(std::move(session)),
        m_named(std::move(named)),
        m_dialled(dialled) {}

  /** Takes @p connection over: the handle first, then the table, so a
   *  message cannot arrive naming a sender nobody can answer, and then
   *  the reference this connection's life stands on. A call holds the
   *  one connection it made and stands in no table. */
  void adopt(HQUIC connection, const std::shared_ptr<Peer>& self) {
    m_self = self;
    {
      const std::lock_guard<std::mutex> lock(m_gate);
      m_connection = connection;
    }
    if (m_dialled) return;
    const std::lock_guard<std::mutex> lock(m_session->gate);
    m_session->peers[m_named] = self;
  }

  /** Gives the handle up WITHOUT closing it, for a connection the
   *  library takes back because this end refused it: nothing more is
   *  indicated on it, so there is nothing left to hold. */
  void abandon() {
    m_stopped.store(true, std::memory_order_release);
    {
      const std::lock_guard<std::mutex> lock(m_gate);
      m_connection = nullptr;
      m_ending = true;
    }
    forget();
    const std::shared_ptr<Peer> last = std::move(m_self);
  }

  /** Closes a connection that was opened and never started, which
   *  indicates nothing at all and so would otherwise never come back. */
  void discard() {
    m_stopped.store(true, std::memory_order_release);
    HQUIC connection = nullptr;
    {
      const std::lock_guard<std::mutex> lock(m_gate);
      connection = m_connection;
      m_connection = nullptr;
      m_ending = true;
    }
    if (connection) library().api->ConnectionClose(connection);
    forget();
    const std::shared_ptr<Peer> last = std::move(m_self);
  }

  /** ONE MESSAGE OUT: a stream of its own, or a datagram where the door
   *  was opened for them. False where the connection is ending, where
   *  the library refused the send, and — for a datagram — where the
   *  message is larger than one packet on this path carries, or where
   *  the path has not yet said how large that is. */
  bool write(const Bytes& message) {
    const Library& lib = library();
    const std::lock_guard<std::mutex> lock(m_gate);
    if (m_ending || !m_connection) return false;
    return m_session->datagrams ? sendDatagram(lib, message)
                                : sendStream(lib, message);
  }

  /** Ends the connection. The other end is told it is over rather than
   *  left to find a connection that stopped answering, and what the
   *  library indicates afterwards is what gives the handle back. It
   *  waits for nothing. */
  void shutdown() {
    if (m_stopped.exchange(true)) return;
    const std::lock_guard<std::mutex> lock(m_gate);
    if (m_connection)
      library().api->ConnectionShutdown(
          m_connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, kNothingToSay);
  }

  /** The feed gave this connection up on purpose, so however it ends
   *  there is nobody it has to be explained to. */
  void givenUp() { m_given.store(true, std::memory_order_release); }

  /** Takes over a stream the other end opened: every message of theirs
   *  arrives on one of these. */
  void takeStream(HQUIC stream) {
    auto* const incoming = new Incoming{m_session, m_named, {}, false, false};
    library().api->SetCallbackHandler(
        stream, reinterpret_cast<void*>(&onReceivedStream), incoming);
  }

  /** ONE DATAGRAM IN, which is one arrival like any other. */
  void takeDatagram(const QUIC_BUFFER& buffer) {
    Bytes message;
    const auto* const first = reinterpret_cast<const std::byte*>(buffer.Buffer);
    message.bytes.assign(first, first + buffer.Length);
    deliver(*m_session, m_named, std::move(message));
  }

  /** How much of a datagram this path carries now; zero while it carries
   *  none. */
  void datagramCeiling(uint16_t most) {
    m_datagramCeiling.store(most, std::memory_order_release);
  }

  /** The connection was reached: the handshake finished, or something
   *  crossed it. Raised once and never lowered — an end that answered
   *  and then went away is not an end that was never there. */
  void reached() { m_reached.store(true, std::memory_order_release); }

  /** What the library said as the connection began to end, kept for the
   *  sentence the feed is left with. */
  void endingWith(QUIC_STATUS status) { m_why = status; }

  /** The other end said it was over, on purpose, rather than the
   *  connection failing under it. */
  void endedByPeer() { m_byPeer = true; }

  /** THE CONNECTION IS DONE: the table loses this peer, a feed whose
   *  life follows one connection is told how it ended, and the handle
   *  comes back. */
  void concluded() {
    // No shutdown touches the handle from here on, which is what lets
    // everything below run without one part waiting for another.
    m_stopped.store(true, std::memory_order_release);
    forget();
    if (m_dialled) tell();
    {
      const std::lock_guard<std::mutex> lock(m_gate);
      m_ending = true;
      if (m_connection) library().api->ConnectionClose(m_connection);
      m_connection = nullptr;
    }
    // The last reference goes here and nowhere earlier: this is the one
    // place a peer may be freed, and it is freed on the way out of this
    // function with nothing left to touch.
    const std::shared_ptr<Peer> last = std::move(m_self);
  }

 private:
  /** A message as a stream of its own: opened, written whole, and ended
   *  by the write, so the other end reads the whole stream as the
   *  message it is. */
  bool sendStream(const Library& lib, const Bytes& message) {
    auto* const sending = new Sending(message);
    HQUIC stream = nullptr;
    if (QUIC_FAILED(lib.api->StreamOpen(m_connection,
                                        QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                                        &onSentStream, sending, &stream))) {
      delete sending;
      return false;
    }
    // The stream is started BY the send rather than before it, so one
    // message crosses as one act and there is no half-open stream to
    // take down when a second call is the one that fails.
    const QUIC_SEND_FLAGS flags = QUIC_SEND_FLAG_START | QUIC_SEND_FLAG_FIN;
    if (QUIC_FAILED(
            lib.api->StreamSend(stream, &sending->buffer, 1, flags, sending))) {
      // A stream that never started indicates nothing, so the handle is
      // given back here rather than waited for.
      lib.api->StreamClose(stream);
      delete sending;
      return false;
    }
    return true;
  }

  /** A message as one datagram: unreliable, unordered, and no larger
   *  than one packet on this path. */
  bool sendDatagram(const Library& lib, const Bytes& message) {
    const uint16_t most = m_datagramCeiling.load(std::memory_order_acquire);
    if (most == 0 || message.bytes.size() > most) return false;
    auto* const sending = new Sending(message);
    if (QUIC_FAILED(lib.api->DatagramSend(m_connection, &sending->buffer, 1,
                                          QUIC_SEND_FLAG_NONE, sending))) {
      delete sending;
      return false;
    }
    return true;
  }

  /** Takes this connection out of its session's table. The reference
   *  that comes out is let go after the lock, since dropping it under
   *  one would be a peer freed while another peer waits for the table.
   */
  void forget() {
    std::shared_ptr<Peer> kept;
    const std::lock_guard<std::mutex> lock(m_session->gate);
    const auto found = m_session->peers.find(m_named);
    if (found == m_session->peers.end()) return;
    if (found->second.get() != this) return;
    kept = std::move(found->second);
    m_session->peers.erase(found);
  }

  /** WHAT A CALL'S FEED IS LEFT WITH. A connection the other end ended
   *  on purpose closes the feed, there being nothing more to come and
   *  nothing to explain. Any other ending is a sentence, and WHICH
   *  ending it was is the difference between an end that was never there
   *  and one that went away mid-conversation, which a reader of the
   *  sentence has to be able to tell apart. */
  void tell() {
    if (m_given.load(std::memory_order_acquire)) return;
    const std::shared_ptr<Feed> feed = m_session->feed.lock();
    if (!feed) return;
    const bool reached = m_reached.load(std::memory_order_acquire);
    if (m_byPeer || (reached && m_why == QUIC_STATUS_SUCCESS)) {
      feed->close();
      return;
    }
    if (reached) {
      feed->fail(m_named + " ended: " + reasonOf(m_why));
      return;
    }
    if (m_why == QUIC_STATUS_CONNECTION_TIMEOUT) {
      feed->fail("could not reach " + m_named + " within ten seconds");
      return;
    }
    feed->fail("could not reach " + m_named + ": " + reasonOf(m_why));
  }

  const std::shared_ptr<Session> m_session;
  /** The address every message of this connection's arrives under,
   *  written once so the sender a message names, the key an answer is
   *  found under, and the key that is cleared cannot drift apart. */
  const std::string m_named;
  /** Whether this end opened the connection, which is also whether the
   *  feed's life follows it: a call holds the one connection it made,
   *  and a port holds as many as reached it. */
  const bool m_dialled;
  /** This connection's own reference to itself, let go where the handle
   *  is given back. */
  std::shared_ptr<Peer> m_self;

  /** Guards the handle and the flag beside it: a send may not be inside
   *  the library while the handle it is using is being given back. */
  std::mutex m_gate;
  HQUIC m_connection = nullptr;
  bool m_ending = false;

  std::atomic<bool> m_stopped{false};
  std::atomic<bool> m_reached{false};
  std::atomic<bool> m_given{false};
  std::atomic<uint16_t> m_datagramCeiling{0};
  /** How the connection began to end. Written as the library says so and
   *  read where the connection is concluded, which the library indicates
   *  after it, on the same worker. */
  QUIC_STATUS m_why = QUIC_STATUS_SUCCESS;
  bool m_byPeer = false;
};

QUIC_STATUS QUIC_API onSentStream(HQUIC stream, void* context,
                                  QUIC_STREAM_EVENT* event) {
  const InCallback callback;
  auto* const sending = static_cast<Sending*>(context);
  if (event->Type == QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE) {
    // The one place a stream this end opened may be given back, and the
    // one place the bytes it wrote from may be freed.
    library().api->StreamClose(stream);
    delete sending;
  }
  return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API onReceivedStream(HQUIC stream, void* context,
                                      QUIC_STREAM_EVENT* event) {
  const InCallback callback;
  auto* const incoming = static_cast<Incoming*>(context);
  switch (event->Type) {
    case QUIC_STREAM_EVENT_RECEIVE: {
      if (incoming->abandoned) break;
      std::vector<std::byte>& gathered = incoming->message.bytes;
      if (gathered.size() + event->RECEIVE.TotalBufferLength >
          kMessageCeiling) {
        // A message is one whole thing, so a stream that will not be one
        // is dropped rather than held: the bytes go, and the other end is
        // told to stop writing this one.
        incoming->abandoned = true;
        gathered.clear();
        gathered.shrink_to_fit();
        library().api->StreamShutdown(
            stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT_RECEIVE, kNothingToSay);
        break;
      }
      for (uint32_t at = 0; at < event->RECEIVE.BufferCount; ++at) {
        const QUIC_BUFFER& piece = event->RECEIVE.Buffers[at];
        const auto* const first =
            reinterpret_cast<const std::byte*>(piece.Buffer);
        gathered.insert(gathered.end(), first, first + piece.Length);
      }
      // THE END OF THE STREAM IS THE END OF THE MESSAGE, and it arrives
      // on the last piece as often as it arrives on its own.
      if ((event->RECEIVE.Flags & QUIC_RECEIVE_FLAG_FIN) != 0) {
        incoming->delivered = true;
        deliver(*incoming->session, incoming->named,
                std::move(incoming->message));
      }
      break;
    }
    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
      if (!incoming->abandoned && !incoming->delivered) {
        incoming->delivered = true;
        deliver(*incoming->session, incoming->named,
                std::move(incoming->message));
      }
      // Nothing is shut down from here. A stream the other end opened
      // one way has no send side at this end, so the end of theirs is
      // the end of the whole stream and the library says so next.
      break;
    case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
      // Half a message is no message: what was gathered is dropped.
      incoming->abandoned = true;
      break;
    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
      library().api->StreamClose(stream);
      delete incoming;
      break;
    default:
      break;
  }
  return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API onConnection(HQUIC /*connection*/, void* context,
                                  QUIC_CONNECTION_EVENT* event) {
  const InCallback callback;
  auto* const peer = static_cast<Peer*>(context);
  switch (event->Type) {
    case QUIC_CONNECTION_EVENT_CONNECTED:
      peer->reached();
      break;
    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
      peer->reached();
      peer->takeStream(event->PEER_STREAM_STARTED.Stream);
      break;
    case QUIC_CONNECTION_EVENT_DATAGRAM_RECEIVED:
      peer->reached();
      peer->takeDatagram(*event->DATAGRAM_RECEIVED.Buffer);
      break;
    case QUIC_CONNECTION_EVENT_DATAGRAM_STATE_CHANGED:
      peer->datagramCeiling(event->DATAGRAM_STATE_CHANGED.SendEnabled
                                ? event->DATAGRAM_STATE_CHANGED.MaxSendLength
                                : 0);
      break;
    case QUIC_CONNECTION_EVENT_DATAGRAM_SEND_STATE_CHANGED:
      // A datagram that will not change state again is one whose bytes
      // nobody is reading any more, however it ended.
      if (QUIC_DATAGRAM_SEND_STATE_IS_FINAL(
              event->DATAGRAM_SEND_STATE_CHANGED.State))
        delete static_cast<Sending*>(
            event->DATAGRAM_SEND_STATE_CHANGED.ClientContext);
      break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
      peer->endingWith(event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
      break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
      peer->endedByPeer();
      break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
      peer->concluded();
      break;
    default:
      break;
  }
  return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API onListener(HQUIC /*listener*/, void* context,
                                QUIC_LISTENER_EVENT* event) {
  const InCallback callback;
  auto* const standing = static_cast<Session*>(context);
  if (event->Type != QUIC_LISTENER_EVENT_NEW_CONNECTION)
    return QUIC_STATUS_SUCCESS;

  const Library& lib = library();
  const std::shared_ptr<Session> session = standing->shared_from_this();
  uint64_t number = 0;
  {
    const std::lock_guard<std::mutex> lock(session->gate);
    number = ++session->taken;
  }
  // ONE MACHINE MAY HOLD SEVERAL CONNECTIONS at once, each of them a
  // conversation of its own with its own answers, so the address alone
  // would not tell two of them apart. The number is what does, and it
  // counts the connections one feed has taken.
  std::string named = peerAddress(*event->NEW_CONNECTION.Info->RemoteAddress) +
                      "#" + std::to_string(number);
  const auto peer =
      std::make_shared<Peer>(session, std::move(named), /*dialled=*/false);

  HQUIC connection = event->NEW_CONNECTION.Connection;
  // The handler and the peer stand before the handshake is let go, so no
  // event can arrive on a connection whose context is not yet this peer.
  lib.api->SetCallbackHandler(
      connection, reinterpret_cast<void*>(&onConnection), peer.get());
  peer->adopt(connection, peer);
  const QUIC_STATUS status =
      lib.api->ConnectionSetConfiguration(connection, session->configuration);
  if (QUIC_FAILED(status)) {
    // A refused connection is the library's again: nothing more is
    // indicated on it, so this end gives the handle up rather than
    // closing it.
    peer->abandon();
    return status;
  }
  return QUIC_STATUS_SUCCESS;
}

/** THE TRANSPORT'S END OF ONE FEED: the listener holding a port, or the
 *  one connection a call opened, and the session both of them deliver
 *  through.
 *
 *  One listener per feed. Closing a feed then gives back a port of its
 *  own and ends connections of its own, and reaches no other feed. */
struct Door {
  ~Door() { close(); }

  void close();
  bool send(const Bytes& message);
  bool sendTo(std::string_view to, const Bytes& message);

  /** Every connection standing now, for a send that reaches all of
   *  them. */
  std::vector<std::shared_ptr<Peer>> standing() const;

  std::shared_ptr<Session> session = std::make_shared<Session>();
  HQUIC listener = nullptr;
  /** The one connection a call holds; null on a door that holds a
   *  port. */
  std::shared_ptr<Peer> dialled;
  /** Raised before the shutdown begins, so a send racing it stops rather
   *  than handing bytes to a door that is ending. */
  std::atomic<bool> closed{false};
};

std::vector<std::shared_ptr<Peer>> Door::standing() const {
  std::vector<std::shared_ptr<Peer>> reached;
  const std::lock_guard<std::mutex> lock(session->gate);
  reached.reserve(session->peers.size());
  for (const auto& [named, peer] : session->peers) reached.push_back(peer);
  return reached;
}

void Door::close() {
  if (closed.exchange(true)) return;
  auto shut = [session = session, held = listener, called = dialled] {
    const Library& lib = library();
    if (held) {
      // In that order: the listener takes no more connections, and then
      // the port comes back, which waits for it to stop indicating.
      lib.api->ListenerStop(held);
      lib.api->ListenerClose(held);
    }
    if (called) {
      called->givenUp();
      called->shutdown();
    }
    std::vector<std::shared_ptr<Peer>> reached;
    {
      const std::lock_guard<std::mutex> lock(session->gate);
      reached.reserve(session->peers.size());
      for (const auto& [named, peer] : session->peers) reached.push_back(peer);
    }
    // Outside the table: ending one connection must not hold the table
    // every other connection is found through.
    for (const std::shared_ptr<Peer>& peer : reached) peer->shutdown();
    // Last, and here rather than wherever the session's last holder
    // happens to let go, which may be a callback of the library's.
    session->giveBack();
  };
  listener = nullptr;
  dialled.reset();
  if (insideCallback) {
    // Giving a listener back waits for its callbacks to end, so it
    // cannot be waited for from inside one. The port comes back a moment
    // after the last holder let go rather than within it.
    std::thread(std::move(shut)).detach();
    return;
  }
  shut();
}

bool Door::send(const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  if (dialled) return dialled->write(message);
  // The connections are taken out from under the lock and written to
  // outside it: a write on one peer must not hold the table every other
  // peer is found through.
  bool went = false;
  for (const std::shared_ptr<Peer>& peer : standing())
    if (peer->write(message)) went = true;
  // A door that holds its own connections can say a broadcast reached
  // none of them, which is what "nobody is listening" looks like from
  // here.
  return went;
}

bool Door::sendTo(std::string_view to, const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  std::shared_ptr<Peer> peer;
  {
    const std::lock_guard<std::mutex> lock(session->gate);
    const auto found = session->peers.find(std::string(to));
    if (found == session->peers.end()) return false;
    peer = found->second;
  }
  return peer->write(message);
}

/** A feed whose transport could not open: the reason stands on the feed,
 *  and there is no door to close or to send through. */
OpenedFeed refuse(const std::weak_ptr<Feed>& into, std::string why) {
  if (const std::shared_ptr<Feed> feed = into.lock())
    feed->fail(std::move(why));
  return {};
}

/** Opens one feed's listener: the port the URI names, answered for with
 *  the certificate and key it names beside it.
 *
 *  The bind is done here rather than left to the background, so a feed
 *  that could not take its port says so by the time it is answered. */
OpenedFeed hold(const Hub& hub, std::string_view uri, const Address& place,
                const std::weak_ptr<Feed>& into) {
  // A QUIC PORT ANSWERS FOR ITSELF OR IT ANSWERS NOBODY: there is no
  // unencrypted form of this door to fall back to.
  if (place.certificate.empty() || place.key.empty())
    return refuse(into, std::string(uri) +
                            " names no certificate: a feed holds a port at "
                            "quic://:port?cert=<file>&key=<file>, a quic "
                            "connection being encrypted or nothing at all");
  // WHERE THE PAIR STANDS IS RESOLVED NOW, through the mount table, and
  // what the listener keeps from here on is what the library read out of
  // the two files: a feed may outlive the hub that opened it.
  const std::filesystem::path certificate = fileAt(hub, place.certificate);
  if (certificate.empty())
    return refuse(into, place.certificate +
                            " is no file: a listener's certificate stands at a "
                            "path, or at a URI the hub resolves to one");
  const std::filesystem::path key = fileAt(hub, place.key);
  if (key.empty())
    return refuse(into, place.key +
                            " is no file: a listener's key stands at a path, "
                            "or at a URI the hub resolves to one");

  const std::string certificateName = certificate.string();
  const std::string keyName = key.string();
  QUIC_CERTIFICATE_FILE pair{};
  pair.CertificateFile = certificateName.c_str();
  pair.PrivateKeyFile = keyName.c_str();
  QUIC_CREDENTIAL_CONFIG credential{};
  credential.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
  credential.Flags = QUIC_CREDENTIAL_FLAG_NONE;
  credential.CertificateFile = &pair;

  const auto door = std::make_shared<Door>();
  door->session->feed = into;
  door->session->datagrams = place.datagrams;
  QUIC_STATUS why = QUIC_STATUS_SUCCESS;
  door->session->configuration = configurationFor(credential, why);
  if (!door->session->configuration)
    return refuse(
        into, "could not listen on " + std::string(uri) + ": " + reasonOf(why));

  const Library& lib = library();
  why = lib.api->ListenerOpen(lib.registration, &onListener,
                              door->session.get(), &door->listener);
  if (QUIC_FAILED(why))
    return refuse(
        into, "could not listen on " + std::string(uri) + ": " + reasonOf(why));

  // Every interface of both families is one dual-stack socket, which is
  // an address with no family named in it.
  QUIC_ADDR asked{};
  QuicAddrSetFamily(&asked, QUIC_ADDRESS_FAMILY_UNSPEC);
  QuicAddrSetPort(&asked, portOf(place));
  const QUIC_BUFFER named = protocol();
  why = lib.api->ListenerStart(door->listener, &named, 1, &asked);
  if (QUIC_FAILED(why))
    return refuse(
        into, "could not listen on " + std::string(uri) + ": " + reasonOf(why));

  // THE PORT THE SYSTEM CHOSE is read back, which is what a URI naming
  // port 0 was asking for.
  QUIC_ADDR took{};
  uint32_t size = sizeof(took);
  if (QUIC_FAILED(lib.api->GetParam(
          door->listener, QUIC_PARAM_LISTENER_LOCAL_ADDRESS, &size, &took)))
    return refuse(into, "could not listen on " + std::string(uri) +
                            ": the port it took could not be read back");

  OpenedFeed opened;
  opened.address = "quic://[::]:" + std::to_string(QuicAddrGetPort(&took));
  opened.close = [door] { door->close(); };
  opened.send = [door](const Bytes& message) { return door->send(message); };
  opened.sendTo = [door](std::string_view to, const Bytes& message) {
    return door->sendTo(to, message);
  };
  return opened;
}

/** Opens one feed's call: one connection to the host and port the URI
 *  names.
 *
 *  The handshake is left to the library rather than waited for here,
 *  because an end that answers slowly, or is not there at all, would
 *  otherwise hold whoever asked for the feed for as long as reaching it
 *  takes. What it decided reaches the feed either way: as arrivals, or
 *  as the sentence error() answers. */
OpenedFeed reach(const Address& place, const std::weak_ptr<Feed>& into) {
  const Library& lib = library();
  QUIC_CREDENTIAL_CONFIG credential{};
  credential.Type = QUIC_CREDENTIAL_TYPE_NONE;
  credential.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
  // A SELF-SIGNED CERTIFICATE ON A STAGE IS THE ORDINARY CASE, and
  // nobody signs for it, so a URI may say plainly that this end will not
  // check what it is shown. What that gives up is the certainty that the
  // machine answering is the one the URI named; what stays is that
  // everything crossing the connection is encrypted all the same.
  if (place.insecure)
    credential.Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;

  const auto door = std::make_shared<Door>();
  door->session->feed = into;
  door->session->datagrams = place.datagrams;
  // The address a CALL reports and every arrival on it names: the
  // authority the URI wrote, the query being this end's own arrangement.
  door->session->called = "quic://" + authorityOf(place);
  const std::string called = door->session->called;
  QUIC_STATUS why = QUIC_STATUS_SUCCESS;
  door->session->configuration = configurationFor(credential, why);
  if (!door->session->configuration)
    return refuse(into, "could not reach " + called + ": " + reasonOf(why));

  const auto peer =
      std::make_shared<Peer>(door->session, called, /*dialled=*/true);
  HQUIC connection = nullptr;
  why = lib.api->ConnectionOpen(lib.registration, &onConnection, peer.get(),
                                &connection);
  if (QUIC_FAILED(why))
    return refuse(into, "could not reach " + called + ": " + reasonOf(why));
  peer->adopt(connection, peer);
  why = lib.api->ConnectionStart(connection, door->session->configuration,
                                 QUIC_ADDRESS_FAMILY_UNSPEC, place.host.c_str(),
                                 portOf(place));
  if (QUIC_FAILED(why)) {
    peer->discard();
    return refuse(into, "could not reach " + called + ": " + reasonOf(why));
  }
  door->dialled = peer;

  OpenedFeed opened;
  // The address a call has is the end it reached: the port its own socket
  // took is the system's to choose and nothing anybody could reach it at.
  opened.address = called;
  opened.close = [door] { door->close(); };
  opened.send = [door](const Bytes& message) { return door->send(message); };
  return opened;
}

/** ONE SCHEME, TWO SHAPES, split by the shape of the URI: a URI naming a
 *  host is an end to call, and a URI naming none is a port to hold. */
OpenedFeed openFeed(const Hub& hub, std::string_view uri,
                    const std::weak_ptr<Feed>& into) {
  const std::optional<Address> address = parseAddress(uri);
  if (!address)
    return refuse(into,
                  std::string(uri) +
                      " is not a quic address: a feed holds a port at "
                      "quic://:port?cert=<file>&key=<file> and calls one at "
                      "quic://host:port");
  if (!library().api)
    return refuse(into, std::string(uri) +
                            " opens nothing: this machine has no quic library");
  if (address->host.empty()) return hold(hub, uri, *address, into);
  return reach(*address, into);
}

}  // namespace

void registerQuic(Hub& hub) {
  // The certificate and key a URI's query names are resolved through
  // this hub as the feed opens. The reference is read there and nowhere
  // else: a transport is registered ON a hub and is held by it, so the
  // hub stands for every call made through this, while the feed that
  // call answers may outlive it — which is why what the listener keeps
  // is what the library read out of the two files, and not a way back
  // here.
  hub.setFeedTransport("quic",
                       [&hub](std::string_view uri, std::weak_ptr<Feed> into) {
                         return openFeed(hub, uri, into);
                       });
}

}  // namespace sigil::io
