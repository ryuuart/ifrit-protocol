#pragma once
#include <QFont>
#include <QObject>
#include <QStringList>
#include <QtQml/QtQml>

/**
 * QML singleton wrapping QFontDatabase so settings UI can offer a searchable,
 * previewable font-family picker and a family-specific style picker (Bold,
 * Light Italic, etc.) without QML needing to touch QFontDatabase's
 * all-static C++ API directly.
 */
class FontDatabase : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(FontDatabase)
  QML_SINGLETON

public:
  explicit FontDatabase(QObject *parent = nullptr);

  /** All installed font family names, sorted. */
  Q_INVOKABLE QStringList families() const;

  /** Family names containing @p query (case-insensitive). Returns all
   *  families when @p query is empty. */
  Q_INVOKABLE QStringList searchFamilies(const QString &query) const;

  /** Style names available for @p family (e.g. "Regular", "Bold Italic"). */
  Q_INVOKABLE QStringList styles(const QString &family) const;

  /** Builds a QFont for @p family/@p style at @p pointSize via
   *  QFontDatabase's own family/style matching. */
  Q_INVOKABLE QFont font(const QString &family, const QString &style,
                         int pointSize) const;

  /** Nearest style name describing an existing QFont, for initializing a
   *  style picker from a font that wasn't itself built via styles()+font(). */
  Q_INVOKABLE QString styleForFont(const QFont &font) const;

private:
  QStringList m_families;
};
