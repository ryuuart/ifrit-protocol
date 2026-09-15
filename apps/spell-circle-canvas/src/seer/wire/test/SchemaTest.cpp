/** @file
 * The reading a schema makes: a buffer of the schema's own root shown
 * as that schema's form, the same form arriving as text shown as what
 * the door at the other end would hold, and a message that fits neither
 * shown as nothing with the sentence that says why.
 *
 * The schema is read out of the FILE the build wrote beside the
 * generated header, because a file is what a reader hands the tool: the
 * reading has to work for a wire whose types this binary was never
 * compiled against, and reading the file is how that is asserted.
 */

#include <gtest/gtest.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigilseer/wire/Rendering.h>
#include <sigilseer/wire/Wires.h>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include "seer_test_generated.h"

using sigil::data::Schema;
using sigil::io::Bytes;
using sigil::seer::schemaReading;
using sigil::seer::Wires;

namespace {

Bytes bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  Bytes out;
  out.bytes.assign(first, first + text.size());
  return out;
}

/** One message of the test schema, built the way a sender builds one. */
Bytes ping(int hops, const char* label) {
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(seer_test::CreatePingDirect(builder, hops, label));
  const auto* const first =
      reinterpret_cast<const std::byte*>(builder.GetBufferPointer());
  Bytes out;
  out.bytes.assign(first, first + builder.GetSize());
  return out;
}

/** The schema as a reader hands it over: the file the build wrote,
 *  read through a hub the way the tool reads it. The token copies the
 *  bytes, so it stands after the hub that answered them is gone. */
Schema schemaFromFile() {
  Wires wires;
  const std::shared_ptr<const Bytes> file =
      wires.hub().blob(SEER_TEST_SCHEMA_FILE);
  if (!file) return {};
  return Schema::fromBinarySchema(file->bytes);
}

/** What both readings below come to: the schema's own form, laid out
 *  the way every other reading of a message is. */
constexpr std::string_view kForm =
    "{\n"
    "  \"hops\": 3,\n"
    "  \"label\": \"north\"\n"
    "}";

}  // namespace

TEST(SeerSchema, AWireIsReadThroughTheSchemaAFileHolds) {
  Wires wires;
  // The schema comes off the disk through the same door every other
  // resource does: a path is a URI a hub resolves, so a reader who was
  // handed a file names it and nothing else.
  const std::shared_ptr<const Bytes> file =
      wires.hub().blob(SEER_TEST_SCHEMA_FILE);
  ASSERT_NE(file, nullptr);

  std::string why;
  const Schema schema = Schema::fromBinarySchema(file->bytes, &why);
  ASSERT_TRUE(static_cast<bool>(schema)) << why;
  EXPECT_EQ(schema.rootName(), "seer_test.Ping");

  // One schema stands over every wire, because the messages are what it
  // was handed over for and they cross whichever wire was opened.
  EXPECT_FALSE(static_cast<bool>(wires.schema()));
  wires.readThrough(schema);
  EXPECT_TRUE(static_cast<bool>(wires.schema()));
  EXPECT_EQ(wires.schema().rootName(), "seer_test.Ping");

  EXPECT_EQ(schemaReading(ping(3, "north"), wires.schema()), kForm);
}

TEST(SeerSchema, TheSchemasOwnTextIsShownAsTheDoorWouldHoldIt) {
  const Schema schema = schemaFromFile();
  ASSERT_TRUE(static_cast<bool>(schema));

  // A message that is the schema's form rather than a buffer is
  // converted through the schema and back, so what is shown is what a
  // door reading this wire holds — every field the schema declares,
  // in the schema's order — and not the text that happened to arrive.
  EXPECT_EQ(schemaReading(bytesOf(R"({"label": "north", "hops": 3})"), schema),
            kForm);
}

TEST(SeerSchema, AMessageTheSchemaCannotHoldIsNoReadingAndSaysWhy) {
  const Schema schema = schemaFromFile();
  ASSERT_TRUE(static_cast<bool>(schema));

  // A document carrying a field the schema does not declare is not read
  // past: half a message read is a reader told something that did not
  // arrive.
  std::string why;
  EXPECT_TRUE(
      schemaReading(bytesOf(R"({"wingspan": 3})"), schema, &why).empty());
  EXPECT_FALSE(why.empty());

  // Bytes that are no buffer of this root are refused before any of
  // them is read.
  why.clear();
  EXPECT_TRUE(schemaReading(bytesOf("a scene arrives"), schema, &why).empty());
  EXPECT_FALSE(why.empty());

  // And with no schema over the wire there is no such reading at all,
  // which is what leaves the tab empty until a reader hands one over.
  why.clear();
  EXPECT_TRUE(schemaReading(ping(3, "north"), Schema(), &why).empty());
  EXPECT_FALSE(why.empty());
}
