/** One table of cities reaches a drawing three ways — the CSV decoded, the
 *  SQLite store that stands beside this sketch, and DuckDB asked over the
 *  same CSV — and every way answers the same Table a bar is drawn from. */
// TAGS: Data/Sources, Data/Tables

#include <sigilcompose/kit/Specimen.h>
#include <sigildata/query/Database.h>
#include <sigildata/scale/Scale.h>
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
constexpr SkColor4f kBar{0.30f, 0.83f, 0.78f, 1};
constexpr SkColor4f kBarDim{0.30f, 0.83f, 0.78f, 0.45f};

/** A column of horizontal bars, one per row: the label from @p names and
 *  the length from @p values against the largest, drawn in the ink in
 *  force. What is missing draws as a note in the ink. */
Element bars(const data::Table* table, const char* names, const char* values,
             std::string_view missing) {
  Element column = box().column().gap(4).width(Dimension(kCell - 28));
  if (!table || !table->has(names) || !table->has(values))
    return column.child(text(std::u8string(missing.begin(), missing.end())));
  const std::span<const std::string> label = table->column<std::string>(names);
  const std::span<const double> value = table->column<double>(values);
  double largest = 0;
  for (const double v : value) largest = std::max(largest, v);
  const data::Scale length{.domain = {0, largest}, .range = {0, kBars}};
  for (size_t row = 0; row < table->size(); ++row) {
    column.child(
        box()
            .row()
            .alignItems(Align::Center)
            .gap(8)
            .child(text(label[row]).width(Dimension(96.0f)))
            .child(box()
                       .height(Dimension(11.0f))
                       .width(Dimension((float)length(value[row])))
                       .fill(Fill::color(row == 0 ? kBar : kBarDim)))
            .child(
                text(std::to_string((long)value[row])).font({.size = 9.5f})));
  }
  return column;
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
    csv = ctx.assets.table(ctx.local("cities.csv"));
    // The SQLite store beside this sketch, opened in place: the same rows,
    // shaped by a query rather than by hand.
    if (const std::shared_ptr<const data::Database> store =
            ctx.assets.database(ctx.local("cities.sqlite"))) {
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
        ctx.assets.hub().resolve(ctx.local("cities.csv"));
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
                      kCell, u8"assets.table(local(\"cities.csv\"))",
                      u8"the decoder types the columns: text, number, flag, "
                      u8"instant",
                      bars(csv.get(), "city", "population",
                           "the CSV has not loaded")),
                  sketch::kit::caption(
                      kCell, u8"assets.database(local(\"cities.sqlite\"))",
                      u8"SUM(population) GROUP BY country · the store "
                      u8"is opened in place and reopened when it changes",
                      bars(fromSqlite ? &*fromSqlite : nullptr, "country",
                           "population", sqliteNote)),
                  sketch::kit::caption(
                      kCell, u8"Database::memory(Engine::Duck)",
                      u8"read_csv('cities.csv') WHERE coastal · the "
                      u8"engine reads the file the hub resolved",
                      bars(fromDuck ? &*fromDuck : nullptr, "city",
                           "population", duckNote))},
             .gap = 14}));
  }
};

SIGIL_SKETCH(
    DataSources, "Data",
    "one table of cities reached three ways: the CSV decoded, the "
    "SQLite store beside the sketch queried, DuckDB asked over the CSV")
