#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Json.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>
#include <sigilprotocol/definition/Definition.h>
#include <sigilprotocol/endpoint/Endpoint.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <system_error>
#include <utility>

namespace sigil::protocol {
namespace {

/** The path a client reaches the protocol at. */
constexpr std::string_view kPath = "/sigil";

/** The file under the state root the address is written to. */
constexpr std::string_view kAddressFile = "protocol-address";

/** The directory under the state root the listener serves pages out of,
 *  and the one page in it: the definition, at `/protocol`. */
constexpr std::string_view kPagesDirectory = "protocol-pages";
constexpr std::string_view kDefinitionPage = "protocol";

/** Bytes carrying @p text, which is what a feed sends. */
io::Bytes bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  io::Bytes out;
  out.bytes.assign(first, first + text.size());
  return out;
}

/** The port out of an address a listening feed reports,
 *  `ws://127.0.0.1:52341/sigil` or `ws://[::]:52341/sigil`; empty where
 *  there is none. The port is what follows the authority's last colon,
 *  an IPv6 host being bracketed. */
std::string portOf(std::string_view address) {
  constexpr std::string_view kScheme = "ws://";
  if (!address.starts_with(kScheme)) return {};
  std::string_view authority = address.substr(kScheme.size());
  authority = authority.substr(0, authority.find('/'));
  const size_t colon = authority.rfind(':');
  if (colon == std::string_view::npos) return {};
  return std::string(authority.substr(colon + 1));
}

/** Writes @p text to @p path whole or not at all: into a file beside it
 *  first, then renamed over it, so a client reading the path never sees
 *  half an address. */
bool writeWhole(const std::filesystem::path& path, std::string_view text) {
  std::filesystem::path writing = path;
  writing += ".writing";
  {
    std::ofstream out(writing, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) return false;
  }
  std::error_code ec;
  std::filesystem::rename(writing, path, ec);
  return !ec;
}

/** The whole file at @p path, or empty where it cannot be read. */
std::string readWhole(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

/** THE SOCKET'S SIDE OF AN ENDPOINT. */
struct Endpoint::Socket {
  data::Connection connection;
  /** What notices a peer leaving: the connection hears only messages. */
  io::DispatchLease departures;
  /** Each peer attached, by the address its messages arrive under, and
   *  the session it is answered as; and the same the other way. */
  std::map<std::string, std::string, std::less<>> sessionOfPeer;
  std::map<std::string, std::string, std::less<>> peerOfSession;
  std::string address;
  std::string error;
  std::filesystem::path addressFile;
};

Endpoint::Endpoint(io::Hub& hub, Program program, EndpointPolicy policy)
    : Dispatcher(std::move(program)), m_socket(std::make_unique<Socket>()) {
  Socket& socket = *m_socket;
  const std::filesystem::path& root = this->program().stateRoot;
  if (root.empty()) {
    socket.error =
        "endpoint: no state root: the address file and the definition are "
        "written under one";
    return;
  }

  // THE DEFINITION, WRITTEN WHERE THE LISTENER SERVES IT: the very bytes
  // the generator read, at /protocol on the same port.
  const std::filesystem::path pages = root / kPagesDirectory;
  std::error_code ec;
  std::filesystem::create_directories(pages, ec);
  if (ec) {
    socket.error =
        "endpoint: " + pages.string() + " could not be made: " + ec.message();
    return;
  }
  // The directory rides in the listener's query, where an ampersand, a
  // question mark or a hash would end it early.
  if (pages.string().find_first_of("&?#") != std::string::npos) {
    socket.error = "endpoint: the state root " + root.string() +
                   " holds a character a listener's query cannot carry";
    return;
  }
  const std::span<const std::byte> served = definition();
  if (!writeWhole(pages / kDefinitionPage,
                  std::string_view(reinterpret_cast<const char*>(served.data()),
                                   served.size()))) {
    socket.error =
        "endpoint: the definition could not be written under " + pages.string();
    return;
  }

  // THE DOOR: loopback alone, unless peers beyond it were stated, and
  // then every interface with those peers and loopback admitted.
  std::string uri = "ws://:" + std::to_string(policy.port) +
                    std::string(kPath) + "?pages=" + pages.string() +
                    "&admit=loopback";
  if (policy.statedPeers.empty()) uri += "&bind=127.0.0.1";
  for (const std::string& peer : policy.statedPeers) uri += "&admit=" + peer;
  if (!hub.feedTransport("ws")) io::registerWebSocket(hub);
  socket.connection = data::Connection(hub, uri);
  if (!socket.connection.error().empty()) {
    socket.error = "endpoint: " + socket.connection.error();
    return;
  }
  const std::string port = portOf(socket.connection.address());
  if (port.empty()) {
    socket.error = "endpoint: the listener named no port it holds";
    return;
  }
  socket.address = "ws://127.0.0.1:" + port + std::string(kPath);

  // THE ADDRESS FILE, before the first frame: a client started beside
  // this program reads it to find the port it was given.
  socket.addressFile = root / kAddressFile;
  if (!writeWhole(socket.addressFile, socket.address + "\n")) {
    socket.error = "endpoint: the address could not be written to " +
                   socket.addressFile.string();
    socket.address.clear();
    socket.addressFile.clear();
    socket.connection = data::Connection();
    return;
  }

  // EVERY REQUEST, on the frame thread: a peer's first message attaches
  // it, and every answer goes back to that peer alone.
  socket.connection.on("*", [this](const data::Json& message) {
    Socket& held = *m_socket;
    const std::string peer = held.connection.sender();
    if (peer.empty()) return;
    auto found = held.sessionOfPeer.find(peer);
    if (found == held.sessionOfPeer.end()) {
      const std::string session = attach();
      held.peerOfSession[session] = peer;
      found = held.sessionOfPeer.emplace(peer, session).first;
    }
    request(found->second, message,
            [feed = held.connection.feed(), peer](std::string answer) {
              feed->sendTo(peer, bytesOf(answer));
            });
  });
  // A PEER THAT LEFT IS DETACHED on the frame after, which clears what
  // it alone set. With no client attached this reads nothing.
  socket.departures = hub.onDispatch([this](double) {
    Socket& held = *m_socket;
    if (held.sessionOfPeer.empty()) return;
    const std::vector<std::string> attached = held.connection.feed()->peers();
    std::vector<std::pair<std::string, std::string>> gone;
    for (const auto& [peer, session] : held.sessionOfPeer)
      if (std::find(attached.begin(), attached.end(), peer) == attached.end())
        gone.emplace_back(peer, session);
    for (const auto& [peer, session] : gone) {
      held.sessionOfPeer.erase(peer);
      held.peerOfSession.erase(session);
      detach(session);
    }
  });
}

Endpoint::~Endpoint() {
  Socket& socket = *m_socket;
  for (const std::string& session : sessions())
    detach(session, "the host is closing");
  socket.departures.release();
  // The address file is taken back only while it still names this
  // endpoint: another program may have written its own since.
  if (!socket.addressFile.empty() &&
      readWhole(socket.addressFile) == socket.address + "\n") {
    std::error_code ec;
    std::filesystem::remove(socket.addressFile, ec);
  }
}

bool Endpoint::listening() const { return !m_socket->address.empty(); }

const std::string& Endpoint::error() const { return m_socket->error; }

const std::string& Endpoint::address() const { return m_socket->address; }

std::filesystem::path Endpoint::addressFile() const {
  return m_socket->addressFile;
}

void Endpoint::deliver(const std::string& session, std::string_view method,
                       std::string_view parameters) {
  const auto found = m_socket->peerOfSession.find(session);
  if (found == m_socket->peerOfSession.end()) return;
  const std::string envelope =
      "{\"session\":" + data::encodeJson(data::Json(session)) +
      ",\"method\":" + data::encodeJson(data::Json(std::string(method))) +
      ",\"parameters\":" + std::string(parameters) + "}";
  m_socket->connection.feed()->sendTo(found->second, bytesOf(envelope));
}

}  // namespace sigil::protocol
