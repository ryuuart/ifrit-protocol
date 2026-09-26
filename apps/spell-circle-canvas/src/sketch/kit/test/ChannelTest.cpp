/** @file
 * The channel: the number it takes off a wire through each of its two
 * readings, what leaves that number standing, and the binding that reads
 * the live value it writes.
 */

#include <gtest/gtest.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Dialect.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/testing/Testing.h>
#include <sigilio/advanced/Time.h>
#include <sigilmotion/advanced/Held.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/values/Animatable.h>
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
using sigil::io::testing::inletOf;

/** A TRANSPORT WITH NO SOCKET UNDER IT. It opens every URI it is given
 *  and keeps nothing of what goes out; what arrives a case puts on the
 *  feed through its inlet, so one thread runs a case from its first line
 *  to its last and no port has to be free for it to pass. */
sigil::io::Transport intoNothing() {
  return [](std::string_view uri, sigil::io::Inlet) {
    sigil::io::TransportEnd opened;
    // It binds nothing, so the local end it names is the URI it was asked
    // for.
    opened.localAddress = std::string(uri);
    opened.send = [](const Bytes&) { return true; };
    return opened;
  };
}

Bytes bytesOf(std::string_view text) {
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  return Bytes(std::span(first, text.size()));
}

Bytes bytesOf(std::vector<std::byte> packet) {
  return Bytes(std::move(packet));
}

/** One OSC message to @p address carrying @p arguments, as a case
 *  delivers it. */
Bytes packet(std::string_view address, data::Json::Array arguments) {
  return bytesOf(data::encode(data::oscMessage(address, data::Json(std::move(arguments))), data::Dialect::Osc));
}

TEST(SketchKitChannel, AnOscArgumentOfTheNamedAddressMovesTheValueOnDispatch) {
  Hub hub;
  sigil::io::registerTransport(hub, "osc", intoNothing());
  data::Connection desk = data::connect(hub, "osc://:27080");
  kit::Channel fader(hub, desk, "/fader/1", 0);

  EXPECT_FALSE(fader.lastRead().has_value());
  EXPECT_FLOAT_EQ(fader.value(), 0.0f);
  EXPECT_EQ(fader.name(), "/fader/1");

  inletOf(desk.feed()).deliver(packet("/fader/1", {data::Json(63.5)}));
  // Nothing has been read yet: the dispatch is what reads it.
  EXPECT_FLOAT_EQ(fader.value(), 0.0f);

  sigil::io::advance(hub, std::chrono::duration<double>(0.0));
  EXPECT_FLOAT_EQ(fader.value(), 63.5f);
  ASSERT_TRUE(fader.lastRead().has_value());
  EXPECT_DOUBLE_EQ(*fader.lastRead(), 63.5);
}

TEST(SketchKitChannel, AMessageOnAnotherAddressLeavesTheValueWhereItWas) {
  Hub hub;
  sigil::io::registerTransport(hub, "osc", intoNothing());
  data::Connection desk = data::connect(hub, "osc://:27080");
  kit::Channel fader(hub, desk, "/fader/1", 0);

  inletOf(desk.feed()).deliver(packet("/fader/1", {data::Json(63.5)}));
  sigil::io::advance(hub, std::chrono::duration<double>(0.0));
  inletOf(desk.feed()).deliver(packet("/fader/2", {data::Json(120.0)}));
  sigil::io::advance(hub, std::chrono::duration<double>(0.1));

  EXPECT_FLOAT_EQ(fader.value(), 63.5f);
  EXPECT_DOUBLE_EQ(*fader.lastRead(), 63.5);
}

TEST(SketchKitChannel, AJsonFieldOfTheNamedKindMovesTheValue) {
  Hub hub;
  sigil::io::registerTransport(hub, "ws", intoNothing());
  data::Connection phone = data::connect(hub, "ws://:8849/desk");
  kit::Channel wind(hub, phone, "Wind", "value");

  inletOf(phone.feed()).deliver(bytesOf(R"({"kind":"Wind","value":12.25})"));
  sigil::io::advance(hub, std::chrono::duration<double>(0.0));
  EXPECT_FLOAT_EQ(wind.value(), 12.25f);

  // Another kind, and the same kind carrying no field of that name:
  // neither says anything about this reading.
  inletOf(phone.feed()).deliver(bytesOf(R"({"kind":"Gust","value":88.0})"));
  inletOf(phone.feed()).deliver(bytesOf(R"({"kind":"Wind","strength":4.0})"));
  sigil::io::advance(hub, std::chrono::duration<double>(0.1));
  EXPECT_FLOAT_EQ(wind.value(), 12.25f);
}

TEST(SketchKitChannel, AReadingThatNamesNoNumberLeavesTheValueWhereItWas) {
  Hub hub;
  sigil::io::registerTransport(hub, "osc", intoNothing());
  data::Connection desk = data::connect(hub, "osc://:27080");
  kit::Channel fader(hub, desk, "/fader/1", 0);

  inletOf(desk.feed()).deliver(packet("/fader/1", {data::Json(63.5)}));
  sigil::io::advance(hub, std::chrono::duration<double>(0.0));

  // The same address carrying a word where the fader stands, and then
  // carrying nothing at all.
  inletOf(desk.feed()).deliver(packet("/fader/1", {data::Json("quiet")}));
  sigil::io::advance(hub, std::chrono::duration<double>(0.1));
  EXPECT_FLOAT_EQ(fader.value(), 63.5f);

  inletOf(desk.feed()).deliver(packet("/fader/1", {}));
  sigil::io::advance(hub, std::chrono::duration<double>(0.2));
  EXPECT_FLOAT_EQ(fader.value(), 63.5f);
  EXPECT_DOUBLE_EQ(*fader.lastRead(), 63.5);

  // And a dispatch that delivered nothing at all.
  sigil::io::advance(hub, std::chrono::duration<double>(0.3));
  EXPECT_FLOAT_EQ(fader.value(), 63.5f);
}

TEST(SketchKitChannel, ABindingOverTheLiveValueReadsTheMappedValue) {
  Hub hub;
  sigil::io::registerTransport(hub, "osc", intoNothing());
  data::Connection desk = data::connect(hub, "osc://:27080");
  kit::Channel fader(hub, desk, "/fader/1", 0);

  // The property a description would carry: the fader's own range mapped
  // onto the unit the property wants, with nothing between the wire and
  // the arithmetic.
  const motion::Animatable<float> level =
      motion::bind(fader.live(), {.from = {0, 127}, .to = {0.0f, 1.0f}});
  EXPECT_FLOAT_EQ(motion::valueOf(nullptr, level), 0.0f);

  inletOf(desk.feed()).deliver(packet("/fader/1", {data::Json(63.5)}));
  sigil::io::advance(hub, std::chrono::duration<double>(0.0));
  EXPECT_NEAR(motion::valueOf(nullptr, level), 0.5f, 0.001f);

  inletOf(desk.feed()).deliver(packet("/fader/1", {data::Json(127.0)}));
  sigil::io::advance(hub, std::chrono::duration<double>(0.1));
  EXPECT_NEAR(motion::valueOf(nullptr, level), 1.0f, 0.001f);
}

TEST(SketchKitChannel, AMovedChannelGoesOnFollowingTheSameWire) {
  Hub hub;
  sigil::io::registerTransport(hub, "osc", intoNothing());
  data::Connection desk = data::connect(hub, "osc://:27080");
  kit::Channel first(hub, desk, "/fader/1", 0);

  // The cell a description binds. It is the whole reason the state stands
  // behind a pointer: a binding that outlives the move has to keep reading
  // the same live value.
  const motion::Animatable<float> bound = first.live();
  kit::Channel moved = std::move(first);
  EXPECT_EQ(moved.live().identity(), bound.identity());

  inletOf(desk.feed()).deliver(packet("/fader/1", {data::Json(63.5)}));
  sigil::io::advance(hub, std::chrono::duration<double>(0.0));
  EXPECT_FLOAT_EQ(moved.value(), 63.5f);
  EXPECT_FLOAT_EQ(bound.value(), 63.5f);
}

TEST(SketchKitChannel, AChannelOntoNothingStandsStillAtZero) {
  const kit::Channel none;
  EXPECT_FLOAT_EQ(none.value(), 0.0f);
  EXPECT_FALSE(none.lastRead().has_value());
  EXPECT_TRUE(none.name().empty());

  const motion::Animatable<float> level =
      motion::bind(none.live(), {.from = {0, 127}, .to = {0.0f, 1.0f}});
  EXPECT_FLOAT_EQ(motion::valueOf(nullptr, level), 0.0f);
}

}  // namespace
