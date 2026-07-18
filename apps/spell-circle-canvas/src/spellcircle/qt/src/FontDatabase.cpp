#include "FontDatabase.h"
#include <QFontDatabase>

FontDatabase::FontDatabase(QObject *parent) : QObject(parent) {
  m_families = QFontDatabase::families();
  m_families.sort(Qt::CaseInsensitive);
}

QStringList FontDatabase::families() const { return m_families; }

QStringList FontDatabase::searchFamilies(const QString &query) const {
  if (query.isEmpty())
    return m_families;

  QStringList matches;
  for (const QString &family : m_families) {
    if (family.contains(query, Qt::CaseInsensitive))
      matches.append(family);
  }
  return matches;
}

QStringList FontDatabase::styles(const QString &family) const {
  return QFontDatabase::styles(family);
}

QFont FontDatabase::font(const QString &family, const QString &style,
                         int pointSize) const {
  return QFontDatabase::font(family, style, pointSize);
}

QString FontDatabase::styleForFont(const QFont &font) const {
  return QFontDatabase::styleString(font);
}
