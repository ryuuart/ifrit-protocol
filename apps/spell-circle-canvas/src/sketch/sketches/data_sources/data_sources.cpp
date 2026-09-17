/** @file
 * One city dataset answers three different questions through a Table.
 * CSV supplies the original rows; SQLite aggregates countries; DuckDB filters
 * coastal cities from the resolved CSV. Every result uses the same bar
 * renderer, with its scale normalized independently. Source files are local
 * to this directory sketch.
 */
// TAGS: Data/Sources, Data/Tables

#include <sigilcompose/kit/Specimen.h>
#include <sigildata/query/Database.h>
#include <sigildata/table/Table.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <array>
#include <memory>
#include <optional>
#include <string>

namespace data = sigil::data;
namespace sketch = sigil::sketch;
using namespace sigil::compose;

namespace {

constexpr float kCell = 324;
constexpr float kBars = 112;

/** THE ANSWER, drawn: one bar a row against the largest of them, read
 *  straight off @p table's two columns. A table that has not loaded draws
 *  @p missing instead, which is the sketch's own note about why. */
Element answer(const data::Table* table, const char* names, const char* values,
               std::string_view missing) {
  if (!table || !table->has(names) || !table->has(values))
    return box().width(kCell - 28).children({text(missing)});
  // The FIRST row is the reading each cell is about — the country the
  // query ordered to the top — so it is lit and the rest stand quiet.
  const SkColor4f figure = sketch::kit::theme().palette.figure;
  const std::array<SkColor4f, 1> lit{figure};
  return sketch::kit::bars(*table, names, values,
                           {.length = kBars,
                            .bar = Fill::color(
                                sigil::material::skia::withAlpha(figure, 0.5f)),
                            .inks = lit})
      .width(kCell - 28);
}

}  // namespace

struct DataSources {
  std::shared_ptr<const data::Table> csv;
  std::optional<data::Table> fromSqlite;
  std::optional<data::Table> fromDuck;
  std::string sqliteNote, duckNote;

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = {1100, 640}, .captureAt = 0.05});
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
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    const auto result = [](Element chart) {
      return sketch::kit::well({.width = kCell, .height = 290, .padding = 14})
          .children({std::move(chart)});
    };
    return sketch::kit::page(
        {.title = "One dataset, three questions",
         .subtitle = "Read the cities, aggregate their countries, then select "
                     "the coast. All three answers use the same Table value "
                     "and bar renderer.",
         .footer =
             "Bar lengths are normalized within each result. Country totals "
             "and individual city populations use different scales."},
        box().column().gap(28).children(
            {sketch::kit::comparison(
                 {.cases = {{.title = "READ EVERY CITY",
                             .control = "CSV → assets.table",
                             .figure =
                                 result(answer(csv.get(), "city", "population",
                                               "The CSV has not loaded")),
                             .note = "Directly decoded rows, in file order."},
                            {.title = "TOTAL BY COUNTRY",
                             .control = "SQLite → GROUP BY country",
                             .figure = result(
                                 answer(fromSqlite ? &*fromSqlite : nullptr,
                                        "country", "population", sqliteNote)),
                             .note = "SUM(population), ordered from the "
                                     "largest country total."},
                            {.title = "ONLY COASTAL CITIES",
                             .control = "DuckDB → WHERE coastal",
                             .figure =
                                 result(answer(fromDuck ? &*fromDuck : nullptr,
                                               "city", "population", duckNote)),
                             .note = "The six largest coastal cities selected "
                                     "from the CSV."}},
                  .measure = 1020,
                  .gap = 24}),
             sketch::kit::sectionHeader(
                 {.label = "FILES BESIDE THE SKETCH",
                  .note = "ctx.local() resolves both sources"}),
             box().row().gap(32).children(
                 {text("cities.csv\nTyped text, number and flag columns")
                      .width(324)
                      .styleClass("readout"),
                  text("cities.sqlite\nA persistent relational store")
                      .width(324)
                      .styleClass("readout"),
                  text("In-memory DuckDB\nQueries the resolved CSV path")
                      .width(308)
                      .styleClass("readout")})}));
  }
};

SIGIL_SKETCH(
    DataSources, "Data",
    "one table of cities reached three ways: the CSV decoded, the "
    "SQLite store beside the sketch queried, DuckDB asked over the CSV")
