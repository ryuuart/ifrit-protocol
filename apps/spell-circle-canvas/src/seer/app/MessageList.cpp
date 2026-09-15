/** @file
 * The log as rows: the one line each message is summarised by, and
 * keeping the rows in step with the log behind them.
 */

#include "MessageList.h"

#include <sigilseer/wire/Rendering.h>

#include <cstddef>
#include <deque>
#include <string>

namespace {

/** How much of a message its one line shows before the rest is cut. A
 *  row is read at a glance and the whole of it is a click away in the
 *  readings above. */
constexpr int kPreviewLength = 160;

/** THE ONE LINE A MESSAGE IS RECOGNISED BY: its text when it is text,
 *  with every run of whitespace closed up so a document does not become
 *  a row of blanks, and its leading bytes when it is not. */
QString previewOf(const sigil::io::Bytes& bytes) {
  const std::string text = sigil::seer::printableText(bytes);
  if (text.empty())
    return QString::fromStdString(sigil::seer::hexadecimal(bytes, 24));
  const QString line = QString::fromStdString(text).simplified();
  if (line.size() <= kPreviewLength) return line;
  return line.left(kPreviewLength) + QStringLiteral("…");
}

}  // namespace

MessageList::MessageList(QObject* parent) : QAbstractListModel(parent) {}

int MessageList::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant MessageList::data(const QModelIndex& index, int role) const {
  if (index.row() < 0 || index.row() >= m_rows.size()) return {};
  const Row& row = m_rows.at(index.row());
  switch (role) {
    case AtField:
      return row.at;
    case GenerationField:
      return row.generation;
    case SizeField:
      return row.size;
    case PreviewField:
      return row.preview;
    default:
      return {};
  }
}

QHash<int, QByteArray> MessageList::roleNames() const {
  return {{AtField, "at"},
          {GenerationField, "generation"},
          {SizeField, "size"},
          {PreviewField, "preview"}};
}

void MessageList::refresh(const sigil::seer::Log& log) {
  const std::deque<sigil::seer::LogEntry>& entries = log.entries();
  // The log is in arrival order, so the run of messages this has not
  // seen is a tail of it: walk back from the end while the generation is
  // past the last one taken.
  size_t first = entries.size();
  while (first != 0 && entries[first - 1].generation > m_seen) --first;

  if (first != entries.size()) {
    const int added = int(entries.size() - first);
    beginInsertRows({}, int(m_rows.size()), int(m_rows.size()) + added - 1);
    for (size_t at = first; at != entries.size(); ++at) {
      const sigil::seer::LogEntry& entry = entries[at];
      Row row;
      row.at = entry.at;
      row.generation = entry.generation;
      row.size = entry.size;
      row.preview = entry.bytes ? previewOf(*entry.bytes) : QString();
      m_rows.append(row);
    }
    m_seen = entries.back().generation;
    endInsertRows();
  }

  const int excess = int(m_rows.size()) - int(log.capacity());
  if (excess > 0) {
    beginRemoveRows({}, 0, excess - 1);
    m_rows.remove(0, excess);
    endRemoveRows();
  }
}

void MessageList::clear() {
  if (!m_rows.isEmpty()) {
    beginResetModel();
    m_rows.clear();
    endResetModel();
  }
  m_seen = 0;
}
