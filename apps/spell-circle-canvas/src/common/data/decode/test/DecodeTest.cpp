/** The decoders, on the text they are given and on a real hub: what a
 *  file turns into, and that a host's one registerDecoders() call is all
 *  it takes for `load<Table>` to answer. */

#include <gtest/gtest.h>
#include <sigildata/decode/Decoders.h>
#include <sigilio/hub/Hub.h>

#include <cmath>
#include <string>
#include <vector>

#include "ScratchDir.h"

using namespace sigil::data;
using sigil::test::ScratchDir;

namespace {

TEST(DataDecode, ACommaFileBecomesTypedColumns) {
  const std::optional<Table> table = decodeCsv(
      "month,deaths,winter,when\n"
      "Jan,2761,yes,1855-01-01\n"
      "Feb,2120,TRUE,1855-02-01\n");
  ASSERT_TRUE(table);
  EXPECT_EQ(2u, table->size());
  ASSERT_EQ(4u, table->columns().size());
  EXPECT_EQ(ColumnType::Text, table->column("month")->type());
  EXPECT_EQ(ColumnType::Number, table->column("deaths")->type());
  EXPECT_EQ(ColumnType::Boolean, table->column("winter")->type());
  EXPECT_EQ(ColumnType::Time, table->column("when")->type());
  EXPECT_DOUBLE_EQ(2120.0, table->column<double>("deaths")[1]);
  EXPECT_TRUE(table->column<Flag>("winter")[1]);
}

TEST(DataDecode, AQuotedFieldHoldsTheDelimiterNewlinesAndQuotes) {
  const std::optional<Table> table = decodeCsv(
      "name,note\n"
      "\"Smolensk, Russia\",\"he said \"\"go\"\"\"\n"
      "\"two\nlines\",plain\n");
  ASSERT_TRUE(table);
  EXPECT_EQ(2u, table->size());
  EXPECT_EQ("Smolensk, Russia", table->column<std::string>("name")[0]);
  EXPECT_EQ("he said \"go\"", table->column<std::string>("note")[0]);
  EXPECT_EQ("two\nlines", table->column<std::string>("name")[1]);
}

TEST(DataDecode, SpaceAroundAnUnquotedFieldIsLayoutAndAQuotedFieldsIsItsOwn) {
  const std::optional<Table> table = decodeCsv(
      "a, b\n"
      "1, 2\n"
      "3,\"  4  \"\n");
  ASSERT_TRUE(table);
  ASSERT_TRUE(table->has("b"));
  EXPECT_EQ(ColumnType::Text, table->column("b")->type());
  EXPECT_EQ("2", table->column<std::string>("b")[0]);
  EXPECT_EQ("  4  ", table->column<std::string>("b")[1]);
}

TEST(DataDecode, AnEmptyCellIsMissingAndInANumberColumnIsNotANumber) {
  const std::optional<Table> table = decodeCsv(
      "city,men\n"
      "Kowno,\n"
      "Wilna,24000\n"
      "Moskva,100000\n");
  ASSERT_TRUE(table);
  EXPECT_EQ(ColumnType::Number, table->column("men")->type());
  EXPECT_TRUE(table->column("men")->missing(0));
  EXPECT_FALSE(table->column("men")->missing(1));
  EXPECT_TRUE(std::isnan(table->column<double>("men")[0]));
}

TEST(DataDecode, AShortRowIsMissingAtItsEndAndALongOneLosesItsExtras) {
  const std::optional<Table> table = decodeCsv(
      "a,b,c\n"
      "1,2\n"
      "3,4,5,6\n");
  ASSERT_TRUE(table);
  EXPECT_EQ(3u, table->columns().size());
  EXPECT_EQ(2u, table->size());
  EXPECT_TRUE(table->column("c")->missing(0));
  EXPECT_DOUBLE_EQ(5.0, table->column<double>("c")[1]);
}

TEST(DataDecode, ATabFileIsReadAsOneWhenItsNameSaysSoOrItsFirstLineDoes) {
  const std::string text =
      "name\tgloss, with a comma\tmag\nBei\tthe north\t2.4\n";
  const std::optional<Table> named = decodeCsv(text, {}, "res://star.tsv");
  ASSERT_TRUE(named);
  EXPECT_EQ(3u, named->columns().size());
  EXPECT_EQ("the north", named->column<std::string>("gloss, with a comma")[0]);

  // With no name to go on, the tab wins because it cuts the line into
  // more fields than the comma inside one of them does.
  const std::optional<Table> sniffed = decodeCsv(text);
  ASSERT_TRUE(sniffed);
  EXPECT_EQ(3u, sniffed->columns().size());
  EXPECT_DOUBLE_EQ(2.4, sniffed->column<double>("mag")[0]);
}

TEST(DataDecode, WithoutAHeaderTheColumnsAreNamedByTheirPosition) {
  const std::optional<Table> table =
      decodeCsv("1,2\n3,4\n", CsvOptions{.header = false});
  ASSERT_TRUE(table);
  EXPECT_EQ(2u, table->size());
  ASSERT_TRUE(table->has("1"));
  EXPECT_DOUBLE_EQ(3.0, table->column<double>("1")[1]);
}

TEST(DataDecode, ACommentLineIsNotData) {
  const std::optional<Table> table = decodeCsv(
      "# generated once and frozen\n"
      "a,b\n"
      "1,2\n"
      "# and a note in the middle\n"
      "3,4\n",
      CsvOptions{.comment = '#'});
  ASSERT_TRUE(table);
  EXPECT_EQ(2u, table->size());
  EXPECT_DOUBLE_EQ(3.0, table->column<double>("a")[1]);
}

TEST(DataDecode, ThousandsSeparatorsAreReadAndADecimalCommaIsNot) {
  const std::optional<Table> grouped =
      decodeCsv("men\n\"422,000\"\n\"1,234,567\"\n");
  ASSERT_TRUE(grouped);
  EXPECT_EQ(ColumnType::Number, grouped->column("men")->type());
  EXPECT_DOUBLE_EQ(422000.0, grouped->column<double>("men")[0]);
  EXPECT_DOUBLE_EQ(1234567.0, grouped->column<double>("men")[1]);

  const std::optional<Table> ambiguous = decodeCsv("v\n\"1,5\"\n");
  ASSERT_TRUE(ambiguous);
  EXPECT_EQ(ColumnType::Text, ambiguous->column("v")->type());
}

TEST(DataDecode, AnInstantIsADateAndOptionallyATimeOnIt) {
  EXPECT_EQ(Instant{0.0}, *decodeInstant("1970-01-01"));
  EXPECT_EQ(Instant{1552003200.0}, *decodeInstant("2019-03-08"));
  EXPECT_EQ(Instant{951782400.0}, *decodeInstant("2000-02-29"));
  EXPECT_EQ(Instant{-86400.0}, *decodeInstant("1969-12-31"));
  EXPECT_EQ(Instant{1552055100.0}, *decodeInstant("2019-03-08T14:25:00"));
  EXPECT_EQ(Instant{1552055100.0}, *decodeInstant("2019-03-08 14:25:00Z"));
  EXPECT_NEAR(1552055100.5, decodeInstant("2019-03-08T14:25:00.5")->seconds,
              1e-6);
  EXPECT_FALSE(decodeInstant("2019-03"));
  EXPECT_FALSE(decodeInstant("08/03/2019"));
  EXPECT_FALSE(decodeInstant("2019-13-08"));
  EXPECT_FALSE(decodeInstant("2019-03-08T14:25:00+01:00"));
}

TEST(DataDecode, AListOfRecordsIsOneRowEach) {
  const std::optional<Json> document = decodeJson(
      R"([{"id": 1, "name": "root", "open": true},
          {"id": 2, "name": "leaf", "open": false, "note": "late"}])");
  ASSERT_TRUE(document);
  const std::optional<Table> table = tableFromJson(*document);
  ASSERT_TRUE(table);
  EXPECT_EQ(2u, table->size());
  ASSERT_EQ(4u, table->columns().size());
  EXPECT_EQ("id", table->columns()[0].name());
  EXPECT_EQ("note", table->columns()[3].name());
  EXPECT_EQ(ColumnType::Number, table->column("id")->type());
  EXPECT_EQ(ColumnType::Boolean, table->column("open")->type());
  EXPECT_TRUE(table->column("note")->missing(0));
  EXPECT_EQ("late", table->column<std::string>("note")[1]);
}

TEST(DataDecode, ARecordOfListsIsOneColumnEach) {
  const std::optional<Json> document =
      decodeJson(R"({"x": [1, 2, 3], "label": ["a", "b", "c"]})");
  ASSERT_TRUE(document);
  const std::optional<Table> table = tableFromJson(*document);
  ASSERT_TRUE(table);
  EXPECT_EQ(3u, table->size());
  EXPECT_DOUBLE_EQ(3.0, table->column<double>("x")[2]);
  EXPECT_EQ("b", table->column<std::string>("label")[1]);
}

TEST(DataDecode, AListOfListsIsNamedByPosition) {
  const std::optional<Json> document = decodeJson(R"([[1, "a"], [2, "b"]])");
  ASSERT_TRUE(document);
  const std::optional<Table> table = tableFromJson(*document);
  ASSERT_TRUE(table);
  EXPECT_EQ(2u, table->size());
  EXPECT_DOUBLE_EQ(2.0, table->column<double>("1")[1]);
  EXPECT_EQ("a", table->column<std::string>("2")[0]);
}

TEST(DataDecode, ACellHoldingAListOfItsOwnHasNoPlaceInARectangle) {
  const std::optional<Json> document =
      decodeJson(R"([{"id": 1, "links": [2, 3]}, {"id": 2, "links": []}])");
  ASSERT_TRUE(document);
  const std::optional<Table> table = tableFromJson(*document);
  ASSERT_TRUE(table);
  EXPECT_TRUE(table->column("links")->missing(0));
  EXPECT_TRUE(table->column("links")->missing(1));
}

TEST(DataDecode, ANestedDocumentIsReadAsAValueAndKeepsItsOrder) {
  const std::optional<Json> document = decodeJson(
      R"({"groups": [{"orbit": 3, "nodes": ["a", "b"]}], "version": 7})");
  ASSERT_TRUE(document);
  EXPECT_EQ(Json::Kind::Record, document->kind());
  ASSERT_EQ(2u, document->fields().size());
  EXPECT_EQ("groups", document->fields()[0].first);
  EXPECT_DOUBLE_EQ(7.0, (*document)["version"].number());
  EXPECT_DOUBLE_EQ(3.0, (*document)["groups"][size_t{0}]["orbit"].number());
  EXPECT_EQ("b", (*document)["groups"][size_t{0}]["nodes"][size_t{1}].text());

  // A chain through members that are not there answers null, not a crash.
  EXPECT_TRUE((*document)["absent"]["deeper"][size_t{4}].null());
  EXPECT_DOUBLE_EQ(-1.0, (*document)["absent"].number(-1.0));
}

TEST(DataDecode, TextThatIsNotJsonIsNotADocument) {
  EXPECT_FALSE(decodeJson("month,deaths\nJan,2761\n"));
  EXPECT_FALSE(decodeJson(""));
}

TEST(DataDecode, OneRegisterCallIsAllAHubNeeds) {
  const ScratchDir scratch("data_decode_hub");
  scratch.write("deaths.csv", "month,rate\nJan,1.1\nFeb,2.2\n");
  scratch.write("stars.tsv", "name\tmag\nBei\t2.4\n");
  scratch.write("tree.json", R"([{"id": 1}, {"id": 2}])");
  scratch.write("nested.json", R"({"root": {"depth": 4}})");

  sigil::io::Hub hub;
  hub.mount("res://", scratch.path);
  registerDecoders(hub);

  const std::shared_ptr<const Table> deaths =
      hub.load<Table>("res://deaths.csv");
  ASSERT_TRUE(deaths);
  EXPECT_EQ(2u, deaths->size());
  EXPECT_DOUBLE_EQ(2.2, deaths->column<double>("rate")[1]);

  // The same decoder, the delimiter taken from the resource's name.
  const std::shared_ptr<const Table> stars = hub.load<Table>("res://stars.tsv");
  ASSERT_TRUE(stars);
  EXPECT_DOUBLE_EQ(2.4, stars->column<double>("mag")[0]);

  // And the same decoder again, on a JSON file that is a rectangle.
  const std::shared_ptr<const Table> tree = hub.load<Table>("res://tree.json");
  ASSERT_TRUE(tree);
  EXPECT_EQ(2u, tree->size());

  const std::shared_ptr<const Json> nested =
      hub.load<Json>("res://nested.json");
  ASSERT_TRUE(nested);
  EXPECT_DOUBLE_EQ(4.0, (*nested)["root"]["depth"].number());

  // A second ask is the cached view, not a second decode.
  EXPECT_EQ(deaths.get(), hub.load<Table>("res://deaths.csv").get());

  // A resource that is not a table at all answers null rather than an
  // empty one, so a caller can tell the two apart.
  scratch.write("empty.csv", "");
  EXPECT_FALSE(hub.load<Table>("res://empty.csv"));
}

TEST(DataDecode, AWriteThroughTheHubDropsTheTableItDecoded) {
  const ScratchDir scratch("data_decode_reload");
  scratch.write("t.csv", "v\n1\n");

  sigil::io::Hub hub;
  hub.mount("res://", scratch.path);
  registerDecoders(hub);
  ASSERT_TRUE(hub.load<Table>("res://t.csv"));
  EXPECT_DOUBLE_EQ(1.0, hub.load<Table>("res://t.csv")->column<double>("v")[0]);

  const std::string edited = "v\n2\n3\n";
  hub.write("res://t.csv", edited.data(), edited.size());
  const std::shared_ptr<const Table> again = hub.load<Table>("res://t.csv");
  ASSERT_TRUE(again);
  EXPECT_EQ(2u, again->size());
  EXPECT_DOUBLE_EQ(2.0, again->column<double>("v")[0]);
}

}  // namespace
