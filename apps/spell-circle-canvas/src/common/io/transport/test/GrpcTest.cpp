/** @file
 * The gRPC transport: the port and method a server feed binds, the call
 * a client feed opens on it, the messages that go each way between the
 * two ends standing in this one binary, the caller each arrival names
 * and the one of several a named send reaches, what a URI nobody can
 * open leaves on its feed, and the port a feed gives back when the last
 * holder lets go.
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

constexpr std::string_view kScheme = "grpc://";

/** Polls @p ready until it holds, or gives up. The work it waits for
 *  runs on gRPC's own threads, so there is nothing here to pump — only a
 *  moment to give it, and a deadline long enough that a loaded machine
 *  is not mistaken for a broken stream. */
bool waitUntil(const std::function<bool()>& ready) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
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

/** The port out of an address a feed reports. The method stands behind
 *  the port, so the number is taken out of the authority alone; an IPv6
 *  address is bracketed, which leaves the port after the last colon of
 *  it. */
uint16_t portOf(const std::string& address) {
  const size_t method = address.find('/', kScheme.size());
  const std::string authority = address.substr(0, method);
  const size_t colon = authority.rfind(':');
  if (colon == std::string::npos) return 0;
  return static_cast<uint16_t>(std::stoul(authority.substr(colon + 1)));
}

/** A port that was bound and given straight back, so nothing answers
 *  there: what a call to a server nobody is holding reaches. */
uint16_t portNobodyHolds(boost::asio::io_context& context) {
  tcp::acceptor holder(context, tcp::endpoint(tcp::v4(), 0));
  return holder.local_endpoint().port();
}

/** WHAT A CASE NEEDS BEFORE IT CAN OPEN ANYTHING: a hub taught the one
 *  scheme, which takes both ends of it — the shape of the URI is what
 *  says which end a feed is, so there is no second registration and no
 *  order to keep. */
class IOGrpc : public ::testing::Test {
 protected:
  IOGrpc() { sigil::io::registerTransports(hub, {"grpc"}); }

  /** The URL a caller reaches @p server at: the loopback, the port it
   *  bound, and the method it answers to. */
  static std::string urlOf(const Feed& server) {
    const std::string address = server.state().localAddress;
    const size_t method = address.find('/', kScheme.size());
    return "grpc://127.0.0.1:" + std::to_string(portOf(address)) +
           (method == std::string::npos ? std::string()
                                        : address.substr(method));
  }

  Hub hub;
  boost::asio::io_context context;
};

TEST_F(IOGrpc, AServerFeedSaysWhichPortAndMethodItBound) {
  const Feed server = hub.listen("grpc://:0/Sky/Watch");
  ASSERT_TRUE(server.state().error.empty()) << server.state().error;
  // Every interface of both families is one dual-stack listener, and the
  // method stands behind the port because it is part of what a caller
  // reaches.
  EXPECT_TRUE(server.state().localAddress.starts_with("grpc://[::]:"))
      << server.state().localAddress;
  EXPECT_TRUE(server.state().localAddress.ends_with("/Sky/Watch")) << server.state().localAddress;
  EXPECT_NE(portOf(server.state().localAddress), 0);
}

TEST_F(IOGrpc, ACallersMessageArrivesOnTheServerNamingTheCallItCameIn) {
  const Feed server = hub.listen("grpc://:0/Sky/Watch");
  ASSERT_TRUE(server.state().error.empty()) << server.state().error;
  const Feed caller = hub.listen(urlOf(server));
  ASSERT_TRUE(caller.state().error.empty()) << caller.state().error;

  // A message handed over before the stream is up waits on it rather
  // than going nowhere, so one send is one message however early it was
  // made.
  EXPECT_TRUE(caller.send(bytesOf("a scene arrives")));
  ASSERT_TRUE(waitUntil([&] { return server.latest().has_value(); }))
      << server.state().error;
  EXPECT_EQ(server.latest()->payload->asText(), "a scene arrives");

  const std::optional<Message> heard = server.receive();
  ASSERT_TRUE(heard.has_value());
  const size_t number = heard->sender().rfind('#');
  ASSERT_NE(number, std::string::npos) << heard->sender();
  // The first call this feed took is the first call it numbered, and the
  // address in front of that number is the caller's own, with a port
  // behind it.
  EXPECT_EQ(heard->sender().substr(number), "#1") << heard->sender();
  const std::string where = heard->sender().substr(0, number);
  EXPECT_TRUE(where.starts_with(kScheme)) << where;
  EXPECT_NE(where.find(':', kScheme.size()), std::string::npos) << where;
}

TEST_F(IOGrpc, TheServersSendReachesTheCallerThatOpenedTheStream) {
  const Feed server = hub.listen("grpc://:0/Sky/Watch");
  ASSERT_TRUE(server.state().error.empty()) << server.state().error;
  const std::string url = urlOf(server);
  const Feed caller = hub.listen(url);
  ASSERT_TRUE(caller.state().error.empty()) << caller.state().error;

  // A send goes out to every call standing, so the call has to have
  // reached the server before it: a message of the caller's the server
  // has taken is what says it has.
  EXPECT_TRUE(caller.send(bytesOf("here")));
  ASSERT_TRUE(waitUntil([&] { return server.state().revision == 1u; }))
      << server.state().error;

  EXPECT_TRUE(server.send(bytesOf("out to every caller")));
  ASSERT_TRUE(waitUntil([&] { return caller.latest().has_value(); }));
  EXPECT_EQ(caller.latest()->payload->asText(), "out to every caller");

  const std::optional<Message> back = caller.receive();
  ASSERT_TRUE(back.has_value());
  // A client has the one peer it called, and every message it takes is
  // named for it.
  EXPECT_EQ(back->sender(), url);
}

TEST_F(IOGrpc, SendToReachesTheOneCallerItNamesAndNoOther) {
  const Feed server = hub.listen("grpc://:0/Sky/Watch");
  ASSERT_TRUE(server.state().error.empty()) << server.state().error;
  const std::string url = urlOf(server);

  // A hub answers one feed per URI, so the second caller is opened on a
  // hub of its own: two feeds on one URI would be one call drained by
  // two readers, which is not two callers.
  Hub elsewhere;
  sigil::io::registerTransports(elsewhere, {"grpc"});
  const Feed first = hub.listen(url);
  ASSERT_TRUE(first.state().error.empty()) << first.state().error;
  const Feed second = elsewhere.listen(url);
  ASSERT_TRUE(second.state().error.empty()) << second.state().error;

  // Each caller says which one it is, and the arrival it says it in
  // names the call that caller is answered on.
  EXPECT_TRUE(first.send(bytesOf("first")));
  EXPECT_TRUE(second.send(bytesOf("second")));
  ASSERT_TRUE(waitUntil([&] { return server.state().revision == 2u; }))
      << server.state().error;

  std::string answering;
  while (const std::optional<Message> arrival = server.receive())
    if (arrival->payload->asText() == "first") answering = arrival->sender();
  ASSERT_FALSE(answering.empty());

  EXPECT_TRUE(server.send(bytesOf("to you alone"), {.to = answering}));
  ASSERT_TRUE(waitUntil([&] { return first.state().revision == 1u; }));
  EXPECT_EQ(first.latest()->payload->asText(), "to you alone");

  // What the OTHER caller reads first is the broadcast that came after,
  // which is what says the message before it went to one call and not to
  // every call standing.
  EXPECT_TRUE(server.send(bytesOf("out to every caller")));
  ASSERT_TRUE(waitUntil([&] { return second.state().revision == 1u; }));
  const std::optional<Message> opening = second.receive();
  ASSERT_TRUE(opening.has_value());
  EXPECT_EQ(opening->payload->asText(), "out to every caller");
}

TEST_F(IOGrpc, ACallNobodyIsHoldingIsNobodyToAnswer) {
  const Feed server = hub.listen("grpc://:0/Sky/Watch");
  ASSERT_TRUE(server.state().error.empty()) << server.state().error;

  // A door that holds its own calls can say a name reaches none of
  // them, which is what a caller that never arrived looks like from
  // here.
  EXPECT_FALSE(server.send(bytesOf("nobody"), {.to = "grpc://127.0.0.1:1#9"}));
  EXPECT_NE(server.state().readiness, sigil::io::ReadyState::Closed);
}

TEST_F(IOGrpc, AServerNobodyIsHoldingLeavesTheReasonOnTheFeed) {
  const uint16_t port = portNobodyHolds(context);
  ASSERT_NE(port, 0);

  const Feed feed =
      hub.listen("grpc://127.0.0.1:" + std::to_string(port) + "/Sky/Watch");
  // The connecting is gRPC's own, so the sentence saying the server was
  // not reached stands on the feed a moment after it is asked for rather
  // than within the ask. A connection nobody takes is refused at once
  // and never waits the bound out.
  EXPECT_TRUE(waitUntil([&] { return !feed.state().error.empty(); }));
  EXPECT_FALSE(feed.latest().has_value());
}

TEST_F(IOGrpc, AServerThatGoesAwayEndedRatherThanNeverHavingBeenReached) {
  Feed server = hub.listen("grpc://:0/Sky/Watch");
  ASSERT_TRUE(server.state().error.empty()) << server.state().error;
  const std::string url = urlOf(server);

  Hub elsewhere;
  sigil::io::registerTransports(elsewhere, {"grpc"});
  const Feed caller = elsewhere.listen(url);
  ASSERT_TRUE(caller.state().error.empty()) << caller.state().error;
  EXPECT_TRUE(caller.send(bytesOf("here")));
  ASSERT_TRUE(waitUntil([&] { return server.state().revision == 1u; }))
      << server.state().error;

  // Letting the server go cancels the call standing on it, which is what
  // the caller is left to read.
  server = {};
  ASSERT_TRUE(
      waitUntil([&] { return !caller.state().error.empty() || (caller.state().readiness == sigil::io::ReadyState::Closed); }));
  // WHICHEVER WAY THE ENDING LANDED — the status the server sent as it
  // was cancelled, or the transport going out from under it — a server
  // that answered and then went away is never worded as one that was
  // not there.
  EXPECT_FALSE(caller.state().error.starts_with("could not reach"))
      << caller.state().error;
  if (!caller.state().error.empty())
    EXPECT_TRUE(caller.state().error.starts_with(url + " ended:"))
        << caller.state().error;
}

TEST_F(IOGrpc, AUriThatNamesNoMethodOpensNothingAndSaysWhy) {
  // Both ends name the method, so a URI carrying a service and nothing
  // after it is half an address and opens neither end.
  const Feed holding = hub.listen("grpc://:0/Sky");
  EXPECT_FALSE(holding.state().error.empty());
  EXPECT_TRUE(holding.state().localAddress.empty());
  EXPECT_FALSE(holding.latest().has_value());

  const Feed calling = hub.listen("grpc://127.0.0.1:50051");
  EXPECT_FALSE(calling.state().error.empty());
  EXPECT_TRUE(calling.state().localAddress.empty());
}

TEST_F(IOGrpc, DroppingTheLastHolderOfAServerFeedGivesUpItsPort) {
  uint16_t port = 0;
  {
    const Feed server = hub.listen("grpc://:0/Sky/Watch");
    ASSERT_TRUE(server.state().error.empty()) << server.state().error;
    port = portOf(server.state().localAddress);
    ASSERT_NE(port, 0);
  }

  // The shutdown runs where the last holder was let go, so the port is
  // back by the time that returns — but a moment is given for it either
  // way, a port being the system's to hand out again.
  const std::string uri = "grpc://:" + std::to_string(port) + "/Sky/Watch";
  Feed again;
  ASSERT_TRUE(waitUntil([&] {
    again = hub.listen(uri);
    if (again.state().error.empty()) return true;
    again = {};
    return false;
  }));
  EXPECT_EQ(portOf(again.state().localAddress), port);
}

}  // namespace
