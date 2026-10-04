#include <gtest/gtest.h>
#include <sigilsketch/core/Catalog.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include "../src/CatalogRowMap.h"

namespace {

using grimoire::catalogRowJson;
using grimoire::catalogRowMap;
using sigil::sketch::CatalogRow;

QJsonObject printed(const CatalogRow& row) {
  const QByteArray line = catalogRowJson(catalogRowMap(row));
  EXPECT_FALSE(line.contains('\n')) << "one row is one line";
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(line, &error);
  EXPECT_EQ(error.error, QJsonParseError::NoError) << line.constData();
  return document.object();
}

// The plate ledger reads the registry off `--catalog` by these four keys:
// the filed name a plate is written under, the runtime a tier selects by,
// and whether this machine can run the sketch and why not. A renamed key
// is a ledger that stops reading the registry, so each is pinned here.
TEST(GrimoireCatalogRow, AnUnavailableRowCarriesItsFiledNameRuntimeAndReason) {
  CatalogRow row;
  row.name = "tile map";
  row.title = "Tile Map";
  row.key = "tile_map";
  row.kind = "set";
  row.available = false;
  row.reason = "needs a device";

  const QJsonObject object = printed(row);
  EXPECT_EQ(object.value("filedName").toString(), QStringLiteral("tile map"));
  EXPECT_EQ(object.value("name").toString(), QStringLiteral("Tile Map"));
  EXPECT_EQ(object.value("kind").toString(), QStringLiteral("set"));
  ASSERT_TRUE(object.value("available").isBool());
  EXPECT_FALSE(object.value("available").toBool());
  EXPECT_EQ(object.value("reason").toString(),
            QStringLiteral("needs a device"));
}

TEST(GrimoireCatalogRow, AnAvailableRowSaysSoWithAnEmptyReason) {
  CatalogRow row;
  row.name = "cascade";
  row.title = "Cascade";
  row.kind = "canvas";

  const QJsonObject object = printed(row);
  EXPECT_EQ(object.value("filedName").toString(), QStringLiteral("cascade"));
  EXPECT_EQ(object.value("kind").toString(), QStringLiteral("canvas"));
  ASSERT_TRUE(object.value("available").isBool());
  EXPECT_TRUE(object.value("available").toBool());
  ASSERT_TRUE(object.contains("reason"));
  EXPECT_TRUE(object.value("reason").toString().isEmpty());
}

}  // namespace
