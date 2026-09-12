/** @file Accepted scene identity and receive-time packet rates. */

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <vector>

#include "SceneSession.h"
#include "SpellCircle_generated.h"

namespace {

using spellcircle::CircleComponent;
using spellcircle::SceneSession;
using spellcircle::SceneUpdate;
using namespace std::chrono_literals;

const SceneSession::Clock::time_point kStart{};

std::vector<uint8_t> circleScene(const char* name = "ring") {
  flatbuffers::FlatBufferBuilder builder;
  const SpellCircle::Vec2 center(120, 240);
  const auto circle =
      SpellCircle::CreateCircleDirect(builder, &center, name, 80);
  const std::vector<flatbuffers::Offset<SpellCircle::Circle>> circles{circle};
  SpellCircle::FinishSceneBuffer(
      builder, SpellCircle::CreateSceneDirect(builder, &circles, nullptr,
                                              nullptr, 640, 480));
  return {builder.GetBufferPointer(),
          builder.GetBufferPointer() + builder.GetSize()};
}

std::vector<uint8_t> emptyScene() {
  flatbuffers::FlatBufferBuilder builder;
  SpellCircle::FinishSceneBuffer(builder, SpellCircle::CreateScene(builder));
  return {builder.GetBufferPointer(),
          builder.GetBufferPointer() + builder.GetSize()};
}

SceneUpdate ingest(SceneSession& session, const std::vector<uint8_t>& payload,
                   SceneSession::Clock::duration offset = {}) {
  return session.ingest(payload.data(), payload.size(), kStart + offset);
}

TEST(SceneSession, DuplicateArrivalsKeepTheDecodedEntitiesAndGeneration) {
  SceneSession session;
  const auto payload = circleScene();
  EXPECT_EQ(session.generation(), 0u);
  EXPECT_FALSE(session.hasScene());
  ASSERT_EQ(ingest(session, payload), SceneUpdate::Changed);
  ASSERT_TRUE(session.hasScene());
  ASSERT_EQ(session.stats().circles, 1);
  const entt::entity circle =
      *session.document().registry().view<CircleComponent>().begin();
  const uint64_t generation = session.generation();

  EXPECT_EQ(ingest(session, payload, 100ms), SceneUpdate::Unchanged);
  EXPECT_EQ(ingest(session, payload, 200ms), SceneUpdate::Unchanged);
  EXPECT_EQ(session.generation(), generation);
  EXPECT_TRUE(session.document().registry().valid(circle));
  EXPECT_EQ(*session.document().registry().view<CircleComponent>().begin(),
            circle);
  EXPECT_DOUBLE_EQ(session.packetRate(kStart + 200ms), 10);

  EXPECT_EQ(ingest(session, circleScene("changed"), 300ms),
            SceneUpdate::Changed);
  EXPECT_EQ(session.generation(), generation + 1);
  EXPECT_EQ(session.stats().circles, 1);
  const auto view = session.document().registry().view<CircleComponent>();
  EXPECT_EQ(view.get<CircleComponent>(*view.begin()).name, "changed");
}

TEST(SceneSession, RejectedPayloadPreservesTheSceneDeduplicationAndRate) {
  SceneSession session;
  const auto payload = circleScene();
  ASSERT_EQ(ingest(session, payload), SceneUpdate::Changed);
  ASSERT_EQ(ingest(session, payload, 100ms), SceneUpdate::Unchanged);
  const uint64_t generation = session.generation();
  const entt::entity circle =
      *session.document().registry().view<CircleComponent>().begin();
  const uint8_t corrupt[] = {0xde, 0xad, 0xbe, 0xef};

  EXPECT_EQ(session.ingest(corrupt, sizeof(corrupt), kStart + 200ms),
            SceneUpdate::Invalid);
  EXPECT_EQ(session.ingest(nullptr, 0, kStart + 10s), SceneUpdate::Invalid);
  EXPECT_EQ(session.ingest(payload.data(),
                           spellcircle::kMaximumScenePayload + 1, kStart + 10s),
            SceneUpdate::Invalid);
  EXPECT_EQ(session.generation(), generation);
  EXPECT_TRUE(session.hasScene());
  EXPECT_EQ(session.stats().circles, 1);
  EXPECT_EQ(session.stats().edges, 0);
  EXPECT_EQ(session.stats().boxes, 0);
  EXPECT_TRUE(session.document().registry().valid(circle));
  EXPECT_EQ(session.document().sceneWidth(), 640);
  EXPECT_EQ(session.document().sceneHeight(), 480);
  EXPECT_DOUBLE_EQ(session.packetRate(kStart + 300ms), 10);
  EXPECT_EQ(ingest(session, payload, 300ms), SceneUpdate::Unchanged);
  EXPECT_EQ(session.generation(), generation);
}

TEST(SceneSession, ClearForgetsAcceptedBytesAndArrivalHistory) {
  SceneSession session;
  const auto payload = circleScene();
  ASSERT_EQ(ingest(session, payload), SceneUpdate::Changed);
  ASSERT_EQ(ingest(session, payload, 100ms), SceneUpdate::Unchanged);
  const uint64_t accepted = session.generation();
  session.clear();

  EXPECT_FALSE(session.hasScene());
  EXPECT_FALSE(session.stats().hasGeometry());
  EXPECT_EQ(session.document().registry().view<CircleComponent>().size(), 0u);
  EXPECT_EQ(session.document().sceneWidth(), 0);
  EXPECT_EQ(session.document().sceneHeight(), 0);
  EXPECT_EQ(session.packetRate(kStart + 100ms), 0);
  EXPECT_GT(session.generation(), accepted);
  const uint64_t cleared = session.generation();
  session.clear();
  EXPECT_EQ(session.generation(), cleared);
  EXPECT_EQ(ingest(session, payload, 200ms), SceneUpdate::Changed);
  EXPECT_GT(session.generation(), cleared);
  EXPECT_EQ(session.packetRate(kStart + 200ms), 0);
}

TEST(SceneSession, PacketRateUsesReceiveTimesAndExpiresAfterSilence) {
  SceneSession session;
  const auto payload = circleScene();
  // The owner consumes the entire queue here without waiting between
  // calls. Only receive times describe how quickly these packets arrived.
  for (int i = 0; i <= 10; ++i) ingest(session, payload, i * 100ms);
  EXPECT_DOUBLE_EQ(session.packetRate(kStart + 1500ms), 10);
  for (int i = 1; i <= 21; ++i) ingest(session, payload, 1s + i * 50ms);
  EXPECT_DOUBLE_EQ(session.packetRate(kStart + 2050ms), 20);
  EXPECT_DOUBLE_EQ(session.packetRate(kStart + 4049ms), 20);
  EXPECT_EQ(session.packetRate(kStart + 4050ms), 0);

  EXPECT_EQ(ingest(session, payload, 5s), SceneUpdate::Unchanged);
  EXPECT_EQ(session.packetRate(kStart + 5s), 0);
  EXPECT_EQ(ingest(session, payload, 5100ms), SceneUpdate::Unchanged);
  EXPECT_DOUBLE_EQ(session.packetRate(kStart + 5100ms), 10);
}

TEST(SceneSession, EmptyValidSceneIsAcceptedAndCanBeClearedAndReingested) {
  SceneSession session;
  ASSERT_EQ(ingest(session, circleScene()), SceneUpdate::Changed);
  const auto empty = emptyScene();
  const uint64_t populated = session.generation();
  EXPECT_EQ(ingest(session, empty, 100ms), SceneUpdate::Changed);
  EXPECT_GT(session.generation(), populated);
  EXPECT_FALSE(session.hasScene());
  EXPECT_FALSE(session.stats().hasGeometry());
  EXPECT_EQ(session.document().registry().view<CircleComponent>().size(), 0u);
  EXPECT_EQ(ingest(session, empty, 200ms), SceneUpdate::Unchanged);
  const uint64_t acceptedEmpty = session.generation();
  session.clear();
  EXPECT_GT(session.generation(), acceptedEmpty);
  EXPECT_EQ(ingest(session, empty, 300ms), SceneUpdate::Changed);
  EXPECT_FALSE(session.hasScene());
}

TEST(SceneModel, DecodeVerifiesAndPreservesTheDocumentOnRejection) {
  spellcircle::SceneDocument document;
  const auto payload = circleScene();
  const auto decoded = document.decode(payload.data(), payload.size());
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->circles, 1);
  const entt::entity circle =
      *document.registry().view<CircleComponent>().begin();
  EXPECT_FALSE(document.decode(payload.data(), payload.size() / 2));
  EXPECT_FALSE(document.decode(nullptr, payload.size()));
  EXPECT_FALSE(document.decode(payload.data(), 0));
  EXPECT_FALSE(
      document.decode(payload.data(), spellcircle::kMaximumScenePayload + 1));
  EXPECT_TRUE(document.registry().valid(circle));
  EXPECT_EQ(document.registry().get<CircleComponent>(circle).name, "ring");
  EXPECT_EQ(document.sceneWidth(), 640);
  EXPECT_EQ(document.sceneHeight(), 480);
}

}  // namespace
