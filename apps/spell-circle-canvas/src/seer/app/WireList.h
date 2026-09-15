#pragma once

/** @file
 * EVERY WIRE AS A ROW, in the order the session opened them.
 *
 * A row says what the last tick read and nothing more: the model asks
 * the wires for nothing of its own, so what the list shows and what the
 * readings beside it show are the same frame's answer rather than two
 * asks a moment apart.
 */

#include <QtQml/qqmlregistration.h>
#include <sigilseer/wire/Wires.h>

#include <QtCore/QAbstractListModel>
#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QModelIndex>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <vector>

/** The connections list: one row per wire the session has open. */
class WireList : public QAbstractListModel {
  Q_OBJECT
  QML_ANONYMOUS

 public:
  enum Field {
    UriField = Qt::UserRole + 1,
    AddressField,
    ErrorField,
    ArrivalsPerSecondField,
    GenerationField,
    DroppedField,
    ClosedField,
  };

  explicit WireList(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  /** Takes what a tick read. The rows are replaced outright only when
   *  the set of wires changed; otherwise every row is updated in place,
   *  which is what keeps a reader's selection and scroll where they left
   *  them while the numbers move. */
  void refresh(const std::vector<sigil::seer::Vitals>& vitals);

  /** The URI of the wire at @p row, or empty when there is no such
   *  row. */
  QString uriAt(int row) const;

  /** The row the wire at @p uri stands in, or -1 when it is not open. */
  int rowOf(const QString& uri) const;

 private:
  struct Row {
    QString uri;
    QString address;
    QString error;
    double arrivalsPerSecond = 0;
    qulonglong generation = 0;
    qulonglong dropped = 0;
    bool closed = false;
  };

  QList<Row> m_rows;
};
