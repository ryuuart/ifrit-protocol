/** @file
 * The session: opening and closing wires, choosing the one to read, the
 * peer a run is pointed at and the one thing it says, recording and
 * replaying, and the frame that moves all of it.
 */

#include "SeerSession.h"

#include <sigildata/decode/FlatBuffer.h>

#include <QtGui/QGuiApplication>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

QStringList SeerSession::opensOn;
QString SeerSession::readsThrough;
QString SeerSession::sendsTo;
std::optional<QString> SeerSession::says;
bool SeerSession::photographed = false;

namespace {

/** The local file @p url names, as a path. A URL a dialog answered with
 *  is a file URL; anything else is taken as the path it spells, which is
 *  what a URI typed by hand is. */
std::filesystem::path pathOf(const QUrl& url) {
  const QString spelled =
      url.isLocalFile() ? url.toLocalFile() : url.toString();
  return std::filesystem::path(spelled.toStdString());
}

}  // namespace

SeerSession::SeerSession(QObject* parent) : QObject(parent) {
  connect(&m_frame, &QTimer::timeout, this, &SeerSession::tick);
  m_frame.setInterval(kFrameMilliseconds);
  m_frame.start();
  // The schema goes on before a wire is opened, so the first message to
  // arrive is read through it rather than shown as bytes for a frame.
  if (!readsThrough.isEmpty()) loadSchema(QUrl::fromLocalFile(readsThrough));
  for (const QString& uri : opensOn) open(uri);
  // Opening a wire is asking to read it, so the last one opened would
  // be the one read — but a command line names them all at once, and
  // the wire a reader is watching is the one they named first; the rest
  // are named to have them open beside it.
  if (!opensOn.isEmpty()) select(0);
  // The peer is opened where the wires are, before the first frame, so
  // the send pane comes up on the form that peer's own dialect asks for
  // rather than on an editor for a moment; a peer nothing can open
  // stands in the list with the sentence that says why, as every other
  // wire does.
  if (!sendsTo.isEmpty()) {
    m_sendForm.setPeerUri(sendsTo);
    m_sendForm.reachPeer();
  }
  // The form is filled in here as well, so the pane comes up holding
  // what this run has to say and a picture of it is a picture of the
  // message that went out. It is sent a frame later, and not filled in
  // then: an editor reads what it holds as it is built and is the
  // reader's to type into afterwards.
  if (says) m_unsaid = m_sendForm.fill(*says);
}

SeerSession::~SeerSession() = default;

bool SeerSession::nativeChrome() const {
  // The dressing is put on the window the machine's own platform gives,
  // which a run drawing against no display has none of — and it is put
  // BEHIND that window, where a picture read back off the frames cannot
  // reach it, so a photographed run keeps its opaque ground and is a
  // picture of something rather than of nothing.
  if (photographed) return false;
  return QGuiApplication::platformName() == QLatin1String("cocoa");
}

double SeerSession::elapsed() const {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                       m_made)
      .count();
}

std::shared_ptr<sigil::io::Feed> SeerSession::selectedFeed() const {
  if (m_selectedUri.isEmpty()) return nullptr;
  return m_wires.feed(m_selectedUri.toStdString());
}

void SeerSession::tick() {
  const double seconds = elapsed();
  // The recordings move first, so what one of them delivered this frame
  // is read by the tick below rather than a frame later.
  m_wires.dispatch(seconds);
  m_wires.tick(seconds);
  m_sender.tick(seconds);
  // What a run was given to say goes out on a frame rather than as the
  // session was made: every wire it opened is standing and being read by
  // then, so a message said to a port this same run listens on crosses
  // onto a wire a reader is already watching.
  if (m_unsaid) {
    m_unsaid = false;
    m_sendForm.sendOnce();
  }

  if (const std::shared_ptr<sigil::io::Feed> feed = selectedFeed()) {
    const size_t taken = m_log.drain(*feed);
    // Echoing takes the messages out of the log rather than off the
    // feed: a feed hands a message out once, and the log is where it
    // is after that.
    const std::deque<sigil::seer::LogEntry>& entries = m_log.entries();
    const size_t echoed = taken < entries.size() ? taken : entries.size();
    for (size_t at = entries.size() - echoed; at != entries.size(); ++at)
      if (entries[at].bytes) m_sendForm.echo(*entries[at].bytes);
  }
  publish();
}

void SeerSession::publish() {
  m_wireList.refresh(m_wires.vitals());
  m_messages.refresh(m_log);
  m_detail.show(m_selectedUri.isEmpty()
                    ? nullptr
                    : m_wires.vitalsOf(m_selectedUri.toStdString()));
  m_sendForm.refresh();

  const int row = m_wireList.rowOf(m_selectedUri);
  if (row != m_selectedRow) {
    m_selectedRow = row;
    emit selectionChanged();
  }
  const bool recording = m_recorder.recording();
  const QString path = QString::fromStdString(m_recorder.path().string());
  if (recording != m_recording || path != m_recordingPath) {
    m_recording = recording;
    m_recordingPath = path;
    emit recordingChanged();
  }
}

void SeerSession::open(const QString& uri) {
  const QString named = uri.trimmed();
  if (named.isEmpty()) return;
  const std::shared_ptr<sigil::io::Feed> feed =
      m_wires.open(named.toStdString());
  m_wires.tick(elapsed());
  m_wireList.refresh(m_wires.vitals());
  setNote(QString::fromStdString(feed->error()));
  // A wire that was just asked for is the wire to look at.
  select(m_wireList.rowOf(named));
}

void SeerSession::close(int row) {
  const QString uri = m_wireList.uriAt(row);
  if (uri.isEmpty()) return;
  m_wires.close(uri.toStdString());
  if (uri == m_selectedUri) select(-1);
  m_wires.tick(elapsed());
  publish();
}

void SeerSession::select(int row) {
  const QString uri = m_wireList.uriAt(row);
  if (uri == m_selectedUri) return;
  m_selectedUri = uri;
  // The log holds one wire's messages, and this is another wire.
  m_log.clear();
  m_messages.clear();
  publish();
}

void SeerSession::recordTo(const QUrl& file) {
  const std::shared_ptr<sigil::io::Feed> feed = selectedFeed();
  if (!feed) {
    setNote(QStringLiteral("nothing to record: no wire is being read"));
    return;
  }
  if (!m_recorder.record(feed, pathOf(file))) {
    setNote(QStringLiteral("the recording could not be started"));
    return;
  }
  setNote({});
  publish();
}

void SeerSession::stopRecording() {
  m_recorder.stop();
  publish();
}

void SeerSession::replay(const QString& uri, const QUrl& file) {
  const QString named = uri.trimmed();
  if (named.isEmpty()) {
    setNote(
        QStringLiteral("a replay needs the URI to open the recording onto"));
    return;
  }
  const std::shared_ptr<sigil::io::Feed> feed =
      m_recorder.replay(named.toStdString(), pathOf(file));
  m_wires.tick(elapsed());
  m_wireList.refresh(m_wires.vitals());
  setNote(QString::fromStdString(feed->error()));
  // The wire that was there was closed to make room for the file, so
  // what the reader was looking at is this.
  m_selectedUri.clear();
  select(m_wireList.rowOf(named));
}

void SeerSession::loadSchema(const QUrl& file) {
  const std::filesystem::path path = pathOf(file);
  const std::shared_ptr<const sigil::io::Bytes> bytes =
      m_wires.hub().blob(path.string());
  if (!bytes) {
    setNote(QStringLiteral("the schema could not be read: ") +
            QString::fromStdString(path.string()));
    return;
  }
  std::string why;
  sigil::data::Schema schema =
      sigil::data::Schema::fromBinarySchema(bytes->bytes, &why);
  if (!schema) {
    // The schema that was loaded stays loaded: a reader who opened the
    // wrong file is left reading what they were reading before it.
    setNote(QString::fromStdString(why));
    return;
  }
  const QString root(QString::fromStdString(std::string(schema.rootName())));
  m_wires.readThrough(std::move(schema));
  m_detail.readThrough(m_wires.schema());
  if (root != m_schemaRoot) {
    m_schemaRoot = root;
    emit schemaChanged();
  }
  setNote({});
  publish();
}

void SeerSession::setNote(const QString& note) {
  if (note == m_note) return;
  m_note = note;
  emit noteChanged();
}
