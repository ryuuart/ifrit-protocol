/** @file
 * The catalog seam, without a window: a compiled-in sketch's row names
 * the runtime it draws through, and a file opened by path — which has no
 * runtime until it is built — learns one through learn().
 */

// Registered the way a sketch file is, so the catalog reads a real
// registry rather than a fixture it would never otherwise see. It must
// stand before the prelude: the macro chooses its form at include time.
#define SIGIL_SKETCH_STATIC "book_probe"

#include <sigilsketch/canvas/Sketch.h>

#include <gtest/gtest.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <filesystem>

#include "SketchCatalog.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

/** A minimal canvas sketch: enough that its kind reports "canvas" without
 *  a session ever opening. */
struct BookProbe : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(320, 200);
    ctx.composer.render(box().width(40).height(40));
  }
};

}  // namespace

SIGIL_SKETCH(BookProbe, "Test", "a probe for the catalog seam")

namespace {

QVariantMap rowAt(const SketchCatalog& catalog, int index) {
  const QVariantList rows = catalog.sketches();
  return rows[index].toMap();
}

/** Where the row under @p key sits. The registry is the process's, and
 *  every file that registered a sketch into it is in this binary, so a
 *  row is found by its key rather than by counting from the front. */
int rowOf(const SketchCatalog& catalog, const QString& key) {
  const QVariantList rows = catalog.sketches();
  for (int index = 0; index < rows.size(); ++index)
    if (rows[index].toMap().value(QStringLiteral("key")).toString() == key)
      return index;
  return -1;
}

TEST(SketchCatalog, RowNamesTheRuntimeAndAnUnbuiltFileLearnsIt) {
  int argc = 1;
  char arg0[] = "sketch_book_test";
  char* argv[] = {arg0, nullptr};
  const QCoreApplication app(argc, argv);

  SketchCatalog::sketchDir = std::filesystem::temp_directory_path();
  SketchCatalog::thumbnailDir.clear();  // nothing is rendered in this test
  SketchCatalog::externals = {std::filesystem::temp_directory_path() /
                              "book_probe_draft.cpp"};

  SketchCatalog catalog;
  const int probe = rowOf(catalog, QStringLiteral("book_probe"));
  ASSERT_GE(probe, 0);

  // A COMPILED-IN SKETCH'S ROW NAMES ITS RUNTIME, read off the kind.
  EXPECT_EQ(rowAt(catalog, probe).value(QStringLiteral("kind")).toString(),
            QStringLiteral("canvas"));

  // A FILE OPENED BY PATH has no runtime until it is built. The externals
  // are appended after every compiled-in entry, so it is the last row.
  const int kExternal = (int)catalog.sketches().size() - 1;
  EXPECT_TRUE(rowAt(catalog, kExternal)
                  .value(QStringLiteral("kind"))
                  .toString()
                  .isEmpty());

  // …and learn() is the first thing that knows it — the fix for a row
  // that read "not yet compiled" under a sketch that was live.
  const QVariantMap learned =
      catalog.learn(kExternal, QStringLiteral("640x480"), 1.5,
                    QStringLiteral("#101018"), QStringLiteral("canvas"));
  EXPECT_FALSE(learned.isEmpty());
  EXPECT_EQ(rowAt(catalog, kExternal).value(QStringLiteral("kind")).toString(),
            QStringLiteral("canvas"));
  EXPECT_EQ(
      rowAt(catalog, kExternal).value(QStringLiteral("canvas")).toString(),
      QStringLiteral("640x480"));
}

}  // namespace
