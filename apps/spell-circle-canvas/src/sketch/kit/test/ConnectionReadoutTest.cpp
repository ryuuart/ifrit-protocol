/** @file
 * A door's own words: the readout of a connection's vitals is the compose
 * kit's readout of the rows those vitals answer — for a door that is open
 * and has heard a sender, one that has shut and bound no address, one
 * whose transport failed after it opened, and one whose URI never opened
 * at all.
 */

#include <gtest/gtest.h>
#include <sigilcompose/kit/Rows.h>
#include <sigildata/connection/Connection.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/testing/Testing.h>
#include <sigilsketch/kit/Connection.h>
#include <sigilsketch/kit/Theme.h>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Drawn.h"

namespace {

namespace kit = sigil::sketch::kit;
namespace compose = sigil::compose;
namespace data = sigil::data;
using compose::Element;
using sigil::io::Bytes;
using sigil::io::Hub;
using sigil::io::testing::inletOf;
using sigil::sketch::kit::test::sameDrawing;

/** A TRANSPORT WITH NO SOCKET UNDER IT, binding @p address as its local
 *  end; what arrives a case puts on the feed through its inlet. */
sigil::io::Transport binding(std::string address) {
  return [address](std::string_view, sigil::io::Inlet) {
    sigil::io::TransportEnd opened;
    opened.localAddress = address;
    opened.send = [](const Bytes&) { return true; };
    return opened;
  };
}

Bytes bytesOf(std::string_view text) {
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  return Bytes(std::span(first, text.size()));
}

/** Both trees under the house sheet, as a page would set them. */
Element onTheSheet(Element tree) {
  return compose::box()
      .applyStyleSheet(kit::houseTheme().styleSheet())
      .children({std::move(tree)});
}

/** The readout spelled by hand from the five rows a door's readout
 *  holds, in its order. */
Element byHand(std::vector<compose::kit::Reading> rows) {
  return onTheSheet(compose::kit::readout(rows, {.measure = 300}));
}

TEST(SketchKitConnectionReadout, AnOpenDoorReadsItsAddressCountsAndSender) {
  Hub hub;
  hub.setFeedTransport("ws", binding("ws://127.0.0.1:8848"));
  data::Connection sky(hub, "ws://:8848/sky");
  inletOf(sky.feed()).deliver(bytesOf(R"({"kind":"gust"})"), "ws://127.0.0.1:52341");
  inletOf(sky.feed()).deliver(bytesOf("this is no document at all"),
                              "ws://127.0.0.1:52341");
  hub.advance(std::chrono::duration<double>(0.0));
  ASSERT_EQ(sky.revision(), 2u);

  EXPECT_TRUE(sameDrawing(
      onTheSheet(kit::connectionReadout(sky, {.rows = {.measure = 300}})),
      byHand({{.name = u8"door", .value = u8"ws://127.0.0.1:8848"},
              {.name = u8"revision", .value = u8"2"},
              {.name = u8"dropped", .value = u8"0"},
              {.name = u8"undecodable", .value = u8"1"},
              {.name = u8"sender", .value = u8"ws://127.0.0.1:52341"}})));
}

TEST(SketchKitConnectionReadout, AShutDoorWithNoAddressReadsWhatItIsCalled) {
  Hub hub;
  hub.setFeedTransport("ws", binding(""));
  data::Connection sky(hub, "ws://:8849/sky");
  sky.feed().close();
  ASSERT_TRUE(sky.closed());

  // Named by the sketch where it names the door, and by its URI where not.
  const auto rows = [](std::u8string_view door) {
    return byHand({{.name = u8"door", .value = door},
                   {.name = u8"revision", .value = u8"0"},
                   {.name = u8"dropped", .value = u8"0"},
                   {.name = u8"undecodable", .value = u8"0"},
                   {.name = u8"sender", .value = u8"-"}});
  };
  EXPECT_TRUE(sameDrawing(
      onTheSheet(kit::connectionReadout(
          sky, {.door = u8"the sky's recording", .rows = {.measure = 300}})),
      rows(u8"the sky's recording")));
  EXPECT_TRUE(sameDrawing(
      onTheSheet(kit::connectionReadout(sky, {.rows = {.measure = 300}})),
      rows(u8"ws://:8849/sky")));
}

TEST(SketchKitConnectionReadout, AFailedDoorReadsItsErrorWhereTheSenderStood) {
  Hub hub;
  hub.setFeedTransport("ws", binding("ws://127.0.0.1:8850"));
  data::Connection sky(hub, "ws://:8850/sky");
  inletOf(sky.feed()).fail("port 8850 is in use");
  ASSERT_EQ(sky.error(), "port 8850 is in use");

  const Element readout =
      onTheSheet(kit::connectionReadout(sky, {.rows = {.measure = 300}}));
  EXPECT_TRUE(sameDrawing(
      readout,
      byHand({{.name = u8"door", .value = u8"ws://127.0.0.1:8850"},
              {.name = u8"revision", .value = u8"0"},
              {.name = u8"dropped", .value = u8"0"},
              {.name = u8"undecodable", .value = u8"0"},
              {.name = u8"error", .value = u8"port 8850 is in use"}})));
  // What gives the case its power: the sender's row is another picture.
  EXPECT_FALSE(sameDrawing(
      onTheSheet(kit::connectionReadout(sky, {.rows = {.measure = 300}})),
      byHand({{.name = u8"door", .value = u8"ws://127.0.0.1:8850"},
              {.name = u8"revision", .value = u8"0"},
              {.name = u8"dropped", .value = u8"0"},
              {.name = u8"undecodable", .value = u8"0"},
              {.name = u8"sender", .value = u8"-"}})));
}

TEST(SketchKitConnectionReadout,
     ADoorThatNeverOpenedReadsWhyWhereTheSenderStood) {
  // No transport is registered for the scheme, so the URI opens nothing:
  // no door stands, no address is bound, nothing arrives, and the feed
  // says why.
  Hub hub;
  data::Connection sky(hub, "ws://:8851/sky");
  ASSERT_FALSE(sky.feed().state().isOpen());
  ASSERT_TRUE(sky.localAddress().empty());
  const std::string why = sky.error();
  ASSERT_FALSE(why.empty());

  const auto rows = [&why](std::u8string_view door) {
    return byHand({{.name = u8"door", .value = door},
                   {.name = u8"revision", .value = u8"0"},
                   {.name = u8"dropped", .value = u8"0"},
                   {.name = u8"undecodable", .value = u8"0"},
                   {.name = u8"error", .value = compose::Utf8(why)}});
  };
  EXPECT_TRUE(sameDrawing(
      onTheSheet(kit::connectionReadout(sky, {.rows = {.measure = 300}})),
      rows(u8"ws://:8851/sky")));
  EXPECT_TRUE(sameDrawing(
      onTheSheet(kit::connectionReadout(
          sky, {.door = u8"the sky's door", .rows = {.measure = 300}})),
      rows(u8"the sky's door")));
}

}  // namespace
