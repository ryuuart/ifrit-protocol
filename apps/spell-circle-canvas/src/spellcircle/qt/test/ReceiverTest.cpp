/** @file Qt delivery lifetime and accepted-scene presentation. */

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/post.hpp>
#include <chrono>
#include <functional>
#include <thread>
#include <utility>
#include <vector>

#include "NetworkManager.h"
#include "SpellCircleModel.h"
#include "SpellCircle_generated.h"

namespace {

using boost::asio::ip::udp;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

void ensureApplication() {
  static int argc = 1;
  static char name[] = "spellcircle_qt_test";
  static char* argv[] = {name, nullptr};
  static QCoreApplication application(argc, argv);
}

class SpellCircleQt : public ::testing::Test {
 protected:
  SpellCircleQt() { ensureApplication(); }

  bool pumpUntil(const std::function<bool()>& complete) {
    const auto deadline = Clock::now() + 2s;
    do {
      context.poll();
      QCoreApplication::processEvents(QEventLoop::AllEvents);
      if (complete()) return true;
      std::this_thread::yield();
    } while (Clock::now() < deadline);
    return false;
  }

  uint16_t availablePort() {
    udp::socket reservation(context, udp::endpoint(udp::v6(), 0));
    return reservation.local_endpoint().port();
  }

  void send(uint16_t port, const QByteArray& payload) {
    udp::socket sender(context, udp::v4());
    sender.send_to(
        boost::asio::buffer(payload.constData(), payload.size()),
        udp::endpoint(boost::asio::ip::address_v4::loopback(), port));
  }

  boost::asio::io_context context;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      work = boost::asio::make_work_guard(context);
};

// Retire a binding immediately before its next queued delivery executes.
// The event remains queued and must reject its own obsolete generation.
class BeforeDelivery final : public QObject {
 public:
  explicit BeforeDelivery(std::function<void()> callback)
      : callback(std::move(callback)) {}

  bool invoked = false;

 protected:
  bool eventFilter(QObject*, QEvent* event) override {
    if (event->type() == QEvent::MetaCall && callback) {
      auto invoke = std::move(callback);
      invoked = true;
      invoke();
    }
    return false;
  }

 private:
  std::function<void()> callback;
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

TEST_F(SpellCircleQt, BindingStatusIsQueuedAndPendingBindingCanBeStopped) {
  NetworkManager network(context.get_executor(), availablePort());
  network.start();
  EXPECT_TRUE(network.starting());
  EXPECT_FALSE(network.listening());

  context.poll();
  EXPECT_TRUE(network.starting());
  EXPECT_FALSE(network.listening());

  network.stop();
  EXPECT_FALSE(network.starting());
  EXPECT_FALSE(network.listening());
  bool unrelatedWork = false;
  boost::asio::post(context, [&] { unrelatedWork = true; });
  ASSERT_TRUE(pumpUntil([&] { return unrelatedWork; }));
  EXPECT_FALSE(network.starting());
  EXPECT_FALSE(network.listening());
  EXPECT_EQ(network.statusText(), "Stopped");
}

TEST_F(SpellCircleQt, BindFailureCompletesThePendingState) {
  udp::socket reservation(context, udp::endpoint(udp::v6(), 0));
  const auto port = reservation.local_endpoint().port();
  NetworkManager network(context.get_executor(), port);
  network.start();
  EXPECT_TRUE(network.starting());
  ASSERT_TRUE(pumpUntil([&] { return !network.starting(); }));
  EXPECT_FALSE(network.listening());
  EXPECT_TRUE(network.statusText().contains(QString::number(port)));
  EXPECT_TRUE(network.statusText().contains("failed"));
}

TEST_F(SpellCircleQt, PortChangeRetiresAnAlreadyQueuedBindingStatus) {
  NetworkManager network(context.get_executor(), availablePort());
  network.start();
  context.poll();
  const auto replacement = availablePort();
  ASSERT_NE(network.port(), replacement);
  network.setPort(replacement);
  QCoreApplication::processEvents(QEventLoop::AllEvents);
  EXPECT_TRUE(network.starting());
  EXPECT_FALSE(network.listening());
  EXPECT_TRUE(network.statusText().contains(QString::number(replacement)));
  ASSERT_TRUE(pumpUntil([&] { return network.listening(); }));
  EXPECT_FALSE(network.starting());
  EXPECT_EQ(network.port(), replacement);
  EXPECT_EQ(network.statusText(),
            QString("Listening on UDP :%1").arg(replacement));
}

TEST_F(SpellCircleQt, StopDiscardsADatagramAlreadyQueuedToTheObject) {
  NetworkManager network(context.get_executor(), availablePort());
  network.start();
  ASSERT_TRUE(pumpUntil([&] { return network.listening(); }));
  int received = 0;
  QObject::connect(&network, &NetworkManager::spellCircleReceived, &network,
                   [&] { ++received; });
  BeforeDelivery beforeDelivery([&] { network.stop(); });
  network.installEventFilter(&beforeDelivery);

  send(static_cast<uint16_t>(network.port()), "retired");
  ASSERT_TRUE(pumpUntil([&] { return beforeDelivery.invoked; }));
  EXPECT_EQ(received, 0);
  EXPECT_FALSE(network.listening());
  EXPECT_FALSE(network.starting());
}

TEST_F(SpellCircleQt, RebindDiscardsQueuedDataAndAcceptsTheNewBinding) {
  NetworkManager network(context.get_executor(), availablePort());
  network.start();
  ASSERT_TRUE(pumpUntil([&] { return network.listening(); }));
  const auto replacement = availablePort();
  ASSERT_NE(network.port(), replacement);
  std::vector<QByteArray> received;
  QObject::connect(&network, &NetworkManager::spellCircleReceived, &network,
                   [&](const QString&, const QByteArray& payload,
                       Clock::time_point) { received.push_back(payload); });
  BeforeDelivery beforeDelivery([&] { network.setPort(replacement); });
  network.installEventFilter(&beforeDelivery);

  send(static_cast<uint16_t>(network.port()), "retired");
  ASSERT_TRUE(
      pumpUntil([&] { return beforeDelivery.invoked && network.listening(); }));
  EXPECT_TRUE(received.empty());
  send(replacement, "current");
  ASSERT_TRUE(pumpUntil([&] { return !received.empty(); }));
  ASSERT_EQ(received.size(), 1u);
  EXPECT_EQ(received.front(), "current");
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
