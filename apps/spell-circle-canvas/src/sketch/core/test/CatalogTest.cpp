/** @file
 * The catalog: the registry's entries first and the files a session was
 * pointed at after them, each row saying what the browser can know before
 * anything is opened.
 */

#include <gtest/gtest.h>
#include <sigilsketch/core/Catalog.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>

#include <string>
#include <string_view>
#include <vector>

#include "ScratchDir.h"

namespace {

using namespace sigil::sketch;
using sigil::test::ScratchDir;

/** A kind that draws nothing: the catalog never opens one. */
struct LedgerKind final : KindOperations {
  [[nodiscard]] std::string_view runtime() const override { return "ledger"; }
  [[nodiscard]] std::unique_ptr<Session> open(sigil::weave::FontContext&,
                                              Assets&, bool,
                                              std::string_view) const override {
    return nullptr;
  }
  bool operator==(const LedgerKind&) const { return true; }
};

Kind ledgerKind() { return LedgerKind{}; }

const bool kRegistered =
    add("catalog_probe", "catalog probe", "Catalog \xc2\xb7 Probe", "one entry",
        &ledgerKind);

/** The row a catalog holds under @p key, or a failure naming it. */
const CatalogRow& rowOf(const std::vector<CatalogRow>& rows,
                        std::string_view key) {
  for (const CatalogRow& row : rows)
    if (row.key == key) return row;
  ADD_FAILURE() << "no row for " << key;
  static const CatalogRow kNone;
  return kNone;
}

TEST(SketchCatalogRows, ARegistryEntryNamesItsRuntimeAndItsFile) {
  ASSERT_TRUE(kRegistered);
  const ScratchDir sketches("catalog_sketches");
  sketches.write("catalog_probe.cpp",
                 "// Catalog probe\n//\n// Stands for a sketch in a folder.\n"
                 "int main() {}\n");
  const std::vector<CatalogRow> rows =
      catalog({.sketchDirectory = sketches.path});
  ASSERT_EQ(rows.size(), registry().size());
  const CatalogRow& probe = rowOf(rows, "catalog_probe");
  EXPECT_EQ(probe.name, "catalog probe");
  EXPECT_EQ(probe.title, "catalog probe");
  EXPECT_EQ(probe.category, "Catalog \xc2\xb7 Probe");
  EXPECT_EQ(probe.kind, "ledger");
  EXPECT_TRUE(probe.available);
  EXPECT_FALSE(probe.external);
  EXPECT_TRUE(probe.videoExportable);
  EXPECT_EQ(probe.path, sketches.path / "catalog_probe.cpp");
  EXPECT_EQ(probe.entryPath, "catalog_probe.cpp");
  EXPECT_EQ(probe.source.lines, 4);
  EXPECT_EQ(rows[(size_t)probe.index].key, "catalog_probe");
}

TEST(SketchCatalogRows, AFileOpenedByPathHasNoRuntimeUntilItIsBuilt) {
  // A draft nothing has built, standing in a directory of its own, as a
  // sketch outside this checkout does: its row says so rather than
  // guessing a runtime from the folder it stands in. A Python file always
  // draws through a canvas.
  const ScratchDir drafts("catalog_drafts");
  drafts.write("book_probe_draft.cpp", "// a draft nothing has built\n");
  drafts.write("python_draft.py", "\"\"\"A draft in Python.\"\"\"\n");
  const std::vector<CatalogRow> rows =
      catalog({.files = {drafts.path / "book_probe_draft.cpp",
                         drafts.path / "python_draft.py"}});
  ASSERT_EQ(rows.size(), registry().size() + 2);

  const CatalogRow& draft = rows[rows.size() - 2];
  EXPECT_EQ(draft.key, "book_probe_draft");
  EXPECT_EQ(draft.name, "book_probe_draft");
  EXPECT_EQ(draft.index, (int)registry().size());
  EXPECT_TRUE(draft.external);
  EXPECT_EQ(draft.kind, "");
  EXPECT_TRUE(draft.available);
  EXPECT_FALSE(draft.videoExportable);
  EXPECT_EQ(draft.category, "Workspace");
  EXPECT_EQ(draft.blurb, drafts.path.string());
  EXPECT_EQ(draft.entryPath, drafts.path / "book_probe_draft.cpp");

  EXPECT_EQ(rows.back().key, "python_draft");
  EXPECT_EQ(rows.back().kind, "canvas");
}

TEST(SketchCatalogRows, AWorkspaceGroupsItsFilesByTheDirectoriesUnderIt) {
  const ScratchDir workspace("catalog_workspace");
  workspace.write("studies/light/lamp.cpp", "// a lamp\n");
  const std::filesystem::path lamp = workspace.path / "studies/light/lamp.cpp";
  const std::vector<CatalogRow> rows =
      catalog({.files = {lamp}, .workspaceRoot = workspace.path});
  const CatalogRow& row = rows.back();
  EXPECT_EQ(row.category, "Workspace \xc2\xb7 " +
                              workspace.path.filename().string() +
                              " \xc2\xb7 studies \xc2\xb7 light");
  EXPECT_EQ(row.entryPath, std::filesystem::path("studies/light/lamp.cpp"));
}

}  // namespace
