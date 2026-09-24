/** @file
 * One catalog row as the map the browser reads and `--catalog` prints.
 */

#include "CatalogRowMap.h"

#include <sigilsketch/core/Catalog.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <string>

namespace sketchbook {

namespace {

QString text(const std::string& value) { return QString::fromStdString(value); }

}  // namespace

QVariantMap catalogRowMap(const sigil::sketch::CatalogRow& row) {
  QVariantMap map;
  map.insert(QStringLiteral("sketchIndex"), row.index);
  // What the browser shows as the name is the display spelling; the filed
  // name a plate is written under travels beside it.
  map.insert(QStringLiteral("name"), text(row.title));
  map.insert(QStringLiteral("filedName"), text(row.name));
  map.insert(QStringLiteral("key"), text(row.key));
  map.insert(QStringLiteral("folder"), text(row.category));
  map.insert(QStringLiteral("blurb"), text(row.blurb));
  map.insert(QStringLiteral("path"), text(row.path.string()));
  map.insert(QStringLiteral("entryPath"), text(row.entryPath.string()));
  map.insert(QStringLiteral("external"), row.external);
  map.insert(QStringLiteral("lines"), row.source.lines);
  map.insert(QStringLiteral("subject"), text(row.source.subject));
  map.insert(QStringLiteral("editFirst"), text(row.source.editFirst));
  QStringList tags;
  for (const auto& tag : row.source.tags) tags.push_back(text(tag));
  map.insert(QStringLiteral("tags"), tags);
  map.insert(QStringLiteral("kind"), text(row.kind));
  map.insert(QStringLiteral("available"), row.available);
  map.insert(QStringLiteral("reason"), text(row.reason));
  map.insert(QStringLiteral("videoExportable"), row.videoExportable);
  // The thumbnail is filled from the store afterward, and re-filled as the
  // worker renders one; the canvas is answered by a running session and
  // is empty until one has run.
  map.insert(QStringLiteral("plate"), QString());
  map.insert(QStringLiteral("canvas"), QString());
  map.insert(QStringLiteral("background"), QString());
  map.insert(QStringLiteral("moment"), -1.0);
  return map;
}

QByteArray catalogRowJson(const QVariantMap& row) {
  return QJsonDocument(QJsonObject::fromVariantMap(row))
      .toJson(QJsonDocument::Compact);
}

}  // namespace sketchbook
