/** Each case asserts one thing the header promises about the table as a
 *  VALUE: what a reshaping answers, and what it leaves alone. */

#include <gtest/gtest.h>
#include <sigildata/table/Table.h>

#include <cmath>
#include <string>
#include <vector>

using namespace sigil::data;

namespace {

Table months() {
  Table table;
  table.add("month", std::vector<std::string>{"Jan", "Feb", "Mar", "Apr"});
  table.add("deaths", std::vector<double>{2761, 2120, 1205, 477});
  table.add("winter", std::vector<Flag>{true, true, false, false});
  return table;
}

TEST(DataTable, AColumnKnowsItsNameItsTypeAndItsLength) {
  const Table table = months();
  EXPECT_EQ(4u, table.size());
  EXPECT_EQ(3u, table.columns().size());
  ASSERT_TRUE(table.has("deaths"));
  EXPECT_EQ(ColumnType::Number, table.column("deaths")->type());
  EXPECT_EQ(ColumnType::Text, table.column("month")->type());
  EXPECT_EQ(ColumnType::Boolean, table.column("winter")->type());
  EXPECT_FALSE(table.has("rainfall"));
}

TEST(DataTable, ANumericColumnIsOneContiguousSpan) {
  const Table table = months();
  const std::span<const double> deaths = table.column<double>("deaths");
  ASSERT_EQ(4u, deaths.size());
  EXPECT_DOUBLE_EQ(2761.0, deaths[0]);
  EXPECT_DOUBLE_EQ(477.0, deaths[3]);
  EXPECT_EQ(deaths.data() + 1, &deaths[1]);
}

TEST(DataTable, AskingAColumnForTheWrongTypeReadsNothing) {
  const Table table = months();
  EXPECT_TRUE(table.column<double>("month").empty());
  EXPECT_TRUE(table.column<std::string>("deaths").empty());
  EXPECT_TRUE(table.column<double>("rainfall").empty());
}

TEST(DataTable, ACellReadsOutAsWhicheverThingItsColumnHolds) {
  const Table table = months();
  EXPECT_EQ("Mar", std::get<std::string>(table.cell("month", 2)));
  EXPECT_DOUBLE_EQ(1205.0, std::get<double>(table.cell("deaths", 2)));
  EXPECT_TRUE(std::get<Flag>(table.cell("winter", 0)));
  EXPECT_DOUBLE_EQ(0.0, std::get<double>(table.cell("rainfall", 0)));
}

TEST(DataTable, AddingAColumnTwiceReplacesItWhereItStood) {
  Table table = months();
  table.add("deaths", std::vector<double>{1, 2, 3, 4});
  EXPECT_EQ(3u, table.columns().size());
  EXPECT_EQ("deaths", table.columns()[1].name());
  EXPECT_DOUBLE_EQ(1.0, table.column<double>("deaths")[0]);
}

TEST(DataTable, ADerivedColumnIsComputedOncePerRow) {
  Table table = months();
  const std::span<const double> deaths = table.column<double>("deaths");
  table.derive("root", [&](size_t row) { return std::sqrt(deaths[row]); });
  ASSERT_TRUE(table.has("root"));
  EXPECT_EQ(ColumnType::Number, table.column("root")->type());
  EXPECT_DOUBLE_EQ(std::sqrt(2761.0), table.column<double>("root")[0]);

  table.derive("label", [&](size_t row) {
    return std::get<std::string>(table.cell("month", row)) + "!";
  });
  EXPECT_EQ(ColumnType::Text, table.column("label")->type());
  EXPECT_EQ("Apr!", table.column<std::string>("label")[3]);
}

TEST(DataTable, SelectKeepsTheNamedColumnsInTheOrderNamed) {
  const Table table = months();
  const Table two = table.select({"deaths", "month"});
  ASSERT_EQ(2u, two.columns().size());
  EXPECT_EQ("deaths", two.columns()[0].name());
  EXPECT_EQ("month", two.columns()[1].name());
  EXPECT_EQ(4u, two.size());

  // A name with no column behind it is left out, not answered empty.
  EXPECT_EQ(1u, table.select({"month", "rainfall"}).columns().size());
}

TEST(DataTable, FilterKeepsTheRowsItAnswersTrueFor) {
  const Table table = months();
  const std::span<const double> deaths = table.column<double>("deaths");
  const Table heavy =
      table.filter([&](size_t row) { return deaths[row] > 1000.0; });
  EXPECT_EQ(3u, heavy.size());
  EXPECT_EQ(3u, heavy.columns().size());
  EXPECT_EQ("Mar", heavy.column<std::string>("month")[2]);
  // The table it came from is untouched.
  EXPECT_EQ(4u, table.size());
}

TEST(DataTable, SortOrdersEveryColumnTogetherAndTiesKeepTheirOrder) {
  const Table table = months();
  const Table up = table.sort("deaths");
  EXPECT_EQ("Apr", up.column<std::string>("month")[0]);
  EXPECT_EQ("Jan", up.column<std::string>("month")[3]);
  EXPECT_DOUBLE_EQ(477.0, up.column<double>("deaths")[0]);

  const Table down = table.sort("deaths", Order::Descending);
  EXPECT_EQ("Jan", down.column<std::string>("month")[0]);

  Table tied;
  tied.add("rank", std::vector<double>{1, 1, 1});
  tied.add("name", std::vector<std::string>{"a", "b", "c"});
  EXPECT_EQ("a", tied.sort("rank").column<std::string>("name")[0]);
  EXPECT_EQ("c", tied.sort("rank").column<std::string>("name")[2]);

  // An unknown column is not an order.
  EXPECT_EQ(table, table.sort("rainfall"));
}

TEST(DataTable, AMissingCellSortsLastWhicheverWayTheOrderRuns) {
  Column values("value", std::vector<double>{3, 1, 2});
  values.markMissing(1);

  Table table;
  table.add(std::move(values));
  table.add("name", std::vector<std::string>{"three", "gap", "two"});

  const Table up = table.sort("value");
  EXPECT_EQ("two", up.column<std::string>("name")[0]);
  EXPECT_EQ("gap", up.column<std::string>("name")[2]);

  const Table down = table.sort("value", Order::Descending);
  EXPECT_EQ("three", down.column<std::string>("name")[0]);
  EXPECT_EQ("gap", down.column<std::string>("name")[2]);
}

TEST(DataTable, ANumberThatIsNotANumberIsMissing) {
  Column column("value", std::vector<double>{1.0, std::nan(""), 3.0});
  EXPECT_FALSE(column.missing(0));
  EXPECT_TRUE(column.missing(1));
  EXPECT_TRUE(column.missing(3));  // past the end
}

TEST(DataTable, GroupGathersRowsByTheirKeyInFirstAppearanceOrder) {
  Table table;
  table.add("kind", std::vector<std::string>{"a", "b", "a", "c", "b"});
  table.add("n", std::vector<double>{1, 2, 3, 4, 5});

  const std::vector<Table::Group> groups = table.group("kind");
  ASSERT_EQ(3u, groups.size());
  EXPECT_EQ("a", std::get<std::string>(groups[0].key));
  EXPECT_EQ(std::vector<size_t>({0, 2}), groups[0].rows);
  EXPECT_EQ("b", std::get<std::string>(groups[1].key));
  EXPECT_EQ("c", std::get<std::string>(groups[2].key));

  const Table first = table.take(groups[0].rows);
  EXPECT_EQ(2u, first.size());
  EXPECT_DOUBLE_EQ(3.0, first.column<double>("n")[1]);
}

TEST(DataTable, TakeAnswersTheRowsNamedAndLeavesOutTheOnesThatAreNotThere) {
  const Table table = months();
  const std::vector<size_t> rows{3, 0, 9};
  const Table picked = table.take(rows);
  ASSERT_EQ(2u, picked.size());
  EXPECT_EQ("Apr", picked.column<std::string>("month")[0]);
  EXPECT_EQ("Jan", picked.column<std::string>("month")[1]);
}

TEST(DataTable, AShortColumnReadsAsMissingRatherThanShorteningTheTable) {
  Table table;
  table.add("long", std::vector<double>{1, 2, 3});
  table.add("short", std::vector<std::string>{"a"});
  EXPECT_EQ(3u, table.size());
  EXPECT_FALSE(table.column("short")->missing(0));
  EXPECT_TRUE(table.column("short")->missing(1));
  EXPECT_EQ("", std::get<std::string>(table.cell("short", 2)));
}

TEST(DataTable, ATimeColumnIsSecondsAndReadsAsASpanLikeAnyOther) {
  Table table;
  table.add("at", std::vector<Instant>{{0.0}, {86400.0}});
  EXPECT_EQ(ColumnType::Time, table.column("at")->type());
  const std::span<const Instant> at = table.column<Instant>("at");
  ASSERT_EQ(2u, at.size());
  EXPECT_DOUBLE_EQ(86400.0, at[1].seconds);
  EXPECT_EQ(Instant{86400.0}, std::get<Instant>(table.cell("at", 1)));
}

TEST(DataTable, TwoTablesWithTheSameColumnsAreTheSameValue) {
  const Table a = months();
  Table b = months();
  EXPECT_EQ(a, b);
  b.remove("winter");
  EXPECT_NE(a, b);
  EXPECT_EQ(2u, b.columns().size());
  EXPECT_FALSE(b.remove("winter"));
}

}  // namespace
