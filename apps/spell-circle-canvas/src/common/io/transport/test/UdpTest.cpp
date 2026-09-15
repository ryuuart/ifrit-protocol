/** @file
 * The UDP transport: the port a listening feed binds and the datagrams
 * that arrive there, the peer a sending feed reaches, what a URI nobody
 * can open leaves on its feed, and the port a feed gives back when the
 * last holder lets go.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace {

using boost::asio::ip::udp;
using sigil::io::Bytes;
using sigil::io::Feed;
using sigil::io::Hub;
using namespace std::chrono_literals;

/** Polls @p ready until it holds, or gives up. The work it waits for
 *  runs on the transport's own thread, so there is nothing here to pump
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
 *  bracketed, so the port is always what follows the last colon. */
uint16_t portOf(const std::string& address) {
  const size_t colon = address.rfind(':');
  if (colon == std::string::npos) return 0;
  return static_cast<uint16_t>(std::stoul(address.substr(colon + 1)));
}

/** WHAT A TRANSPORT CASE NEEDS BEFORE IT CAN OPEN ANYTHING: a hub that
 *  has been taught the schemes, and one Asio context of the test's own
 *  for the raw sockets it sends datagrams and holds ports with. */
class IOUdp : public ::testing::Test {
 protected:
  IOUdp() { sigil::io::registerTransports(hub); }

  /** One datagram to a port on loopback, from a socket of its own. */
  void sendTo(uint16_t port, std::string_view text) {
    udp::socket sender(context, udp::v4());
    sender.send_to(
        boost::asio::buffer(text.data(), text.size()),
        udp::endpoint(boost::asio::ip::address_v4::loopback(), port));
  }

  Hub hub;
  boost::asio::io_context context;
};

TEST_F(IOUdp, AListeningFeedSaysWhichPortItBoundAndTakesWhatArrivesThere) {
  const std::shared_ptr<Feed> listener = hub.feed("udp://:0");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  // Every interface of both families is one dual-stack socket, which is
  // a v6 address with nothing in it.
  EXPECT_TRUE(listener->address().starts_with("udp://["))
      << listener->address();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);

  sendTo(port, "a scene arrives");
  ASSERT_TRUE(waitUntil([&] { return listener->latest() != nullptr; }));
  EXPECT_EQ(listener->latest()->asText(), "a scene arrives");
}

TEST_F(IOUdp, ASendingFeedReachesTheListenerItNames) {
  const std::shared_ptr<Feed> listener = hub.feed("udp://:0");
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const uint16_t port = portOf(listener->address());
  ASSERT_NE(port, 0);

  const std::shared_ptr<Feed> sender =
      hub.feed("udp://127.0.0.1:" + std::to_string(port));
  ASSERT_TRUE(sender->error().empty()) << sender->error();
  EXPECT_FALSE(sender->address().empty());
  EXPECT_TRUE(sender->send(bytesOf("through the door")));

  ASSERT_TRUE(waitUntil([&] { return listener->latest() != nullptr; }));
  EXPECT_EQ(listener->latest()->asText(), "through the door");
  // A listener answers whoever writes to it and holds no peer of its
  // own, so there is no way back out through it.
  EXPECT_FALSE(listener->send(bytesOf("no way back")));
}

TEST_F(IOUdp, AUriThatNamesNoAddressOpensNothingAndSaysWhy) {
  const std::shared_ptr<Feed> feed = hub.feed("udp://localhost");
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
  EXPECT_EQ(feed->latest(), nullptr);
}

TEST_F(IOUdp, APortSomebodyElseHoldsOpensNothingAndSaysWhy) {
  udp::socket holder(context, udp::v6());
  holder.set_option(boost::asio::ip::v6_only(false));
  holder.bind(udp::endpoint(udp::v6(), 0));
  const uint16_t port = holder.local_endpoint().port();

  const std::shared_ptr<Feed> feed = hub.feed("udp://:" + std::to_string(port));
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
}

TEST_F(IOUdp, DroppingTheLastHolderOfAFeedGivesUpItsPort) {
  uint16_t port = 0;
  {
    const std::shared_ptr<Feed> listener = hub.feed("udp://:0");
    ASSERT_TRUE(listener->error().empty()) << listener->error();
    port = portOf(listener->address());
    ASSERT_NE(port, 0);
  }

  // The close travels to the transport's thread, so the port comes back
  // a moment after the last holder lets go rather than within it.
  const std::string uri = "udp://:" + std::to_string(port);
  std::shared_ptr<Feed> again;
  ASSERT_TRUE(waitUntil([&] {
    again = hub.feed(uri);
    if (again->error().empty()) return true;
    again.reset();
    return false;
  }));
  EXPECT_EQ(portOf(again->address()), port);
}

TEST_F(IOUdp, TwoAsksForOneUriAnswerOneFeed) {
  const std::shared_ptr<Feed> first = hub.feed("udp://:0");
  ASSERT_TRUE(first->error().empty()) << first->error();
  const std::shared_ptr<Feed> second = hub.feed("udp://:0");
  EXPECT_EQ(first, second);
  EXPECT_EQ(first->address(), second->address());
}

}  // namespace
