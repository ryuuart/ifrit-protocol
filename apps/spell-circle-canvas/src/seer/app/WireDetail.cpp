/** @file
 * The selected wire's detail, the seven readings of its newest message,
 * and which of them the wire is opened on.
 */

#include "WireDetail.h"

#include <sigilseer/wire/Rendering.h>

#include <QtCore/QString>
#include <cstddef>
#include <string>

namespace {

/** How many bytes of a message the hexadecimal reading shows. A message
 *  is recognised by its opening — a magic number, a header, the first
 *  field — and a reader who needs every byte of a long one is reading
 *  the recording rather than the pane. */
constexpr size_t kHexadecimalLimit = 1024;

}  // namespace

WireDetail::WireDetail(QObject* parent) : QObject(parent) {}

int WireDetail::naturalReading() const {
  // A schema was gone and found for these messages, so a message it
  // reads is read through it: the names of the fields are in the schema
  // and nowhere in the bytes, and no other reading can show them.
  if (!m_schema.isEmpty()) return Schema;
  // The scheme says what the wire carries, which the bytes of one
  // message often do not: a packet and a document are both a run of
  // printable characters to look at, and the wire is where a reader was
  // told which one they are looking at.
  const int mark = m_uri.indexOf(QLatin1String("://"));
  const QString scheme = mark < 0 ? QString() : m_uri.left(mark);
  if (scheme == QLatin1String("midi") && !m_midi.isEmpty()) return Midi;
  if (scheme == QLatin1String("artnet") && !m_dmx.isEmpty()) return Dmx;
  // A cable ends its messages at a newline, so what arrives on one is a
  // line somebody wrote — and where it is not text it is a board
  // speaking bytes, which is the reading that shows them.
  if (scheme == QLatin1String("serial"))
    return m_text.isEmpty() ? Hexadecimal : Text;
  if (scheme == QLatin1String("osc") && !m_osc.isEmpty()) return Osc;
  if (!m_json.isEmpty()) return Json;
  return Hexadecimal;
}

void WireDetail::readThrough(const sigil::data::Schema& schema) {
  m_readThrough = schema;
  // The message on screen was read before this schema was here, so the
  // mark that says it has been read is taken off and the next tick
  // reads it again.
  m_readUri.clear();
  m_readGeneration = 0;
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
    m_midi.clear();
    m_dmx.clear();
    m_schema.clear();
    m_schemaNote.clear();
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
        m_json.isEmpty() && m_osc.isEmpty() && m_midi.isEmpty() &&
        m_dmx.isEmpty() && m_schema.isEmpty())
      return false;
    m_byteSize = 0;
    m_hexadecimal.clear();
    m_text.clear();
    m_json.clear();
    m_osc.clear();
    m_midi.clear();
    m_dmx.clear();
    m_schema.clear();
    m_schemaNote.clear();
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
  m_midi = QString::fromStdString(sigil::seer::midiReading(bytes));
  m_dmx = QString::fromStdString(sigil::seer::dmxReading(bytes));
  // The sentence stands only where a schema was handed over and this
  // message is not one it holds. A reader who has asked for no schema is
  // not told on every message that they have none: the disabled tab is
  // the whole of what there is to say to them.
  std::string why;
  m_schema = QString::fromStdString(
      sigil::seer::schemaReading(bytes, m_readThrough, &why));
  m_schemaNote = m_schema.isEmpty() && static_cast<bool>(m_readThrough)
                     ? QString::fromStdString(why)
                     : QString();
  m_readUri = m_uri;
  m_readGeneration = m_generation;
  return true;
}
