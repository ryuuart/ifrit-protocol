/** @file
 * The selected wire's detail, the four readings of its newest message,
 * and which of them the wire is opened on.
 */

#include "WireDetail.h"

#include <sigilseer/wire/Rendering.h>

#include <QtCore/QString>
#include <cstddef>

namespace {

/** How many bytes of a message the hexadecimal reading shows. A message
 *  is recognised by its opening — a magic number, a header, the first
 *  field — and a reader who needs every byte of a long one is reading
 *  the recording rather than the pane. */
constexpr size_t kHexadecimalLimit = 1024;

}  // namespace

WireDetail::WireDetail(QObject* parent) : QObject(parent) {}

int WireDetail::naturalReading() const {
  // The scheme says what the wire carries, which the bytes of one
  // message often do not: a packet and a document are both a run of
  // printable characters to look at, and the wire is where a reader was
  // told which one they are looking at.
  const int mark = m_uri.indexOf(QLatin1String("://"));
  const QString scheme = mark < 0 ? QString() : m_uri.left(mark);
  if (scheme == QLatin1String("osc") && !m_osc.isEmpty()) return Osc;
  if (!m_json.isEmpty()) return Json;
  return Hexadecimal;
}

void WireDetail::show(const sigil::seer::Vitals* vitals) {
  if (!vitals) {
    if (!m_present) return;
    m_present = false;
    m_uri.clear();
    m_address.clear();
    m_error.clear();
    m_arrivalsPerSecond = 0;
    m_generation = 0;
    m_dropped = 0;
    m_closed = false;
    m_byteSize = 0;
    m_hexadecimal.clear();
    m_text.clear();
    m_json.clear();
    m_osc.clear();
    m_readUri.clear();
    m_readGeneration = 0;
    emit changed();
    return;
  }

  const QString uri = QString::fromStdString(vitals->uri);
  const QString address = QString::fromStdString(vitals->address);
  const QString error = QString::fromStdString(vitals->error);
  bool moved = !m_present || uri != m_uri || address != m_address ||
               error != m_error ||
               vitals->arrivalsPerSecond != m_arrivalsPerSecond ||
               vitals->generation != m_generation ||
               vitals->dropped != m_dropped || vitals->closed != m_closed;

  m_present = true;
  m_uri = uri;
  m_address = address;
  m_error = error;
  m_arrivalsPerSecond = vitals->arrivalsPerSecond;
  m_generation = vitals->generation;
  m_dropped = vitals->dropped;
  m_closed = vitals->closed;
  moved = readMessage(*vitals) || moved;
  if (moved) emit changed();
}

bool WireDetail::readMessage(const sigil::seer::Vitals& vitals) {
  if (!vitals.newest) {
    if (m_byteSize == 0 && m_hexadecimal.isEmpty() && m_text.isEmpty() &&
        m_json.isEmpty() && m_osc.isEmpty())
      return false;
    m_byteSize = 0;
    m_hexadecimal.clear();
    m_text.clear();
    m_json.clear();
    m_osc.clear();
    m_readUri.clear();
    m_readGeneration = 0;
    return true;
  }
  if (m_readUri == m_uri && m_readGeneration == m_generation) return false;

  const sigil::io::Bytes& bytes = *vitals.newest;
  m_byteSize = bytes.bytes.size();
  m_hexadecimal = QString::fromStdString(
      sigil::seer::hexadecimal(bytes, kHexadecimalLimit));
  m_text = QString::fromStdString(sigil::seer::printableText(bytes));
  m_json = QString::fromStdString(sigil::seer::indentedJson(bytes));
  m_osc = QString::fromStdString(sigil::seer::oscReading(bytes));
  m_readUri = m_uri;
  m_readGeneration = m_generation;
  return true;
}
