/** @file
 * A database behind the seam: a Table round-trips through either engine
 * with its types and its missing cells, a query aggregates, a SQLite file
 * opens by its extension and from its bytes, DuckDB reads a CSV straight
 * from a query, and an error is named rather than thrown.
 */

#include <gtest/gtest.h>
#include <sigildata/query/Database.h>
#include <sigilio/source/Source.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace sigil::data;

namespace {

Table sample() {
  Table t;
  t.add("n", std::vector<double>{3, 1, 2});
  t.add("s", std::vector<std::string>{"c", "a", "b"});
  t.add("f", std::vector<Flag>{Flag(true), Flag(false), Flag(true)});
  t.add("when",
        std::vector<Instant>{Instant{86400.0}, Instant{0.0}, Instant{3600.0}});
  Column with;
  with = Column("gap", std::vector<double>{10, 20, 30});
  with.markMissing(1);
  t.add(std::move(with));
  return t;
}

std::filesystem::path scratch(const char* name) {
  return std::filesystem::temp_directory_path() / name;
}

/** The whole of a file as the bytes a hub hands a decoder. */
sigil::io::Bytes readAll(const std::filesystem::path& file) {
  sigil::io::Bytes bytes;
  std::ifstream in(file, std::ios::binary | std::ios::ate);
  const std::streamsize size = in.tellg();
  in.seekg(0);
  bytes.bytes.resize((size_t)std::max<std::streamsize>(size, 0));
  in.read(reinterpret_cast<char*>(bytes.bytes.data()), size);
  return bytes;
}

}  // namespace

class QueryEngines : public testing::TestWithParam<Engine> {};

TEST_P(QueryEngines, OpeningAnAbsentStoreDoesNotCreateIt) {
  const auto file =
      scratch(GetParam() == Engine::Sqlite ? "sigil-absent-store.sqlite"
                                           : "sigil-absent-store.duckdb");
  std::filesystem::remove(file);
  std::string why;
  EXPECT_FALSE(Database::open(file, &why));
  EXPECT_FALSE(why.empty());
  EXPECT_FALSE(std::filesystem::exists(file));
}

TEST_P(QueryEngines, AnExistingStoreCanStillBeWritten) {
  const auto file =
      scratch(GetParam() == Engine::Sqlite ? "sigil-writable-store.sqlite"
                                           : "sigil-writable-store.duckdb");
  std::filesystem::remove(file);
  std::string why;
  {
    auto memory = Database::memory(GetParam(), &why);
    ASSERT_TRUE(memory) << why;
    const std::string create = GetParam() == Engine::Sqlite
                                   ? "VACUUM INTO '" + file.string() + "'"
                                   : "ATTACH '" + file.string() + "' AS store";
    ASSERT_TRUE(memory->execute(create, &why)) << why;
  }
  {
    auto opened = Database::open(file, &why);
    ASSERT_TRUE(opened) << why;
    ASSERT_TRUE(opened->insert("records", sample(), &why)) << why;
    ASSERT_TRUE(opened->execute("DELETE FROM records WHERE n = 1", &why))
        << why;
    const auto rows = opened->query("SELECT n FROM records ORDER BY n", &why);
    ASSERT_TRUE(rows) << why;
    ASSERT_EQ(rows->size(), 2u);
    EXPECT_EQ(rows->column<double>("n")[0], 2);
  }
  std::filesystem::remove(file);
}

TEST_P(QueryEngines, ATableRoundTripsWithItsTypesAndItsMissingCells) {
  std::string why;
  std::optional<Database> db = Database::memory(GetParam(), &why);
  ASSERT_TRUE(db) << why;
  ASSERT_TRUE(db->insert("t", sample(), &why)) << why;
  const std::optional<Table> back =
      db->query("SELECT * FROM t ORDER BY n", &why);
  ASSERT_TRUE(back) << why;
  ASSERT_EQ(back->size(), 3u);
  ASSERT_TRUE(back->has("n") && back->has("s") && back->has("f") &&
              back->has("when") && back->has("gap"));
  EXPECT_EQ(back->column("n")->type(), ColumnType::Number);
  EXPECT_EQ(back->column("s")->type(), ColumnType::Text);
  EXPECT_EQ(back->column("f")->type(), ColumnType::Boolean);
  EXPECT_EQ(back->column("when")->type(), ColumnType::Time);
  EXPECT_EQ(back->column<double>("n")[0], 1);
  EXPECT_EQ(back->column<double>("n")[2], 3);
  EXPECT_EQ(back->column<std::string>("s")[0], "a");
  EXPECT_TRUE(back->column<Flag>("f")[0] == Flag(false));
  EXPECT_DOUBLE_EQ(back->column<Instant>("when")[2].seconds, 86400.0);
  // The gap of the row whose n is 1 (sorted first) was missing.
  EXPECT_TRUE(back->column("gap")->missing(0));
  EXPECT_FALSE(back->column("gap")->missing(1));
}

TEST_P(QueryEngines, AnAggregateAnswersATable) {
  std::optional<Database> db = Database::memory(GetParam());
  ASSERT_TRUE(db);
  ASSERT_TRUE(db->insert("t", sample()));
  const std::optional<Table> sum =
      db->query("SELECT f, SUM(n) AS total FROM t GROUP BY f ORDER BY f");
  ASSERT_TRUE(sum);
  ASSERT_EQ(sum->size(), 2u);
  EXPECT_EQ(sum->column("total")->type(), ColumnType::Number);
  EXPECT_DOUBLE_EQ(sum->column<double>("total")[0], 1);  // the false row
  EXPECT_DOUBLE_EQ(sum->column<double>("total")[1], 5);  // the true rows
}

TEST_P(QueryEngines, AnErrorIsNamedRatherThanThrown) {
  std::optional<Database> db = Database::memory(GetParam());
  ASSERT_TRUE(db);
  std::string why;
  EXPECT_FALSE(db->query("SELECT nothing FROM nowhere", &why));
  EXPECT_FALSE(why.empty());
  EXPECT_FALSE(db->execute("NOT SQL", &why));
}

INSTANTIATE_TEST_SUITE_P(Engines, QueryEngines,
                         testing::Values(Engine::Sqlite, Engine::Duck),
                         [](const testing::TestParamInfo<Engine>& info) {
                           return info.param == Engine::Sqlite ? "Sqlite"
                                                               : "Duck";
                         });

TEST(Query, ASqliteFileOpensByItsExtensionAndFromItsBytes) {
  const std::filesystem::path file = scratch("sigil-query-test.sqlite");
  std::filesystem::remove(file);
  {
    std::optional<Database> memory = Database::memory(Engine::Sqlite);
    ASSERT_TRUE(memory);
    ASSERT_TRUE(memory->insert("t", sample()));
    std::string why;
    ASSERT_TRUE(memory->execute("VACUUM INTO '" + file.string() + "'", &why))
        << why;
  }
  std::string why;
  std::optional<Database> opened = Database::open(file, &why);
  ASSERT_TRUE(opened) << why;
  EXPECT_EQ(opened->engine(), Engine::Sqlite);
  EXPECT_EQ(opened->file(), file);
  const std::optional<Table> rows = opened->query("SELECT n FROM t ORDER BY n");
  ASSERT_TRUE(rows);
  EXPECT_EQ(rows->size(), 3u);

  const sigil::io::Bytes bytes = readAll(file);
  std::optional<Database> fromBytes =
      Database::fromBytes(bytes, file.string(), &why);
  ASSERT_TRUE(fromBytes) << why;
  EXPECT_TRUE(fromBytes->file().empty());
  EXPECT_EQ(
      fromBytes->query("SELECT COUNT(*) AS c FROM t")->column<double>("c")[0],
      3);

  EXPECT_FALSE(Database::open(scratch("nothing.txt"), &why));
  EXPECT_FALSE(why.empty());
  EXPECT_FALSE(engineOf("x.csv"));
  EXPECT_EQ(engineOf("x.duckdb"), Engine::Duck);
  std::filesystem::remove(file);
}

TEST(Query, DuckReadsACsvFileStraightFromAQuery) {
  const std::filesystem::path csv = scratch("sigil-query-test.csv");
  {
    std::ofstream out(csv);
    out << "city,population,coastal\nAntwerp,530000,true\nGhent,265000,false\n";
  }
  std::optional<Database> db = Database::memory(Engine::Duck);
  ASSERT_TRUE(db);
  std::string why;
  const std::optional<Table> rows =
      db->query("SELECT city, population FROM read_csv('" + csv.string() +
                    "') WHERE coastal ORDER BY city",
                &why);
  ASSERT_TRUE(rows) << why;
  ASSERT_EQ(rows->size(), 1u);
  EXPECT_EQ(rows->column<std::string>("city")[0], "Antwerp");
  EXPECT_DOUBLE_EQ(rows->column<double>("population")[0], 530000);
  std::filesystem::remove(csv);
}

TEST(Query, TheDecoderOpensAFileInPlaceAndBytesAsSqlite) {
  const std::filesystem::path file = scratch("sigil-query-decoder.sqlite");
  std::filesystem::remove(file);
  {
    std::optional<Database> memory = Database::memory(Engine::Sqlite);
    ASSERT_TRUE(memory->insert("t", sample()));
    ASSERT_TRUE(memory->execute("VACUUM INTO '" + file.string() + "'"));
  }
  const sigil::io::Bytes bytes = readAll(file);
  const DatabaseDecoder decoder;
  std::optional<Database> inPlace = decoder.decode(bytes, file.string());
  ASSERT_TRUE(inPlace);
  EXPECT_EQ(inPlace->file(), file) << "a file on disk is opened where it is";
  std::optional<Database> fromBytes = decoder.decode(bytes, "");
  ASSERT_TRUE(fromBytes);
  EXPECT_TRUE(fromBytes->file().empty()) << "bytes alone: a SQLite store";
  EXPECT_FALSE(decoder.decode(bytes, "store.duckdb"))
      << "a DuckDB store opens from a file only";
  std::filesystem::remove(file);
}
