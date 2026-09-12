/** @file
 * The registry, the files behind it and the thumbnails beside it, read
 * once into the rows a browser shows — and the background worker that
 * fills a row's thumbnail on demand.
 */

#include "SketchCatalog.h"

#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/plate/ThumbnailQueue.h>
#include <sigilsketch/plate/Thumbnails.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace sketch = sigil::sketch;

// Set by main() before QML loads, and before the rows are printed.
fs::path SketchCatalog::sketchDirectory;
std::vector<fs::path> SketchCatalog::externals;
fs::path SketchCatalog::thumbnailDirectory;
std::chrono::milliseconds SketchCatalog::thumbnailBudget =
    sketch::kThumbnailBudget;
bool SketchCatalog::thumbnailHeavy = false;
int SketchCatalog::opensAt = 0;
bool SketchCatalog::opensWithoutFill = false;
sigil::weave::FontContext* SketchCatalog::thumbnailFonts = nullptr;
sigil::sketch::Assets* SketchCatalog::thumbnailAssets = nullptr;

namespace {

/** One row, with everything the file can say filled in and the canvas
 *  left for a session to answer. The plate is filled from the store
 *  afterward, and re-filled as the worker renders one. */
QVariantMap rowFor(int index, const std::string& name, const std::string& key,
                   const QString& folder, const QString& blurb,
                   const fs::path& file) {
  const sketch::SourceMetadata header = sketch::sourceMetadata(file);
  QVariantMap row;
  row.insert(QStringLiteral("sketchIndex"), index);
  row.insert(QStringLiteral("name"),
             QString::fromStdString(sketch::title(name)));
  row.insert(QStringLiteral("key"), QString::fromStdString(key));
  row.insert(QStringLiteral("folder"), folder);
  row.insert(QStringLiteral("blurb"), blurb);
  row.insert(QStringLiteral("path"), QString::fromStdString(file.string()));
  row.insert(QStringLiteral("lines"), header.lines);
  row.insert(QStringLiteral("subject"), QString::fromStdString(header.subject));
  row.insert(QStringLiteral("editFirst"),
             QString::fromStdString(header.editFirst));
  QStringList tags;
  for (const auto& tag : header.tags)
    tags.push_back(QString::fromStdString(tag));
  row.insert(QStringLiteral("tags"), tags);
  row.insert(QStringLiteral("plate"), QString());
  // Answered by a running session, and empty until one has run.
  row.insert(QStringLiteral("canvas"), QString());
  row.insert(QStringLiteral("background"), QString());
  row.insert(QStringLiteral("moment"), -1.0);
  row.insert(QStringLiteral("videoExportable"), true);
  return row;
}

}  // namespace

SketchCatalog::SketchCatalog(QObject* parent) : QObject(parent) {
  const auto& entries = sketch::registry();
  m_rows.reserve((qsizetype)entries.size() +
                 (qsizetype)SketchCatalog::externals.size());
  for (int i = 0; i < (int)entries.size(); ++i) {
    const sketch::Entry& entry = entries[i];
    // The bare file, or the entry of a directory sketch: what the row
    // reads its header and its line count from, and what a click opens.
    const fs::path file =
        sketch::sourceOf(SketchCatalog::sketchDirectory, entry.key);
    QVariantMap row =
        rowFor(i, entry.name, entry.key, QString::fromUtf8(entry.category),
               QString::fromUtf8(entry.blurb), file);
    // Which runtime it draws through, read off the kind rather than
    // guessed from the folder: opening it costs nothing, and running it
    // is what a session does.
    const sketch::Kind kind = entry.kind();
    row.insert(QStringLiteral("kind"),
               kind ? QString::fromUtf8(kind->runtime().data(),
                                        (qsizetype)kind->runtime().size())
                    : QString());
    // A sketch over an SDK whose runtime data this machine lacks is
    // UNAVAILABLE rather than broken, and says what is missing.
    std::string why;
    row.insert(QStringLiteral("available"), entry.available(&why));
    row.insert(QStringLiteral("reason"), QString::fromStdString(why));
    // A fresh thumbnail already in the store shows at once, without a
    // render — a warm command or an earlier look left it behind.
    if (!SketchCatalog::thumbnailDirectory.empty()) {
      const std::string k = sketch::thumbnailKey(file);
      const fs::path fresh = sketch::freshThumbnail(
          SketchCatalog::thumbnailDirectory, entry.name, k);
      if (!fresh.empty())
        row.insert(QStringLiteral("plate"),
                   QUrl::fromLocalFile(QString::fromStdString(fresh.string()))
                       .toString());
    }
    m_rows.push_back(row);
  }
  // …and the files this session was pointed at, under their own stems.
  // Their directory stands in for a folder: two drafts may share a stem,
  // and where they stand is the only thing that tells them apart.
  for (int i = 0; i < (int)SketchCatalog::externals.size(); ++i) {
    const fs::path& file = SketchCatalog::externals[i];
    const std::string stem = file.stem().string();
    QVariantMap row =
        rowFor((int)entries.size() + i, stem, stem, QStringLiteral("Workspace"),
               QString::fromStdString(file.parent_path().string()), file);
    // A file opened by path is compiled when it is opened, so which
    // runtime it draws through is not known until it has been.
    row.insert(QStringLiteral("kind"), QString());
    row.insert(QStringLiteral("available"), true);
    row.insert(QStringLiteral("reason"), QString());
    row.insert(QStringLiteral("videoExportable"), false);
    m_rows.push_back(row);
  }

  // THE RENDER IS THIS CLASS'S and the ORDER is the library's: the queue
  // knows nothing about a registry, a store or a window, and this lambda
  // is the whole of what a still costs to draw.
  m_thumbnails = std::make_unique<sketch::ThumbnailQueue>(
      [](int index, const std::atomic_bool& stop) {
        const sketch::Entry& entry = sketch::registry()[index];
        const fs::path file =
            sketch::sourceOf(SketchCatalog::sketchDirectory, entry.key);
        sketch::ThumbnailRun run;
        run.outputPath =
            sketch::thumbnailFile(SketchCatalog::thumbnailDirectory, entry.name,
                                  sketch::thumbnailKey(file));
        run.stem = entry.name;
        run.maxDimension = sketch::kThumbnailWidth;
        run.budget = SketchCatalog::thumbnailBudget;
        run.heavy = SketchCatalog::thumbnailHeavy;
        run.stop = &stop;
        return sketch::renderThumbnail(entry, *SketchCatalog::thumbnailFonts,
                                       *SketchCatalog::thumbnailAssets, run);
      },
      [this](int index, sketch::ThumbnailOutcome outcome, int remaining) {
        reportThumbnail(index, outcome, remaining);
      });
}

SketchCatalog::~SketchCatalog() { stopThumbnails(); }

void SketchCatalog::stopThumbnails() {
  if (m_thumbnails) m_thumbnails->stop();
  m_filling = false;
}

QVariantMap SketchCatalog::learn(int index, const QString& canvas,
                                 double moment, const QString& background,
                                 const QString& runtime) {
  if (index < 0 || index >= m_rows.size()) return {};
  QVariantMap row = m_rows[index].toMap();
  const bool kindKnown =
      !row.value(QStringLiteral("kind")).toString().isEmpty();
  const bool learnKind = !kindKnown && !runtime.isEmpty();
  if (!learnKind && row.value(QStringLiteral("canvas")).toString() == canvas &&
      row.value(QStringLiteral("moment")).toDouble() == moment &&
      row.value(QStringLiteral("background")).toString() == background)
    return {};
  row.insert(QStringLiteral("canvas"), canvas);
  row.insert(QStringLiteral("moment"), moment);
  row.insert(QStringLiteral("background"), background);
  // A file opened by path first learns its runtime here: the row could
  // not read it off a file that had not been built.
  if (learnKind) row.insert(QStringLiteral("kind"), runtime);
  m_rows[index] = row;
  return row;
}

bool SketchCatalog::fillFromDisk(int index) {
  if (index < 0 || index >= (int)sketch::registry().size()) return false;
  if (SketchCatalog::thumbnailDirectory.empty()) return false;
  const sketch::Entry& entry = sketch::registry()[index];
  const fs::path file =
      sketch::sourceOf(SketchCatalog::sketchDirectory, entry.key);
  const std::string key = sketch::thumbnailKey(file);
  const fs::path fresh = sketch::freshThumbnail(
      SketchCatalog::thumbnailDirectory, entry.name, key);
  if (fresh.empty()) return false;
  const QString url =
      QUrl::fromLocalFile(QString::fromStdString(fresh.string())).toString();
  QVariantMap row = m_rows[index].toMap();
  if (row.value(QStringLiteral("plate")).toString() == url) return true;
  row.insert(QStringLiteral("plate"), url);
  m_rows[index] = row;
  emit thumbnailReady(index, row);
  return true;
}

namespace {

/** True when the sketch at @p index still owes the store a still: it is
 *  a registry sketch this machine can run, nothing fresh is on disk for
 *  it, and no note says why there never will be. */
bool wantsThumbnail(int index) {
  const auto& entries = sketch::registry();
  if (index < 0 || index >= (int)entries.size()) return false;
  if (!entries[index].available()) return false;
  if (SketchCatalog::thumbnailDirectory.empty()) return false;
  const sketch::Entry& entry = entries[index];
  const fs::path file =
      sketch::sourceOf(SketchCatalog::sketchDirectory, entry.key);
  const std::string key = sketch::thumbnailKey(file);
  if (!sketch::freshThumbnail(SketchCatalog::thumbnailDirectory, entry.name,
                              key)
           .empty())
    return false;
  return sketch::thumbnailNote(SketchCatalog::thumbnailDirectory, entry.name,
                               key)
      .empty();
}

}  // namespace

void SketchCatalog::fillThumbnails() {
  if (m_filling || !m_thumbnails || m_thumbnails->ended()) return;
  if (SketchCatalog::thumbnailDirectory.empty() ||
      SketchCatalog::thumbnailFonts == nullptr ||
      SketchCatalog::thumbnailAssets == nullptr)
    return;
  std::vector<int> wanted;
  for (int i = 0; i < (int)sketch::registry().size(); ++i)
    if (wantsThumbnail(i)) wanted.push_back(i);
  if (wanted.empty()) return;
  m_fillTotal = (int)wanted.size();
  m_fillDone = 0;
  m_fillNote.clear();
  m_filling = true;
  m_thumbnails->fill(std::move(wanted));
  emit fillChanged();
}

void SketchCatalog::endFill() {
  // The stop stands whether or not a fill was running: what it says is
  // that this worker will not be asked for another still, and opening a
  // sketch says that whenever it happens.
  if (m_thumbnails) m_thumbnails->endFill();
  if (!m_filling) return;
  m_filling = false;
  emit fillChanged();
}

void SketchCatalog::requestThumbnail(int index) {
  const auto& entries = sketch::registry();
  // A file opened by path would have to be built to be rendered; the row
  // keeps its runtime glyph until it is presented.
  if (index < 0 || index >= (int)entries.size()) return;
  if (!entries[index].available()) return;
  if (fillFromDisk(index)) return;  // already on disk
  // OUTSIDE THE FILL NOTHING IS QUEUED AT ALL: the canvas is presenting,
  // and a second renderer beside it is what makes opening a sketch feel
  // slow. Inside it, the queue puts an asked-for row at the front.
  if (!m_filling || !m_thumbnails) return;
  m_thumbnails->request(index);
}

void SketchCatalog::cancelThumbnail(int index) {
  if (m_thumbnails) m_thumbnails->cancel(index);
}

void SketchCatalog::adoptThumbnail(int index) { fillFromDisk(index); }

void SketchCatalog::reportThumbnail(int index, sketch::ThumbnailOutcome outcome,
                                    int remaining) {
  const sketch::Entry& entry = sketch::registry()[index];
  // ONE LINE PER SKETCH THAT HAS NO STILL, written beside where the still
  // would have gone so the next launch does not spend the budget finding
  // out again. A failure is the exception: it says nothing about how long
  // the sketch takes, only that this host could not draw it, and it is
  // remembered for this run alone.
  std::string note;
  switch (outcome) {
    case sketch::ThumbnailOutcome::Heavy:
      note = "declared a plate";
      break;
    case sketch::ThumbnailOutcome::OverBudget:
      note = "still ran past its budget";
      break;
    case sketch::ThumbnailOutcome::Failed:
      note = "could not be drawn";
      break;
    case sketch::ThumbnailOutcome::Wrote:
    case sketch::ThumbnailOutcome::Stopped:
      break;
  }
  if (!note.empty() && outcome != sketch::ThumbnailOutcome::Failed) {
    const fs::path file =
        sketch::sourceOf(SketchCatalog::sketchDirectory, entry.key);
    sketch::noteThumbnail(SketchCatalog::thumbnailDirectory, entry.name,
                          sketch::thumbnailKey(file), note);
  }
  const QString name = QString::fromUtf8(entry.name);
  const QString why = QString::fromStdString(note);
  // Back to the GUI thread to touch the model.
  QMetaObject::invokeMethod(
      this,
      [this, index, name, why, remaining] {
        finished(index, name, why, remaining);
      },
      Qt::QueuedConnection);
}

void SketchCatalog::finished(int index, const QString& name,
                             const QString& note, int remaining) {
  if (note.isEmpty())
    fillFromDisk(index);
  else
    emit thumbnailNoted(name, note);
  if (!m_filling) return;
  m_fillDone = std::max(m_fillDone + 1, m_fillTotal - remaining);
  if (!note.isEmpty()) m_fillNote = name + QStringLiteral(" — ") + note;
  if (remaining <= 0) m_filling = false;
  emit fillChanged();
}
