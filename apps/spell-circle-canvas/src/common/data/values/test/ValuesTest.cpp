/** The friendly header over a generated one, judged against the wire:
 *  a value written and read back field for field, a union through each
 *  alternative and through none, a table field that is absent and one
 *  that is there, a vector of tables, the root's own JSON form both
 *  ways, bytes that are not this schema at all, and the sheet the
 *  buffer cases read coming back as a value.
 */

#include <flatbuffers/flatbuffers.h>
#include <gtest/gtest.h>
#include <sigildata/values/Values.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "flatbuffer_test_generated.h"
#include "flatbuffer_test_values.h"
#include "values_test_generated.h"
#include "values_test_values.h"

// Field for field, so a case says which field moved rather than that
// something did. These stand in the schema's own value namespace,
// because that is where a comparison of its types is looked up from.
namespace values_test::values {

bool operator==(const Span& a, const Span& b) {
  return a.low == b.low && a.high == b.high;
}

bool operator==(const Note& a, const Note& b) {
  return a.text == b.text && a.weight == b.weight;
}

bool operator==(const Mark& a, const Mark& b) { return a.name == b.name; }

bool operator==(const Reading& a, const Reading& b) {
  return a.at == b.at && a.span == b.span && a.note == b.note &&
         a.tags == b.tags;
}

bool operator==(const Sheet& a, const Sheet& b) {
  return a.title == b.title && a.weather == b.weather && a.bounds == b.bounds &&
         a.readings == b.readings && a.depths == b.depths &&
         a.skies == b.skies && a.wet == b.wet;
}

bool operator==(const Log& a, const Log& b) {
  return a.count == b.count && a.entry == b.entry;
}

}  // namespace values_test::values

namespace values = values_test::values;
namespace sheet = flatbuffer_test::values;

namespace {

// What the generator leaves out is as much of the promise as what it
// writes: a field the schema deprecated may not be read or written, and
// the field a union's tag travels in is no member of a value, since the
// alternative the variant holds already says which type it is. Asking
// whether a type HAS a member is only a question a template may ask —
// spelled straight, the member access is checked where it stands and a
// name that is not there is an error rather than an answer — so each
// absence stands as a concept over the value type.
template <class Value>
concept CarriesRetired = requires(Value one) { one.retired; };
template <class Value>
concept CarriesEntryType = requires(Value one) { one.entry_type; };

static_assert(!CarriesRetired<values::Sheet>,
              "a deprecated field is no member of the value");
static_assert(!CarriesEntryType<values::Log>,
              "a union's tag is no member of the value");

/** A page with everything on it: a required title, an enum, a nested
 *  struct, two readings — one carrying a note and one carrying none —
 *  and a vector of scalars. */
values::Sheet aSheet() {
  values::Reading first;
  first.at = 12.5;
  first.span = values::Span{.low = 0.0f, .high = 1.0f};
  first.note = values::Note{.text = "wet", .weight = 3};
  first.tags = {"dawn", "north"};

  values::Reading second;
  second.at = 13.25;
  second.span = values::Span{.low = 1.0f, .high = 2.5f};
  second.tags = {"noon"};

  values::Sheet page;
  page.title = "north wall";
  page.weather = ::values_test::Weather_Rain;
  page.bounds = values::Span{.low = -1.5f, .high = 4.25f};
  page.readings = {first, second};
  page.depths = {0.5f, 1.5f, 2.5f};
  page.skies = {::values_test::Weather_Clear, ::values_test::Weather_Rain};
  page.wet = {false, true};
  return page;
}

/** That page in an entry, numbered. */
values::Log aLog() {
  values::Log one;
  one.count = 4;
  one.entry = aSheet();
  return one;
}

TEST(DataValues, AValueWrittenAndReadBackIsTheValue) {
  const values::Log one = aLog();
  const std::optional<values::Log> back =
      values::readLog(values::writeLog(one));
  ASSERT_TRUE(back);
  EXPECT_TRUE(one == *back);

  // And field for field, so a failure names what moved.
  ASSERT_TRUE(std::holds_alternative<values::Sheet>(back->entry));
  const values::Sheet& page = std::get<values::Sheet>(back->entry);
  EXPECT_EQ(4u, back->count);
  EXPECT_EQ("north wall", page.title);
  EXPECT_EQ(::values_test::Weather_Rain, page.weather);
  EXPECT_FLOAT_EQ(-1.5f, page.bounds.low);
  EXPECT_FLOAT_EQ(4.25f, page.bounds.high);
  ASSERT_EQ(3u, page.depths.size());
  EXPECT_FLOAT_EQ(2.5f, page.depths[2]);
  // A vector of enums comes back as the enum, not as the integer the
  // wire holds it in, and a vector of bools as bools.
  ASSERT_EQ(2u, page.skies.size());
  EXPECT_EQ(::values_test::Weather_Rain, page.skies[1]);
  ASSERT_EQ(2u, page.wet.size());
  EXPECT_FALSE(page.wet[0]);
  EXPECT_TRUE(page.wet[1]);
}

TEST(DataValues, AUnionRoundTripsThroughEachAlternativeAndThroughNone) {
  std::vector<values::Entry> each;
  each.push_back(aSheet());
  each.push_back(values::Note{.text = "a remark", .weight = -2});
  each.push_back(values::Mark{.name = "here"});
  for (const values::Entry& entry : each) {
    values::Log one;
    one.count = 1;
    one.entry = entry;
    const std::optional<values::Log> back =
        values::readLog(values::writeLog(one));
    ASSERT_TRUE(back);
    EXPECT_EQ(entry.index(), back->entry.index());
    EXPECT_TRUE(one == *back);
  }

  // A log carrying no entry reads as the variant holding none, which is
  // the one alternative the wire spells as the absence of the field.
  values::Log bare;
  bare.count = 9;
  const std::optional<values::Log> back =
      values::readLog(values::writeLog(bare));
  ASSERT_TRUE(back);
  EXPECT_EQ(9u, back->count);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(back->entry));
}

TEST(DataValues, ATableFieldThatIsAbsentReadsAsNothing) {
  values::Reading alone;
  alone.at = 1.0;
  const std::optional<values::Reading> bare =
      values::readReading(values::writeReading(alone));
  ASSERT_TRUE(bare);
  EXPECT_FALSE(bare->note.has_value());
  EXPECT_TRUE(bare->tags.empty());

  values::Reading noted = alone;
  noted.note = values::Note{.text = "seen", .weight = 1};
  const std::optional<values::Reading> back =
      values::readReading(values::writeReading(noted));
  ASSERT_TRUE(back);
  ASSERT_TRUE(back->note.has_value());
  EXPECT_EQ("seen", back->note->text);
  EXPECT_EQ(1, back->note->weight);
}

TEST(DataValues, AVectorOfTablesComesBackInOrder) {
  const values::Sheet page = aSheet();
  const std::optional<values::Sheet> back =
      values::readSheet(values::writeSheet(page));
  ASSERT_TRUE(back);
  ASSERT_EQ(2u, back->readings.size());
  EXPECT_DOUBLE_EQ(12.5, back->readings[0].at);
  EXPECT_DOUBLE_EQ(13.25, back->readings[1].at);
  ASSERT_TRUE(back->readings[0].note.has_value());
  EXPECT_EQ("wet", back->readings[0].note->text);
  EXPECT_FALSE(back->readings[1].note.has_value());
  ASSERT_EQ(2u, back->readings[0].tags.size());
  EXPECT_EQ("north", back->readings[0].tags[1]);

  // An empty vector comes back empty rather than as something else.
  values::Sheet nothing;
  nothing.title = "blank";
  const std::optional<values::Sheet> none =
      values::readSheet(values::writeSheet(nothing));
  ASSERT_TRUE(none);
  EXPECT_TRUE(none->readings.empty());
  EXPECT_TRUE(none->depths.empty());
  EXPECT_TRUE(none->skies.empty());
  EXPECT_TRUE(none->wet.empty());
  EXPECT_EQ("blank", none->title);
}

TEST(DataValues, TheRootConvertsToAndFromItsOwnJsonForm) {
  const values::Log one = aLog();
  std::string why;
  const std::optional<std::string> text = values::toJson(one, &why);
  ASSERT_TRUE(text) << why;
  EXPECT_NE(std::string::npos, text->find("north wall"));

  const std::optional<values::Log> back = values::fromJson(*text, &why);
  ASSERT_TRUE(back) << why;
  EXPECT_TRUE(one == *back);

  // Text the schema cannot hold is no value, and the reader's own
  // message says what stopped it.
  why.clear();
  EXPECT_FALSE(values::fromJson(
      R"({"count": 1, "entry_type": "Sheet", "entry": {"weather": "Rain"}})",
      &why));
  EXPECT_FALSE(why.empty());
}

TEST(DataValues, BytesThatDoNotVerifyReadAsNothing) {
  const std::vector<std::byte> bytes = values::writeLog(aLog());
  ASSERT_TRUE(values::readLog(bytes));
  // A buffer cut in half is refused before a byte of it is read, and no
  // bytes at all are no buffer.
  EXPECT_FALSE(values::readLog(
      std::span<const std::byte>(bytes).first(bytes.size() / 2)));
  EXPECT_FALSE(values::readLog(std::span<const std::byte>()));
}

TEST(DataValues, TheSheetTheBufferCasesReadComesBackAsAValue) {
  // Built the way a sender builds one, through the generated builders
  // alone, so what the value reads is a buffer nothing here wrote.
  flatbuffers::FlatBufferBuilder builder;
  const std::vector<flatbuffers::Offset<flatbuffer_test::Reading>> rows{
      flatbuffer_test::CreateReadingDirect(builder, "a", 2.5f),
      flatbuffer_test::CreateReadingDirect(builder, "c", -1.0f)};
  builder.Finish(flatbuffer_test::CreateSheetDirect(builder, &rows));
  const std::span<const std::byte> bytes(
      reinterpret_cast<const std::byte*>(builder.GetBufferPointer()),
      builder.GetSize());

  const std::optional<sheet::Sheet> read = sheet::readSheet(bytes);
  ASSERT_TRUE(read);
  ASSERT_EQ(2u, read->readings.size());
  EXPECT_EQ("a", read->readings[0].name);
  EXPECT_FLOAT_EQ(2.5f, read->readings[0].value);
  EXPECT_EQ("c", read->readings[1].name);
  EXPECT_FLOAT_EQ(-1.0f, read->readings[1].value);

  // And the value written back out is the same sheet again.
  const std::optional<sheet::Sheet> twice =
      sheet::readSheet(sheet::writeSheet(*read));
  ASSERT_TRUE(twice);
  ASSERT_EQ(2u, twice->readings.size());
  EXPECT_EQ("c", twice->readings[1].name);
  EXPECT_FLOAT_EQ(-1.0f, twice->readings[1].value);
}

}  // namespace
