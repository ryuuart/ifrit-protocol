/** @file
 * The WebSocket client transport: the server a feed calls, the
 * readiness that says its handshake is through, the messages that go
 * each way between it and a listener in this same process, the sender
 * each end names, what a URI nobody can call leaves on its feed, and the
 * session a feed ends when its last holder lets go.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace {

using boost::asio::ip::tcp;
using sigil::io::Message;
using sigil::io::Bytes;
using sigil::io::Feed;
using sigil::io::Hub;
using sigil::io::ReadyState;
using namespace std::chrono_literals;

/** Polls @p ready until it holds, or gives up. The work it waits for
 *  runs on the transport's own threads, so there is nothing here to pump
 *  — only a moment to give it, and a deadline long enough that a loaded
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
  return Bytes(std::span(first, text.size()));
}

/** The port out of an address a feed reports. An IPv6 address is
 *  bracketed, so the port is always what follows the last colon, and a
 *  path behind it ends the number rather than joining it. */
uint16_t portOf(const std::string& address) {
  const size_t colon = address.rfind(':');
  if (colon == std::string::npos) return 0;
  return static_cast<uint16_t>(std::stoul(address.substr(colon + 1)));
}

/** WHAT A CLIENT CASE NEEDS BEFORE IT CAN OPEN ANYTHING: a hub taught
 *  the websocket schemes, which puts the listener behind the client so
 *  the client can hand the URIs that name a port to it. */
class IOWebSocketClient : public ::testing::Test {
 protected:
  IOWebSocketClient() {
    sigil::io::registerTransports(hub, {"ws"});
  }

  /** The URL a client reaches @p listener at: the loopback, the port it
   *  bound, and the path it answers. */
  static std::string urlOf(const Feed& listener) {
    const std::string address = listener.state().localAddress;
    const size_t path = address.find('/', address.find("://") + 3);
    return "ws://127.0.0.1:" + std::to_string(portOf(address)) +
           (path == std::string::npos ? std::string() : address.substr(path));
  }

  Hub hub;
  boost::asio::io_context context;
};

TEST_F(IOWebSocketClient, AUriThatNamesNoHostStillOpensTheListenerBehindIt) {
  // One scheme, two shapes: the client stands in front of the listener
  // and hands it every URI that names a port to hold rather than a
  // server to call.
  const Feed listener = hub.listen("ws://:0/sky");
  ASSERT_TRUE(listener.state().error.empty()) << listener.state().error;
  EXPECT_TRUE(listener.state().localAddress.starts_with("ws://[::]:"))
      << listener.state().localAddress;
  EXPECT_NE(portOf(listener.state().localAddress), 0);
}

TEST_F(IOWebSocketClient, AClientSaysTheServerItCalledIsItsAddress) {
  const Feed listener = hub.listen("ws://:0/sky");
  ASSERT_TRUE(listener.state().error.empty()) << listener.state().error;

  const std::string url = urlOf(listener);
  const Feed client = hub.listen(url);
  ASSERT_TRUE(client.state().error.empty()) << client.state().error;
  // A client has the one address it dialled: the port its own socket
  // took is the system's to choose and nothing anybody could reach it
  // at.
  EXPECT_EQ(client.state().localAddress, url);
}

TEST_F(IOWebSocketClient, AClientsMessageArrivesOnTheListenerNamingTheClient) {
  const Feed listener = hub.listen("ws://:0/sky");
  ASSERT_TRUE(listener.state().error.empty()) << listener.state().error;
  const Feed client = hub.listen(urlOf(listener));
  ASSERT_TRUE(client.state().error.empty()) << client.state().error;

  ASSERT_TRUE(waitUntil([&] { return client.state().isOpen(); }))
      << client.state().error;
  ASSERT_TRUE(client.send(bytesOf("a scene arrives")));

  ASSERT_TRUE(waitUntil([&] { return listener.latest().has_value(); }));
  EXPECT_EQ(listener.latest()->payload->asText(), "a scene arrives");
  const std::optional<Message> heard = listener.receive();
  ASSERT_TRUE(heard.has_value());
  // The client reached the listener over loopback, and the arrival names
  // that address with the port the client's own socket took, which is
  // never zero.
  EXPECT_TRUE(heard->sender().starts_with("ws://127.0.0.1:")) << heard->sender();
  EXPECT_NE(portOf(heard->sender()), 0) << heard->sender();
}

TEST_F(IOWebSocketClient, AListenersSendArrivesOnTheClientNamingTheServer) {
  const Feed listener = hub.listen("ws://:0/sky");
  ASSERT_TRUE(listener.state().error.empty()) << listener.state().error;
  const std::string url = urlOf(listener);
  const Feed client = hub.listen(url);
  ASSERT_TRUE(client.state().error.empty()) << client.state().error;

  // The listener's send goes out to every peer on the path, so the
  // client has to be attached before it: a message of the client's the
  // listener has taken is what says it is.
  ASSERT_TRUE(waitUntil([&] { return client.state().isOpen(); }))
      << client.state().error;
  ASSERT_TRUE(client.send(bytesOf("here")));
  ASSERT_TRUE(waitUntil([&] { return listener.state().revision == 1u; }));

  EXPECT_TRUE(listener.send(bytesOf("out to everyone")));
  ASSERT_TRUE(waitUntil([&] { return client.latest().has_value(); }));
  EXPECT_EQ(client.latest()->payload->asText(), "out to everyone");
  const std::optional<Message> back = client.receive();
  ASSERT_TRUE(back.has_value());
  // A client has one peer, the server it called, and every message it
  // takes is named for it.
  EXPECT_EQ(back->sender(), url);
}

TEST_F(IOWebSocketClient, AServerNobodyIsHoldingLeavesTheReasonOnTheFeed) {
  // A port that was bound and given back: nothing answers there, so the
  // connection is refused rather than left to run out its patience.
  uint16_t port = 0;
  {
    tcp::acceptor holder(context, tcp::endpoint(tcp::v4(), 0));
    port = holder.local_endpoint().port();
  }
  ASSERT_NE(port, 0);

  const std::string url = "ws://127.0.0.1:" + std::to_string(port) + "/sky";
  const Feed feed = hub.listen(url);
  // The handshake runs on the feed's own thread, so the sentence it
  // could not connect stands on the feed a moment after it is asked for
  // rather than within the ask — and at no moment on the way does the
  // feed read as a door that stands.
  bool everOpen = false;
  EXPECT_TRUE(waitUntil([&] {
    const sigil::io::FeedState now = feed.state();
    everOpen = everOpen || now.isOpen();
    return !now.error.empty();
  }));
  EXPECT_FALSE(everOpen);
  EXPECT_EQ(feed.state().readiness, ReadyState::Connecting);
  EXPECT_NE(feed.state().error.find(url), std::string::npos)
      << feed.state().error;
  EXPECT_FALSE(feed.send(bytesOf("nobody to hear it")));
  EXPECT_FALSE(feed.latest().has_value());

  // Never opened, so the next ask for the URI calls again, into the
  // same feed, and finds nobody there again.
  const Feed again = hub.listen(url);
  EXPECT_EQ(again, feed);
  EXPECT_TRUE(waitUntil([&] { return !again.state().error.empty(); }));
  EXPECT_FALSE(again.state().isOpen());
}

TEST_F(IOWebSocketClient, AClientWhoseServerHasNotAnsweredReadsConnecting) {
  // A port that takes the connection and never answers the upgrade: the
  // system completes the connect into the acceptor's backlog, and nobody
  // reads the request.
  const tcp::acceptor silent(context, tcp::endpoint(tcp::v4(), 0));
  const std::string url = "ws://127.0.0.1:" +
                          std::to_string(silent.local_endpoint().port()) + "/sky";

  Feed client = hub.listen(url);
  EXPECT_EQ(client.state().readiness, ReadyState::Connecting);
  // The server being called is the address from the ask on, before it
  // has answered.
  EXPECT_EQ(client.state().localAddress, url);
  // A handshake still running is waited on by a second ask, not doubled.
  EXPECT_EQ(hub.listen(url), client);
  const auto watchUntil = std::chrono::steady_clock::now() + 200ms;
  while (std::chrono::steady_clock::now() < watchUntil) {
    ASSERT_EQ(client.state().readiness, ReadyState::Connecting);
    std::this_thread::sleep_for(5ms);
  }
  EXPECT_TRUE(client.state().error.empty()) << client.state().error;
  EXPECT_FALSE(client.send(bytesOf("too early")));

  // Letting go gives the handshake up rather than waiting out the ten
  // seconds it is allowed: libcurl asks whether the connect is still
  // wanted at least once a second while it waits.
  const auto before = std::chrono::steady_clock::now();
  client = {};
  EXPECT_LT(std::chrono::steady_clock::now() - before, 5s);
}

TEST_F(IOWebSocketClient, AClientReadsOpenOnceItsHandshakeIsThroughAndItsFirstSendGoesOut) {
  const Feed listener = hub.listen("ws://:0/sky");
  ASSERT_TRUE(listener.state().error.empty()) << listener.state().error;
  const Feed client = hub.listen(urlOf(listener));

  // Every reading before the handshake is through says Connecting; the
  // first that says otherwise says Open.
  ReadyState last = client.state().readiness;
  ASSERT_TRUE(waitUntil([&] {
    last = client.state().readiness;
    return last != ReadyState::Connecting;
  })) << client.state().error;
  ASSERT_EQ(last, ReadyState::Open) << client.state().error;
  EXPECT_TRUE(client.state().error.empty()) << client.state().error;

  // Open means the server holds this client: the first send goes out,
  // with no probing and no second try.
  EXPECT_TRUE(client.send(bytesOf("the first word")));
  ASSERT_TRUE(waitUntil([&] { return listener.latest().has_value(); }));
  EXPECT_EQ(listener.latest()->payload->asText(), "the first word");
}

TEST_F(IOWebSocketClient, AUriThatNamesNoPortIsNotAServerToCallAndSaysWhy) {
  // The port a scheme would stand in for is left to be spelled, so what
  // a feed reaches is what its URI says and not a default it is not
  // holding.
  const Feed feed = hub.listen("ws://desk.local/scene");
  EXPECT_FALSE(feed.state().error.empty());
  EXPECT_TRUE(feed.state().localAddress.empty());
  EXPECT_FALSE(feed.latest().has_value());
}

TEST_F(IOWebSocketClient,
       DroppingTheLastHolderEndsTheSessionRatherThanWaiting) {
  const Feed listener = hub.listen("ws://:0/sky");
  ASSERT_TRUE(listener.state().error.empty()) << listener.state().error;

  Feed client = hub.listen(urlOf(listener));
  ASSERT_TRUE(client.state().error.empty()) << client.state().error;
  ASSERT_TRUE(waitUntil([&] { return client.state().isOpen(); }))
      << client.state().error;
  ASSERT_TRUE(client.send(bytesOf("here")));
  ASSERT_TRUE(waitUntil([&] { return listener.state().revision == 1u; }));

  // Letting the last holder go closes the session from this thread: the
  // close frame goes out and the thread that was reading is joined
  // before the call returns. What this pins is that it returns at all —
  // a thread waiting for itself would never come back.
  const auto before = std::chrono::steady_clock::now();
  client = {};
  EXPECT_LT(std::chrono::steady_clock::now() - before, 2s);
}

}  // namespace
