/** @file
 * The QUIC transport: the port a listening feed takes, the certificate
 * it has to name to take one at all, the connection a calling feed
 * opens on it, the messages that go each way between the two ends
 * standing in this one binary, the connection each arrival names and the
 * one of several a named send reaches, the datagram that crosses the
 * same connection when a URI asks for one, what a URI nobody can open
 * leaves on its feed, and the port a feed gives back when the last
 * holder lets go.
 */

#include <gtest/gtest.h>
#include <openssl/bio.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "ScratchDir.h"

namespace {

using boost::asio::ip::udp;
using sigil::io::Arrival;
using sigil::io::Bytes;
using sigil::io::Feed;
using sigil::io::Hub;
using namespace std::chrono_literals;

constexpr std::string_view kScheme = "quic://";

/** How long a call is given to say it reached nothing. A call may end
 *  either way — at once, where the machine answers that nothing is
 *  listening, or at the transport's own ten-second bound, where the
 *  connection is swallowed — so the deadline outlasts the slower one and
 *  leaves room for a loaded machine besides. */
constexpr std::chrono::seconds kBeyondTheBound{25};

/** Polls @p ready until it holds, or gives up. The work it waits for
 *  runs on the quic library's own workers, so there is nothing here to
 *  pump — only a moment to give it, and a deadline long enough that a
 *  loaded machine is not mistaken for a broken connection. */
bool waitUntil(const std::function<bool()>& ready,
               std::chrono::seconds within = 5s) {
  const auto deadline = std::chrono::steady_clock::now() + within;
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

/** Hands @p message to @p feed EXACTLY ONCE, as soon as the feed will
 *  take it. A send is taken as soon as there is a connection to open a
 *  stream on, and one made before that answers false, so it is offered
 *  until it is taken — and once, a message being one message however
 *  early it was first offered. */
bool sendOnce(const std::shared_ptr<Feed>& feed, std::string_view message) {
  return waitUntil([&] { return feed->send(bytesOf(message)); });
}

/** A SELF-SIGNED PAIR FOR ONE CASE TO ANSWER WITH, written into @p
 *  scratch as cert.pem and key.pem.
 *
 *  A QUIC port answers for itself or it answers nobody, and nobody signs
 *  for a machine in a test, so the pair is made here: an elliptic-curve
 *  key on the P-256 curve, and a certificate naming localhost that
 *  vouches for itself. It is the shape a machine on a stage carries, and
 *  the call that reaches it is opened the way one reaches such a
 *  machine. False where the pair could not be made or written, which is
 *  a case that cannot run rather than one that failed. */
bool standCredentials(const sigil::test::ScratchDir& scratch) {
  EVP_PKEY* key = EVP_EC_gen("P-256");
  if (!key) return false;
  X509* certificate = X509_new();
  if (!certificate) {
    EVP_PKEY_free(key);
    return false;
  }
  bool made = false;
  do {
    // Version 3, which is what the number 2 spells here.
    if (X509_set_version(certificate, 2) != 1) break;
    if (ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1) != 1) break;
    if (!X509_gmtime_adj(X509_getm_notBefore(certificate), 0)) break;
    if (!X509_gmtime_adj(X509_getm_notAfter(certificate), 60 * 60)) break;
    if (X509_set_pubkey(certificate, key) != 1) break;
    X509_NAME* const named = X509_get_subject_name(certificate);
    if (X509_NAME_add_entry_by_txt(
            named, "CN", MBSTRING_ASC,
            reinterpret_cast<const unsigned char*>("localhost"), -1, -1,
            0) != 1)
      break;
    // Signed by itself, which is what makes it self-signed.
    if (X509_set_issuer_name(certificate, named) != 1) break;
    if (X509_sign(certificate, key, EVP_sha256()) == 0) break;

    const std::string certificatePath = (scratch.path / "cert.pem").string();
    const std::string keyPath = (scratch.path / "key.pem").string();
    BIO* out = BIO_new_file(certificatePath.c_str(), "wb");
    if (!out) break;
    const int wroteCertificate = PEM_write_bio_X509(out, certificate);
    BIO_free(out);
    if (wroteCertificate != 1) break;
    out = BIO_new_file(keyPath.c_str(), "wb");
    if (!out) break;
    const int wroteKey = PEM_write_bio_PrivateKey(out, key, nullptr, nullptr, 0,
                                                  nullptr, nullptr);
    BIO_free(out);
    if (wroteKey != 1) break;
    made = true;
  } while (false);
  X509_free(certificate);
  EVP_PKEY_free(key);
  return made;
}

/** The port out of an address a feed reports. An IPv6 address is
 *  bracketed, which leaves the port after the last colon of it. */
uint16_t portOf(const std::string& address) {
  const size_t colon = address.rfind(':');
  if (colon == std::string::npos) return 0;
  return static_cast<uint16_t>(std::stoul(address.substr(colon + 1)));
}

/** A UDP port that was bound and given straight back, so nothing answers
 *  there: what a call to a port nobody is holding reaches. */
uint16_t portNobodyHolds(boost::asio::io_context& context) {
  udp::socket holder(context, udp::endpoint(udp::v4(), 0));
  return holder.local_endpoint().port();
}

/** WHAT A CASE NEEDS BEFORE IT CAN OPEN ANYTHING: a hub taught the one
 *  scheme, which takes both ends of it — the shape of the URI is what
 *  says which end a feed is, so there is no second registration and no
 *  order to keep — and the pair a port answers with, standing in a
 *  directory this case owns. */
class IOQuic : public ::testing::Test {
 protected:
  IOQuic() : scratch("sigilio_quic") {
    sigil::io::registerQuic(hub);
    standing = standCredentials(scratch);
  }

  /** The URI a feed holds a port at: any free port, answered for with
   *  the pair this case made. */
  std::string listening(bool datagrams = false) const {
    std::string uri = "quic://:0?cert=" + (scratch.path / "cert.pem").string() +
                      "&key=" + (scratch.path / "key.pem").string();
    if (datagrams) uri += "&datagrams=1";
    return uri;
  }

  /** The URI a call reaches @p port at: the loopback, and no certificate
   *  checked, which is what a self-signed pair on a stage is reached
   *  with. */
  static std::string calling(uint16_t port, bool datagrams = false) {
    std::string uri =
        "quic://127.0.0.1:" + std::to_string(port) + "?insecure=1";
    if (datagrams) uri += "&datagrams=1";
    return uri;
  }

  Hub hub;
  boost::asio::io_context context;
  sigil::test::ScratchDir scratch;
  /** Whether the pair could be made at all. Every case that needs a port
   *  to answer asks this first. */
  bool standing = false;
};

TEST_F(IOQuic, AListeningFeedSaysWhichPortItTook) {
  ASSERT_TRUE(standing) << "no certificate could be made on this machine";
  const std::shared_ptr<Feed> listener = hub.feed(listening());
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  // Every interface of both families is one dual-stack socket, and the
  // query is the door's own arrangement rather than an address anybody
  // reaches, so it is not in what the feed reports.
  EXPECT_TRUE(listener->address().starts_with("quic://[::]:"))
      << listener->address();
  EXPECT_NE(portOf(listener->address()), 0);
}

TEST_F(IOQuic, ACallsMessageArrivesNamingTheConnectionItCameInOn) {
  ASSERT_TRUE(standing) << "no certificate could be made on this machine";
  const std::shared_ptr<Feed> listener = hub.feed(listening());
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const std::string url = calling(portOf(listener->address()));
  const std::shared_ptr<Feed> caller = hub.feed(url);
  ASSERT_TRUE(caller->error().empty()) << caller->error();

  ASSERT_TRUE(sendOnce(caller, "a scene arrives")) << caller->error();
  ASSERT_TRUE(waitUntil([&] { return listener->generation() == 1u; }))
      << caller->error();
  EXPECT_EQ(listener->latest()->asText(), "a scene arrives");

  const std::optional<Arrival> heard = listener->receive();
  ASSERT_TRUE(heard.has_value());
  const size_t number = heard->from.rfind('#');
  ASSERT_NE(number, std::string::npos) << heard->from;
  // The first connection this feed took is the first one it numbered,
  // and the address in front of that number is the caller's own, with a
  // port behind it.
  EXPECT_EQ(heard->from.substr(number), "#1") << heard->from;
  const std::string where = heard->from.substr(0, number);
  EXPECT_TRUE(where.starts_with(kScheme)) << where;
  EXPECT_NE(where.find(':', kScheme.size()), std::string::npos) << where;
}

TEST_F(IOQuic, TheListenersSendReachesTheCallThatOpenedTheConnection) {
  ASSERT_TRUE(standing) << "no certificate could be made on this machine";
  const std::shared_ptr<Feed> listener = hub.feed(listening());
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const std::string url = calling(portOf(listener->address()));
  const std::shared_ptr<Feed> caller = hub.feed(url);
  ASSERT_TRUE(caller->error().empty()) << caller->error();

  // A send goes out to every connection standing, so one has to have
  // reached the listener before it: a message of the caller's the
  // listener has taken is what says one has.
  ASSERT_TRUE(sendOnce(caller, "here")) << caller->error();
  ASSERT_TRUE(waitUntil([&] { return listener->generation() == 1u; }))
      << caller->error();

  EXPECT_TRUE(listener->send(bytesOf("out to every caller")));
  ASSERT_TRUE(waitUntil([&] { return caller->latest() != nullptr; }))
      << caller->error();
  EXPECT_EQ(caller->latest()->asText(), "out to every caller");

  const std::optional<Arrival> back = caller->receive();
  ASSERT_TRUE(back.has_value());
  // A call holds the one connection it opened, and every message it
  // takes is named for the end it reached — the authority alone, the
  // query being the call's own arrangement.
  EXPECT_EQ(back->from,
            "quic://127.0.0.1:" + std::to_string(portOf(listener->address())));
}

TEST_F(IOQuic, SendToReachesTheOneConnectionItNamesAndNoOther) {
  ASSERT_TRUE(standing) << "no certificate could be made on this machine";
  const std::shared_ptr<Feed> listener = hub.feed(listening());
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const std::string url = calling(portOf(listener->address()));

  // A hub answers one feed per URI, so the second caller is opened on a
  // hub of its own: two feeds on one URI would be one connection drained
  // by two readers, which is not two callers.
  Hub elsewhere;
  sigil::io::registerQuic(elsewhere);
  const std::shared_ptr<Feed> first = hub.feed(url);
  ASSERT_TRUE(first->error().empty()) << first->error();
  const std::shared_ptr<Feed> second = elsewhere.feed(url);
  ASSERT_TRUE(second->error().empty()) << second->error();

  // Each caller says which one it is, and the arrival it says it in
  // names the connection that caller is answered on.
  ASSERT_TRUE(sendOnce(first, "first")) << first->error();
  ASSERT_TRUE(sendOnce(second, "second")) << second->error();
  ASSERT_TRUE(waitUntil([&] { return listener->generation() == 2u; }))
      << listener->error();

  std::string answering;
  while (const std::optional<Arrival> arrival = listener->receive())
    if (arrival->bytes->asText() == "first") answering = arrival->from;
  ASSERT_FALSE(answering.empty());

  EXPECT_TRUE(listener->sendTo(answering, bytesOf("to you alone")));
  ASSERT_TRUE(waitUntil([&] { return first->generation() >= 1u; }));
  EXPECT_EQ(first->latest()->asText(), "to you alone");

  // What the OTHER caller reads first is the broadcast that came after,
  // which is what says the message before it went to one connection and
  // not to every connection standing.
  EXPECT_TRUE(listener->send(bytesOf("out to every caller")));
  ASSERT_TRUE(waitUntil([&] { return second->generation() >= 1u; }));
  const std::optional<Arrival> opening = second->receive();
  ASSERT_TRUE(opening.has_value());
  EXPECT_EQ(opening->bytes->asText(), "out to every caller");
}

TEST_F(IOQuic, ADatagramCrossesTheSameConnection) {
  ASSERT_TRUE(standing) << "no certificate could be made on this machine";
  // THE LISTENER'S URI ASKS FOR NOTHING: a datagram is always taken,
  // whatever a door sends, so one end may send them while the other end
  // sends streams.
  const std::shared_ptr<Feed> listener = hub.feed(listening());
  ASSERT_TRUE(listener->error().empty()) << listener->error();
  const std::shared_ptr<Feed> caller =
      hub.feed(calling(portOf(listener->address()), /*datagrams=*/true));
  ASSERT_TRUE(caller->error().empty()) << caller->error();

  // A datagram is unreliable and the path says how large one may be only
  // once it is up, so the send is made until one lands: a send before
  // then is false, and one that went may still be dropped.
  ASSERT_TRUE(waitUntil([&] {
    caller->send(bytesOf("one packet, no promises"));
    return listener->generation() >= 1u;
  })) << caller->error();
  EXPECT_EQ(listener->latest()->asText(), "one packet, no promises");

  const std::optional<Arrival> heard = listener->receive();
  ASSERT_TRUE(heard.has_value());
  // A datagram arrives naming the connection it crossed, exactly as a
  // stream does.
  EXPECT_NE(heard->from.rfind('#'), std::string::npos) << heard->from;

  // A message larger than one packet on this path carries is not a
  // datagram at all, and the door says so rather than cutting it in
  // half.
  EXPECT_FALSE(caller->send(bytesOf(std::string(70000, 'x'))));
}

TEST_F(IOQuic, ACallToAPortNobodyHoldsSaysItReachedNothing) {
  const uint16_t port = portNobodyHolds(context);
  ASSERT_NE(port, 0);

  const std::shared_ptr<Feed> feed = hub.feed(calling(port));
  // A machine that answers at once that nothing is listening on that
  // port ends the call there and then; one that swallows the connection
  // instead is what the bound on reaching an end answers for. The
  // deadline here covers the slower of the two.
  EXPECT_TRUE(
      waitUntil([&] { return !feed->error().empty(); }, kBeyondTheBound));
  // The sentence names the end that was never reached, and names it
  // WITHOUT the query: what a call reaches is an authority, and whether
  // this end checks a certificate is its own arrangement.
  const std::string called = "quic://127.0.0.1:" + std::to_string(port);
  EXPECT_TRUE(feed->error().starts_with("could not reach " + called + ":"))
      << feed->error();
  EXPECT_EQ(feed->latest(), nullptr);
}

TEST_F(IOQuic, AListeningUriThatNamesNoCertificateOpensNothingAndSaysWhy) {
  // A quic connection is encrypted or it is nothing, so a port that
  // cannot answer for itself is a port nobody may hold.
  const std::shared_ptr<Feed> bare = hub.feed("quic://:0");
  EXPECT_FALSE(bare->error().empty());
  EXPECT_TRUE(bare->address().empty());
  EXPECT_EQ(bare->latest(), nullptr);

  // Half a pair is no pair.
  const std::shared_ptr<Feed> half =
      hub.feed("quic://:0?cert=" + (scratch.path / "cert.pem").string());
  EXPECT_FALSE(half->error().empty());
  EXPECT_TRUE(half->address().empty());

  // A pair that names files nobody wrote is no pair either.
  const std::shared_ptr<Feed> missing =
      hub.feed("quic://:0?cert=" + (scratch.path / "nobody.pem").string() +
               "&key=" + (scratch.path / "nothing.pem").string());
  EXPECT_FALSE(missing->error().empty());
  EXPECT_TRUE(missing->address().empty());
}

TEST_F(IOQuic, DroppingTheLastHolderOfAListeningFeedGivesUpItsPort) {
  ASSERT_TRUE(standing) << "no certificate could be made on this machine";
  uint16_t port = 0;
  {
    const std::shared_ptr<Feed> listener = hub.feed(listening());
    ASSERT_TRUE(listener->error().empty()) << listener->error();
    port = portOf(listener->address());
    ASSERT_NE(port, 0);
  }

  // The shutdown runs where the last holder was let go, so the port is
  // back by the time that returns — but a moment is given for it either
  // way, a port being the system's to hand out again.
  const std::string uri = "quic://:" + std::to_string(port) +
                          "?cert=" + (scratch.path / "cert.pem").string() +
                          "&key=" + (scratch.path / "key.pem").string();
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
