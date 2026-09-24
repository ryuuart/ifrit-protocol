#pragma once

/** @file
 * One catalog row as the map the browser reads and `--catalog` prints.
 */

#include <QtCore/QByteArray>
#include <QtCore/QVariantMap>

namespace sigil::sketch {
struct CatalogRow;
}

namespace sketchbook {

/** ONE CATALOG ROW AS A MAP, with its thumbnail and canvas still to be
 *  learned. `name` is the display spelling and `filedName` the name a
 *  plate is written under; `kind`, `available` and `reason` say which
 *  runtime it draws through and whether this machine can run it. A script
 *  that reads the registry off `--catalog` reads these keys, so renaming
 *  one breaks that script. */
[[nodiscard]] QVariantMap catalogRowMap(const sigil::sketch::CatalogRow& row);

/** A row map as the one compact JSON object `--catalog` prints for it on
 *  its own line, without the newline. */
[[nodiscard]] QByteArray catalogRowJson(const QVariantMap& row);

}  // namespace sketchbook
