/** The FlatBuffer decoder: a FlatBuffer read in place once verified, the
 *  schema's own JSON form converted through the schema the generated
 *  root carries, and a hub answering either form for a file. */

#include <gtest/gtest.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigilio/hub/Hub.h>

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "ScratchDir.h"
#include "flatbuffer_test_generated.h"

using sigil::data::FlatBuffer;
using sigil::data::flatBufferFromBytes;
using sigil::data::flatBufferFromJson;
using sigil::data::registerFlatBuffer;
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
