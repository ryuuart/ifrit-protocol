/** The schema token on its own: one translation unit that opens the
 *  header declaring it and the generated header carrying the schema,
 *  and nothing else. It makes a schema out of bytes and converts both
 *  ways, so what compiles here is the whole of what a consumer holding
 *  a schema needs — and the reader under the token, whose headers this
 *  unit never opens, stays where it is put. */

#include <gtest/gtest.h>
#include <sigildata/decode/Schema.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "flatbuffer_test_generated.h"

using sigil::data::Schema;

namespace {

TEST(DataSchemaHeader, TheTokenConvertsWithNothingOfTheReaderInReach) {
  const std::span<const std::byte> embedded(
      reinterpret_cast<const std::byte*>(
          flatbuffer_test::Sheet::BinarySchema::data()),
      flatbuffer_test::Sheet::BinarySchema::size());

  std::string why;
  const Schema sheet = Schema::fromBinarySchema(embedded, &why);
  ASSERT_TRUE(static_cast<bool>(sheet)) << why;
  EXPECT_EQ("flatbuffer_test.Sheet", sheet.rootName());

  const std::optional<std::vector<std::byte>> buffer =
      sheet.binary(R"({"readings": [{"name": "a", "value": 2.5}]})");
  ASSERT_TRUE(buffer.has_value());
  // The check the conversions make, asked on its own: what a holder
  // that keeps the bytes proves once instead of converting them.
  EXPECT_TRUE(sheet.verifies(*buffer));
  why.clear();
  EXPECT_FALSE(sheet.verifies(std::span<const std::byte>(*buffer).first(4),
                              &why));
  EXPECT_FALSE(why.empty());
  const std::optional<std::string> form = sheet.text(*buffer);
  ASSERT_TRUE(form.has_value());
  EXPECT_NE(std::string::npos, form->find("\"name\""));
  EXPECT_EQ(sheet.binary(*form), buffer);
}

}  // namespace
