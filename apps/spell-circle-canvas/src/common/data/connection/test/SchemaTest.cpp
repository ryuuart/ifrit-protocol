/** The schema as a value, and the door read through one: what the two
 *  conversions answer and what they refuse, what a JSON arrival that
 *  fits becomes and what one that does not costs, what a buffer arrives
 *  as, what a send writes, and what an OSC door does with a schema it
 *  cannot speak.
 */

#include <gtest/gtest.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigildata/decode/Json.h>
#include <sigilio/hub/Hub.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "flatbuffer_test_generated.h"

using namespace sigil::data;
using sigil::io::Bytes;
using sigil::io::Hub;

namespace {

/** The schema every case here reads through: a sheet of named
 *  readings, whose generated header carries its own binary schema. */
Schema sheetSchema() { return schema<flatbuffer_test::Sheet>(); }

/** What a case reads to say what went out: every message a connection
 *  sent, in order. */
using Sent = std::vector<std::vector<std::byte>>;

/** A TRANSPORT WITH NO SOCKET UNDER IT: it opens every URI it is given
 *  and takes what is sent into @p sent, while what arrives a case
 *  delivers into the feed itself, so one thread runs a case from its
 *  first line to its last and no port has to be free for it to pass. */
sigil::io::FeedTransport intoVector(std::shared_ptr<Sent> sent) {
  return [sent](std::string_view uri, std::weak_ptr<sigil::io::Feed>) {
    sigil::io::OpenedFeed opened;
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

Bytes bytesOf(std::vector<std::byte> buffer) {
  Bytes bytes;
  bytes.bytes = std::move(buffer);
  return bytes;
}

/** One sheet of two readings, built the way a sender that speaks the
 *  schema builds one. */
std::vector<std::byte> builtSheet() {
  flatbuffers::FlatBufferBuilder builder;
  const std::vector<flatbuffers::Offset<flatbuffer_test::Reading>> rows{
      flatbuffer_test::CreateReadingDirect(builder, "a", 2.5f),
      flatbuffer_test::CreateReadingDirect(builder, "c", -1.0f)};
  builder.Finish(flatbuffer_test::CreateSheetDirect(builder, &rows));
  const auto* first =
      reinterpret_cast<const std::byte*>(builder.GetBufferPointer());
  return {first, first + builder.GetSize()};
}

/** The message the cases send and expect back: one reading of a sheet,
 *  as the value a sender writes. */
Json oneReading(double value) {
  return Json(Json::Object{
      {"readings", Json(Json::Array{Json(Json::Object{
                       {"name", Json("a")}, {"value", Json(value)}})})}});
}

TEST(DataSchema, TheJsonFormGoesToABufferAndBackToTheSameText) {
  const Schema sheet = sheetSchema();
  ASSERT_TRUE(sheet);
  EXPECT_EQ(sheet.rootName(), "flatbuffer_test.Sheet");

  const std::optional<std::vector<std::byte>> buffer =
      sheet.binary(R"({"readings": [{"name": "a", "value": 2.5}]})");
  ASSERT_TRUE(buffer.has_value());
  const std::optional<std::string> form = sheet.text(*buffer);
  ASSERT_TRUE(form.has_value());
  const std::optional<Json> read = decodeJson(*form);
  ASSERT_TRUE(read.has_value());
  EXPECT_EQ((*read)["readings"][0]["name"].text(), "a");
  EXPECT_DOUBLE_EQ((*read)["readings"][0]["value"].number(), 2.5);

  // The schema's form is where both conversions come to rest: what one
  // wrote the other reads back to the same buffer and prints as the
  // same text, so a message converted twice is the message once.
  const std::optional<std::vector<std::byte>> again = sheet.binary(*form);
  ASSERT_TRUE(again.has_value());
  EXPECT_EQ(*again, *buffer);
  EXPECT_EQ(sheet.text(*again), form);
}

TEST(DataSchema, WhatDoesNotFitTheSchemaIsNeitherTextNorBuffer) {
  const Schema sheet = sheetSchema();
  // A value of the wrong type, with the parser's own message naming
  // what it found.
  std::string why;
  EXPECT_FALSE(
      sheet.binary(R"({"readings": [{"name": "a", "value": "tall"}]})", &why));
  EXPECT_NE(std::string::npos, why.find("tall"));
  // A field the schema does not declare is not skipped past.
  why.clear();
  EXPECT_FALSE(sheet.binary(R"({"gust": 0.5})", &why));
  EXPECT_FALSE(why.empty());
  // A buffer cut short is refused before any of it is read, and no
  // bytes are no buffer.
  const std::vector<std::byte> whole = builtSheet();
  EXPECT_FALSE(
      sheet.text(std::span<const std::byte>(whole).first(whole.size() / 2)));
  EXPECT_FALSE(sheet.text({}));
  // And the schema that is none converts nothing either way.
  const Schema nothing;
  EXPECT_FALSE(nothing);
  EXPECT_TRUE(nothing.rootName().empty());
  EXPECT_FALSE(nothing.binary(R"({"readings": []})"));
  EXPECT_FALSE(nothing.text(builtSheet()));
}

TEST(DataSchema, AJsonArrivalThatFitsIsTheLatestInTheSchemasForm) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  Connection sheet(hub, "ws://:8848/sheet", sheetSchema());
  int handled = 0;
  sheet.on("*", [&handled](const Json&) { ++handled; });

  sheet.feed()->deliver(bytesOf(R"({"readings":[{"name":"a","value":2.5}]})"));
  hub.dispatch(0.0);

  EXPECT_EQ(handled, 1);
  EXPECT_EQ(sheet.undecodable(), 0u);
  EXPECT_EQ(sheet.generation(), 1u);
  EXPECT_EQ(sheet.latest()["readings"][0]["name"].text(), "a");
  EXPECT_DOUBLE_EQ(sheet.latest()["readings"][0]["value"].number(), 2.5);
}

TEST(DataSchema, AJsonArrivalThatDoesNotFitLeavesTheLatestStanding) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  Connection sheet(hub, "ws://:8848/sheet", sheetSchema());
  int handled = 0;
  sheet.on("*", [&handled](const Json&) { ++handled; });

  sheet.feed()->deliver(bytesOf(R"({"readings":[{"name":"a","value":2.5}]})"));
  // A document the schema does not declare a field of. It is a document
  // and it is not this schema's, which is the whole difference a schema
  // makes: without one it would be the newest message.
  sheet.feed()->deliver(bytesOf(R"({"gust":0.5})"));
  hub.dispatch(0.0);

  EXPECT_EQ(handled, 1);
  EXPECT_EQ(sheet.undecodable(), 1u);
  EXPECT_EQ(sheet.generation(), 2u);  // the feed took it; no reader saw it
  EXPECT_DOUBLE_EQ(sheet.latest()["readings"][0]["value"].number(), 2.5);
}

TEST(DataSchema, ABufferArrivesAsItsOwnJsonForm) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  Connection sheet(hub, "ws://:8848/sheet", sheetSchema());
  sheet.feed()->deliver(bytesOf(builtSheet()));
  hub.dispatch(0.0);

  // The same reading as the JSON form: which form the sender wrote is
  // not something a reader has to know.
  EXPECT_EQ(sheet.undecodable(), 0u);
  EXPECT_EQ(sheet.latest()["readings"][0]["name"].text(), "a");
  EXPECT_DOUBLE_EQ(sheet.latest()["readings"][0]["value"].number(), 2.5);
  EXPECT_EQ(sheet.latest()["readings"][1]["name"].text(), "c");
  EXPECT_DOUBLE_EQ(sheet.latest()["readings"][1]["value"].number(), -1.0);
}

TEST(DataSchema, SendWritesTheBufferTheSchemaMakesOfTheMessage) {
  Hub hub;
  const auto sent = std::make_shared<Sent>();
  hub.setFeedTransport("ws", intoVector(sent));
  const Connection sheet(hub, "ws://:8848/sheet", sheetSchema());

  ASSERT_TRUE(sheet.send(oneReading(2.5)));
  ASSERT_EQ(sent->size(), 1u);
  // What went out is a buffer, not the text: it verifies as the root
  // and reads in place.
  const std::optional<FlatBuffer<flatbuffer_test::Sheet>> verified =
      flatBufferFromBytes<flatbuffer_test::Sheet>(
          {reinterpret_cast<const uint8_t*>(sent->front().data()),
           sent->front().size()});
  ASSERT_TRUE(verified.has_value());
  ASSERT_EQ(1u, (*verified)->readings()->size());
  EXPECT_EQ("a", (*verified)->readings()->Get(0)->name()->str());
  EXPECT_FLOAT_EQ(2.5f, (*verified)->readings()->Get(0)->value());

  // A message the schema cannot hold does not go out at all, the
  // address-and-arguments spelling among them: the schema declares
  // neither of those fields.
  EXPECT_FALSE(sheet.send(Json(Json::Object{{"gust", Json(0.5)}})));
  EXPECT_FALSE(sheet.send("/sheet/ping", Json(Json::Array{})));
  EXPECT_EQ(sent->size(), 1u);
}

TEST(DataSchema, AnOscDoorTakesNoSchema) {
  Hub hub;
  hub.setFeedTransport("osc", intoVector(std::make_shared<Sent>()));

  const Connection desk(hub, "osc://:9000", sheetSchema());
  EXPECT_FALSE(desk.error().empty());
  // Refused is not opened: nothing was bound, so nothing arrives and
  // nothing goes out.
  EXPECT_EQ(desk.feed(), nullptr);
  EXPECT_TRUE(desk.closed());
  EXPECT_TRUE(desk.latest().null());
  EXPECT_EQ(desk.generation(), 0u);
  EXPECT_FALSE(desk.send(oneReading(2.5)));
  EXPECT_EQ(desk.uri(), "osc://:9000");

  // The same door with no schema is the door it always was.
  const Connection open(hub, "osc://:9000");
  EXPECT_TRUE(open.error().empty());
  EXPECT_NE(open.feed(), nullptr);
}

}  // namespace
