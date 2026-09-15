/** @file
 * The channel: the number it takes off a wire through each of its two
 * readings, what leaves that number standing, and the binding chain that
 * reads the output it writes.
 */

#include <gtest/gtest.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Osc.h>
#include <sigilio/hub/Hub.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilmotion/values/Animated.h>
#include <sigilsketch/kit/Channel.h>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace kit = sigil::sketch::kit;
namespace data = sigil::data;
namespace motion = sigil::motion;
using sigil::io::Bytes;
using sigil::io::Hub;

/** A TRANSPORT WITH NO SOCKET UNDER IT. It opens every URI it is given
 *  and keeps nothing of what goes out; what arrives a case delivers into
 *  the feed itself, so one thread runs a case from its first line to its
 *  last and no port has to be free for it to pass. */
sigil::io::FeedTransport intoNothing() {
  return [](std::string_view uri, std::weak_ptr<sigil::io::Feed>) {
    sigil::io::OpenedFeed opened;
    // It binds nothing, so the local end it names is the URI it was asked
    // for.
    opened.address = std::string(uri);
    opened.send = [](const Bytes&) { return true; };
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

/** One OSC message to @p address carrying @p arguments, as a case
 *  delivers it. */
Bytes packet(std::string_view address, data::Json::Array arguments) {
  return bytesOf(data::encodeOsc(address, data::Json(std::move(arguments))));
}

TEST(SketchKitChannel, AnOscArgumentOfTheNamedAddressMovesTheOutputOnDispatch) {
  Hub hub;
  hub.setFeedTransport("osc", intoNothing());
  data::Connection desk(hub, "osc://:27080");
  kit::Channel fader(hub, desk, "/fader/1", 0);

  EXPECT_FALSE(fader.lastRead().has_value());
  EXPECT_FLOAT_EQ(fader.value(), 0.0f);
  EXPECT_EQ(fader.name(), "/fader/1");

  desk.feed()->deliver(packet("/fader/1", {data::Json(63.5)}));
  // Nothing has been read yet: the dispatch is what reads it.
  EXPECT_FLOAT_EQ(fader.value(), 0.0f);

  hub.dispatch(0.0);
  EXPECT_FLOAT_EQ(fader.value(), 63.5f);
  ASSERT_TRUE(fader.lastRead().has_value());
  EXPECT_DOUBLE_EQ(*fader.lastRead(), 63.5);
}

TEST(SketchKitChannel, AMessageOnAnotherAddressLeavesTheOutputWhereItWas) {
  Hub hub;
  hub.setFeedTransport("osc", intoNothing());
  data::Connection desk(hub, "osc://:27080");
  kit::Channel fader(hub, desk, "/fader/1", 0);

  desk.feed()->deliver(packet("/fader/1", {data::Json(63.5)}));
  hub.dispatch(0.0);
  desk.feed()->deliver(packet("/fader/2", {data::Json(120.0)}));
  hub.dispatch(0.1);

  EXPECT_FLOAT_EQ(fader.value(), 63.5f);
  EXPECT_DOUBLE_EQ(*fader.lastRead(), 63.5);
}

TEST(SketchKitChannel, AJsonFieldOfTheNamedKindMovesTheOutput) {
  Hub hub;
  hub.setFeedTransport("ws", intoNothing());
  data::Connection phone(hub, "ws://:8849/desk");
  kit::Channel wind(hub, phone, "Wind", "value");

  phone.feed()->deliver(bytesOf(R"({"kind":"Wind","value":12.25})"));
  hub.dispatch(0.0);
  EXPECT_FLOAT_EQ(wind.value(), 12.25f);

  // Another kind, and the same kind carrying no field of that name:
  // neither says anything about this reading.
  phone.feed()->deliver(bytesOf(R"({"kind":"Gust","value":88.0})"));
  phone.feed()->deliver(bytesOf(R"({"kind":"Wind","strength":4.0})"));
  hub.dispatch(0.1);
  EXPECT_FLOAT_EQ(wind.value(), 12.25f);
}

TEST(SketchKitChannel, AReadingThatNamesNoNumberLeavesTheOutputWhereItWas) {
  Hub hub;
  hub.setFeedTransport("osc", intoNothing());
  data::Connection desk(hub, "osc://:27080");
  kit::Channel fader(hub, desk, "/fader/1", 0);

  desk.feed()->deliver(packet("/fader/1", {data::Json(63.5)}));
  hub.dispatch(0.0);

  // The same address carrying a word where the fader stands, and then
  // carrying nothing at all.
  desk.feed()->deliver(packet("/fader/1", {data::Json("quiet")}));
  hub.dispatch(0.1);
  EXPECT_FLOAT_EQ(fader.value(), 63.5f);

  desk.feed()->deliver(packet("/fader/1", {}));
  hub.dispatch(0.2);
  EXPECT_FLOAT_EQ(fader.value(), 63.5f);
  EXPECT_DOUBLE_EQ(*fader.lastRead(), 63.5);

  // And a dispatch that delivered nothing at all.
  hub.dispatch(0.3);
  EXPECT_FLOAT_EQ(fader.value(), 63.5f);
}

TEST(SketchKitChannel, ABoundChainOverTheOutputReadsTheMappedValue) {
  Hub hub;
  hub.setFeedTransport("osc", intoNothing());
  data::Connection desk(hub, "osc://:27080");
  kit::Channel fader(hub, desk, "/fader/1", 0);

  // The property a description would carry: the fader's own range mapped
  // onto the unit the property wants, with nothing between the wire and
  // the arithmetic.
  const motion::Animatable<float> level =
      motion::bind(&fader.output()).source(0, 127).target(0, 1);
  EXPECT_FLOAT_EQ(motion::resolveFloatAt(nullptr, level), 0.0f);

  desk.feed()->deliver(packet("/fader/1", {data::Json(63.5)}));
  hub.dispatch(0.0);
  EXPECT_NEAR(motion::resolveFloatAt(nullptr, level), 0.5f, 0.001f);

  desk.feed()->deliver(packet("/fader/1", {data::Json(127.0)}));
  hub.dispatch(0.1);
  EXPECT_NEAR(motion::resolveFloatAt(nullptr, level), 1.0f, 0.001f);
}

TEST(SketchKitChannel, AMovedChannelGoesOnFollowingTheSameWire) {
  Hub hub;
  hub.setFeedTransport("osc", intoNothing());
  data::Connection desk(hub, "osc://:27080");
  kit::Channel first(hub, desk, "/fader/1", 0);

  // The address a description binds. It is the whole reason the state
  // stands behind a pointer: a binding that outlives the move has to keep
  // reading the same output.
  const choreograph::Output<float>* bound = &first.output();
  kit::Channel moved = std::move(first);
  EXPECT_EQ(&moved.output(), bound);

  desk.feed()->deliver(packet("/fader/1", {data::Json(63.5)}));
  hub.dispatch(0.0);
  EXPECT_FLOAT_EQ(moved.value(), 63.5f);
  EXPECT_FLOAT_EQ(bound->value(), 63.5f);
}

TEST(SketchKitChannel, AChannelOntoNothingStandsStillAtZero) {
  const kit::Channel none;
  EXPECT_FLOAT_EQ(none.value(), 0.0f);
  EXPECT_FALSE(none.lastRead().has_value());
  EXPECT_TRUE(none.name().empty());

  const motion::Animatable<float> level =
      motion::bind(&none.output()).source(0, 127).target(0, 1);
  EXPECT_FLOAT_EQ(motion::resolveFloatAt(nullptr, level), 0.0f);
}

}  // namespace
