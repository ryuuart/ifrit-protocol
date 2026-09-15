/** The FlatBuffer decoder: a FlatBuffer read in place once verified, the
 *  schema's own JSON form converted through the schema the generated
 *  root carries, the same schema made out of a binary schema's own
 *  bytes, and a hub answering either form for a file. */

#include <gtest/gtest.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigilio/hub/Hub.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ScratchDir.h"
#include "flatbuffer_test_generated.h"

using sigil::data::FlatBuffer;
using sigil::data::flatBufferFromBytes;
using sigil::data::flatBufferFromJson;
using sigil::data::registerFlatBuffer;
using sigil::data::Schema;
using sigil::data::schema;
using sigil::test::ScratchDir;

namespace {

/** One sheet of two readings, built the way a sender builds one. */
std::vector<uint8_t> sheet() {
  flatbuffers::FlatBufferBuilder builder;
  const std::vector<flatbuffers::Offset<flatbuffer_test::Reading>> rows{
      flatbuffer_test::CreateReadingDirect(builder, "a", 2.5f),
      flatbuffer_test::CreateReadingDirect(builder, "c", -1.0f)};
  builder.Finish(flatbuffer_test::CreateSheetDirect(builder, &rows));
  return {builder.GetBufferPointer(),
          builder.GetBufferPointer() + builder.GetSize()};
}

TEST(DataFlatBuffer, ABufferIsReadInPlaceOnceVerified) {
  const std::vector<uint8_t> bytes = sheet();
  const std::optional<FlatBuffer<flatbuffer_test::Sheet>> flat =
      flatBufferFromBytes<flatbuffer_test::Sheet>(bytes);
  ASSERT_TRUE(flat);
  ASSERT_EQ(2u, (*flat)->readings()->size());
  EXPECT_EQ("a", (*flat)->readings()->Get(0)->name()->str());
  EXPECT_FLOAT_EQ(2.5f, (*flat)->readings()->Get(0)->value());
  EXPECT_EQ(bytes.size(), flat->bytes().size());
  // A buffer cut short is refused before any of it is read.
  std::string why;
  EXPECT_FALSE(flatBufferFromBytes<flatbuffer_test::Sheet>(
      std::span<const uint8_t>(bytes).first(bytes.size() / 2), &why));
  EXPECT_FALSE(why.empty());
}

TEST(DataFlatBuffer, TheJsonFormConvertsThroughTheSchemaTheRootCarries) {
  const std::optional<FlatBuffer<flatbuffer_test::Sheet>> flat =
      flatBufferFromJson<flatbuffer_test::Sheet>(
          R"({"readings": [{"name": "a", "value": 2.5}]})");
  ASSERT_TRUE(flat);
  ASSERT_EQ(1u, (*flat)->readings()->size());
  EXPECT_FLOAT_EQ(2.5f, (*flat)->readings()->Get(0)->value());
  // A value that is not the schema's is refused, with the parser's own
  // message naming what it found.
  std::string why;
  EXPECT_FALSE(flatBufferFromJson<flatbuffer_test::Sheet>(
      R"({"readings": [{"name": "a", "value": "tall"}]})", &why));
  EXPECT_NE(std::string::npos, why.find("tall"));
}

TEST(DataFlatBuffer, ASchemaIsMadeOutOfABinarySchemasOwnBytes) {
  // The bytes a generated header embeds are the bytes `flatc -b
  // --schema` writes into a `.bfbs` file, so this is the file a tool
  // with no generated header would be handed.
  const std::span<const std::byte> embedded(
      reinterpret_cast<const std::byte*>(
          flatbuffer_test::Sheet::BinarySchema::data()),
      flatbuffer_test::Sheet::BinarySchema::size());

  std::string why;
  const Schema fromBytes = Schema::fromBinarySchema(embedded, &why);
  ASSERT_TRUE(static_cast<bool>(fromBytes)) << why;
  EXPECT_TRUE(why.empty());
  EXPECT_EQ("flatbuffer_test.Sheet", fromBytes.rootName());

  // One schema however it was made: the token from the bytes converts
  // a message to the same form the token from the type does, which is
  // what lets a tool read a wire a generated header was never built
  // for.
  const Schema fromType = schema<flatbuffer_test::Sheet>();
  const std::vector<uint8_t> bytes = sheet();
  const std::span<const std::byte> message(
      reinterpret_cast<const std::byte*>(bytes.data()), bytes.size());
  const std::optional<std::string> form = fromBytes.text(message);
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(fromType.text(message), form);
  EXPECT_EQ(fromBytes.binary(*form), fromType.binary(*form));

  // Bytes that are no schema make no schema, and say what stopped them
  // rather than a token that silently converts nothing.
  const std::vector<std::byte> garbage(64, std::byte{0x5A});
  why.clear();
  const Schema none = Schema::fromBinarySchema(garbage, &why);
  EXPECT_FALSE(static_cast<bool>(none));
  EXPECT_FALSE(why.empty());
  EXPECT_TRUE(none.rootName().empty());
  // And no bytes at all are no schema either.
  why.clear();
  EXPECT_FALSE(static_cast<bool>(Schema::fromBinarySchema({}, &why)));
  EXPECT_FALSE(why.empty());
}

TEST(DataFlatBuffer, AHubAnswersAFileInEitherForm) {
  const ScratchDir scratch("data_flat_hub");
  scratch.write("sheet.json", R"({"readings": [{"name": "a", "value": 2.5}]})");
  const std::vector<uint8_t> bytes = sheet();
  scratch.write("sheet.bin", std::string(bytes.begin(), bytes.end()));

  sigil::io::Hub hub;
  hub.mount("res://", scratch.path);
  registerFlatBuffer<flatbuffer_test::Sheet>(hub);
  const std::shared_ptr<const FlatBuffer<flatbuffer_test::Sheet>> fromJson =
      hub.load<FlatBuffer<flatbuffer_test::Sheet>>("res://sheet.json");
  const std::shared_ptr<const FlatBuffer<flatbuffer_test::Sheet>> fromBytes =
      hub.load<FlatBuffer<flatbuffer_test::Sheet>>("res://sheet.bin");
  ASSERT_TRUE(fromJson);
  ASSERT_TRUE(fromBytes);
  EXPECT_FLOAT_EQ(2.5f, (*fromJson)->readings()->Get(0)->value());
  EXPECT_EQ("c", (*fromBytes)->readings()->Get(1)->name()->str());
  // One view per resource: a second ask is the same decoded value.
  EXPECT_EQ(
      fromJson.get(),
      hub.load<FlatBuffer<flatbuffer_test::Sheet>>("res://sheet.json").get());
}

}  // namespace
