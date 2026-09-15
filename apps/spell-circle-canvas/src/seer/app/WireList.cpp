/** @file
 * The connections list: the rows, and taking a tick's reading into them.
 */

#include "WireList.h"

#include <sigilseer/wire/Rendering.h>

#include <QtCore/QString>
#include <string>
#include <string_view>

WireList::WireList(QObject* parent) : QAbstractListModel(parent) {}

int WireList::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant WireList::data(const QModelIndex& index, int role) const {
  if (index.row() < 0 || index.row() >= m_rows.size()) return {};
  const Row& row = m_rows.at(index.row());
  switch (role) {
    case UriField:
      return row.uri;
    case AddressField:
      return row.address;
    case DialectField:
      return row.dialect;
    case LastFromField:
      return row.lastFrom;
    case ErrorField:
      return row.error;
    case ArrivalsPerSecondField:
      return row.arrivalsPerSecond;
    case GenerationField:
      return row.generation;
    case DroppedField:
      return row.dropped;
    case ClosedField:
      return row.closed;
    default:
      return {};
  }
}

QHash<int, QByteArray> WireList::roleNames() const {
  return {{UriField, "uri"},
          {AddressField, "address"},
          {DialectField, "dialect"},
          {LastFromField, "lastFrom"},
          {ErrorField, "error"},
          {ArrivalsPerSecondField, "arrivalsPerSecond"},
          {GenerationField, "generation"},
          {DroppedField, "dropped"},
          {ClosedField, "closed"}};
}

void WireList::refresh(const std::vector<sigil::seer::Vitals>& vitals) {
  const auto take = [](const sigil::seer::Vitals& read) {
    Row row;
    row.uri = QString::fromStdString(read.uri);
    row.address = QString::fromStdString(read.address);
    const std::string_view dialect = sigil::seer::dialect(read.uri);
    row.dialect = QString::fromUtf8(dialect.data(), qsizetype(dialect.size()));
    row.lastFrom =
        QString::fromStdString(sigil::seer::hostAndPort(read.lastFrom));
    row.error = QString::fromStdString(read.error);
    row.arrivalsPerSecond = read.arrivalsPerSecond;
    row.generation = read.generation;
    row.dropped = read.dropped;
    row.closed = read.closed;
    return row;
  };

  bool sameWires = size_t(m_rows.size()) == vitals.size();
  for (int at = 0; sameWires && at != m_rows.size(); ++at)
    sameWires = m_rows.at(at).uri.toStdString() == vitals[size_t(at)].uri;

  if (!sameWires) {
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(int(vitals.size()));
    for (const sigil::seer::Vitals& read : vitals) m_rows.append(take(read));
    endResetModel();
    return;
  }
  if (m_rows.isEmpty()) return;
  for (int at = 0; at != m_rows.size(); ++at)
    m_rows[at] = take(vitals[size_t(at)]);
  emit dataChanged(index(0), index(int(m_rows.size()) - 1));
}

QString WireList::uriAt(int row) const {
  if (row < 0 || row >= m_rows.size()) return {};
  return m_rows.at(row).uri;
}

int WireList::rowOf(const QString& uri) const {
  for (int at = 0; at != m_rows.size(); ++at)
    if (m_rows.at(at).uri == uri) return at;
  return -1;
}
