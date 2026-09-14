/** One table of cities reaches a drawing three ways — the CSV decoded, the
 *  SQLite store that stands beside this sketch, and DuckDB asked over the
 *  same CSV — and every way answers the same Table a bar is drawn from. */
// TAGS: Data/Sources, Data/Tables

#include <sigilcompose/kit/Specimen.h>
#include <sigildata/query/Database.h>
#include <sigildata/table/Table.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <memory>
#include <optional>
#include <string>

namespace data = sigil::data;
namespace sketch = sigil::sketch;
using namespace sigil::compose;

namespace {

constexpr float kCell = 330;
constexpr float kBars = 150;

/** THE ANSWER, drawn: one bar a row against the largest of them, read
 *  straight off @p table's two columns. A table that has not loaded draws
 *  @p missing instead, which is the sketch's own note about why. */
Element answer(const data::Table* table, const char* names, const char* values,
               std::string_view missing) {
  if (!table || !table->has(names) || !table->has(values))
    return box().width(kCell - 28).children({text(missing)});
  return sketch::kit::bars(*table, names, values, {.length = kBars})
      .width(kCell - 28);
}

}  // namespace

struct DataSources final : sketch::Sketch {
  std::shared_ptr<const data::Table> csv;
  std::optional<data::Table> fromSqlite;
  std::optional<data::Table> fromDuck;
  std::string sqliteNote, duckNote;

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = {1100, 420}, .captureAt = 0.05});
    // The CSV beside this sketch, decoded to a Table by the hub.
    csv = ctx.assets.table(ctx.local("data/cities.csv"));
    // The SQLite store beside this sketch, opened in place: the same rows,
    // shaped by a query rather than by hand.
    if (const std::shared_ptr<const data::Database> store =
            ctx.assets.database(ctx.local("data/cities.sqlite"))) {
      fromSqlite = store->query(
          "SELECT country, SUM(population) AS population FROM cities "
          "GROUP BY country ORDER BY population DESC",
          &sqliteNote);
    } else {
      sqliteNote = "the store did not open";
    }
    // DuckDB over the CSV file itself, from a store that lives in memory
    // for as long as this frame is described.
    const std::filesystem::path path =
        ctx.assets.hub().resolve(ctx.local("data/cities.csv"));
    if (std::optional<data::Database> scratch =
            data::Database::memory(data::Engine::Duck, &duckNote)) {
      fromDuck = scratch->query(
          "SELECT city, population FROM read_csv('" + path.string() +
              "') WHERE coastal ORDER BY population DESC LIMIT 6",
          &duckNote);
    }
    ctx.composer.render(describe());
  }

  Element describe() {
    const sketch::kit::Provide look(sketch::kit::theme());
    return sketch::kit::page(
        {.title = u8"DATA SOURCES · one table, three ways",
         .subtitle = u8"a CSV decoded · a SQLite store beside the sketch "
                     u8"· DuckDB asked over the CSV · every answer "
                     u8"is a Table",
         .footer = u8"the files stand next to the sketch and are named through "
                   u8"ctx.local(); a query shapes the rows where a filter and "
                   u8"a group would, and draws from the same value"},
        kit::cells(
            {.cells =
                 {sketch::kit::caption(
                      kCell, u8"assets.table(local(\"data/cities.csv\"))",
                      u8"the decoder types the columns: text, number, flag, "
                      u8"instant",
                      answer(csv.get(), "city", "population",
                             "the CSV has not loaded")),
                  sketch::kit::caption(
                      kCell, u8"assets.database(local(\"data/cities.sqlite\"))",
                      u8"SUM(population) GROUP BY country · the store "
                      u8"is opened in place and reopened when it changes",
                      answer(fromSqlite ? &*fromSqlite : nullptr, "country",
                             "population", sqliteNote)),
                  sketch::kit::caption(
                      kCell, u8"Database::memory(Engine::Duck)",
                      u8"read_csv('cities.csv') WHERE coastal · the "
                      u8"engine reads the file the hub resolved",
                      answer(fromDuck ? &*fromDuck : nullptr, "city",
                             "population", duckNote))},
             .gap = 14}));
  }
};

SIGIL_SKETCH(
    DataSources, "Data",
    "one table of cities reached three ways: the CSV decoded, the "
    "SQLite store beside the sketch queried, DuckDB asked over the CSV")
