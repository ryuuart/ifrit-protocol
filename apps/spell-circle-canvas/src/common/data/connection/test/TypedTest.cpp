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
#include <sigilio/testing/Testing.h>
#include <sigilio/advanced/Time.h>

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
using sigil::io::testing::inletOf;

namespace sheet = flatbuffer_test::values;

namespace {

/** A TRANSPORT WITH NO SOCKET UNDER IT: it opens every URI it is given
 *  and swallows what is sent, while what arrives a case puts on the
 *  feed through its inlet, so one thread runs a case from its first line
 *  to its last and no port has to be free for it to pass. */
sigil::io::Transport intoNowhere() {
  return [](std::string_view uri, sigil::io::Inlet) {
    sigil::io::TransportEnd opened;
    opened.localAddress = std::string(uri);
    opened.send = [](const Bytes&) { return true; };
    return opened;
  };
}

Bytes bytesOf(std::string_view text) {
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  return Bytes(std::span(first, text.size()));
}

Bytes bytesOf(std::vector<std::byte> buffer) {
  return Bytes(std::move(buffer));
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
  sigil::io::registerTransport(hub, "ws", intoNowhere());

  Connection door = connect(hub, "ws://:8850/sheet",
                            {.schema = schema<flatbuffer_test::Sheet>()});
  // Nothing has arrived, so there is no value to hand out.
  EXPECT_FALSE(door.latest().as<sheet::Sheet>());
  EXPECT_FALSE(door.latest().bytes());

  inletOf(door.feed()).deliver(bytesOf(sheet::writeSheet(aSheet())));
  // THE FRAME IS WHAT READS IT. A delivery the advance has not taken off
  // the feed yet is no message here.
  EXPECT_FALSE(door.latest().as<sheet::Sheet>());
  EXPECT_TRUE(door.latest().empty());

  sigil::io::advance(hub, std::chrono::duration<double>(0.0));

  const std::optional<sheet::Sheet> read = door.latest().as<sheet::Sheet>();
  ASSERT_TRUE(read);
  ASSERT_EQ(read->readings.size(), 2u);
  EXPECT_EQ(read->readings[0].name, "a");
  EXPECT_FLOAT_EQ(read->readings[0].value, 2.5f);
  EXPECT_EQ(read->readings[1].name, "c");
  EXPECT_FLOAT_EQ(read->readings[1].value, -1.0f);
  // The same message reads as the schema's JSON form, and its bytes are
  // the buffer as it arrived.
  EXPECT_EQ(door.latest()["readings"][1]["name"].string(), "c");
  ASSERT_TRUE(door.latest().bytes());
  EXPECT_EQ(*door.latest().bytes(), Bytes(sheet::writeSheet(aSheet())));

  // And it is a reading rather than a latch: asking again reads the
  // same bytes again and answers the same value.
  const std::optional<sheet::Sheet> twice = door.latest().as<sheet::Sheet>();
  ASSERT_TRUE(twice);
  EXPECT_EQ(twice->readings.size(), 2u);

  // A buffer at a door read as JSON text is no message there: it is
  // counted against the door and leaves nothing to read.
  Connection plain = connect(hub, "ws://:8854/sheet");
  inletOf(plain.feed()).deliver(bytesOf(sheet::writeSheet(aSheet())));
  sigil::io::advance(hub, std::chrono::duration<double>(0.0));
  EXPECT_TRUE(plain.latest().empty());
  EXPECT_EQ(plain.state().undecodable, 1u);
}

TEST(DataTyped, TheSchemasJsonFormReadsAsTheSameValue) {
  Hub hub;
  sigil::io::registerTransport(hub, "ws", intoNowhere());

  Connection door = connect(hub, "ws://:8851/sheet", {.schema = schema<flatbuffer_test::Sheet>()});
  ASSERT_TRUE(door.schema());
  inletOf(door.feed()).deliver(bytesOf(R"({"readings": [{"name": "a", "value": 2.5},)"
                                       R"( {"name": "c", "value": -1.0}]})"));
  sigil::io::advance(hub, std::chrono::duration<double>(0.0));

  const std::optional<sheet::Sheet> read = door.latest().as<sheet::Sheet>();
  ASSERT_TRUE(read);
  ASSERT_EQ(read->readings.size(), 2u);
  EXPECT_EQ(read->readings[0].name, "a");
  EXPECT_FLOAT_EQ(read->readings[0].value, 2.5f);
  EXPECT_FLOAT_EQ(read->readings[1].value, -1.0f);
  // Both readings of one door say the same thing: a sender speaking the
  // schema's JSON form is a sender speaking the schema.
  EXPECT_EQ(door.latest()["readings"][0]["name"].string(), "a");

  // A buffer on that same door reads as a value too, which form an
  // arrival is in being read off the bytes and not off the door. A
  // different sheet, so which frame's message is answered is visible.
  sheet::Sheet later;
  later.readings = {sheet::Reading{.name = "z", .value = 9.0f}};
  inletOf(door.feed()).deliver(bytesOf(sheet::writeSheet(later)));

  const std::optional<sheet::Sheet> before = door.latest().as<sheet::Sheet>();
  ASSERT_TRUE(before);
  EXPECT_EQ(before->readings.size(), 2u);  // still the frame's own message

  sigil::io::advance(hub, std::chrono::duration<double>(0.0));
  const std::optional<sheet::Sheet> again = door.latest().as<sheet::Sheet>();
  ASSERT_TRUE(again);
  ASSERT_EQ(again->readings.size(), 1u);
  EXPECT_EQ(again->readings[0].name, "z");
  EXPECT_FLOAT_EQ(again->readings[0].value, 9.0f);
  EXPECT_EQ(door.state().undecodable, 0u);
}

TEST(DataTyped, BytesThatAreNoSheetReadAsNothing) {
  Hub hub;
  sigil::io::registerTransport(hub, "ws", intoNowhere());

  Connection plain = connect(hub, "ws://:8852/sheet");
  inletOf(plain.feed()).deliver(bytesOf("not a sheet at all"));
  sigil::io::advance(hub, std::chrono::duration<double>(0.0));
  // The bytes do not verify as the root the value is read from, so
  // there is no value rather than a reading of whatever they were.
  EXPECT_FALSE(plain.latest().as<sheet::Sheet>());

  // Through a schema the refusal is the schema's: text it cannot hold
  // makes no buffer, and there is nothing to read a value out of.
  Connection through = connect(hub, "ws://:8853/sheet", {.schema = schema<flatbuffer_test::Sheet>()});
  inletOf(through.feed()).deliver(
              bytesOf(R"({"readings": [{"name": "a", "value": "tall"}]})"));
  sigil::io::advance(hub, std::chrono::duration<double>(0.0));
  EXPECT_FALSE(through.latest().as<sheet::Sheet>());
  EXPECT_EQ(through.state().undecodable, 1u);
}

TEST(DataTyped, AConnectionOntoNothingReadsAsNothing) {
  const Connection none;
  EXPECT_FALSE(none.latest().as<sheet::Sheet>());
  EXPECT_FALSE(none.latest().bytes());
  EXPECT_FALSE(none.schema());
  EXPECT_EQ(none.state().revision, 0u);
}

}  // namespace
