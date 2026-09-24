/** Every table of the definition across its JSON form: the text a value
 *  writes reads back as the same value and writes the same text again,
 *  byte for byte, for every parameter and result table and every event,
 *  from the value made with nothing set and from samples that set a
 *  field of every kind — an enumeration of another namespace, an
 *  optional scalar, a required table, vectors of strings and of tables.
 */

#include <flatbuffers/reflection.h>
#include <gtest/gtest.h>
#include <sigildata/values/Values.h>
#include <sigilprotocol/Tables.h>
#include <sigilprotocol/definition/Definition.h>
#include <sigilprotocol/protocol_values.h>

#include <cstddef>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace {

namespace data = sigil::data;
namespace protocol = sigil::protocol;

/** @p text read as Value, written, read and written again: the text is
 *  the table's own form after one crossing and never moves after. */
template <class Value>
void crossesUnchanged(std::string_view text, const std::string& name) {
  std::string why;
  const std::optional<Value> first = data::values::fromJson<Value>(text, &why);
  ASSERT_TRUE(first) << name << ": " << why;
  const std::optional<std::string> written = data::values::toJson(*first, &why);
  ASSERT_TRUE(written) << name << ": " << why;
  const std::optional<Value> second =
      data::values::fromJson<Value>(*written, &why);
  ASSERT_TRUE(second) << name << ": " << why;
  EXPECT_EQ(*first, *second) << name;
  EXPECT_EQ(written, data::values::toJson(*second)) << name;
}

template <class Value>
void valueCrossesUnchanged(const Value& value, const std::string& name) {
  std::string why;
  const std::optional<std::string> text = data::values::toJson(value, &why);
  ASSERT_TRUE(text) << name << ": " << why;
  const std::optional<Value> back = data::values::fromJson<Value>(*text, &why);
  ASSERT_TRUE(back) << name << ": " << why;
  EXPECT_EQ(value, *back) << name;
  crossesUnchanged<Value>(*text, name);
}

template <size_t... Index>
void everyTableCrosses(std::index_sequence<Index...>) {
  (valueCrossesUnchanged(std::tuple_element_t<Index, protocol::Tables>{},
                         std::string(protocol::tableNames[Index])),
   ...);
}

TEST(ProtocolRoundTrip, TheListIsEveryTableTheDefinitionDeclares) {
  const reflection::Schema* schema =
      reflection::GetSchema(protocol::definition().data());
  std::set<std::string> declared;
  for (const reflection::Object* table : *schema->objects())
    declared.insert(table->name()->str());
  std::set<std::string> listed(protocol::tableNames.begin(),
                               protocol::tableNames.end());
  EXPECT_EQ(declared, listed);
  EXPECT_EQ(std::tuple_size_v<protocol::Tables>, protocol::tableNames.size());
}

TEST(ProtocolRoundTrip, EveryTableMadeWithNothingSetCrossesUnchanged) {
  everyTableCrosses(
      std::make_index_sequence<std::tuple_size_v<protocol::Tables>>());
}

TEST(ProtocolRoundTrip, AValueMadeWithNothingSetHoldsTheDeclaredDefaults) {
  // The defaults the definition declares are what a caller gets without
  // saying anything, on either side of the wire.
  EXPECT_EQ(1u, protocol::clock::values::StepParameters{}.frames);
  EXPECT_EQ(60.0, protocol::clock::values::StepParameters{}.rate);
  EXPECT_FALSE(protocol::clock::values::StepParameters{}.seconds);
  EXPECT_TRUE(protocol::clock::values::PauseParameters{}.paused);
  EXPECT_EQ(protocol::session::Promotion_Auto,
            protocol::session::values::PromotionParameters{}.promotion);
  EXPECT_EQ(0u, protocol::values::Revision{}.breaking);
  EXPECT_EQ(1u, protocol::values::Revision{}.compatible);
}

TEST(ProtocolRoundTrip, ASampleOfEveryShapeCrossesUnchanged) {
  crossesUnchanged<protocol::clock::values::SetPolicyParameters>(
      R"({"policy": "Advance", "budget_seconds": 2.5})", "SetPolicyParameters");
  crossesUnchanged<protocol::clock::values::StepParameters>(
      R"({"seconds": 1.25, "rate": 30})", "StepParameters");
  crossesUnchanged<protocol::host::values::DescribeResult>(
      R"({"version": {"revision": {"breaking": 0, "compatible": 1},
                      "program": "Sketchbook"},
          "domains": ["host", "clock"],
          "clock": "PauseWhileLoading",
          "state_root": "/tmp/state",
          "sessions": [{"sketch": "hello", "kind": "canvas",
                        "width": 800, "height": 600, "moment": 2}]})",
      "DescribeResult");
  crossesUnchanged<protocol::registry::values::ListResult>(
      R"({"sketches": [{"name": "hello", "stem": "hello", "kind": "canvas",
                        "available": true},
                       {"name": "usd_roundtrip", "available": false,
                        "reason": "built without OpenUSD"}]})",
      "ListResult");
  crossesUnchanged<protocol::session::values::TimingResult>(
      R"({"total_milliseconds": 4.5, "lanes": [{"name": "paint",
                                                 "milliseconds": 3.25}]})",
      "TimingResult");
  crossesUnchanged<protocol::values::Error>(
      R"({"code": "invalidParameters", "message": "clock.step: frames"})",
      "Error");
}

TEST(ProtocolRoundTrip, TextTheTableCannotHoldIsNoValue) {
  std::string why;
  EXPECT_FALSE(data::values::fromJson<protocol::clock::values::StepParameters>(
      R"({"frame": 2})", &why));
  EXPECT_NE(std::string::npos, why.find("frame"));
  EXPECT_FALSE(
      data::values::fromJson<protocol::clock::values::SetPolicyParameters>(
          R"({"policy": "Sideways"})"));
  // A table whose field is required refuses text that leaves it out.
  EXPECT_FALSE(data::values::fromJson<protocol::session::values::OpenParameters>(
      R"({"kind": "canvas"})"));
}

}  // namespace
