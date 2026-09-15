/** @file What the Qt receiver's door reports when it opens, what a drain
 *  hands the scene model, and what a closed door leaves behind. */

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

#include "NetworkManager.h"
#include "SpellCircleModel.h"
#include "SpellCircle_generated.h"

namespace {

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

void ensureApplication() {
  static int argc = 1;
  static char name[] = "spellcircle_qt_test";
  static char* argv[] = {name, nullptr};
  static QCoreApplication application(argc, argv);
}

/** One socket bound the way the UDP transport binds its own: a dual-stack
 *  IPv6 socket, so a port this holds is a port the receiver cannot take.
 *  -1 when the bind failed. */
int bindDualStack(uint16_t port) {
  const int handle = ::socket(AF_INET6, SOCK_DGRAM, 0);
  if (handle < 0) return -1;
  int both = 0;
  ::setsockopt(handle, IPPROTO_IPV6, IPV6_V6ONLY, &both, sizeof(both));
  sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = htons(port);
  if (::bind(handle, reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) != 0) {
    ::close(handle);
    return -1;
  }
  return handle;
}

/** The port a socket was given. */
uint16_t portOf(int handle) {
  sockaddr_in6 address{};
  socklen_t size = sizeof(address);
  if (::getsockname(handle, reinterpret_cast<sockaddr*>(&address), &size) != 0)
    return 0;
  return ntohs(address.sin6_port);
}

class SpellCircleQt : public ::testing::Test {
 protected:
  SpellCircleQt() { ensureApplication(); }

  /** Reads the receiver's door and runs the event loop until @p complete
   *  holds. The datagram lands on the transport's own thread, so what
   *  this waits for is the drain, which is the call under test. */
  bool pumpUntil(NetworkManager& network,
                 const std::function<bool()>& complete) {
    const auto deadline = Clock::now() + 2s;
    do {
      network.readArrivals();
      QCoreApplication::processEvents(QEventLoop::AllEvents);
      if (complete()) return true;
      std::this_thread::sleep_for(1ms);
    } while (Clock::now() < deadline);
    return complete();
  }

  /** A port nobody holds: one taken the transport's way and given back. */
  uint16_t availablePort() {
    const int handle = bindDualStack(0);
    if (handle < 0) return 0;
    const uint16_t port = portOf(handle);
    ::close(handle);
    return port;
  }

  /** One datagram to a port on loopback, from a socket of its own;
   *  answers the port that socket was given, which is what the arrival
   *  should name it by. */
  uint16_t sendTo(uint16_t port, const QByteArray& payload) {
    const int handle = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (handle < 0) return 0;
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    destination.sin_port = htons(port);
    ::sendto(handle, payload.constData(), static_cast<size_t>(payload.size()),
             0, reinterpret_cast<const sockaddr*>(&destination),
             sizeof(destination));
    sockaddr_in bound{};
    socklen_t size = sizeof(bound);
    ::getsockname(handle, reinterpret_cast<sockaddr*>(&bound), &size);
    ::close(handle);
    return ntohs(bound.sin_port);
  }
};

QByteArray circleScene() {
  flatbuffers::FlatBufferBuilder builder;
  const SpellCircle::Vec2 center(120, 240);
  const auto circle =
      SpellCircle::CreateCircleDirect(builder, &center, "ring", 80);
  const std::vector<flatbuffers::Offset<SpellCircle::Circle>> circles{circle};
  SpellCircle::FinishSceneBuffer(
      builder, SpellCircle::CreateSceneDirect(builder, &circles));
  return {reinterpret_cast<const char*>(builder.GetBufferPointer()),
          static_cast<qsizetype>(builder.GetSize())};
}

TEST_F(SpellCircleQt, APortSomebodyElseHoldsIsReportedWhenTheDoorIsOpened) {
  const int holder = bindDualStack(0);
  ASSERT_GE(holder, 0);
  const uint16_t port = portOf(holder);
  ASSERT_NE(port, 0);

  NetworkManager network(port);
  network.start();
  // The bind happens inside start(), so its outcome is readable the
  // moment it returns and nothing has to be pumped for.
  EXPECT_FALSE(network.listening());
  EXPECT_TRUE(network.statusText().contains(QString::number(port)));
  EXPECT_TRUE(network.statusText().contains("failed"));
  ::close(holder);
}

TEST_F(SpellCircleQt, APortChangeReopensTheDoorAndTheStatusNamesTheNewPort) {
  const uint16_t first = availablePort();
  ASSERT_NE(first, 0);
  NetworkManager network(first);
  network.start();
  ASSERT_TRUE(network.listening()) << network.statusText().toStdString();

  const uint16_t replacement = availablePort();
  ASSERT_NE(replacement, 0);
  ASSERT_NE(network.port(), replacement);
  network.setPort(replacement);
  EXPECT_TRUE(network.listening());
  EXPECT_EQ(network.port(), replacement);
  EXPECT_EQ(network.statusText(),
            QString("Listening on UDP :%1").arg(replacement));

  // The new port is the one that receives: a scene sent to it reaches the
  // drain.
  int received = 0;
  QObject::connect(&network, &NetworkManager::spellCircleReceived, &network,
                   [&] { ++received; });
  ASSERT_NE(sendTo(replacement, circleScene()), 0);
  ASSERT_TRUE(pumpUntil(network, [&] { return received == 1; }));
}

TEST_F(SpellCircleQt, ASceneReachesTheModelThroughTheDrain) {
  SpellCircleModel model;
  NetworkManager network(availablePort());
  ASSERT_NE(network.port(), 0);
  QObject::connect(&network, &NetworkManager::spellCircleReceived, &model,
                   &SpellCircleModel::onSpellCircleReceived);
  QString source;
  QObject::connect(&network, &NetworkManager::spellCircleReceived, &network,
                   [&](const QString& from, const QByteArray&,
                       Clock::time_point) { source = from; });
  network.start();
  ASSERT_TRUE(network.listening()) << network.statusText().toStdString();

  const uint16_t sender =
      sendTo(static_cast<uint16_t>(network.port()), circleScene());
  ASSERT_NE(sender, 0);
  ASSERT_TRUE(pumpUntil(network, [&] { return model.rowCount() == 1; }));
  EXPECT_EQ(source, QString("127.0.0.1:%1").arg(sender));
  EXPECT_EQ(
      model.document().registry().view<spellcircle::CircleComponent>().size(),
      1u);
}

TEST_F(SpellCircleQt, AClosedDoorLeavesNothingToRead) {
  NetworkManager network(availablePort());
  ASSERT_NE(network.port(), 0);
  network.start();
  ASSERT_TRUE(network.listening()) << network.statusText().toStdString();
  const auto port = static_cast<uint16_t>(network.port());
  int received = 0;
  QObject::connect(&network, &NetworkManager::spellCircleReceived, &network,
                   [&] { ++received; });

  network.stop();
  EXPECT_FALSE(network.listening());
  EXPECT_EQ(network.statusText(), "Stopped");

  // Nothing is listening for it any more, and nothing the closed door may
  // still hold is read: the drain answers a receiver that has no door.
  sendTo(port, circleScene());
  const auto deadline = Clock::now() + 200ms;
  while (Clock::now() < deadline) {
    network.readArrivals();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    std::this_thread::sleep_for(1ms);
  }
  EXPECT_EQ(received, 0);
}

TEST_F(SpellCircleQt, InvalidAndRepeatedPacketsPreserveRenderedGeneration) {
  SpellCircleModel model;
  int geometryChanges = 0;
  int rateChanges = 0;
  QObject::connect(&model, &SpellCircleModel::geometryChanged, &model,
                   [&] { ++geometryChanges; });
  QObject::connect(&model, &SpellCircleModel::scenesPerSecondChanged, &model,
                   [&] { ++rateChanges; });
  const auto payload = circleScene();
  const auto receivedAt = Clock::now();
  model.onSpellCircleReceived("source", payload, receivedAt - 100ms);
  const auto generation = model.generation();
  ASSERT_EQ(model.rowCount(), 1);
  ASSERT_EQ(geometryChanges, 1);
  const auto circles =
      model.document().registry().view<spellcircle::CircleComponent>();
  ASSERT_EQ(circles.size(), 1u);
  const auto circle = *circles.begin();

  model.onSpellCircleReceived("source", "invalid", receivedAt - 50ms);
  EXPECT_EQ(model.generation(), generation);
  EXPECT_EQ(model.rowCount(), 1);
  EXPECT_EQ(geometryChanges, 1);
  EXPECT_TRUE(model.document().registry().valid(circle));

  model.onSpellCircleReceived("source", payload, receivedAt);
  EXPECT_EQ(model.generation(), generation);
  EXPECT_EQ(model.rowCount(), 2);
  EXPECT_EQ(geometryChanges, 1);
  EXPECT_TRUE(model.document().registry().valid(circle));
  EXPECT_DOUBLE_EQ(model.scenesPerSecond(), 10);
  EXPECT_EQ(rateChanges, 1);

  model.clear();
  EXPECT_GT(model.generation(), generation);
  EXPECT_EQ(geometryChanges, 2);
  EXPECT_TRUE(
      model.document().registry().view<spellcircle::CircleComponent>().empty());
  EXPECT_EQ(model.scenesPerSecond(), 0);
  model.onSpellCircleReceived("source", payload, Clock::now());
  EXPECT_EQ(geometryChanges, 3);
}

}  // namespace
