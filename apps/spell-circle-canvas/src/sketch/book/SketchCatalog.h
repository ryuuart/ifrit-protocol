#pragma once

/** @file
 * What the browser knows about every sketch before one is opened, and the
 * thumbnail store it fills on demand.
 */

#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtCore/QUrl>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch {
class Assets;
}

/** EVERY SKETCH AS A ROW, so a reader can go through the registry
 *  without opening anything.
 *
 *  Three sources, and each row says which of them it came from:
 *
 *  * the **registry** — the filed name, the folder, the blurb, the file
 *    stem, which runtime it draws through, and whether this machine can
 *    run it;
 *  * the **file** — the bare file, or the entry of a sketch that is a
 *    directory — how many lines it is, the subject it states at the top
 *    of itself, and the knobs it says to reach for first;
 *  * the **plate** — the still, as a thumbnail, from this app's own
 *    store: rendered on demand into a cache beside the binary and kept
 *    until the sketch's source or the host changes.
 *
 *  What is NOT here is the canvas: a sketch declares its size, its
 *  ground and the moment it is worth photographing from inside its own
 *  setup, so those are facts of a RUNNING session and cannot be read off
 *  a file that has not run. They arrive through learn() as sketches are
 *  presented, and a row that has never been presented says so rather
 *  than guessing.
 *
 *  The rows index the same two lists SketchbookView selects by, in the
 *  same order: the registry first, then the files this session was
 *  pointed at. */
class SketchCatalog : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList sketches READ sketches NOTIFY sketchesChanged)
  /** The one line the last Frame, Video or Bench run left behind, and whether
   *  one is still going. These are the whole of what the inspector shows
   *  for a subprocess: these runs answer in one line by design. */
  Q_PROPERTY(QString taskLine READ taskLine NOTIFY taskChanged)
  Q_PROPERTY(bool taskRunning READ taskRunning NOTIFY taskChanged)

 public:
  explicit SketchCatalog(QObject* parent = nullptr);
  ~SketchCatalog() override;

  [[nodiscard]] QVariantList sketches() const { return m_rows; }
  [[nodiscard]] QString taskLine() const { return m_taskLine; }
  [[nodiscard]] bool taskRunning() const {
    return m_task.state() != QProcess::NotRunning;
  }

  /** What a running session declared about itself. A row keeps the last
   *  answer it was given: a sketch looked at once still reads its canvas
   *  back after the resident set has let it go. @p runtime fills an empty
   *  `kind` — a file opened by path does not draw through any runtime
   *  until it has been built, and this is the first thing that knows.
   *  Returns the changed row, or an empty map when the answer was already
   *  known. Learning one row deliberately does not reset the whole
   *  sketches model. */
  Q_INVOKABLE QVariantMap learn(int index, const QString& canvas, double moment,
                                const QString& background,
                                const QString& runtime);

  /** ASK FOR THE THUMBNAIL of the sketch at @p index — what a browser row
   *  calls as it comes on screen. A fresh one already on disk fills the
   *  row at once; a missing or stale one is queued for the background
   *  worker, which renders one at a time in the order rows asked. A row
   *  that is unavailable, or a file opened by path (which would have to be
   *  built to be seen), is left with its runtime glyph. */
  Q_INVOKABLE void requestThumbnail(int index);
  /** Drops a pending request — what a row calls as it scrolls away, so
   *  the worker spends its one render on what is still on screen. A
   *  render already in flight finishes. */
  Q_INVOKABLE void cancelThumbnail(int index);

  /** Render one still of the sketch through this same binary's `--frame`
   *  path, into `captures/` beside the file. */
  Q_INVOKABLE void frame(int index);
  /** Export the registry, or one registry sketch, as a vertical MP4. */
  Q_INVOKABLE void video(int index, const QUrl& output);
  /** A writable filename for the video save dialog. */
  Q_INVOKABLE QUrl videoDefault(int index) const;
  /** Run the 60 FPS gate over it, and keep the verdict line. */
  Q_INVOKABLE void bench(int index);
  /** Show the file in the Finder. */
  Q_INVOKABLE void reveal(int index);

  /** WHERE THE SKETCH SOURCES STAND, and the files this session was
   *  pointed at beyond the registry. Set by main() before QML loads; the
   *  book library owns them because the catalog and the live view both
   *  read them and neither should reach into the other. */
  static std::filesystem::path sketchDir;
  static std::vector<std::filesystem::path> externals;

  /** THE THUMBNAIL STORE: one directory this app owns, under the platform
   *  cache location unless the command line or an environment variable
   *  named another. Set by main() before QML loads. */
  static std::filesystem::path thumbnailDir;
  /** What the background worker renders a still with — the process's one
   *  font context and asset store. Set by main() before QML loads; the
   *  worker renders nothing until both are here. */
  static sigil::weave::FontContext* thumbnailFonts;
  static sigil::sketch::Assets* thumbnailAssets;

 signals:
  void sketchesChanged();
  void taskChanged();
  /** A thumbnail landed for @p index: the row, with its plate filled in,
   *  for QML to overlay without remounting every other thumbnail. */
  void thumbnailReady(int index, QVariantMap row);
  /** A sketch could not be rendered — named once, for the status strip,
   *  and never retried, so a broken sketch is not a render storm. */
  void thumbnailFailed(const QString& name);

 private:
  /** Runs this binary against one sketch file and keeps one line of what
   *  it said. Only one at a time: these are seconds-long renders, and a
   *  second one started over the first would report whichever finished
   *  last under whichever button was pressed first. */
  void run(const QString& label, const QStringList& arguments,
           const QString& prefix);

  /** The background worker loop: one render at a time, in the order rows
   *  asked, marshalling each result back to the GUI thread. */
  void renderLoop();
  /** Fills @p index's row plate from a fresh thumbnail already on disk,
   *  emitting thumbnailReady when it changes. True when one was there. */
  bool fillFromDisk(int index);

  QVariantList m_rows;
  QProcess m_task;
  QString m_taskLine;
  QString m_taskPrefix;

  // The thumbnail worker and its queue. The mutex guards the queue, the
  // in-flight index and the stop flag; the row model is touched only on
  // the GUI thread.
  std::thread m_worker;
  std::mutex m_mutex;
  std::condition_variable m_wake;
  std::deque<int> m_pending;
  std::set<int> m_queued;   // what is pending or in flight, to dedupe
  std::set<int> m_failed;   // rendered once and failed — never retried
  int m_inFlight = -1;
  bool m_stop = false;
};
