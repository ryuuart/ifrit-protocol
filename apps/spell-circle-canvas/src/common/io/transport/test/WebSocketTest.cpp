/** @file
 * The WebSocket transport: the port and path a listening feed binds, the
 * messages peers send it and the sender each one names, the broadcast a
 * send is and the one peer a named send reaches instead, the pages a
 * URI's query stands the same port over, what a URI nobody can open
 * leaves on its feed, and the port a feed gives back when the last
 * holder lets go.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
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

#include "ScratchDir.h"

namespace {

using boost::asio::ip::tcp;
using sigil::io::Arrival;
using sigil::io::Bytes;
using sigil::io::Feed;
using sigil::io::Hub;
using namespace std::chrono_literals;

/** Polls @p ready until it holds, or gives up. The work it waits for
 *  runs on the listener's own thread, so there is nothing here to pump —
 *  only a moment to give it, and a deadline long enough that a loaded
 *  machine is not mistaken for a broken socket. */
bool waitUntil(const std::function<bool()>& ready) {
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (ready()) return true;
    std::this_thread::sleep_for(1ms);
  }
  return ready();
}

Bytes bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  Bytes out;
  out.bytes.assign(first, first + text.size());
  return out;
}

/** The port out of an address a feed reports. An IPv6 address is
 *  bracketed, so the port is always what follows the last colon, and a
 *  path behind it ends the number rather than joining it. */
uint16_t portOf(const std::string& address) {
  const size_t colon = address.rfind(':');
  if (colon == std::string::npos) return 0;
  return static_cast<uint16_t>(std::stoul(address.substr(colon + 1)));
}

/** A WEBSOCKET PEER WRITTEN OUT BY HAND: the upgrade request over a
 *  plain TCP socket, then frames masked the way a client must mask them
 *  and read back unmasked the way a server sends them.
 *
 *  It is here and not in the library because the library the transport
 *  stands on carries no client. A case may speak the protocol by hand to
 *  prove what the listener does with it; a transport may not, because a
 *  client written to the length of one test is not a door anybody else
 *  could open.
 *
 *  Every read waits with a deadline and then gives up, so a case whose
 *  message never comes fails rather than hanging on a socket. */
class Peer {
 public:
  Peer(boost::asio::io_context& context, uint16_t port, std::string_view path)
      : m_socket(context) {
    m_socket.connect(
        tcp::endpoint(boost::asio::ip::address_v4::loopback(), port));
    std::string request;
    request += "GET " + std::string(path) + " HTTP/1.1\r\n";
    request += "Host: 127.0.0.1\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    // Twenty-four characters is what a key is, and the length is what
    // the listener reads a websocket request by.
    request += "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n";
    request += "Sec-WebSocket-Version: 13\r\n\r\n";
    boost::asio::write(m_socket, boost::asio::buffer(request));

    if (!waitUntil([this] { return m_socket.available() != 0; })) return;
    boost::system::error_code error;
    const size_t greeting =
        boost::asio::read_until(m_socket, m_incoming, "\r\n\r\n", error);
    if (error) return;
    m_greeting.assign(boost::asio::buffers_begin(m_incoming.data()),
                      boost::asio::buffers_begin(m_incoming.data()) +
                          static_cast<std::ptrdiff_t>(greeting));
    m_incoming.consume(greeting);
  }

  /** Whether the listener switched protocols rather than answering with
   *  a page. */
  bool upgraded() const { return m_greeting.find("101") != std::string::npos; }

  /** One whole message in one masked frame. A payload of fewer than
   *  65536 bytes is what the two-byte extended length carries, which is
   *  as much as any case here sends. */
  void send(unsigned char opCode, std::string_view payload) {
    const unsigned char mask[4] = {0x37, 0xfa, 0x21, 0x3d};
    std::vector<unsigned char> frame;
    frame.push_back(static_cast<unsigned char>(0x80 | opCode));
    if (payload.size() < 126) {
      frame.push_back(static_cast<unsigned char>(0x80 | payload.size()));
    } else {
      frame.push_back(static_cast<unsigned char>(0x80 | 126));
      frame.push_back(static_cast<unsigned char>(payload.size() >> 8));
      frame.push_back(static_cast<unsigned char>(payload.size() & 0xff));
    }
    frame.insert(frame.end(), mask, mask + 4);
    for (size_t index = 0; index != payload.size(); ++index)
      frame.push_back(static_cast<unsigned char>(
          static_cast<unsigned char>(payload[index]) ^ mask[index % 4]));
    boost::asio::write(m_socket, boost::asio::buffer(frame));
  }

  /** The payload of the next frame the listener sends; empty when none
   *  arrives before the deadline. A server never masks, so the payload
   *  stands as it is written. */
  std::string receive() {
    const std::string head = take(2);
    if (head.size() != 2) return {};
    size_t size = static_cast<unsigned char>(head[1]) & 0x7f;
    if (size == 126) {
      const std::string extended = take(2);
      if (extended.size() != 2) return {};
      size =
          (static_cast<size_t>(static_cast<unsigned char>(extended[0])) << 8) |
          static_cast<size_t>(static_cast<unsigned char>(extended[1]));
    }
    return take(size);
  }

 private:
  /** The next @p count bytes off the socket; fewer when the deadline
   *  passes first. */
  std::string take(size_t count) {
    if (count == 0) return {};
    if (!waitUntil([this, count] {
          return m_incoming.size() + m_socket.available() >= count;
        }))
      return {};
    if (m_incoming.size() < count) {
      boost::system::error_code error;
      boost::asio::read(
          m_socket, m_incoming,
          boost::asio::transfer_at_least(count - m_incoming.size()), error);
      if (m_incoming.size() < count) return {};
    }
    std::string out(boost::asio::buffers_begin(m_incoming.data()),
                    boost::asio::buffers_begin(m_incoming.data()) +
                        static_cast<std::ptrdiff_t>(count));
    m_incoming.consume(count);
    return out;
  }

  tcp::socket m_socket;
  boost::asio::streambuf m_incoming;
  std::string m_greeting;
};

/** WHAT ONE HTTP ANSWER SAYS: the number on its status line, the content
 *  type it named, and the body behind the blank line. */
struct Answer {
  int status = 0;
  std::string type;
  std::string body;
};

/** ONE HTTP GET WRITTEN OUT BY HAND, and the whole answer read back.
 *
 *  It is here for the reason the peer above is: the library the
 *  transport stands on carries no client of any kind, and a case may
 *  speak a protocol by hand to prove what the listener answers. The
 *  connection asks to be closed, so the end of the stream is the end of
 *  the answer and no length has to be believed to find it. The read runs
 *  on the case's own context with a deadline, so an answer that never
 *  comes fails the case rather than hanging it. */
Answer fetch(boost::asio::io_context& context, uint16_t port,
             std::string_view path) {
  tcp::socket socket(context);
  socket.connect(tcp::endpoint(boost::asio::ip::address_v4::loopback(), port));
  const std::string request = "GET " + std::string(path) +
                              " HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                              "Connection: close\r\n\r\n";
  boost::asio::write(socket, boost::asio::buffer(request));

  boost::asio::streambuf incoming;
  boost::asio::async_read(socket, incoming,
                          [](const boost::system::error_code&, size_t) {});
  context.restart();
  context.run_for(2s);
  boost::system::error_code closing;
  socket.close(closing);
  // A read the deadline cut short is cancelled by that close, and its
  // handler is run here — while the buffer it was reading into is still
  // standing.
  context.restart();
  context.run();

  const std::string whole(boost::asio::buffers_begin(incoming.data()),
                          boost::asio::buffers_end(incoming.data()));
  Answer answer;
  const size_t blank = whole.find("\r\n\r\n");
  if (blank == std::string::npos) return answer;
  const std::string head = whole.substr(0, blank);
  answer.body = whole.substr(blank + 4);
  if (const size_t space = head.find(' '); space != std::string::npos)
    answer.status = std::atoi(head.c_str() + space + 1);
  // The key is the one the transport writes, spelled the way it writes
  // it.
  constexpr std::string_view kType = "\r\nContent-Type: ";
  if (const size_t at = head.find(kType); at != std::string::npos) {
    const size_t from = at + kType.size();
    const size_t end = head.find("\r\n", from);
    answer.type = head.substr(from, end - from);
  }
  return answer;
}

/** A page small enough for a case to compare whole, and HTML so that
 *  what it is served as is the type its extension names. */
constexpr std::string_view kIndex = "<!doctype html><title>the sky</title>";

/** A directory of pages under @p scratch, mounted on @p hub at
 *  "pages://", with the file a climb out of that directory would reach
 *  standing one level above it. */
void standPages(Hub& hub, const sigil::test::ScratchDir& scratch) {
  scratch.write("pages/index.html", kIndex);
  scratch.write("secret", "what stands outside the pages");
  hub.mount("pages://", scratch.path / "pages");
}

/** WHAT A WEBSOCKET CASE NEEDS BEFORE IT CAN OPEN ANYTHING: a hub that
 *  has been taught the schemes, and one Asio context of the test's own
 *  for the sockets it speaks the protocol over by hand. */
class IOWebSocket : public ::testing::Test {
 protected:
  IOWebSocket() { sigil::io::registerTransports(hub); }

  Hub hub;
  boost::asio::io_context context;
};

TEST_F(IOWebSocket, AListeningFeedSaysWhichPortAndPathItBound) {
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  // Every interface of both families is one dual-stack socket, which is
  // a v6 address with nothing in it, and the path it answers stands
  // behind the port.
  EXPECT_TRUE(listener->address().starts_with("ws://[::]:"))
      << listener->address();
  EXPECT_TRUE(listener->address().ends_with("/sky")) << listener->address();
  EXPECT_NE(portOf(listener->address()), 0);
}

TEST_F(IOWebSocket, APeersTextMessageArrivesNamingWhereItCameFrom) {
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);

  Peer peer(context, port, "/sky");
  ASSERT_TRUE(peer.upgraded());
  peer.send(0x1, "a scene arrives");

  ASSERT_TRUE(waitUntil([&] { return listener->latest() != nullptr; }));
  EXPECT_EQ(listener->latest()->asText(), "a scene arrives");
  const std::optional<Arrival> arrival = listener->receive();
  ASSERT_TRUE(arrival.has_value());
  // The peer reached the listener over loopback, and the arrival names
  // that address — not the mapping a dual-stack socket holds it as —
  // with the port the peer's own socket took, which is never zero.
  EXPECT_TRUE(arrival->from.starts_with("ws://127.0.0.1:")) << arrival->from;
  EXPECT_NE(portOf(arrival->from), 0) << arrival->from;
}

TEST_F(IOWebSocket, ABinaryMessageArrivesWhole) {
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/scene");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);

  // Long enough to need the extended length, and holding the bytes a
  // text message could not: a feed answers bytes and counts them.
  std::string payload;
  for (int number = 0; number != 300; ++number)
    payload.push_back(static_cast<char>(number % 256));

  Peer peer(context, port, "/scene");
  ASSERT_TRUE(peer.upgraded());
  peer.send(0x2, payload);

  ASSERT_TRUE(waitUntil([&] { return listener->latest() != nullptr; }));
  EXPECT_EQ(listener->latest()->bytes.size(), payload.size());
  EXPECT_EQ(listener->latest()->asText(), payload);
}

TEST_F(IOWebSocket, SendReachesEveryPeerOnThePath) {
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);

  Peer first(context, port, "/sky");
  ASSERT_TRUE(first.upgraded());
  Peer second(context, port, "/sky");
  ASSERT_TRUE(second.upgraded());
  // Both peers have to have subscribed before the publish, and what says
  // they have is a message of theirs the feed has taken.
  first.send(0x1, "here");
  second.send(0x1, "here");
  ASSERT_TRUE(waitUntil([&] { return listener->generation() == 2u; }));

  EXPECT_TRUE(listener->send(bytesOf("out to everyone")));
  EXPECT_EQ(first.receive(), "out to everyone");
  EXPECT_EQ(second.receive(), "out to everyone");
}

TEST_F(IOWebSocket, SendToReachesTheOnePeerItNamesAndNoOther) {
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);

  Peer first(context, port, "/sky");
  ASSERT_TRUE(first.upgraded());
  Peer second(context, port, "/sky");
  ASSERT_TRUE(second.upgraded());
  // Each peer says which one it is, and the arrival it says it in names
  // the address that peer is answered by.
  first.send(0x1, "first");
  second.send(0x1, "second");
  ASSERT_TRUE(waitUntil([&] { return listener->generation() == 2u; }));

  std::string answering;
  while (const std::optional<Arrival> arrival = listener->receive())
    if (arrival->bytes->asText() == "first") answering = arrival->from;
  ASSERT_FALSE(answering.empty());

  EXPECT_TRUE(listener->sendTo(answering, bytesOf("to you alone")));
  EXPECT_EQ(first.receive(), "to you alone");
  // What the other peer reads first is the broadcast that came after,
  // which is what says the message before it went to one peer and not
  // to the path.
  EXPECT_TRUE(listener->send(bytesOf("out to everyone")));
  EXPECT_EQ(second.receive(), "out to everyone");
  EXPECT_EQ(first.receive(), "out to everyone");
}

TEST_F(IOWebSocket, APeerNobodyIsAttachedUnderIsNobodyToAnswer) {
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
  ASSERT_TRUE(listener->error().empty()) << listener->error();

  // The loop is what holds the peers, so a name it has nobody under is
  // answered by nothing being written — and the send says it was posted,
  // which is all a caller on another thread can be told.
  EXPECT_TRUE(listener->sendTo("ws://127.0.0.1:1", bytesOf("nobody")));
  EXPECT_FALSE(listener->closed());
}

TEST_F(IOWebSocket, AListenerServesThePagesItsUriNames) {
  const sigil::test::ScratchDir scratch("sigilio_ws_pages");
  standPages(hub, scratch);

  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky?pages=pages://");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);

  const Answer named = fetch(context, port, "/index.html");
  EXPECT_EQ(named.status, 200);
  EXPECT_EQ(named.type, "text/html");
  EXPECT_EQ(named.body, kIndex);
  // The root is that same index: it is what a person who typed the
  // address and nothing after it asked for.
  const Answer root = fetch(context, port, "/");
  EXPECT_EQ(root.status, 200);
  EXPECT_EQ(root.body, kIndex);
}

TEST_F(IOWebSocket, APathClimbingOutOfThePagesReachesNothing) {
  const sigil::test::ScratchDir scratch("sigilio_ws_pages");
  standPages(hub, scratch);

  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky?pages=pages://");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);

  // The file is really there, one directory above the pages, and the
  // request spells the way to it — which is the spelling a mount refuses
  // too: what a name reaches stands beneath the directory it resolved
  // from and nothing around it.
  const Answer climbed = fetch(context, port, "/../secret");
  EXPECT_EQ(climbed.status, 404);
  EXPECT_EQ(climbed.body.find("what stands outside"), std::string::npos)
      << climbed.body;
}

TEST_F(IOWebSocket, TheSocketStandsBesideThePagesOnTheSamePort) {
  const sigil::test::ScratchDir scratch("sigilio_ws_pages");
  standPages(hub, scratch);

  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky?pages=pages://");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);
  // The path peers reach is the path alone: the query the pages were
  // named in is no part of the address the feed reports.
  EXPECT_TRUE(listener->address().ends_with("/sky")) << listener->address();

  Peer peer(context, port, "/sky");
  ASSERT_TRUE(peer.upgraded());
  peer.send(0x1, "a phone is looking");

  ASSERT_TRUE(waitUntil([&] { return listener->latest() != nullptr; }));
  EXPECT_EQ(listener->latest()->asText(), "a phone is looking");
}

TEST_F(IOWebSocket, AListenerWhoseUriNamedNoPagesServesNone) {
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);

  EXPECT_EQ(fetch(context, port, "/index.html").status, 404);
}

TEST_F(IOWebSocket, PagesMountedNowhereOpenNothingAndSayWhy) {
  const std::shared_ptr<Feed> feed = hub.feed("ws://:0/sky?pages=pages://");
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
}

TEST_F(IOWebSocket, AUriThatNamesNoPortOpensNothingAndSaysWhy) {
  const std::shared_ptr<Feed> feed = hub.feed("ws://localhost");
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
  EXPECT_EQ(feed->latest(), nullptr);
}

TEST_F(IOWebSocket, AUriThatNamesAHostToReachOpensNothingOnTheListenerAlone) {
  // The listener does not call, so on a hub taught the listener and
  // nothing else a peer to reach is not something it can open — and the
  // feed says that rather than binding something else. A hub taught the
  // caller as well hands such a URI to the caller.
  Hub listening;
  sigil::io::registerWebSocket(listening);
  const std::shared_ptr<Feed> feed =
      listening.feed("ws://desk.local:9001/scene");
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
}

TEST_F(IOWebSocket, APortSomebodyElseHoldsOpensNothingAndSaysWhy) {
  tcp::acceptor holder(context, tcp::endpoint(tcp::v6(), 0));
  const uint16_t port = holder.local_endpoint().port();

  const std::shared_ptr<Feed> feed =
      hub.feed("ws://:" + std::to_string(port) + "/sky");
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
}

TEST_F(IOWebSocket, DroppingTheLastHolderOfAFeedGivesUpItsPort) {
  uint16_t port = 0;
  {
    const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
    ASSERT_TRUE(listener->error().empty()) << listener->error();
    port = portOf(listener->address());
    ASSERT_NE(port, 0);
  }

  // The close travels to the listener's own loop, so the port comes back
  // a moment after the last holder lets go rather than within it.
  const std::string uri = "ws://:" + std::to_string(port) + "/sky";
  std::shared_ptr<Feed> again;
  ASSERT_TRUE(waitUntil([&] {
    again = hub.feed(uri);
    if (again->error().empty()) return true;
    again.reset();
    return false;
  }));
  EXPECT_EQ(portOf(again->address()), port);
}

}  // namespace
