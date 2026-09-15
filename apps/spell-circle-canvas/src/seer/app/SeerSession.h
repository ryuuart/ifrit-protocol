#pragma once

/** @file
 * THE SESSION: the wires, the log of the one being read, the way out
 * through a peer, the recorder, and the one timer that moves all of them
 * a frame at a time.
 *
 * Every pane in the window reads what this last wrote. Nothing asks a
 * wire a question of its own, so the list, the readings and the log are
 * one frame's answer rather than three asks a moment apart — and a wire
 * that took a message while the window was being painted shows up whole
 * on the next frame instead of half on this one.
 */

#include <QtQml/qqmlregistration.h>
#include <sigilseer/wire/Log.h>
#include <sigilseer/wire/Recorder.h>
#include <sigilseer/wire/Sender.h>
#include <sigilseer/wire/Wires.h>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <chrono>
#include <memory>

#include "MessageList.h"
#include "SendForm.h"
#include "WireDetail.h"
#include "WireList.h"

/** What the window is built around: one of these, made by the QML that
 *  declares the window. */
class SeerSession : public QObject {
  Q_OBJECT
  QML_ELEMENT

  /** The four things the panes are drawn from. Constant: the objects
   *  stand for the life of the session and it is their contents that
   *  move. */
  Q_PROPERTY(WireList* wires READ wires CONSTANT)
  Q_PROPERTY(MessageList* messages READ messages CONSTANT)
  Q_PROPERTY(WireDetail* reading READ reading CONSTANT)
  Q_PROPERTY(SendForm* sending READ sending CONSTANT)
  /** The row of the wire being read, or -1 when none is. */
  Q_PROPERTY(int selected READ selected WRITE select NOTIFY selectionChanged)
  /** Whether a wire is being written down, and to which file. */
  Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
  Q_PROPERTY(QString recordingPath READ recordingPath NOTIFY recordingChanged)
  /** The last thing the session has to say — why a wire would not open,
   *  what a recording could not do. Empty when there is nothing. */
  Q_PROPERTY(QString note READ note NOTIFY noteChanged)
  /** The root the loaded schema reads a message as, `feed_sky.Sky`;
   *  empty when no schema is loaded. It is the one thing about a schema
   *  a reader has to recognise to know they handed over the file they
   *  meant to. */
  Q_PROPERTY(QString schemaRoot READ schemaRoot NOTIFY schemaChanged)
  /** Whether this run's window can wear the machine's own dressing. The
   *  native glass is put on behind a native window, and a run drawing
   *  against no display has none to put it behind. */
  Q_PROPERTY(bool nativeChrome READ nativeChrome CONSTANT)

 public:
  /** How often the wires are read. It is the window's frame, so a
   *  repeating message goes out once a frame and a reading is never
   *  older than one. */
  static constexpr int kFrameMilliseconds = 16;
  static constexpr double kFrameSeconds = kFrameMilliseconds / 1000.0;

  explicit SeerSession(QObject* parent = nullptr);
  ~SeerSession() override;

  /** The wires a run opens before the window comes up: every URI the
   *  command line named, in the order it named them, the first of which
   *  is the one read. A static because the session is made by the QML
   *  that declares the window, which the command line is out of reach
   *  of. */
  static QStringList opensOn;

  /** The schema file a run is handed before the window comes up: what
   *  `--schema` named, loaded into the wires so the first message to
   *  arrive is already read through it. A file picker does the same
   *  thing once the window is up, but a picker needs a person, so a run
   *  that is driven from a script names the file here. */
  static QString readsThrough;

  /** Whether this run is here to be photographed. What is read back off
   *  a window is the frames that window drew, and the machine's glass is
   *  not among them — it lies behind the window — so a run that is going
   *  to be looked at as a picture keeps its opaque ground instead. */
  static bool photographed;

  [[nodiscard]] WireList* wires() { return &m_wireList; }
  [[nodiscard]] MessageList* messages() { return &m_messages; }
  [[nodiscard]] WireDetail* reading() { return &m_detail; }
  [[nodiscard]] SendForm* sending() { return &m_sendForm; }
  [[nodiscard]] int selected() const { return m_selectedRow; }
  [[nodiscard]] bool recording() const { return m_recording; }
  [[nodiscard]] QString recordingPath() const { return m_recordingPath; }
  [[nodiscard]] QString note() const { return m_note; }
  [[nodiscard]] QString schemaRoot() const { return m_schemaRoot; }
  [[nodiscard]] bool nativeChrome() const;

  /** Opens @p uri as a wire and reads it. A URI nothing can open is a
   *  wire all the same, with the note saying why. */
  Q_INVOKABLE void open(const QString& uri);

  /** Closes the wire in row @p row and drops it from the list. */
  Q_INVOKABLE void close(int row);

  /** Reads the wire in row @p row; -1 reads none. The log starts again,
   *  because it holds one wire's messages and this is another wire. */
  Q_INVOKABLE void select(int row);

  /** Writes every arrival on the wire being read to @p file from now
   *  on. */
  Q_INVOKABLE void recordTo(const QUrl& file);
  Q_INVOKABLE void stopRecording();

  /** Opens @p uri onto the recording in @p file: whatever was on that
   *  URI is closed, and the file takes its place. */
  Q_INVOKABLE void replay(const QString& uri, const QUrl& file);

  /** Reads every wire through the binary schema in @p file — what
   *  `flatc -b --schema` wrote beside a sender's generated header — so
   *  the messages are shown as the fields the schema names. A file that
   *  is no schema leaves the one that was loaded standing and says so
   *  in the note. */
  Q_INVOKABLE void loadSchema(const QUrl& file);

 signals:
  void selectionChanged();
  void recordingChanged();
  void noteChanged();
  void schemaChanged();

 private:
  /** One frame: the recordings are moved forward, every wire is read,
   *  the repeat sends what is due, the wire being read is drained into
   *  the log, and every pane is handed the result. */
  void tick();

  /** Seconds since the session was made — the clock every wire is moved
   *  and measured against. */
  [[nodiscard]] double elapsed() const;

  /** The feed being read, or null. */
  [[nodiscard]] std::shared_ptr<sigil::io::Feed> selectedFeed() const;

  /** Hands every pane what the last tick read. */
  void publish();

  void setNote(const QString& note);

  sigil::seer::Wires m_wires;
  sigil::seer::Log m_log;
  sigil::seer::Sender m_sender{m_wires};
  sigil::seer::Recorder m_recorder{m_wires};

  WireList m_wireList{this};
  MessageList m_messages{this};
  WireDetail m_detail{this};
  SendForm m_sendForm{m_sender, kFrameSeconds, this};
  QTimer m_frame{this};

  const std::chrono::steady_clock::time_point m_made =
      std::chrono::steady_clock::now();
  /** The wire being read, by URI rather than by row: rows move as wires
   *  are opened and closed, and what a reader picked was a wire. */
  QString m_selectedUri;
  int m_selectedRow = -1;
  QString m_note;
  QString m_recordingPath;
  QString m_schemaRoot;
  bool m_recording = false;
};
