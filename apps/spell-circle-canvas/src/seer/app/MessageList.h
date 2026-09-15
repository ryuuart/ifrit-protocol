#pragma once

/** @file
 * THE LOG AS ROWS: every message taken off the wire being read, newest
 * last, each with the sender it came from.
 *
 * The log below it is the truth, and this mirrors it by appending what
 * it has not seen and letting go of what the log has already forgotten —
 * so a reader scrolled back through a busy wire stays where they were
 * instead of being thrown to the end every frame.
 */

#include <QtQml/qqmlregistration.h>
#include <sigilseer/wire/Log.h>

#include <QtCore/QAbstractListModel>
#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QModelIndex>
#include <QtCore/QString>
#include <QtCore/QVariant>

/** The receive pane's log: one row per message. */
class MessageList : public QAbstractListModel {
  Q_OBJECT
  QML_ANONYMOUS

 public:
  enum Field {
    AtField = Qt::UserRole + 1,
    GenerationField,
    FromField,
    SizeField,
    PreviewField,
  };

  explicit MessageList(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  /** Appends whatever @p log has taken since the last refresh, and drops
   *  as many of the oldest rows as the log itself has let go. */
  void refresh(const sigil::seer::Log& log);

  /** Empties the rows, and forgets which message was last seen — which
   *  is what makes the next wire's first message an appended row rather
   *  than one this thinks it already has. */
  void clear();

 private:
  struct Row {
    double at = 0;
    qulonglong generation = 0;
    /** Who sent it, without the scheme: the wire the log belongs to
     *  spells that already, and what a reader is comparing down the
     *  column is which end the message came from. */
    QString from;
    qulonglong size = 0;
    QString preview;
  };

  QList<Row> m_rows;
  qulonglong m_seen = 0;
};
