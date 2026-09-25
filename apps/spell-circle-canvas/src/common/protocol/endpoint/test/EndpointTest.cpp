/** @file
 * The endpoint on a real socket: the address file written before the
 * first frame and taken back after, the definition served at /protocol,
 * a command answered over the wire only inside the hub's dispatch, an
 * event sent to the client that enabled its domain, a client attached in
 * process standing on the same dispatcher, a peer that leaves detached,
 * a client that enabled host told the endpoint is closing, and a peer
 * beyond loopback refused unless it was stated — and a stated peer that
 * is no address refused before anything is written.
 */

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sigildata/decode/Json.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>
#include <sigilprotocol/clock/ClockAgent.h>
#include <sigilprotocol/clock/ClockClient.h>
#include <sigilprotocol/definition/Definition.h>
#include <sigilprotocol/dispatch/InProcess.h>
#include <sigilprotocol/endpoint/Endpoint.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "ClockUnderTest.h"
#include "ScratchDir.h"

namespace {

namespace protocol = sigil::protocol;
using sigil::data::Json;
using namespace std::chrono_literals;

/** Polls @p ready until it holds, or gives up; the socket's work runs on
 *  its own threads, so the deadline is long enough that a loaded machine
 *  is not mistaken for a broken one. */
bool waitUntil(const std::function<bool()>& ready) {
  const auto deadline = std::chrono::steady_clock::now() + 3s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (ready()) return true;
    std::this_thread::sleep_for(1ms);
  }
  return ready();
}

sigil::io::Bytes bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  return sigil::io::Bytes(std::span(first, text.size()));
}

std::string readWhole(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/** The port out of `ws://127.0.0.1:52341/sigil`. */
uint16_t portOf(std::string_view address) {
  const size_t slash = address.find('/', address.find("://") + 3);
  const size_t colon = address.rfind(':', slash);
  return static_cast<uint16_t>(
      std::stoul(std::string(address.substr(colon + 1, slash - colon - 1))));
}

/** WHAT ONE HTTP ANSWER SAYS: its status, and its body. Zero where the
 *  connection could not be made at all. */
struct Page {
  int status = 0;
  std::string body;
};

/** ONE HTTP GET for @p path at @p host and @p port, written by hand on a
 *  plain socket: the listener underneath is the only HTTP here, and a
 *  case says exactly what it asked. The connection asks to be closed,
 *  so the end of the stream is the end of the answer. */
Page get(const std::string& host, uint16_t port, std::string_view path) {
  Page page;
  const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
  if (descriptor < 0) return page;
  timeval patience{3, 0};
  ::setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, &patience, sizeof patience);
  sockaddr_in to{};
  to.sin_family = AF_INET;
  to.sin_port = htons(port);
  ::inet_pton(AF_INET, host.c_str(), &to.sin_addr);
  if (::connect(descriptor, reinterpret_cast<sockaddr*>(&to), sizeof to) != 0) {
    ::close(descriptor);
    return page;
  }
  const std::string request = "GET " + std::string(path) +
                              " HTTP/1.1\r\nHost: " + host +
                              "\r\nConnection: close\r\n\r\n";
  ::send(descriptor, request.data(), request.size(), 0);
  std::string whole;
  char chunk[4096];
  for (ssize_t read; (read = ::recv(descriptor, chunk, sizeof chunk, 0)) > 0;)
    whole.append(chunk, static_cast<size_t>(read));
  ::close(descriptor);
  const size_t blank = whole.find("\r\n\r\n");
  if (blank == std::string::npos) return page;
  page.status = std::atoi(whole.c_str() + whole.find(' ') + 1);
  page.body = whole.substr(blank + 4);
  return page;
}

/** An IPv4 address of this machine that is not loopback; nothing where
 *  it has none, and then the cases that need one skip. */
std::optional<std::string> addressBeyondLoopback() {
  ifaddrs* interfaces = nullptr;
  if (::getifaddrs(&interfaces) != 0) return std::nullopt;
  std::optional<std::string> found;
  for (const ifaddrs* each = interfaces; each && !found;
       each = each->ifa_next) {
    if (!each->ifa_addr || each->ifa_addr->sa_family != AF_INET) continue;
    if ((each->ifa_flags & IFF_UP) == 0 || (each->ifa_flags & IFF_LOOPBACK))
      continue;
    char text[INET_ADDRSTRLEN] = {};
    const auto* four = reinterpret_cast<const sockaddr_in*>(each->ifa_addr);
    if (::inet_ntop(AF_INET, &four->sin_addr, text, sizeof text)) found = text;
  }
  ::freeifaddrs(interfaces);
  return found;
}

/** WHAT AN ENDPOINT CASE STANDS ON: a state root of its own, the host's
 *  hub and dispatcher the endpoint is put on, and a second hub a client
 *  dials it from. */
class ProtocolEndpoint : public ::testing::Test {
 protected:
  ProtocolEndpoint() { sigil::io::registerTransports(clientHub, {"ws"}); }

  protocol::Program program() const {
    protocol::Program named;
    named.name = "EndpointTest";
    named.stateRoot = state.path;
    return named;
  }

  /** A client on @p endpoint, once its first send has gone out. */
  std::shared_ptr<sigil::io::Feed> dial(const protocol::Endpoint& endpoint,
                                        std::string_view first) {
    std::shared_ptr<sigil::io::Feed> client =
        clientHub.feed(endpoint.address());
    EXPECT_TRUE(waitUntil([&] { return client->send(bytesOf(first)); }))
        << client->error();
    return client;
  }

  /** The next message @p client hears, the host's hub dispatched until
   *  it comes; nothing where none does. */
  std::optional<Json> hear(const std::shared_ptr<sigil::io::Feed>& client) {
    std::optional<Json> heard;
    waitUntil([&] {
      hostHub.dispatch();
      if (const std::optional<sigil::io::Arrival> arrival = client->receive())
        heard = sigil::data::decodeJson(arrival->bytes->asText());
      return heard.has_value();
    });
    return heard;
  }

  const sigil::test::ScratchDir state{"sigilprotocol_endpoint"};
  sigil::io::Hub hostHub;
  sigil::io::Hub clientHub;
  protocol::Dispatcher dispatcher{program()};
};

TEST_F(ProtocolEndpoint, WritesItsAddressBeforeTheFirstFrameAndTakesItBack) {
  std::string address;
  {
    const protocol::Endpoint endpoint(hostHub, dispatcher);
    ASSERT_TRUE(endpoint.listening()) << endpoint.error();
    address = endpoint.address();
    EXPECT_TRUE(address.starts_with("ws://127.0.0.1:")) << address;
    EXPECT_TRUE(address.ends_with("/sigil")) << address;
    EXPECT_NE(portOf(address), 0);
    // No dispatch has run: the file is there as the constructor returns.
    EXPECT_EQ(endpoint.addressFile(), state.path / "protocol-address");
    EXPECT_EQ(readWhole(endpoint.addressFile()), address + "\n");
  }
  EXPECT_FALSE(std::filesystem::exists(state.path / "protocol-address"));
}

TEST_F(ProtocolEndpoint, ServesTheDefinitionAtProtocol) {
  const protocol::Endpoint endpoint(hostHub, dispatcher);
  ASSERT_TRUE(endpoint.listening()) << endpoint.error();

  const Page page = get("127.0.0.1", portOf(endpoint.address()), "/protocol");
  EXPECT_EQ(page.status, 200);
  const std::span<const std::byte> definition = protocol::definition();
  EXPECT_EQ(page.body,
            std::string(reinterpret_cast<const char*>(definition.data()),
                        definition.size()));
}

TEST_F(ProtocolEndpoint, AnswersACommandOverTheWireInsideTheDispatch) {
  const protocol::Endpoint endpoint(hostHub, dispatcher);
  ASSERT_TRUE(endpoint.listening()) << endpoint.error();

  const auto client = dial(endpoint, R"({"id": 1, "method": "host.describe"})");
  // Nothing is answered until the host's hub dispatches.
  std::this_thread::sleep_for(50ms);
  EXPECT_FALSE(client->receive());
  const std::optional<Json> answer = hear(client);
  ASSERT_TRUE(answer);
  EXPECT_EQ((*answer)["id"].number(), 1);
  EXPECT_EQ((*answer)["result"]["version"]["program"].text(), "EndpointTest");
  EXPECT_EQ((*answer)["result"]["state_root"].text(), state.path.string());
  ASSERT_EQ((*answer)["result"]["attached"].size(), 1u);
  EXPECT_EQ((*answer)["result"]["attached"][0].text(),
            (*answer)["session"].text());

  // A method nobody declared is refused over the wire in the same words.
  ASSERT_TRUE(client->send(bytesOf(R"({"id": 2, "method": "clock.warp"})")));
  const std::optional<Json> refused = hear(client);
  ASSERT_TRUE(refused);
  EXPECT_EQ((*refused)["error"]["code"].text(), "methodNotFound");
}

TEST_F(ProtocolEndpoint, SendsAnEventToTheClientThatEnabledItsDomain) {
  // The agent outlives the endpoint put on its dispatcher, as a host's
  // agents do: the endpoint is let go first.
  protocol::test::ClockUnderTest clock(dispatcher);
  const protocol::Endpoint endpoint(hostHub, dispatcher);
  ASSERT_TRUE(endpoint.listening()) << endpoint.error();
  const protocol::clock::ClockEvents events(dispatcher.events());

  const auto client = dial(endpoint, R"({"id": 1, "method": "clock.enable"})");
  const std::optional<Json> enabled = hear(client);
  ASSERT_TRUE(enabled);
  ASSERT_TRUE((*enabled)["error"].null()) << sigil::data::encodeJson(*enabled);

  protocol::clock::values::BudgetExpiredEvent expired;
  expired.seconds = 4;
  EXPECT_TRUE(events.budgetExpired(expired));
  const std::optional<Json> event = hear(client);
  ASSERT_TRUE(event);
  EXPECT_EQ((*event)["method"].text(), "clock.budgetExpired");
  EXPECT_EQ((*event)["session"].text(), (*enabled)["session"].text());
  EXPECT_EQ((*event)["parameters"]["seconds"].number(), 4);
}

TEST_F(ProtocolEndpoint, AClientInProcessStandsOnTheSameDispatcher) {
  // One dispatcher, the agents mounted on it once: a client in the same
  // process and one on the socket are both its sessions, and an event
  // reaches each that enabled its domain, whichever way it came.
  protocol::test::ClockUnderTest clock(dispatcher);
  const protocol::Endpoint endpoint(hostHub, dispatcher);
  ASSERT_TRUE(endpoint.listening()) << endpoint.error();
  const protocol::InProcess inside(dispatcher);
  std::vector<double> heard;
  const protocol::clock::ClockClient watcher(inside.caller());
  watcher.onBudgetExpired(
      [&](const auto& event) { heard.push_back(event.seconds); });
  watcher.enable([](protocol::Answer<protocol::values::Empty> answer) {
    ASSERT_TRUE(answer) << answer.error().message;
  });

  const auto client = dial(endpoint, R"({"id": 1, "method": "clock.enable"})");
  ASSERT_TRUE(hear(client));
  ASSERT_TRUE(client->send(bytesOf(R"({"id": 2, "method": "host.describe"})")));
  const std::optional<Json> described = hear(client);
  ASSERT_TRUE(described);
  ASSERT_EQ((*described)["result"]["attached"].size(), 2u)
      << sigil::data::encodeJson(*described);
  EXPECT_EQ((*described)["result"]["attached"][0].text(), inside.session());

  protocol::clock::values::BudgetExpiredEvent expired;
  expired.seconds = 6;
  EXPECT_TRUE(protocol::clock::ClockEvents(dispatcher.events())
                  .budgetExpired(expired));
  EXPECT_EQ(heard, std::vector<double>{6});
  const std::optional<Json> event = hear(client);
  ASSERT_TRUE(event);
  EXPECT_EQ((*event)["parameters"]["seconds"].number(), 6);
}

TEST_F(ProtocolEndpoint, LettingItGoTellsAClientThatEnabledHostWhy) {
  auto endpoint = std::make_unique<protocol::Endpoint>(hostHub, dispatcher);
  ASSERT_TRUE(endpoint->listening()) << endpoint->error();
  const protocol::InProcess inside(dispatcher);

  const auto client = dial(*endpoint, R"({"id": 1, "method": "host.enable"})");
  const std::optional<Json> enabled = hear(client);
  ASSERT_TRUE(enabled);
  ASSERT_TRUE((*enabled)["error"].null()) << sigil::data::encodeJson(*enabled);

  endpoint.reset();
  const std::optional<Json> told = hear(client);
  ASSERT_TRUE(told);
  EXPECT_EQ((*told)["method"].text(), "host.detached");
  EXPECT_EQ((*told)["parameters"]["reason"].text(), "the endpoint is closing");
  // Only its own clients go with it: the one attached in process stays.
  EXPECT_EQ(dispatcher.sessions(), std::vector<std::string>{inside.session()});
}

TEST_F(ProtocolEndpoint, APeerThatLeavesIsDetached) {
  protocol::Endpoint endpoint(hostHub, dispatcher);
  ASSERT_TRUE(endpoint.listening()) << endpoint.error();

  auto client = dial(endpoint, R"({"id": 1, "method": "host.version"})");
  ASSERT_TRUE(hear(client));
  EXPECT_EQ(dispatcher.sessions().size(), 1u);
  client->close();
  client.reset();
  EXPECT_TRUE(waitUntil([&] {
    hostHub.dispatch();
    return dispatcher.sessions().empty();
  }));
}

TEST_F(ProtocolEndpoint, WithoutAStateRootItHoldsNoPort) {
  protocol::Dispatcher rootless;
  const protocol::Endpoint endpoint(hostHub, rootless);
  EXPECT_FALSE(endpoint.listening());
  EXPECT_TRUE(endpoint.error().starts_with("endpoint: ")) << endpoint.error();
  EXPECT_TRUE(endpoint.address().empty());
}

TEST_F(ProtocolEndpoint, AStatedPeerThatIsNoAddressOpensNothing) {
  // A stated peer rides in the listener's query, so one that could say
  // more there — where the pages stand, say — is refused naming it.
  protocol::EndpointPolicy policy;
  policy.statedPeers = {"10.0.0.5&pages=/"};
  const protocol::Endpoint endpoint(hostHub, dispatcher, policy);
  EXPECT_FALSE(endpoint.listening());
  EXPECT_TRUE(endpoint.error().starts_with("endpoint: ")) << endpoint.error();
  EXPECT_NE(endpoint.error().find("10.0.0.5&pages=/"), std::string::npos)
      << endpoint.error();
  EXPECT_FALSE(std::filesystem::exists(state.path / "protocol-address"));
}

TEST_F(ProtocolEndpoint, CannotBeReachedBeyondLoopbackUnlessAPeerIsStated) {
  const std::optional<std::string> beyond = addressBeyondLoopback();
  if (!beyond) GTEST_SKIP() << "this machine has no address beyond loopback";

  // Bound to loopback, the port is not there at all from another
  // interface.
  {
    const protocol::Endpoint endpoint(hostHub, dispatcher);
    ASSERT_TRUE(endpoint.listening()) << endpoint.error();
    EXPECT_EQ(get(*beyond, portOf(endpoint.address()), "/protocol").status, 0);
  }
  // Stating another peer holds every interface, and a peer neither on
  // loopback nor stated is refused naming it, while loopback still
  // reaches it.
  {
    protocol::EndpointPolicy policy;
    policy.statedPeers = {"192.0.2.10"};
    const protocol::Endpoint endpoint(hostHub, dispatcher, policy);
    ASSERT_TRUE(endpoint.listening()) << endpoint.error();
    const uint16_t port = portOf(endpoint.address());
    const Page refused = get(*beyond, port, "/protocol");
    EXPECT_EQ(refused.status, 403);
    EXPECT_NE(refused.body.find(*beyond), std::string::npos) << refused.body;
    EXPECT_EQ(get("127.0.0.1", port, "/protocol").status, 200);
  }
  // The stated peer itself is let in.
  {
    protocol::EndpointPolicy policy;
    policy.statedPeers = {*beyond};
    const protocol::Endpoint endpoint(hostHub, dispatcher, policy);
    ASSERT_TRUE(endpoint.listening()) << endpoint.error();
    EXPECT_EQ(get(*beyond, portOf(endpoint.address()), "/protocol").status,
              200);
  }
}

}  // namespace
