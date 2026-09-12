#pragma once

/** @file
 * What the browser knows about every sketch before one is opened, and the
 * thumbnail store it fills on demand.
 */

#include <QtQml/qqmlregistration.h>
#include <sigilsketch/plate/ThumbnailQueue.h>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <chrono>
#include <filesystem>
#include <memory>
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
 *    until the sketch's source changes.
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
  QML_ELEMENT
  /** CONSTANT because the LIST is: every row is built once, when the
   *  catalog is constructed, and the registry a binary was built with
   *  cannot gain or lose an entry while it runs. A row's own fields do
   *  change — a session tells the catalog what canvas a sketch declared,
   *  a thumbnail lands — and each of those is handed back as one row for
   *  the browser to overlay, which is what keeps a thumbnail that has
   *  already mounted from being remounted. */
  Q_PROPERTY(QVariantList sketches READ sketches CONSTANT)
  /** THE FILL, WHILE IT IS HAPPENING: whether it is, how many stills it
   *  set out to draw, how many of them are answered, and the last thing
   *  it had to say about a sketch it could not draw. */
  Q_PROPERTY(bool filling READ filling NOTIFY fillChanged)
  Q_PROPERTY(int fillTotal READ fillTotal NOTIFY fillChanged)
  Q_PROPERTY(int fillDone READ fillDone NOTIFY fillChanged)
  Q_PROPERTY(QString fillNote READ fillNote NOTIFY fillChanged)
  /** WHICH SKETCH THIS RUN OPENS ON, and whether it opens before the
   *  thumbnails are filled rather than after. Constant: both are decided
   *  from the command line before any of this exists. */
  Q_PROPERTY(int openIndex READ openIndex CONSTANT)
  Q_PROPERTY(bool openAtOnce READ openAtOnce CONSTANT)

 public:
  explicit SketchCatalog(QObject* parent = nullptr);
  ~SketchCatalog() override;

  [[nodiscard]] QVariantList sketches() const { return m_rows; }
  [[nodiscard]] bool filling() const { return m_filling; }
  [[nodiscard]] int fillTotal() const { return m_fillTotal; }
  [[nodiscard]] int fillDone() const { return m_fillDone; }
  [[nodiscard]] QString fillNote() const { return m_fillNote; }
  [[nodiscard]] int openIndex() const { return opensAt; }
  [[nodiscard]] bool openAtOnce() const { return opensWithoutFill; }
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

  /** THE FILL: draw a still for every sketch that has none.
   *
   *  Sketchbook's one stretch of background rendering, taken while
   *  nothing is being presented — before the reader has opened anything,
   *  which is the only time the machine is free. It queues every
   *  registry sketch whose thumbnail is missing or stale and that has no
   *  note saying why it has none, and renders them one at a time under a
   *  per-sketch budget. Nothing happens when the store, the fonts or the
   *  assets were never handed over. */
  Q_INVOKABLE void fillThumbnails();
  /** ENDS THE FILL AND JOINS THE WORKER, after which this object
   *  renders nothing again.
   *
   *  The destructor calls it; a host calls it FIRST when it is about to
   *  let go of something a still could be drawn through, because a
   *  worker joined after the release would be finishing its frame
   *  against what was released. Idempotent, and it costs one frame of
   *  whatever sketch was being walked: the walk is let go before the
   *  join rather than waited out. */
  void stopThumbnails();

  /** ENDS THE FILL, which opening a sketch does. Whatever was in flight
   *  is let go at its next frame and the queue is dropped: from here on
   *  the canvas is what draws, and a thumbnail is refreshed by looking at
   *  the sketch rather than by a second renderer competing with it. Once
   *  ended it does not begin again. */
  Q_INVOKABLE void endFill();

  /** ASK FOR THE THUMBNAIL of the sketch at @p index — what a browser row
   *  calls as it comes on screen. A fresh one already on disk fills the
   *  row at once. While the fill is running, a missing one is moved to
   *  the front of its queue, so what is on screen is drawn first; after
   *  it, nothing is queued — the sketch gets its still by being opened. */
  Q_INVOKABLE void requestThumbnail(int index);
  /** Drops a pending request — what a row calls as it scrolls away, so
   *  the fill spends its one render on what is still on screen. A render
   *  already in flight is left to finish. */
  Q_INVOKABLE void cancelThumbnail(int index);
  /** THE LIVE CANVAS LEFT A STILL for the sketch at @p index: read it off
   *  disk into the row. What the window calls after the sketch it is
   *  presenting has reached the moment it named. */
  Q_INVOKABLE void adoptThumbnail(int index);

  /** WHERE THE SKETCH SOURCES STAND, and the files this session was
   *  pointed at beyond the registry. Set by main() before QML loads; the
   *  book library owns them because the catalog and the live view both
   *  read them and neither should reach into the other. */
  static std::filesystem::path sketchDirectory;
  static std::vector<std::filesystem::path> externals;

  /** THE THUMBNAIL STORE: one directory this app owns, under the platform
   *  cache location unless the command line or an environment variable
   *  named another. Set by main() before QML loads. */
  static std::filesystem::path thumbnailDirectory;
  /** What one still of the fill is allowed, and whether a sketch that
   *  declared itself a plate is walked at all. Set by main() before QML
   *  loads. */
  static std::chrono::milliseconds thumbnailBudget;
  static bool thumbnailHeavy;

  /** THE SKETCH THE CANVAS OPENS ON, and whether it waits for the fill.
   *  A run that named a sketch, or that is here to photograph or measure
   *  one, is not browsing: it opens at once and no fill starts. Set by
   *  main() before QML loads. */
  static int opensAt;
  static bool opensWithoutFill;
  /** What the background worker renders a still with — the process's one
   *  font context and asset store. Set by main() before QML loads; the
   *  worker renders nothing until both are here. */
  static sigil::weave::FontContext* thumbnailFonts;
  static sigil::sketch::Assets* thumbnailAssets;

 signals:
  void fillChanged();
  /** A thumbnail landed for @p index: the row, with its plate filled in,
   *  for QML to overlay without remounting every other thumbnail. */
  void thumbnailReady(int index, QVariantMap row);
  /** A sketch has no still and @p why is the one line saying so — it ran
   *  past its budget, it declared itself a plate, or it could not be
   *  drawn at all. Said once and written down, so it is neither a render
   *  storm nor a question asked again at every launch. */
  void thumbnailNoted(const QString& name, const QString& why);

 private:
  /** WHAT THE QUEUE'S WORKER LEFT, on the worker's own thread: the note
   *  a sketch with no still is remembered by, and the marshalling back to
   *  the GUI thread of everything that touches the model. */
  void reportThumbnail(int index, sigil::sketch::ThumbnailOutcome outcome,
                       int remaining);
  /** What one still came to, on the GUI thread: the row or the note, and
   *  how much of the fill is left. */
  void finished(int index, const QString& name, const QString& note,
                int remaining);
  /** Fills @p index's row plate from a fresh thumbnail already on disk,
   *  emitting thumbnailReady when it changes. True when one was there. */
  bool fillFromDisk(int index);

  QVariantList m_rows;

  /** THE ORDER STILLS ARE DRAWN IN, and the worker that draws them —
   *  the library's, so what is ordered is testable without a window.
   *  This class supplies the render and marshals every report back to
   *  the GUI thread, and touches the row model nowhere else. */
  std::unique_ptr<sigil::sketch::ThumbnailQueue> m_thumbnails;

  // The fill, touched only on the GUI thread.
  bool m_filling = false;
  int m_fillTotal = 0;
  int m_fillDone = 0;
  QString m_fillNote;
};
