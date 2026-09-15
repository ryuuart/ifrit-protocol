#pragma once

/** @file
 * THE WIRE BEING READ: what it is doing, and its newest message shown
 * three ways at once.
 *
 * The readings are worked out when the message changes rather than when
 * they are asked for, so a wire carrying nothing new costs nothing to
 * keep on screen, and a wire carrying a message a frame is rendered
 * exactly once per message however many panes are looking at it.
 */

#include <QtQml/qqmlregistration.h>
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
  /** The newest message: how many bytes it is, and the three readings —
   *  each empty when the message is not that. */
  Q_PROPERTY(qulonglong byteSize READ byteSize NOTIFY changed)
  Q_PROPERTY(QString hexadecimal READ hexadecimal NOTIFY changed)
  Q_PROPERTY(QString text READ text NOTIFY changed)
  Q_PROPERTY(QString json READ json NOTIFY changed)

 public:
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

  /** Shows what a tick read; null shows no wire at all. */
  void show(const sigil::seer::Vitals* vitals);

 signals:
  void changed();

 private:
  /** Reads the newest message the three ways, unless it is the message
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
  /** Which message the readings were made from: the wire it came off
   *  and the arrival it was, because generations begin again on every
   *  wire. */
  QString m_readUri;
  qulonglong m_readGeneration = 0;
};
