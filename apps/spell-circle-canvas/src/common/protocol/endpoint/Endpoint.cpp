#include <sigildata/decode/Dialect.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Json.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>
#include <sigilio/advanced/Time.h>
#include <sigilprotocol/definition/Definition.h>
#include <sigilprotocol/endpoint/Endpoint.h>

#include <algorithm>
#include <boost/asio/ip/address.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <system_error>
#include <utility>

namespace sigil::protocol {
namespace {

/** A value as JSON text. */
std::string jsonText(const sigil::data::Json& value) {
  const std::vector<std::byte> bytes =
      sigil::data::encode(value, sigil::data::Dialect::Json);
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

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
  return io::Bytes(std::span(first, text.size()));
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
  std::error_code failure;
  std::filesystem::rename(writing, path, failure);
  return !failure;
}

/** The whole file at @p path, or empty where it cannot be read. */
std::string readWhole(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/** Whether @p peer is a bare IP address, which is all a stated peer may
 *  be: it rides in the listener's query, where anything else could say
 *  more than whom to admit. */
bool bareAddress(const std::string& peer) {
  boost::system::error_code unread;
  boost::asio::ip::make_address(peer, unread);
  return !unread;
}

}  // namespace

/** THE SOCKET'S SIDE OF AN ENDPOINT. */
struct Endpoint::Socket {
  explicit Socket(Dispatcher& answering) : dispatcher(answering) {}

  Dispatcher& dispatcher;
  data::Connection connection;
  /** What notices a peer leaving: the connection hears only messages. */
  io::Lease departures;
  /** Each peer attached, by the address its messages arrive under, and
   *  the session it is answered as. */
  std::map<std::string, std::string, std::less<>> sessionOfPeer;
  std::string address;
  std::string error;
  std::filesystem::path addressFile;
};

Endpoint::Endpoint(io::Hub& hub, Dispatcher& dispatcher,
                   EndpointPolicy policy)
    : m_socket(std::make_unique<Socket>(dispatcher)) {
  Socket& socket = *m_socket;
  const std::filesystem::path& root = dispatcher.program().stateRoot;
  if (root.empty()) {
    socket.error =
        "endpoint: no state root: the address file and the definition are "
        "written under one";
    return;
  }
  // A stated peer rides in the listener's query beside the directory
  // and the interface, so one that is not a bare address is refused
  // before anything is written, naming it.
  for (const std::string& peer : policy.statedPeers)
    if (!bareAddress(peer)) {
      socket.error = "endpoint: the stated peer " + peer +
                     " is no IP address: a peer beyond loopback is admitted "
                     "by its address alone";
      return;
    }

  // THE DEFINITION, WRITTEN WHERE THE LISTENER SERVES IT: the very bytes
  // the generator read, at /protocol on the same port.
  const std::filesystem::path pages = root / kPagesDirectory;
  std::error_code failure;
  std::filesystem::create_directories(pages, failure);
  if (failure) {
    socket.error = "endpoint: " + pages.string() +
                   " could not be made: " + failure.message();
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
  // then every interface with those peers and loopback admitted. Its
  // frames are text, as the envelope is.
  std::string uri = "ws://:" + std::to_string(policy.port) +
                    std::string(kPath) + "?pages=" + pages.string() +
                    "&frames=text&admit=loopback";
  if (policy.statedPeers.empty()) uri += "&bind=127.0.0.1";
  for (const std::string& peer : policy.statedPeers) uri += "&admit=" + peer;
  if (!io::transport(hub, "ws")) io::registerTransports(hub, {"ws"});
  socket.connection = data::connect(hub, uri);
  if (!socket.connection.state().error.empty()) {
    socket.error = "endpoint: " + socket.connection.state().error;
    return;
  }
  const std::string port = portOf(socket.connection.state().localAddress);
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
  socket.connection.on("*", [this](const data::Message& message) {
    Socket& held = *m_socket;
    const std::string peer = message.sender();
    if (peer.empty()) return;
    const io::Feed feed = held.connection.feed();
    auto found = held.sessionOfPeer.find(peer);
    if (found == held.sessionOfPeer.end()) {
      // An event reaches this peer as its envelope, written to it alone.
      Attachment attachment;
      attachment.deliver = [feed, peer](const std::string& session,
                                        std::string_view method,
                                        std::string_view parameters) {
        feed.send(bytesOf("{\"session\":" + jsonText(data::Json(session)) +
                    ",\"method\":" +
                    jsonText(data::Json(std::string(method))) +
                    ",\"parameters\":" + std::string(parameters) + "}"), {.to = peer});
      };
      const std::string session = held.dispatcher.attach(std::move(attachment));
      found = held.sessionOfPeer.emplace(peer, session).first;
    }
    held.dispatcher.request(found->second, message.payload,
                            [feed, peer](std::string answer) {
                              feed.send(bytesOf(answer), {.to = peer});
                            });
  });
  // A PEER THAT LEFT IS DETACHED on the frame after, which clears what
  // it alone set. With no client attached this reads nothing.
  socket.departures = io::onAdvance(hub, [this](std::chrono::duration<double>) {
    Socket& held = *m_socket;
    if (held.sessionOfPeer.empty()) return;
    const std::vector<std::string> attached = held.connection.feed().peers();
    std::vector<std::pair<std::string, std::string>> gone;
    for (const auto& [peer, session] : held.sessionOfPeer)
      if (std::find(attached.begin(), attached.end(), peer) == attached.end())
        gone.emplace_back(peer, session);
    for (const auto& [peer, session] : gone) {
      held.sessionOfPeer.erase(peer);
      held.dispatcher.detach(session);
    }
  });
}

Endpoint::~Endpoint() {
  Socket& socket = *m_socket;
  // Copied: letting a client go may reach back into the map. The
  // dispatcher's clients attached in process are not this endpoint's.
  const std::map<std::string, std::string, std::less<>> attached =
      socket.sessionOfPeer;
  socket.sessionOfPeer.clear();
  for (const auto& [peer, session] : attached)
    socket.dispatcher.detach(session, "the endpoint is closing");
  socket.departures.release();
  // The address file is taken back only while it still names this
  // endpoint: another program may have written its own since.
  if (!socket.addressFile.empty() &&
      readWhole(socket.addressFile) == socket.address + "\n") {
    std::error_code failure;
    std::filesystem::remove(socket.addressFile, failure);
  }
}

bool Endpoint::listening() const { return !m_socket->address.empty(); }

const std::string& Endpoint::error() const { return m_socket->error; }

const std::string& Endpoint::address() const { return m_socket->address; }

std::filesystem::path Endpoint::addressFile() const {
  return m_socket->addressFile;
}

}  // namespace sigil::protocol
