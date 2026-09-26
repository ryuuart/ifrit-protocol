/** @file
 * The one-line readers against a real hub over a scratch directory: a
 * document, a table, a query over a CSV run as `source`, a store read
 * through a query, and what each answers where the file is not there.
 */

#include <gtest/gtest.h>
#include <sigildata/query/Database.h>
#include <sigildata/read/Read.h>
#include <sigilio/advanced/Places.h>
#include <sigilio/hub/Hub.h>

#include <string>

#include "ScratchDir.h"

using namespace sigil::data;
using sigil::test::ScratchDir;

TEST(DataRead, AFileReadsWholeInOneLine) {
  const ScratchDir scratch("data_read_whole");
  scratch.write("content.json", R"({"title": "Sky", "bands": [1, 2, 3]})");
  scratch.write("cities.csv",
                "city,population,coastal\nLisbon,545,true\nMadrid,3223,false\n"
                "Porto,232,true\n");

  sigil::io::Hub hub;
  sigil::io::mount(hub, "res://", scratch.path);

  const Json words = json(hub, "res://content.json");
  EXPECT_EQ("Sky", words["title"].string());
  EXPECT_DOUBLE_EQ(2.0, words["bands"][1].number());

  const Table sheet = csv(hub, "res://cities.csv");
  EXPECT_EQ(3u, sheet.size());
  EXPECT_DOUBLE_EQ(3223.0, sheet.column<double>("population")[1]);

  // A query over a CSV runs with the file's rows as `source`.
  std::string why;
  const Table coastal = table(
      hub, "res://cities.csv",
      {.query = "SELECT city FROM source WHERE coastal ORDER BY population DESC",
       .why = &why});
  ASSERT_EQ(2u, coastal.size()) << why;
  EXPECT_EQ("Lisbon", coastal.column<std::string>("city")[0]);

  // Missing is null or empty, with the reason where it is asked for.
  why.clear();
  EXPECT_TRUE(json(hub, "res://absent.json", {.why = &why}).null());
  EXPECT_FALSE(why.empty());
  EXPECT_TRUE(csv(hub, "res://absent.csv").empty());
}

TEST(DataRead, AStoreIsReadThroughAQuery) {
  const ScratchDir scratch("data_read_store");
  {
    std::string why;
    std::optional<Database> store = Database::memory(Engine::Sqlite, &why);
    ASSERT_TRUE(store) << why;
    ASSERT_TRUE(store->execute(
        "CREATE TABLE cities (city TEXT, population REAL);"
        "INSERT INTO cities VALUES ('Lisbon', 545), ('Madrid', 3223);"
        "VACUUM INTO '" + (scratch.path / "cities.sqlite").string() + "'",
        &why)) << why;
  }
  sigil::io::Hub hub;
  sigil::io::mount(hub, "res://", scratch.path);

  const Table biggest = table(
      hub, "res://cities.sqlite",
      {.query = "SELECT city FROM cities ORDER BY population DESC LIMIT 1"});
  ASSERT_EQ(1u, biggest.size());
  EXPECT_EQ("Madrid", biggest.column<std::string>("city")[0]);

  std::string why;
  EXPECT_TRUE(table(hub, "res://cities.sqlite", {.why = &why}).empty());
  EXPECT_FALSE(why.empty());

  // One call puts the store's decoder on the hub beside the table's and
  // the document's.
  registerDecoders(hub);
  EXPECT_TRUE(hub.load<Database>("res://cities.sqlite"));
}
