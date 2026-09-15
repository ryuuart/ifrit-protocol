/** @file
 * The WebSocket transport: the port and the path a feed's URI names, the
 * listener holding them, the loop that listener runs on, and the peers
 * whose messages arrive in the feed and whom one send reaches all at
 * once — or, named one by one, alone.
 *
 * A URI's query may name a directory of PAGES, and the same port then
 * answers HTTP GET out of it, so what a peer loads and the socket it
 * opens back stand at one address.
 *
 * Listening only. The library underneath carries a websocket client as a
 * name and an empty body, so a URI naming a host to reach opens nothing
 * and says so; a peer connects inward, and the feed is the far end of
 * however many of them are attached.
 */

#include <libusockets.h>
#include <uwebsockets/App.h>

#include <atomic>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
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

/** The largest message a peer may send. A feed's message is one whole
 *  thing rather than a stream, so a peer that announces more than this
 *  is closed instead of the listener holding a growing fragment for it.
 */
constexpr unsigned int kMessageCeiling = 16u * 1024u * 1024u;

/** The largest page this listener hands back. A page is read whole to be
 *  answered in one write, so a file past this is refused rather than
 *  held in memory for whoever asked. */
constexpr std::uintmax_t kPageCeiling = 16u * 1024u * 1024u;

/** What a request naming no file of its own is served: a directory's
 *  index. */
constexpr std::string_view kIndexPage = "index.html";

/** WHAT A ws:// URI NAMES: the port to listen on, the path peers reach
 *  it at, the host that makes it a peer to call rather than a door to
 *  hold, and the URI a directory of pages stands at. */
struct Address {
  std::string host;
  std::uint16_t port = 0;
  std::string path = "/";
  std::string pages;
};

/** The `pages` value out of @p query — the URI the pages stand at — or
 *  nothing when the query names none. A query is ampersand-separated
 *  pairs, so a listener that grows a second setting spells it beside
 *  this one and neither has to know about the other. */
std::string_view pagesNamed(std::string_view query) {
  constexpr std::string_view kPages = "pages=";
  while (!query.empty()) {
    const size_t next = query.find('&');
    const std::string_view pair = query.substr(0, next);
    if (pair.starts_with(kPages)) return pair.substr(kPages.size());
    if (next == std::string_view::npos) break;
    query = query.substr(next + 1);
  }
  return {};
}

/** The address @p uri names, or nothing when it names none: a host and a
 *  port, then a path and the query behind it. The host is bracketed when
 *  it is an IPv6 literal and left out altogether to listen on every
 *  interface; the port is decimal digits and nothing else; an omitted
 *  path is the root. */
std::optional<Address> parseAddress(std::string_view uri) {
  constexpr std::string_view kScheme = "ws://";
  if (!uri.starts_with(kScheme)) return std::nullopt;
  std::string_view rest = uri.substr(kScheme.size());
  Address address;
  // The path is taken off the back first, so what remains is an
  // authority the same shape as any other and the slashes inside a path
  // are never mistaken for one.
  if (const size_t slash = rest.find('/'); slash != std::string_view::npos) {
    std::string_view named = rest.substr(slash);
    rest = rest.substr(0, slash);
    // A QUERY IS THE LISTENER'S OWN ARRANGEMENT and no part of the path
    // peers reach, so it comes off here: what the socket stands on, and
    // what the feed reports as its address, is the path alone.
    if (const size_t question = named.find('?');
        question != std::string_view::npos) {
      address.pages = std::string(pagesNamed(named.substr(question + 1)));
      named = named.substr(0, question);
    }
    if (!named.empty()) address.path = std::string(named);
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
  address.port = static_cast<std::uint16_t>(port);
  return address;
}

/** One peer's address, spelled the way a URI of @p scheme is,
 *  "ws://127.0.0.1:52341"; empty when @p binary is neither four bytes
 *  nor sixteen. An IPv6 address is bracketed, so what follows the last
 *  colon is always the port. It is the same spelling the datagram
 *  transport beside this one gives an endpoint, so one reader reads the
 *  sender of an arrival off either.
 *
 *  workaround: the library underneath prints every address it holds as
 *  eight hexadecimal groups, which is neither the shortened form an IPv6
 *  address is written in nor anything that could be written back to.
 *  Its raw bytes are taken instead and printed here. */
std::string peerAddress(std::string_view scheme, std::string_view binary,
                        unsigned int port) {
  std::ostringstream printed;
  printed.imbue(std::locale::classic());
  printed << scheme << "://";
  if (binary.size() == 4) {
    boost::asio::ip::address_v4::bytes_type four{};
    std::memcpy(four.data(), binary.data(), four.size());
    printed << boost::asio::ip::address_v4(four);
  } else if (binary.size() == 16) {
    boost::asio::ip::address_v6::bytes_type sixteen{};
    std::memcpy(sixteen.data(), binary.data(), sixteen.size());
    const boost::asio::ip::address_v6 six(sixteen);
    // An IPv4 peer that reached a dual-stack listener arrives as its
    // address mapped into IPv6, and is named by the IPv4 address it can
    // be written back to rather than by the mapping.
    if (six.is_v4_mapped())
      printed << boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped,
                                                  six);
    else
      printed << "[" << six << "]";
  } else {
    return {};
  }
  printed << ":" << port;
  return printed.str();
}

/** WHAT A PAGE IS SERVED AS, by the extension it ends in; anything else
 *  is bytes with no name. The type says what the bytes ARE and nothing
 *  about how they are encoded, which a document declares for itself. */
std::string_view contentType(const std::filesystem::path& path) {
  static constexpr std::pair<std::string_view, std::string_view> kTypes[] = {
      {".html", "text/html"},     {".css", "text/css"},
      {".js", "text/javascript"}, {".json", "application/json"},
      {".png", "image/png"},      {".jpg", "image/jpeg"},
      {".svg", "image/svg+xml"},  {".txt", "text/plain"}};
  const std::string extension = path.extension().string();
  for (const auto& [ending, type] : kTypes)
    if (extension == ending) return type;
  return "application/octet-stream";
}

/** Does @p relative stay beneath the directory it is joined onto? A
 *  rooted path and one climbing through ".." both name something outside
 *  it. It is the rule a mounted URI is read by: what a name reaches is
 *  beneath the directory it was resolved from and nothing standing
 *  around it. */
bool beneathDirectory(std::string_view relative) {
  const std::filesystem::path path{std::string(relative)};
  if (path.has_root_path()) return false;
  for (const std::filesystem::path& component : path)
    if (component == "..") return false;
  return true;
}

/** The whole file at @p path, or nothing when it cannot be read. */
std::optional<std::string> readPage(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return std::nullopt;
  std::string body((std::istreambuf_iterator<char>(stream)),
                   std::istreambuf_iterator<char>());
  if (stream.bad()) return std::nullopt;
  return body;
}

/** Answers @p response with the plain sentence @p why under @p status.
 *  A refusal is text and says so, so nothing a request spelled can come
 *  back as a page. */
void refusePage(uWS::HttpResponse<false>* response, std::string_view status,
                std::string_view why) {
  response->writeStatus(status);
  response->writeHeader("Content-Type", "text/plain");
  response->end(why);
}

/** ANSWERS ONE GET OUT OF @p directory: the file @p url names beneath
 *  it, read from disk now.
 *
 *  Read per request and cached nowhere, so a page edited on disk is the
 *  page the next reload is handed. The read is on the loop's own thread,
 *  which the small local files a page directory holds cost little enough
 *  to sit on. */
void servePage(const std::filesystem::path& directory,
               uWS::HttpResponse<false>* response, std::string_view url) {
  std::string_view named = url.starts_with('/') ? url.substr(1) : url;
  // A request naming no file of its own is the index, which is what a
  // person who typed the address and nothing after it asked for.
  if (named.empty()) named = kIndexPage;
  // The path is taken exactly as the request wrote it and nothing is
  // decoded out of it, so an escape is a name here and never a climb.
  if (!beneathDirectory(named))
    return refusePage(response, "404 Not Found", "no page stands here");
  const std::filesystem::path path = directory / std::string(named);
  std::error_code ec;
  // A directory has an entry like any other, so the question is whether
  // the path names a FILE: bytes are what a page is.
  if (!std::filesystem::is_regular_file(path, ec) || ec)
    return refusePage(response, "404 Not Found", "no page stands here");
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec) return refusePage(response, "404 Not Found", "no page stands here");
  if (size > kPageCeiling)
    return refusePage(response, "413 Payload Too Large",
                      "that page is larger than this listener hands back");
  const std::optional<std::string> body = readPage(path);
  if (!body)
    return refusePage(response, "404 Not Found", "no page stands here");
  response->writeHeader("Content-Type", contentType(path));
  response->end(*body);
}

/** WHAT ONE PEER CARRIES: the address it is named by. It is written
 *  once, when the peer arrives, so the sender every message of its
 *  names, the key it is found under to be answered alone, and the key
 *  its leaving clears are one string and cannot drift apart. The topic
 *  it subscribes to is the path it reached, and what it sends goes to
 *  the feed. */
struct Peer {
  std::string address;
};

/** WHAT THE LISTENER'S THREAD AND ITS CALLBACKS SHARE: the feed every
 *  peer delivers into, the topic a send goes out on, and the app and the
 *  loop the peers live on.
 *
 *  Held by the thread and by the door alike, so a callback still running
 *  when the feed is let go keeps everything it reads. The feed itself is
 *  held weakly: when it cannot be locked there is nobody left to deliver
 *  to, and the message is dropped there.
 *
 *  Only the loop's own thread reads or writes `app`, `listening` and
 *  `peers`; a door on another thread reaches them by deferring onto
 *  `loop`, which is published once the listener is bound and never
 *  changes after. */
struct Session {
  std::weak_ptr<Feed> feed;
  std::string scheme;
  std::string topic;
  /** WHERE THE PAGES STAND, or empty when the URI named none: the
   *  directory a GET is answered out of. It is resolved and written
   *  before the thread starts and never written again, so the loop
   *  reads it with nobody to race. */
  std::filesystem::path pages;
  uWS::App* app = nullptr;
  us_listen_socket_t* listening = nullptr;
  std::atomic<uWS::Loop*> loop{nullptr};
  /** EVERY PEER ATTACHED, under the address its messages are named by,
   *  which is what an answer to one of them is found through. A peer
   *  puts itself in as it arrives and takes itself out as it leaves, so
   *  nothing here outlives the socket it points at. */
  std::unordered_map<std::string, uWS::WebSocket<false, true, Peer>*> peers;
};

/** What a listener's thread answers with once it has tried to bind: the
 *  port it took, or the sentence saying why it took none. */
struct Bound {
  std::uint16_t port = 0;
  std::string failure;
};

/** Holds one listener until its loop is asked to stop, answering @p
 *  place's path for whoever connects.
 *
 *  The app is made, run and destroyed here because uWebSockets keeps one
 *  loop per thread and reaches it from wherever an app is touched: an
 *  app made on another thread would run against a loop that is not its
 *  own. */
void hold(const std::shared_ptr<Session>& session, const Address& place,
          std::promise<Bound> answer) {
  uWS::App app;
  session->app = &app;
  Bound bound;
  if (app.constructorFailed()) {
    bound.failure = "a websocket listener could not be made";
  } else {
    uWS::App::WebSocketBehavior<Peer> behavior;
    behavior.maxPayloadLength = kMessageCeiling;
    behavior.open = [session](auto* peer) {
      // Every peer on the path subscribes to that path, which is what
      // makes a broadcast one publish rather than a walk over a list
      // this file would have to keep. Answering ONE peer is the other
      // half, and that one is found by name, so the address is read
      // here — where the socket is certainly still open — and kept.
      peer->subscribe(session->topic);
      std::string named = peerAddress(session->scheme, peer->getRemoteAddress(),
                                      peer->getRemotePort());
      if (named.empty()) return;
      peer->getUserData()->address = named;
      session->peers[std::move(named)] = peer;
    };
    behavior.message = [session](auto* peer, std::string_view message,
                                 uWS::OpCode) {
      const std::shared_ptr<Feed> feed = session->feed.lock();
      if (!feed) return;
      const auto* const first =
          reinterpret_cast<const std::byte*>(message.data());
      Bytes payload;
      payload.bytes.assign(first, first + message.size());
      feed->deliver(std::move(payload), peer->getUserData()->address);
    };
    behavior.close = [session](auto* peer, int, std::string_view) {
      // A peer that has left is nobody to answer, and the entry that
      // would answer it points at a socket about to be freed.
      session->peers.erase(peer->getUserData()->address);
    };
    app.ws<Peer>(place.path, std::move(behavior));
    // PAGES, WHERE THE URI NAMED THEM: the same port answers GET out of
    // one directory, so the page a peer loads and the socket it opens
    // back stand at one address. A listener whose URI named none leaves
    // every request to the answer the app already carries, which is a
    // 404. The handler holds the directory and nothing else of the
    // session, because a page is a file and needs nothing else.
    if (!session->pages.empty())
      app.get("/*", [pages = session->pages](uWS::HttpResponse<false>* response,
                                             uWS::HttpRequest* request) {
        servePage(pages, response, request->getUrl());
      });
    app.listen(place.port, [&session](us_listen_socket_t* token) {
      session->listening = token;
    });
    if (!session->listening) {
      // A refused bind comes back as no listen socket and nothing else,
      // so what the feed is told is what is known: the port was not
      // taken, whether because somebody holds it or because this process
      // may not have it.
      bound.failure = "the port could not be taken";
    } else {
      bound.port = static_cast<std::uint16_t>(us_socket_local_port(
          0, reinterpret_cast<us_socket_t*>(session->listening)));
      session->loop.store(uWS::Loop::get(), std::memory_order_release);
    }
  }

  const bool listening = bound.failure.empty();
  answer.set_value(std::move(bound));
  // Answered before the run, so a feed is opened as soon as its port is
  // taken rather than when the first peer arrives.
  if (listening) app.run();
  session->app = nullptr;
}

/** THE TRANSPORT'S END OF ONE FEED: the thread its listener runs on and
 *  everything that thread shares.
 *
 *  One app per feed, and one thread per app. Closing a feed then closes
 *  an app of its own — its listen socket gives the port back, its peers
 *  end — and reaches no other feed. One app shared by several paths
 *  would have to outlive whichever of its feeds closed first, and could
 *  give no port back at all while another feed still stood on it. */
struct Door {
  ~Door() { close(); }

  void close();
  bool send(const Bytes& message);
  bool sendTo(std::string_view to, const Bytes& message);

  std::shared_ptr<Session> session = std::make_shared<Session>();
  std::thread thread;
  /** Raised before the close is deferred, so a send racing it stops
   *  rather than handing bytes to a loop that is ending. */
  std::atomic<bool> closed{false};
};

void Door::close() {
  if (closed.exchange(true)) return;
  if (uWS::Loop* const loop = session->loop.load(std::memory_order_acquire))
    loop->defer([session = session] {
      // Everything the loop still polls has to go before its run
      // returns: the listen socket is what holds the port, and the app
      // is what holds the peers still attached to it.
      if (session->listening) {
        us_listen_socket_close(0, session->listening);
        session->listening = nullptr;
      }
      if (session->app) session->app->close();
    });
  if (!thread.joinable()) return;
  // A message in flight holds the feed while it delivers, so the last
  // holder of a feed can be the listener's own thread. It cannot wait
  // for itself: the close is already queued, and the loop drains it and
  // ends the thread on the way out of the callback this is inside.
  if (thread.get_id() == std::this_thread::get_id()) {
    thread.detach();
    return;
  }
  thread.join();
}

bool Door::send(const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  uWS::Loop* const loop = session->loop.load(std::memory_order_acquire);
  if (!loop) return false;
  // The app belongs to its loop, so the bytes travel there in a copy of
  // their own and the caller's Bytes are its own again as soon as this
  // returns. A feed carries bytes rather than text, so what goes out is
  // a binary frame.
  loop->defer([session = session, payload = message.bytes] {
    if (!session->app) return;
    session->app->publish(
        session->topic,
        std::string_view(reinterpret_cast<const char*>(payload.data()),
                         payload.size()),
        uWS::OpCode::BINARY);
  });
  return true;
}

bool Door::sendTo(std::string_view to, const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  uWS::Loop* const loop = session->loop.load(std::memory_order_acquire);
  if (!loop) return false;
  // The peer is looked up on the loop that owns it, so the answer is
  // true when it was posted rather than when it was written: a peer
  // that left in between is gone by the time the loop reaches this, and
  // its message stops here.
  loop->defer(
      [session = session, named = std::string(to), payload = message.bytes] {
        const auto found = session->peers.find(named);
        if (found == session->peers.end()) return;
        found->second->send(
            std::string_view(reinterpret_cast<const char*>(payload.data()),
                             payload.size()),
            uWS::OpCode::BINARY);
      });
  return true;
}

/** A feed whose transport could not open: the reason stands on the feed,
 *  and there is no door to close or to send through. */
OpenedFeed refuse(const std::weak_ptr<Feed>& into, std::string why) {
  if (const std::shared_ptr<Feed> feed = into.lock())
    feed->fail(std::move(why));
  return {};
}

/** Opens one feed's listener: the port and path the URI names, held on a
 *  thread of its own.
 *
 *  The bind is waited for here rather than left to the background, so a
 *  feed that could not take its port says so by the time it is
 *  answered. */
OpenedFeed openFeed(const Hub& hub, std::string_view uri,
                    const std::weak_ptr<Feed>& into) {
  const std::optional<Address> address = parseAddress(uri);
  if (!address)
    return refuse(into, std::string(uri) +
                            " is not a ws address: a feed listens on "
                            "ws://:port/path");
  if (!address->host.empty())
    return refuse(into, std::string(uri) +
                            " names a host to reach, and this transport only "
                            "listens: a feed opens on ws://:port/path and "
                            "peers connect to it");

  const auto door = std::make_shared<Door>();
  door->session->feed = into;
  door->session->scheme = "ws";
  door->session->topic = address->path;
  if (!address->pages.empty()) {
    // WHERE THE PAGES STAND IS RESOLVED NOW, through the mount table,
    // and what the listener keeps from here on is the directory: a feed
    // may outlive the hub that opened it, and every request answered
    // afterwards reads the filesystem and nothing else.
    door->session->pages = hub.resolve(address->pages);
    if (door->session->pages.empty())
      return refuse(into, address->pages +
                              " is mounted nowhere: a listener's pages stand "
                              "at a URI the hub resolves to a directory");
  }

  std::promise<Bound> answer;
  std::future<Bound> waiting = answer.get_future();
  door->thread = std::thread([session = door->session, place = *address,
                              answer = std::move(answer)]() mutable {
    hold(session, place, std::move(answer));
  });
  const Bound bound = waiting.get();
  // Nothing was bound and nothing is running, so letting the door go
  // here is what joins the thread that said so.
  if (!bound.failure.empty())
    return refuse(
        into, "could not listen on " + std::string(uri) + ": " + bound.failure);

  OpenedFeed opened;
  // Every interface of both families is one dual-stack socket, which is
  // a v6 address with nothing in it, and the path stands behind the
  // port. It is the address a PEER reaches, so the query is not in it:
  // where the pages stand is this listener's own arrangement.
  opened.address = "ws://[::]:" + std::to_string(bound.port) + address->path;
  opened.close = [door] { door->close(); };
  opened.send = [door](const Bytes& message) { return door->send(message); };
  opened.sendTo = [door](std::string_view to, const Bytes& message) {
    return door->sendTo(to, message);
  };
  return opened;
}

}  // namespace

void registerWebSocket(Hub& hub) {
  // The pages a URI's query names are resolved through this hub as the
  // feed opens. The reference is read there and nowhere else: a
  // transport is registered ON a hub and is held by it, so the hub
  // stands for every call made through this, while the feed that call
  // answers may outlive it — which is why what the listener keeps is a
  // path and not a way back here.
  hub.setFeedTransport("ws",
                       [&hub](std::string_view uri, std::weak_ptr<Feed> into) {
                         return openFeed(hub, uri, into);
                       });
}

}  // namespace sigil::io
