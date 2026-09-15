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
using sigil::io::Arrival;
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

/** WHAT A CLIENT CASE NEEDS BEFORE IT CAN OPEN ANYTHING: a hub with the
 *  listener taught first and the client standing in front of it, which
 *  is the order the two are registered in — the client reads the
 *  transport already holding the scheme so it can hand the URIs that
 *  name a port to it. */
class IOWebSocketClient : public ::testing::Test {
 protected:
  IOWebSocketClient() {
    sigil::io::registerWebSocket(hub);
    sigil::io::registerWebSocketClient(hub);
  }

  /** The URL a client reaches @p listener at: the loopback, the port it
   *  bound, and the path it answers. */
  static std::string urlOf(const std::shared_ptr<Feed>& listener) {
    const std::string address = listener->address();
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
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  EXPECT_TRUE(listener->address().starts_with("ws://[::]:"))
      << listener->address();
  EXPECT_NE(portOf(listener->address()), 0);
}

TEST_F(IOWebSocketClient, AClientSaysTheServerItCalledIsItsAddress) {
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
  ASSERT_TRUE(listener->error().empty()) << listener->error();

  const std::string url = urlOf(listener);
  const std::shared_ptr<Feed> client = hub.feed(url);
  ASSERT_TRUE(client->error().empty()) << client->error();
  // A client has the one address it dialled: the port its own socket
  // took is the system's to choose and nothing anybody could reach it
  // at.
  EXPECT_EQ(client->address(), url);
}

TEST_F(IOWebSocketClient, AClientsMessageArrivesOnTheListenerNamingTheClient) {
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const std::shared_ptr<Feed> client = hub.feed(urlOf(listener));
  ASSERT_TRUE(client->error().empty()) << client->error();

  // The handshake runs on the client's own thread, so what says the
  // session is up is the first send that goes out over it.
  ASSERT_TRUE(waitUntil([&] {
    return client->send(bytesOf("a scene arrives"));
  })) << client->error();

  ASSERT_TRUE(waitUntil([&] { return listener->latest() != nullptr; }));
  EXPECT_EQ(listener->latest()->asText(), "a scene arrives");
  const std::optional<Arrival> heard = listener->receive();
  ASSERT_TRUE(heard.has_value());
  // The client reached the listener over loopback, and the arrival names
  // that address with the port the client's own socket took, which is
  // never zero.
  EXPECT_TRUE(heard->from.starts_with("ws://127.0.0.1:")) << heard->from;
  EXPECT_NE(portOf(heard->from), 0) << heard->from;
}

TEST_F(IOWebSocketClient, AListenersSendArrivesOnTheClientNamingTheServer) {
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const std::string url = urlOf(listener);
  const std::shared_ptr<Feed> client = hub.feed(url);
  ASSERT_TRUE(client->error().empty()) << client->error();

  // The listener's send goes out to every peer on the path, so the
  // client has to be attached before it: a message of the client's the
  // listener has taken is what says it is.
  ASSERT_TRUE(waitUntil([&] { return client->send(bytesOf("here")); }))
      << client->error();
  ASSERT_TRUE(waitUntil([&] { return listener->generation() == 1u; }));

  EXPECT_TRUE(listener->send(bytesOf("out to everyone")));
  ASSERT_TRUE(waitUntil([&] { return client->latest() != nullptr; }));
  EXPECT_EQ(client->latest()->asText(), "out to everyone");
  const std::optional<Arrival> back = client->receive();
  ASSERT_TRUE(back.has_value());
  // A client has one peer, the server it called, and every message it
  // takes is named for it.
  EXPECT_EQ(back->from, url);
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

  const std::shared_ptr<Feed> feed =
      hub.feed("ws://127.0.0.1:" + std::to_string(port) + "/sky");
  // The handshake runs on the feed's own thread, so the sentence it
  // could not connect stands on the feed a moment after it is asked for
  // rather than within the ask.
  EXPECT_TRUE(waitUntil([&] { return !feed->error().empty(); }));
  EXPECT_EQ(feed->latest(), nullptr);
}

TEST_F(IOWebSocketClient, AUriThatNamesNoPortIsNotAServerToCallAndSaysWhy) {
  // The port a scheme would stand in for is left to be spelled, so what
  // a feed reaches is what its URI says and not a default it is not
  // holding.
  const std::shared_ptr<Feed> feed = hub.feed("ws://desk.local/scene");
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
  EXPECT_EQ(feed->latest(), nullptr);
}

TEST_F(IOWebSocketClient,
       DroppingTheLastHolderEndsTheSessionRatherThanWaiting) {
  const std::shared_ptr<Feed> listener = hub.feed("ws://:0/sky");
  ASSERT_TRUE(listener->error().empty()) << listener->error();

  std::shared_ptr<Feed> client = hub.feed(urlOf(listener));
  ASSERT_TRUE(client->error().empty()) << client->error();
  ASSERT_TRUE(waitUntil([&] { return client->send(bytesOf("here")); }))
      << client->error();
  ASSERT_TRUE(waitUntil([&] { return listener->generation() == 1u; }));

  // Letting the last holder go closes the session from this thread: the
  // close frame goes out and the thread that was reading is joined
  // before the call returns. What this pins is that it returns at all —
  // a thread waiting for itself would never come back.
  const auto before = std::chrono::steady_clock::now();
  client.reset();
  EXPECT_LT(std::chrono::steady_clock::now() - before, 2s);
}

}  // namespace
