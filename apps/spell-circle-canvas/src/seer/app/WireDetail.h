#pragma once

/** @file
 * THE WIRE BEING READ: what it is doing, and its newest message shown
 * five ways at once.
 *
 * The readings are worked out when the message changes rather than when
 * they are asked for, so a wire carrying nothing new costs nothing to
 * keep on screen, and a wire carrying a message a frame is rendered
 * exactly once per message however many panes are looking at it. A
 * schema handed over is a sixth thing that can change the readings, so
 * it puts the message back up for reading as a new message would.
 */

#include <QtQml/qqmlregistration.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigilseer/wire/Wires.h>

#include <QtCore/QObject>
#include <QtCore/QString>

/** The selected wire's detail: the same numbers the row carries, and the
 *  message the row has no space for. */
class WireDetail : public QObject {
  Q_OBJECT
  QML_ANONYMOUS

  Q_PROPERTY(QString uri READ uri NOTIFY changed)
  Q_PROPERTY(QString address READ address NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(double arrivalsPerSecond READ arrivalsPerSecond NOTIFY changed)
  Q_PROPERTY(qulonglong generation READ generation NOTIFY changed)
  Q_PROPERTY(qulonglong dropped READ dropped NOTIFY changed)
  Q_PROPERTY(bool closed READ closed NOTIFY changed)
  /** Whether a wire is being read at all. Everything below is empty
   *  while this is false. */
  Q_PROPERTY(bool present READ present NOTIFY changed)
  /** The newest message: how many bytes it is, and the five readings —
   *  each empty when the message is not that. */
  Q_PROPERTY(qulonglong byteSize READ byteSize NOTIFY changed)
  Q_PROPERTY(QString hexadecimal READ hexadecimal NOTIFY changed)
  Q_PROPERTY(QString text READ text NOTIFY changed)
  Q_PROPERTY(QString json READ json NOTIFY changed)
  Q_PROPERTY(QString osc READ osc NOTIFY changed)
  /** The message as the loaded schema reads it, and — where a schema is
   *  loaded and this message is not one it holds — the sentence saying
   *  what stopped it. A reader who has asked for no schema is told
   *  nothing here: the disabled tab already says the reading is not
   *  there, and a wire that was never going to have one would otherwise
   *  carry that sentence on every message. */
  Q_PROPERTY(QString schema READ schema NOTIFY changed)
  Q_PROPERTY(QString schemaNote READ schemaNote NOTIFY changed)
  /** Which reading this wire is opened on, as its place among the
   *  readings named below. */
  Q_PROPERTY(int naturalReading READ naturalReading NOTIFY changed)

 public:
  /** THE READINGS, in the order a pane offers them: the bytes first,
   *  because every message has them, then what the message may be, and
   *  last the one a reader had to go and find a schema for. */
  enum Reading { Hexadecimal, Text, Json, Osc, Schema };
  Q_ENUM(Reading)

  explicit WireDetail(QObject* parent = nullptr);

  [[nodiscard]] QString uri() const { return m_uri; }
  [[nodiscard]] QString address() const { return m_address; }
  [[nodiscard]] QString error() const { return m_error; }
  [[nodiscard]] double arrivalsPerSecond() const { return m_arrivalsPerSecond; }
  [[nodiscard]] qulonglong generation() const { return m_generation; }
  [[nodiscard]] qulonglong dropped() const { return m_dropped; }
  [[nodiscard]] bool closed() const { return m_closed; }
  [[nodiscard]] bool present() const { return m_present; }
  [[nodiscard]] qulonglong byteSize() const { return m_byteSize; }
  [[nodiscard]] QString hexadecimal() const { return m_hexadecimal; }
  [[nodiscard]] QString text() const { return m_text; }
  [[nodiscard]] QString json() const { return m_json; }
  [[nodiscard]] QString osc() const { return m_osc; }
  [[nodiscard]] QString schema() const { return m_schema; }
  [[nodiscard]] QString schemaNote() const { return m_schemaNote; }

  /** THE READING A WIRE IS OPENED ON, before a reader picks one for
   *  themselves. A message the loaded schema reads is opened on that,
   *  since a reader who went and found a schema for this wire found it
   *  to read the fields; otherwise a wire of OSC packets is opened on
   *  the packet and every other wire on the document, each of them
   *  falling back to the bytes when the message is not that — because
   *  the bytes are the one reading every message has, and a pane opened
   *  on a reading that is empty says nothing about what arrived. */
  [[nodiscard]] int naturalReading() const;

  /** Reads every message from now on through @p schema as well, which
   *  is the fifth reading. A schema that is none takes that reading
   *  away. The message on screen is read again rather than left as it
   *  was, because a schema arriving changes what it says. */
  void readThrough(const sigil::data::Schema& schema);

  /** Shows what a tick read; null shows no wire at all. */
  void show(const sigil::seer::Vitals* vitals);

 signals:
  void changed();

 private:
  /** Reads the newest message every way, unless it is the message
   *  already read. */
  bool readMessage(const sigil::seer::Vitals& vitals);

  QString m_uri;
  QString m_address;
  QString m_error;
  double m_arrivalsPerSecond = 0;
  qulonglong m_generation = 0;
  qulonglong m_dropped = 0;
  bool m_closed = false;
  bool m_present = false;
  qulonglong m_byteSize = 0;
  QString m_hexadecimal;
  QString m_text;
  QString m_json;
  QString m_osc;
  QString m_schema;
  QString m_schemaNote;
  /** The schema the fifth reading is made through; none until one is
   *  handed over. */
  sigil::data::Schema m_readThrough;
  /** Which message the readings were made from: the wire it came off
   *  and the arrival it was, because generations begin again on every
   *  wire. */
  QString m_readUri;
  qulonglong m_readGeneration = 0;
};
