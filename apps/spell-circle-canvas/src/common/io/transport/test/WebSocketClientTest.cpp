/** @file
 * The WebSocket client transport: the server a feed calls, the messages
 * that go each way between it and a listener in this same process, the
 * sender each end names, what a URI nobody can call leaves on its feed,
 * and the session a feed ends when its last holder lets go.
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

  // The handshake runs on the client's own thread, so what says the
  // session is up is the first send that goes out over it.
  ASSERT_TRUE(waitUntil([&] {
    return client.send(bytesOf("a scene arrives"));
  })) << client.state().error;

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
  ASSERT_TRUE(waitUntil([&] { return client.send(bytesOf("here")); }))
      << client.state().error;
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

  const Feed feed =
      hub.listen("ws://127.0.0.1:" + std::to_string(port) + "/sky");
  // The handshake runs on the feed's own thread, so the sentence it
  // could not connect stands on the feed a moment after it is asked for
  // rather than within the ask.
  EXPECT_TRUE(waitUntil([&] { return !feed.state().error.empty(); }));
  EXPECT_FALSE(feed.latest().has_value());
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
  ASSERT_TRUE(waitUntil([&] { return client.send(bytesOf("here")); }))
      << client.state().error;
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
