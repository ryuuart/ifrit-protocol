/** The typed door: the newest message read as the value type its
 *  schema's generated header declares — a buffer that arrives on a door
 *  read as JSON, the schema's own JSON form on a door read through one,
 *  bytes that are no sheet at all, a connection onto nothing, and, of
 *  every one of them, that the frame is what reads.
 */

#include <gtest/gtest.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Json.h>
#include <sigilio/hub/Hub.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "flatbuffer_test_generated.h"
#include "flatbuffer_test_values.h"

using namespace sigil::data;
using sigil::io::Bytes;
using sigil::io::Hub;

namespace sheet = flatbuffer_test::values;

namespace {

/** A TRANSPORT WITH NO SOCKET UNDER IT: it opens every URI it is given
 *  and swallows what is sent, while what arrives a case delivers into
 *  the feed itself, so one thread runs a case from its first line to
 *  its last and no port has to be free for it to pass. */
sigil::io::FeedTransport intoNowhere() {
  return [](std::string_view uri, std::weak_ptr<sigil::io::Feed>) {
    sigil::io::OpenedFeed opened;
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

Bytes bytesOf(std::vector<std::byte> buffer) {
  Bytes bytes;
  bytes.bytes = std::move(buffer);
  return bytes;
}

/** The sheet every case reads: two readings, each named and measured. */
sheet::Sheet aSheet() {
  sheet::Sheet page;
  page.readings = {sheet::Reading{.name = "a", .value = 2.5f},
                   sheet::Reading{.name = "c", .value = -1.0f}};
  return page;
}

TEST(DataTyped, ABufferOnTheDoorIsTheNewestValue) {
  Hub hub;
  hub.setFeedTransport("ws", intoNowhere());

  Connection door(hub, "ws://:8850/sheet");
  // Nothing has arrived, so there is no value to hand out.
  EXPECT_FALSE(door.latest<sheet::Sheet>());
  EXPECT_FALSE(door.latestBytes());

  door.feed()->deliver(bytesOf(sheet::writeSheet(aSheet())));
  // THE FRAME IS WHAT READS IT. A delivery the dispatch has not taken
  // off the feed yet is no message here, exactly as it is none to the
  // reading beside this one.
  EXPECT_FALSE(door.latest<sheet::Sheet>());
  EXPECT_TRUE(door.latest().null());

  hub.dispatch(0.0);

  const std::optional<sheet::Sheet> read = door.latest<sheet::Sheet>();
  ASSERT_TRUE(read);
  ASSERT_EQ(read->readings.size(), 2u);
  EXPECT_EQ(read->readings[0].name, "a");
  EXPECT_FLOAT_EQ(read->readings[0].value, 2.5f);
  EXPECT_EQ(read->readings[1].name, "c");
  EXPECT_FLOAT_EQ(read->readings[1].value, -1.0f);

  // The typed reading is a reading of the BYTES the dispatch latched.
  // This door was opened with no schema, so its messages are read as
  // JSON text, which a buffer is not: it has no Json message at all and
  // counts the arrival against itself, and the bytes are there and the
  // value is handed out all the same.
  EXPECT_TRUE(door.latest().null());
  EXPECT_EQ(door.undecodable(), 1u);
  ASSERT_TRUE(door.latestBytes());
  EXPECT_EQ(door.latestBytes()->bytes, sheet::writeSheet(aSheet()));

  // And it is a reading rather than a latch: asking again reads the
  // same bytes again and answers the same value.
  const std::optional<sheet::Sheet> twice = door.latest<sheet::Sheet>();
  ASSERT_TRUE(twice);
  EXPECT_EQ(twice->readings.size(), 2u);
}

TEST(DataTyped, TheSchemasJsonFormReadsAsTheSameValue) {
  Hub hub;
  hub.setFeedTransport("ws", intoNowhere());

  Connection door(hub, "ws://:8851/sheet", schema<flatbuffer_test::Sheet>());
  ASSERT_TRUE(door.schema());
  door.feed()->deliver(bytesOf(R"({"readings": [{"name": "a", "value": 2.5},)"
                               R"( {"name": "c", "value": -1.0}]})"));
  hub.dispatch(0.0);

  const std::optional<sheet::Sheet> read = door.latest<sheet::Sheet>();
  ASSERT_TRUE(read);
  ASSERT_EQ(read->readings.size(), 2u);
  EXPECT_EQ(read->readings[0].name, "a");
  EXPECT_FLOAT_EQ(read->readings[0].value, 2.5f);
  EXPECT_FLOAT_EQ(read->readings[1].value, -1.0f);
  // Both readings of one door say the same thing: a sender speaking the
  // schema's JSON form is a sender speaking the schema.
  EXPECT_EQ(door.latest()["readings"][0]["name"].text(), "a");

  // A buffer on that same door reads as a value too, which form an
  // arrival is in being read off the bytes and not off the door. A
  // different sheet, so which frame's message is answered is visible.
  sheet::Sheet later;
  later.readings = {sheet::Reading{.name = "z", .value = 9.0f}};
  door.feed()->deliver(bytesOf(sheet::writeSheet(later)));

  const std::optional<sheet::Sheet> before = door.latest<sheet::Sheet>();
  ASSERT_TRUE(before);
  EXPECT_EQ(before->readings.size(), 2u);  // still the frame's own message

  hub.dispatch(0.0);
  const std::optional<sheet::Sheet> again = door.latest<sheet::Sheet>();
  ASSERT_TRUE(again);
  ASSERT_EQ(again->readings.size(), 1u);
  EXPECT_EQ(again->readings[0].name, "z");
  EXPECT_FLOAT_EQ(again->readings[0].value, 9.0f);
  EXPECT_EQ(door.undecodable(), 0u);
}

TEST(DataTyped, BytesThatAreNoSheetReadAsNothing) {
  Hub hub;
  hub.setFeedTransport("ws", intoNowhere());

  Connection plain(hub, "ws://:8852/sheet");
  plain.feed()->deliver(bytesOf("not a sheet at all"));
  hub.dispatch(0.0);
  // The bytes do not verify as the root the value is read from, so
  // there is no value rather than a reading of whatever they were.
  EXPECT_FALSE(plain.latest<sheet::Sheet>());

  // Through a schema the refusal is the schema's: text it cannot hold
  // makes no buffer, and there is nothing to read a value out of.
  Connection through(hub, "ws://:8853/sheet", schema<flatbuffer_test::Sheet>());
  through.feed()->deliver(
      bytesOf(R"({"readings": [{"name": "a", "value": "tall"}]})"));
  hub.dispatch(0.0);
  EXPECT_FALSE(through.latest<sheet::Sheet>());
  EXPECT_EQ(through.undecodable(), 1u);
}

TEST(DataTyped, AConnectionOntoNothingReadsAsNothing) {
  const Connection none;
  EXPECT_FALSE(none.latest<sheet::Sheet>());
  EXPECT_FALSE(none.latestBytes());
  EXPECT_FALSE(none.schema());
  EXPECT_EQ(none.generation(), 0u);
}

}  // namespace
