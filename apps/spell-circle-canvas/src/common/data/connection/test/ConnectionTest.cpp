/** The connection: what a message reads as through the door it came in
 *  by, which handler it reaches, what receive() hands out, what a
 *  message that cannot be read costs, what goes back down the wire, and
 *  what a recording replays through one.
 */

#include <gtest/gtest.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Osc.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Recording.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ScratchDir.h"

using namespace sigil::data;
using sigil::io::Bytes;
using sigil::io::Hub;
using sigil::test::ScratchDir;

namespace {

/** What a case reads to say what went out: every message a connection
 *  sent, in order. */
using Sent = std::vector<std::vector<std::byte>>;

/** A TRANSPORT WITH NO SOCKET UNDER IT. It opens every URI it is given
 *  and takes what is sent into @p sent; what arrives a case delivers
 *  into the feed itself, so one thread runs a case from its first line
 *  to its last and no port has to be free for it to pass. */
sigil::io::FeedTransport intoVector(std::shared_ptr<Sent> sent) {
  return [sent](std::string_view uri, std::weak_ptr<sigil::io::Feed>) {
    sigil::io::OpenedFeed opened;
    // It binds nothing, so the local end it names is the URI it was
    // asked for: enough for a case to see that the end reaches through.
    opened.address = std::string(uri);
    opened.send = [sent](const Bytes& bytes) {
      sent->push_back(bytes.bytes);
      return true;
    };
    return opened;
  };
}

Bytes bytesOf(std::string_view text) {
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  Bytes bytes;
  bytes.bytes.assign(first, first + text.size());
  return bytes;
}

Bytes bytesOf(std::vector<std::byte> packet) {
  Bytes bytes;
  bytes.bytes = std::move(packet);
  return bytes;
}

std::string textOf(const std::vector<std::byte>& raw) {
  return std::string(reinterpret_cast<const char*>(raw.data()), raw.size());
}

TEST(DataConnection, AJsonMessageIsTheLatestAndReachesEveryHandlerNamingIt) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  Connection scene(hub, "ws://:8848/scene");
  int every = 0;
  std::vector<double> gusts;
  scene.on("*", [&every](const Json&) { ++every; });
  scene.on("gust", [&gusts](const Json& message) {
    gusts.push_back(message["strength"].number());
  });
  scene.on("calm", [&gusts](const Json&) { gusts.push_back(-1); });

  scene.feed()->deliver(bytesOf(R"({"kind":"gust","strength":0.5})"));
  // Nothing has been read yet: the frame is what reads it.
  EXPECT_TRUE(scene.latest().null());

  hub.dispatch(0.0);
  EXPECT_EQ(every, 1);
  ASSERT_EQ(gusts.size(), 1u);
  EXPECT_DOUBLE_EQ(gusts.front(), 0.5);
  EXPECT_EQ(scene.latest()["kind"].text(), "gust");
  EXPECT_EQ(scene.generation(), 1u);
  EXPECT_EQ(scene.undecodable(), 0u);
  EXPECT_EQ(scene.uri(), "ws://:8848/scene");
  EXPECT_EQ(scene.address(), "ws://:8848/scene");
  EXPECT_TRUE(scene.error().empty());
  EXPECT_FALSE(scene.closed());
}

TEST(DataConnection, AnOscPacketIsItsAddressAndReachesTheHandlerOnThatAddress) {
  Hub hub;
  hub.setFeedTransport("osc", intoVector(std::make_shared<Sent>()));

  Connection desk(hub, "osc://:9000");
  std::vector<double> faders;
  desk.on("/sky/gust", [&faders](const Json& message) {
    faders.push_back(message["arguments"][0].number());
  });
  desk.on("/sky/calm", [&faders](const Json&) { faders.push_back(-1); });

  desk.feed()->deliver(
      bytesOf(encodeOsc("/sky/gust", Json(Json::Array{Json(0.5)}))));
  hub.dispatch(0.0);

  ASSERT_EQ(faders.size(), 1u);
  EXPECT_DOUBLE_EQ(faders.front(), 0.5);
  EXPECT_EQ(desk.latest()["address"].text(), "/sky/gust");
  ASSERT_EQ(desk.latest()["arguments"].size(), 1u);
  EXPECT_DOUBLE_EQ(desk.latest()["arguments"][0].number(), 0.5);
}

TEST(DataConnection, ReceiveHandsOutEveryMessageInOrderAndThenNothing) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  Connection scene(hub, "ws://:8848/scene");
  for (int number = 0; number != 3; ++number)
    scene.feed()->deliver(
        bytesOf(R"({"kind":"step","n":)" + std::to_string(number) + "}"));
  hub.dispatch(0.0);

  for (int number = 0; number != 3; ++number) {
    const std::optional<Json> message = scene.receive();
    ASSERT_TRUE(message.has_value());
    EXPECT_DOUBLE_EQ((*message)["n"].number(), number);
  }
  EXPECT_FALSE(scene.receive().has_value());
  // Taking is not forgetting: the newest is still the newest.
  EXPECT_DOUBLE_EQ(scene.latest()["n"].number(), 2);
}

TEST(DataConnection, AMessageThatCannotBeReadLeavesTheLatestStanding) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  Connection scene(hub, "ws://:8848/scene");
  int handled = 0;
  scene.on("*", [&handled](const Json&) { ++handled; });

  scene.feed()->deliver(bytesOf(R"({"kind":"gust","strength":0.5})"));
  scene.feed()->deliver(bytesOf("this is no document at all"));
  hub.dispatch(0.0);

  EXPECT_EQ(scene.undecodable(), 1u);
  EXPECT_EQ(handled, 1);
  EXPECT_EQ(scene.generation(), 2u);  // the feed took it; no reader saw it
  EXPECT_EQ(scene.latest()["kind"].text(), "gust");
  ASSERT_TRUE(scene.receive().has_value());
  EXPECT_FALSE(scene.receive().has_value());
}

TEST(DataConnection, SendWritesThePacketOnOscAndTheTextOnEveryOtherDoor) {
  Hub hub;
  const auto desked = std::make_shared<Sent>();
  const auto browsed = std::make_shared<Sent>();
  hub.setFeedTransport("osc", intoVector(desked));
  hub.setFeedTransport("ws", intoVector(browsed));

  const Connection desk(hub, "osc://:9000");
  const Connection browser(hub, "ws://:8848/scene");
  const Json arguments = Json(Json::Array{Json(0.5), Json("gust")});

  EXPECT_TRUE(desk.send("/sky/gust", arguments));
  ASSERT_EQ(desked->size(), 1u);
  EXPECT_EQ(desked->front(), encodeOsc("/sky/gust", arguments));

  EXPECT_TRUE(browser.send("/sky/gust", arguments));
  ASSERT_EQ(browsed->size(), 1u);
  const std::optional<Json> readBack = decodeJson(textOf(browsed->front()));
  ASSERT_TRUE(readBack.has_value());
  EXPECT_EQ(textOf(browsed->front()), encodeJson(*readBack));
  EXPECT_EQ((*readBack)["address"].text(), "/sky/gust");
  EXPECT_TRUE((*readBack)["arguments"] == arguments);

  // The message form goes out the same way on either door: the record a
  // packet reads as is the record a packet is written from.
  EXPECT_TRUE(desk.send(*readBack));
  ASSERT_EQ(desked->size(), 2u);
  EXPECT_EQ(desked->back(), desked->front());
}

TEST(DataConnection, AMovedConnectionGoesOnDispatchingToItsHandlers) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  int seen = 0;
  Connection opened(hub, "ws://:8848/scene");
  opened.on("*", [&seen](const Json&) { ++seen; });
  Connection moved = std::move(opened);

  moved.feed()->deliver(bytesOf(R"({"kind":"gust"})"));
  hub.dispatch(0.0);

  EXPECT_EQ(seen, 1);
  EXPECT_EQ(moved.uri(), "ws://:8848/scene");
  EXPECT_EQ(moved.latest()["kind"].text(), "gust");
}

TEST(DataConnection, AConnectionOntoNothingAnswersNothing) {
  Connection none;
  EXPECT_TRUE(none.latest().null());
  EXPECT_TRUE(none.uri().empty());
  EXPECT_TRUE(none.address().empty());
  EXPECT_TRUE(none.error().empty());
  EXPECT_EQ(none.generation(), 0u);
  EXPECT_EQ(none.dropped(), 0u);
  EXPECT_EQ(none.undecodable(), 0u);
  EXPECT_TRUE(none.closed());
  EXPECT_EQ(none.feed(), nullptr);
  EXPECT_FALSE(none.receive().has_value());
  EXPECT_FALSE(none.send(Json(Json::Object{{"kind", Json("gust")}})));
  EXPECT_FALSE(none.send("/sky/gust", Json(Json::Array{})));
  none.on("*", [](const Json&) { FAIL() << "no message arrives at no door"; });
}

TEST(DataConnection, ARecordingReplaysThroughAConnectionByTheTimeDispatched) {
  const ScratchDir scratch("data_connection_replay");
  const std::filesystem::path path = scratch.path / "scene.feed";
  {
    sigil::io::RecordingWriter writer(path);
    ASSERT_TRUE(writer.good());
    writer.append({1, 0.0,
                   std::make_shared<const Bytes>(
                       bytesOf(R"({"kind":"gust","strength":1})"))});
    writer.append({2, 1.0,
                   std::make_shared<const Bytes>(
                       bytesOf(R"({"kind":"gust","strength":2})"))});
  }

  Hub hub;
  // The URI is mounted straight onto the file: nothing is beneath it,
  // and no transport is needed for a door that reads a recording.
  hub.mount("ws://:8848/scene", path);

  Connection scene(hub, "ws://:8848/scene");
  std::vector<double> strengths;
  scene.on("gust", [&strengths](const Json& message) {
    strengths.push_back(message["strength"].number());
  });
  EXPECT_TRUE(scene.error().empty());
  EXPECT_EQ(scene.generation(), 0u);  // nothing arrives until time moves

  hub.dispatch(0.0);
  ASSERT_EQ(strengths.size(), 1u);
  EXPECT_DOUBLE_EQ(strengths.front(), 1.0);
  EXPECT_FALSE(scene.closed());

  hub.dispatch(1.0);
  ASSERT_EQ(strengths.size(), 2u);
  EXPECT_DOUBLE_EQ(strengths.back(), 2.0);
  // The recording ran out: the door is shut and what it delivered
  // stands.
  EXPECT_TRUE(scene.closed());
  EXPECT_DOUBLE_EQ(scene.latest()["strength"].number(), 2.0);
}

}  // namespace
