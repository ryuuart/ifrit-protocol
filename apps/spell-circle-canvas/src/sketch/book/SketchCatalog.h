#pragma once

/** @file
 * What the browser knows about every sketch before one is opened.
 */

#include <QtQml/qqmlregistration.h>

#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <filesystem>

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
 *  * the **plate** — the quick-tier still, as a thumbnail.
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
  Q_PROPERTY(QVariantList sketches READ sketches NOTIFY sketchesChanged)
  /** The one line the last Frame or Bench run left behind, and whether
   *  one is still going. Both are the whole of what the inspector shows
   *  for a subprocess: these runs answer in one line by design. */
  Q_PROPERTY(QString taskLine READ taskLine NOTIFY taskChanged)
  Q_PROPERTY(bool taskRunning READ taskRunning NOTIFY taskChanged)

 public:
  explicit SketchCatalog(QObject* parent = nullptr);

  [[nodiscard]] QVariantList sketches() const { return m_rows; }
  [[nodiscard]] QString taskLine() const { return m_taskLine; }
  [[nodiscard]] bool taskRunning() const {
    return m_task.state() != QProcess::NotRunning;
  }

  /** What a running session declared about itself. A row keeps the last
   *  answer it was given: a sketch looked at once still reads its canvas
   *  back after the resident set has let it go. Returns the changed row,
   *  or an empty map when the answer was already known. Learning one row
   *  deliberately does not reset the whole sketches model. */
  Q_INVOKABLE QVariantMap learn(int index, const QString& canvas,
                                double moment, const QString& background);

  /** Render one still of the sketch through this same binary's `--frame`
   *  path, into `captures/` beside the file. */
  Q_INVOKABLE void frame(int index);
  /** Run the 60 FPS gate over it, and keep the verdict line. */
  Q_INVOKABLE void bench(int index);
  /** Show the file in the Finder. */
  Q_INVOKABLE void reveal(int index);

  /** Where `plate_<name>.png` is looked for; empty means no thumbnails.
   *  The build writes the quick-tier baseline beside its manifest, which
   *  is what main() fills this with; `--plates <dir>` names another. */
  static std::filesystem::path platesDir;

 signals:
  void sketchesChanged();
  void taskChanged();

 private:
  /** Runs this binary against one sketch file and keeps one line of what
   *  it said. Only one at a time: these are seconds-long renders, and a
   *  second one started over the first would report whichever finished
   *  last under whichever button was pressed first. */
  void run(int index, const QStringList& arguments, const QString& prefix);

  QVariantList m_rows;
  QProcess m_task;
  QString m_taskLine;
  QString m_taskPrefix;
};
