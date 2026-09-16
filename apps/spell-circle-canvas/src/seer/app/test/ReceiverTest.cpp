/** @file What the Qt receiver's door reports when it opens, what a drain
 *  hands the scene model, and what a closed door leaves behind. */

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sigilio/hub/Recording.h>
#include <sys/socket.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

#include "SeerSession.h"
#include "SpellCircleModel.h"
#include "SpellCircle_generated.h"

namespace {

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

void ensureApplication() {
  static int argc = 1;
  static char name[] = "seer_qt_test";
  static char* argv[] = {name, nullptr};
  qputenv("QT_QPA_PLATFORM", "offscreen");
  static QGuiApplication application(argc, argv);
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

class SeerReceiver : public ::testing::Test {
 protected:
  SeerReceiver() { ensureApplication(); }
  QTemporaryDir directory;
  QString uri(uint16_t port) { return QString("udp://:%1").arg(port); }
  std::unique_ptr<SeerSession> session() {
    return std::make_unique<SeerSession>(nullptr, directory.path() + "/new",
                                         directory.path() + "/old");
  }
  void write(const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(bytes), bytes.size());
  }

  /** Reads the receiver's door and runs the event loop until @p complete
   *  holds. The datagram lands on the transport's own thread, so what
   *  this waits for is the drain, which is the call under test. */
  bool pumpUntil(SeerSession& session, const std::function<bool()>& complete) {
    const auto deadline = Clock::now() + 2s;
    do {
      session.tick();
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

TEST_F(SeerReceiver, APlainSessionDoesNotOpenTheSavedPort) {
  write(directory.path() + "/old/network_config.json", R"({"port":27016})");
  auto app = session();
  EXPECT_EQ(app->receiver()->port(), 27016);
  EXPECT_FALSE(app->receiver()->opened());
  EXPECT_FALSE(app->receiver()->listening());
  EXPECT_EQ(app->wires()->rowCount(), 0);
}

TEST_F(SeerReceiver, APortConflictIsReportedAndCanBeRetried) {
  const int holder = bindDualStack(0);
  ASSERT_GE(holder, 0);
  const uint16_t port = portOf(holder);
  auto app = session();
  app->openReceiver(uri(port));
  EXPECT_FALSE(app->receiver()->listening());
  EXPECT_FALSE(app->receiver()->statusText().isEmpty());
  ::close(holder);
  app->receiver()->start();
  EXPECT_TRUE(app->receiver()->listening())
      << app->receiver()->statusText().toStdString();
}

TEST_F(SeerReceiver, OneDrainFeedsBothTraceAndScene) {
  const uint16_t port = availablePort();
  ASSERT_NE(port, 0);
  auto app = session();
  app->openReceiver(uri(port));
  ASSERT_TRUE(app->receiver()->listening());
  const auto sender = sendTo(port, circleScene());
  ASSERT_NE(sender, 0);
  ASSERT_TRUE(pumpUntil(
      *app, [&] { return app->receiver()->model()->rowCount() == 1; }));
  EXPECT_EQ(app->messages()->rowCount(), 1);
  EXPECT_EQ(app->wires()->rowCount(), 1);
  EXPECT_TRUE(app->receiver()
                  ->model()
                  ->data(app->receiver()->model()->index(0, 0),
                         SpellCircleModel::SourceRole)
                  .toString()
                  .endsWith(QString::number(sender)));
  app->tick();
  EXPECT_EQ(app->receiver()->model()->rowCount(), 1);
  EXPECT_EQ(app->messages()->rowCount(), 1);
}

TEST_F(SeerReceiver, InspectingAnotherWireDoesNotChangeThePinnedReceiver) {
  const uint16_t port = availablePort();
  auto app = session();
  app->openReceiver(uri(port));
  const uint16_t inspected = availablePort();
  ASSERT_NE(inspected, port);
  app->open(uri(inspected));
  sendTo(inspected, "ordinary text");
  sendTo(port, circleScene());
  ASSERT_TRUE(pumpUntil(*app, [&] {
    return app->receiver()->model()->rowCount() == 1 &&
           app->messages()->rowCount() == 1;
  }));
  EXPECT_EQ(app->receiver()->uri(), uri(port));
  EXPECT_EQ(app->reading()->uri(), uri(inspected));
}

TEST_F(SeerReceiver, APortChangeRebindsAndStopPreservesTheScene) {
  auto app = session();
  app->openReceiver(uri(availablePort()));
  const uint16_t next = availablePort();
  app->receiver()->setPort(next);
  ASSERT_TRUE(app->receiver()->listening());
  sendTo(next, circleScene());
  ASSERT_TRUE(pumpUntil(
      *app, [&] { return app->receiver()->model()->rowCount() == 1; }));
  const auto generation = app->receiver()->model()->generation();
  app->receiver()->stop();
  EXPECT_FALSE(app->receiver()->listening());
  EXPECT_EQ(app->wires()->rowCount(), 0);
  sendTo(next, circleScene());
  app->tick();
  EXPECT_EQ(app->receiver()->model()->rowCount(), 1);
  EXPECT_EQ(app->receiver()->model()->generation(), generation);
}

TEST_F(SeerReceiver, InvalidAndDuplicatePacketsKeepTheLastGoodGeometry) {
  auto app = session();
  const uint16_t port = availablePort();
  app->openReceiver(uri(port));
  sendTo(port, circleScene());
  ASSERT_TRUE(
      pumpUntil(*app, [&] { return app->messages()->rowCount() == 1; }));
  const auto generation = app->receiver()->model()->generation();
  sendTo(port, "invalid");
  sendTo(port, circleScene());
  ASSERT_TRUE(
      pumpUntil(*app, [&] { return app->messages()->rowCount() == 3; }));
  EXPECT_EQ(app->receiver()->model()->generation(), generation);
  EXPECT_EQ(app->receiver()->model()->rowCount(), 2);
}

TEST_F(SeerReceiver, StoppingFromASceneSignalEndsTheDrain) {
  auto app = session();
  const auto port = availablePort();
  app->openReceiver(uri(port));
  QObject::connect(app->receiver()->model(), &QAbstractItemModel::rowsInserted,
                   app.get(), [&] { app->receiver()->stop(); });
  sendTo(port, circleScene());
  sendTo(port, circleScene());
  ASSERT_TRUE(pumpUntil(*app, [&] { return !app->receiver()->listening(); }));
  app->tick();
  EXPECT_EQ(app->receiver()->model()->rowCount(), 1);
}

TEST_F(SeerReceiver, RestartingFromAStatusSignalReleasesTheStatusFeedFirst) {
  auto app = session();
  const auto port = availablePort();
  bool restarted = false;
  QObject::connect(app->receiver(), &Receiver::changed, app.get(), [&] {
    if (restarted || !app->receiver()->listening()) return;
    restarted = true;
    app->receiver()->stop();
    app->receiver()->start();
  });
  app->openReceiver(uri(port));
  ASSERT_TRUE(restarted);
  ASSERT_TRUE(app->receiver()->listening())
      << app->receiver()->statusText().toStdString();
  sendTo(port, circleScene());
  EXPECT_TRUE(pumpUntil(
      *app, [&] { return app->receiver()->model()->rowCount() == 1; }));
}

TEST_F(SeerReceiver, RestartingFromASceneSignalOpensAFreshLiveFeed) {
  auto app = session();
  const auto port = availablePort();
  app->openReceiver(uri(port));
  bool restarted = false;
  QObject::connect(app->receiver()->model(), &QAbstractItemModel::rowsInserted,
                   app.get(), [&] {
                     if (restarted) return;
                     restarted = true;
                     app->receiver()->stop();
                     app->receiver()->start();
                   });
  sendTo(port, circleScene());
  ASSERT_TRUE(pumpUntil(*app, [&] { return restarted; }));
  ASSERT_TRUE(app->receiver()->listening());
  sendTo(port, circleScene());
  ASSERT_TRUE(pumpUntil(
      *app, [&] { return app->receiver()->model()->rowCount() == 2; }));
  EXPECT_EQ(app->messages()->rowCount(), 1);
  EXPECT_FALSE(app->receiver()->recorded());
}

TEST_F(SeerReceiver, ReplayingFromASceneSignalReleasesTheOldFeedFirst) {
  const QByteArray scene = circleScene();
  auto bytes = std::make_shared<sigil::io::Bytes>();
  const auto* first = reinterpret_cast<const std::byte*>(scene.constData());
  bytes->bytes.assign(first, first + scene.size());
  const QString path = directory.path() + "/reentrant.feed";
  {
    sigil::io::RecordingWriter writer(path.toStdString());
    ASSERT_TRUE(writer.append({1, 0.0, bytes, {}}));
  }
  auto app = session();
  const auto port = availablePort();
  const QString source = uri(port);
  app->openReceiver(source);
  bool replayed = false;
  QObject::connect(app->receiver()->model(), &QAbstractItemModel::rowsInserted,
                   app.get(), [&] {
                     if (replayed) return;
                     replayed = true;
                     app->replay(source, QUrl::fromLocalFile(path));
                   });
  sendTo(port, scene);
  ASSERT_TRUE(pumpUntil(
      *app, [&] { return app->receiver()->model()->rowCount() == 2; }));
  EXPECT_TRUE(replayed);
  EXPECT_TRUE(app->receiver()->recorded());
  EXPECT_EQ(app->messages()->rowCount(), 1);
  EXPECT_EQ(app->receiver()
                ->model()
                ->data(app->receiver()->model()->index(0, 0),
                       SpellCircleModel::SourceRole)
                .toString(),
            "recording");
}

TEST_F(SeerReceiver, RecordingWritesTheArrivalsAlsoSeenByTheReceiver) {
  auto app = session();
  const auto port = availablePort();
  app->openReceiver(uri(port));
  const QString path = directory.path() + "/live.feed";
  app->recordTo(QUrl::fromLocalFile(path));
  ASSERT_TRUE(app->recording());
  sendTo(port, circleScene());
  ASSERT_TRUE(pumpUntil(
      *app, [&] { return app->receiver()->model()->rowCount() == 1; }));
  app->stopRecording();
  const auto recorded = sigil::io::readRecording(path.toStdString());
  ASSERT_TRUE(recorded);
  ASSERT_EQ(recorded->size(), 1u);
  EXPECT_EQ(app->messages()->rowCount(), 1);
  const auto expected = circleScene();
  EXPECT_EQ(recorded->front().bytes->bytes.size(), expected.size());
}

TEST_F(SeerReceiver, ReplayReplacesThePinnedFeedAndCanRestart) {
  const QByteArray scene = circleScene();
  auto bytes = std::make_shared<sigil::io::Bytes>();
  const auto* first = reinterpret_cast<const std::byte*>(scene.constData());
  bytes->bytes.assign(first, first + scene.size());
  const QString path = directory.path() + "/scene.feed";
  {
    sigil::io::RecordingWriter writer(path.toStdString());
    ASSERT_TRUE(writer.append({1, 0.0, bytes, {}}));
  }
  auto app = session();
  const QString source = uri(availablePort());
  app->openReceiver(source);
  app->replay(source, QUrl::fromLocalFile(path));
  app->tick();
  EXPECT_EQ(app->receiver()->model()->rowCount(), 1);
  EXPECT_EQ(app->messages()->rowCount(), 1);
  EXPECT_TRUE(app->receiver()->recorded());
  EXPECT_TRUE(app->receiver()->statusText().contains("Recording"));
  app->receiver()->start();
  app->tick();
  EXPECT_EQ(app->receiver()->model()->rowCount(), 2);
  EXPECT_EQ(app->messages()->rowCount(), 1);
}

TEST_F(SeerReceiver, SettingsImportSaveAndCancelHaveDistinctOwnership) {
  write(directory.path() + "/old/graphics_config.json",
        R"({"color":"#112233","strokeWidth":9})");
  write(directory.path() + "/old/network_config.json", R"({"port":27123})");
  auto app = session();
  EXPECT_EQ(app->receiver()->config()->color(), QColor("#112233"));
  EXPECT_EQ(app->receiver()->port(), 27123);
  app->receiver()->beginSettings();
  app->receiver()->config()->setStrokeWidth(19);
  app->receiver()->setPort(27124);
  app->receiver()->cancelSettings();
  EXPECT_EQ(app->receiver()->config()->strokeWidth(), 9);
  EXPECT_EQ(app->receiver()->port(), 27123);
  app->receiver()->beginSettings();
  app->receiver()->config()->setColor(QColor("#abcdef"));
  app->receiver()->setPort(27125);
  ASSERT_TRUE(app->receiver()->saveSettings());
  auto reopened = session();
  EXPECT_EQ(reopened->receiver()->config()->color(), QColor("#abcdef"));
  EXPECT_EQ(reopened->receiver()->port(), 27125);
  QFile original(directory.path() + "/old/graphics_config.json");
  ASSERT_TRUE(original.open(QIODevice::ReadOnly));
  EXPECT_TRUE(original.readAll().contains("#112233"));
}

TEST_F(SeerReceiver, CombinedSettingsAreAuthoritativeWithoutOpeningSource) {
  write(directory.path() + "/old/graphics_config.json",
        R"({"color":"#112233","strokeWidth":9})");
  write(directory.path() + "/new/graphics_config.json",
        R"({"color":"#445566","strokeWidth":15})");
  write(directory.path() + "/new/network_config.json", R"({"port":27123})");
  write(directory.path() + "/new/receiver_config.json",
        R"({"graphics":{"color":"#abcdef"},"uri":"udp://:27125"})");
  auto app = session();
  EXPECT_EQ(app->receiver()->config()->color(), QColor("#abcdef"));
  EXPECT_EQ(app->receiver()->config()->strokeWidth(), 4);
  EXPECT_EQ(app->receiver()->port(), 27125);
  EXPECT_FALSE(app->receiver()->opened());
  EXPECT_FALSE(app->receiver()->listening());
}

TEST_F(SeerReceiver,
       CurrentLooseSettingsPrecedeImportedSettingsWithoutWriting) {
  write(directory.path() + "/old/graphics_config.json",
        R"({"color":"#112233","strokeWidth":9})");
  write(directory.path() + "/old/network_config.json", R"({"port":27123})");
  const QByteArray graphics = R"({"color":"#445566","strokeWidth":15})";
  write(directory.path() + "/new/graphics_config.json", graphics);
  write(directory.path() + "/new/network_config.json",
        R"({"uri":"udp://:27124"})");
  auto app = session();
  EXPECT_EQ(app->receiver()->config()->color(), QColor("#445566"));
  EXPECT_EQ(app->receiver()->config()->strokeWidth(), 15);
  EXPECT_EQ(app->receiver()->port(), 27124);
  EXPECT_FALSE(QFile::exists(directory.path() + "/new/receiver_config.json"));
  QFile original(directory.path() + "/new/graphics_config.json");
  ASSERT_TRUE(original.open(QIODevice::ReadOnly));
  EXPECT_EQ(original.readAll(), graphics);
}

TEST_F(SeerReceiver, InvalidCombinedSettingsFallBackButInvalidLooseFilesDoNot) {
  write(directory.path() + "/new/receiver_config.json", "not json");
  write(directory.path() + "/old/graphics_config.json",
        R"({"color":"#112233","strokeWidth":9})");
  write(directory.path() + "/old/network_config.json", R"({"port":27123})");
  {
    auto app = session();
    EXPECT_EQ(app->receiver()->config()->strokeWidth(), 9);
    EXPECT_EQ(app->receiver()->port(), 27123);
  }
  write(directory.path() + "/new/graphics_config.json", "not json");
  write(directory.path() + "/new/network_config.json", "not json");
  auto app = session();
  EXPECT_EQ(app->receiver()->config()->strokeWidth(), 4);
  EXPECT_EQ(app->receiver()->port(), 27015);
}

TEST_F(SeerReceiver, GraphicsSnapshotsRestoreGroupedValuesAndNotifyObservers) {
  GraphicsConfig config;
  const auto original = config.snapshot();
  int changes = 0;
  QObject::connect(&config, &GraphicsConfig::generationChanged,
                   [&changes] { ++changes; });
  config.box()->setWidth(720);
  config.canvas()->setHeight(1080);
  config.setColor(QColor("#abcdef"));
  EXPECT_EQ(changes, 3);
  config.box()->setWidth(720);
  EXPECT_EQ(changes, 3);
  const int generation = config.generation();
  config.restore(original);
  EXPECT_EQ(config.box()->width(), 360);
  EXPECT_EQ(config.canvas()->height(), 4000);
  EXPECT_EQ(config.color(), QColor("#ff0000"));
  EXPECT_GT(config.generation(), generation);
  EXPECT_GT(changes, 3);
}

TEST_F(SeerReceiver, FailedSaveAndCancelLeaveBothPersistedSettingsUnchanged) {
  write(directory.path() + "/old/graphics_config.json",
        R"({"color":"#112233","strokeWidth":9})");
  write(directory.path() + "/old/network_config.json", R"({"port":27123})");
  ASSERT_TRUE(QDir().mkpath(directory.path() + "/new/receiver_config.json"));
  auto app = session();
  app->receiver()->beginSettings();
  app->receiver()->config()->setColor(QColor("#abcdef"));
  app->receiver()->setPort(27125);
  EXPECT_FALSE(app->receiver()->saveSettings());
  app->receiver()->cancelSettings();
  EXPECT_EQ(app->receiver()->config()->color(), QColor("#112233"));
  EXPECT_EQ(app->receiver()->port(), 27123);
  auto reopened = session();
  EXPECT_EQ(reopened->receiver()->config()->color(), QColor("#112233"));
  EXPECT_EQ(reopened->receiver()->port(), 27123);
  EXPECT_FALSE(QFile::exists(directory.path() + "/new/graphics_config.json"));
  EXPECT_FALSE(QFile::exists(directory.path() + "/new/network_config.json"));
}

TEST_F(SeerReceiver, CancelRestoresUnsavedDefaults) {
  auto app = session();
  const auto stroke = app->receiver()->config()->strokeWidth();
  app->receiver()->beginSettings();
  app->receiver()->config()->setStrokeWidth(27);
  app->receiver()->cancelSettings();
  EXPECT_EQ(app->receiver()->config()->strokeWidth(), stroke);
}

}  // namespace
