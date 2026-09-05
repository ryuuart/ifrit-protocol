/** @file
 * The registry, the files behind it and the thumbnails beside it, read
 * once into the rows a browser shows — and the background worker that
 * fills a row's thumbnail on demand.
 */

#include "SketchCatalog.h"

#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "Thumbnails.h"

namespace fs = std::filesystem;
namespace sketch = sigil::sketch;
namespace book = sigil::sketch::book;

// Set by main() before QML loads, and by a test before it constructs one.
fs::path SketchCatalog::sketchDir;
std::vector<fs::path> SketchCatalog::externals;
fs::path SketchCatalog::thumbnailDir;
sigil::weave::FontContext* SketchCatalog::thumbnailFonts = nullptr;
sigil::sketch::Assets* SketchCatalog::thumbnailAssets = nullptr;

namespace {

/** WHAT A SKETCH SAYS ABOUT ITSELF AT THE TOP OF ITS OWN FILE, and how
 *  much of it there is.
 *
 *  The rule, stated here and in the library's README so that an author
 *  can write to it:
 *
 *  The header is every line from the first line of the file down to the
 *  first line that is neither a comment nor blank — a run of line
 *  comments, a doc block, or one after the other. The markers come off,
 *  and a line reading only `@file` is dropped. What is left reads as
 *  PARAGRAPHS: runs of non-blank lines, broken by blank lines and by
 *  rule lines (a line of nothing but `=` or `-`).
 *
 *  * The **subject** is the first paragraph after the title paragraph —
 *    the title being the first one, which by convention opens
 *    `stem.cpp — …`. A one-line paragraph that ends no sentence is a
 *    heading: it is kept and read on into the paragraph below it, so a
 *    file that puts `THE PATTERN` over its opening prose shows both.
 *  * **Edit these first** is the paragraph opening with a line that
 *    reads exactly `EDIT THESE FIRST`, minus that line — the knobs the
 *    author says to reach for, stated once, beside the code they name.
 *
 *  A file with neither shows neither. Nothing here is required of a
 *  sketch: the rule describes what is shown, it does not impose a
 *  format. */
struct Header {
  std::string subject;
  std::string editFirst;
  int lines = 0;
};

std::string trimmed(const std::string& text) {
  const size_t from = text.find_first_not_of(" \t\r");
  if (from == std::string::npos) return {};
  return text.substr(from, text.find_last_not_of(" \t\r") + 1 - from);
}

/** A line of nothing but rule characters, which authors use as a
 *  section break where a blank line would be too quiet. */
bool isRule(const std::string& text) {
  return text.size() >= 3 && text.find_first_not_of("=-") == std::string::npos;
}

/** One header line with its comment marker taken off, or nothing when
 *  the line carries no text of its own. Leading indentation SURVIVES:
 *  the knobs an author lists are indented under their heading, and a
 *  list flattened to the left margin stops reading as one. */
std::string undecorate(std::string text) {
  const std::string bare = trimmed(text);
  if (bare.rfind("///", 0) == 0)
    text = text.substr(text.find("///") + 3);
  else if (bare.rfind("//", 0) == 0)
    text = text.substr(text.find("//") + 2);
  else if (bare.rfind("/**", 0) == 0)
    text = text.substr(text.find("/**") + 3);
  else if (bare.rfind("/*", 0) == 0)
    text = text.substr(text.find("/*") + 2);
  else if (bare.rfind('*', 0) == 0)
    text = text.substr(text.find('*') + 1);
  if (const size_t close = text.find("*/"); close != std::string::npos)
    text = text.substr(0, close);
  if (!text.empty() && text.front() == ' ') text = text.substr(1);
  const std::string content = trimmed(text);
  if (content.empty() || content == "@file" || isRule(content)) return {};
  return text;
}

/** Prose wraps in a header and does not wrap in a panel, so a paragraph
 *  of prose comes back as one line. */
std::string unwrap(const std::vector<std::string>& paragraph) {
  std::string out;
  for (const std::string& line : paragraph) {
    if (!out.empty()) out += ' ';
    out += trimmed(line);
  }
  return out;
}

/** A LIST OF KNOBS, one line each. It keeps its own line breaks, unlike
 *  prose — and loses both the indentation that put it under a heading it
 *  is no longer under and the wrapping the file's own margin forced on
 *  it: a line indented deeper than the first is an entry that ran long,
 *  so it rejoins the line above rather than reading as a knob of its
 *  own. */
std::string knobs(const std::vector<std::string>& paragraph) {
  size_t base = std::string::npos;
  for (const std::string& line : paragraph) {
    const size_t indent = line.find_first_not_of(" \t");
    if (indent != std::string::npos) base = std::min(base, indent);
  }
  std::string out;
  for (const std::string& line : paragraph) {
    const size_t indent = line.find_first_not_of(" \t");
    if (indent == std::string::npos) continue;
    if (!out.empty() && indent > base)
      out += ' ' + trimmed(line);
    else
      out += (out.empty() ? "" : "\n") + trimmed(line);
  }
  return out;
}

/** A paragraph of one line that ends no sentence: a heading over the
 *  paragraph below it rather than a statement of its own. */
bool isHeading(const std::vector<std::string>& paragraph) {
  if (paragraph.size() != 1) return false;
  const std::string line = trimmed(paragraph.front());
  return !line.empty() && line.back() != '.';
}

Header readHeader(const fs::path& file) {
  Header header;
  std::ifstream stream(file);
  if (!stream) return header;

  std::vector<std::vector<std::string>> paragraphs;
  std::vector<std::string> current;
  bool inHeader = true;
  bool inBlock = false;
  std::string line;
  while (std::getline(stream, line)) {
    ++header.lines;
    if (!inHeader) continue;
    const std::string bare = trimmed(line);
    std::string text;
    if (inBlock) {
      text = undecorate(line);
      if (bare.find("*/") != std::string::npos) inBlock = false;
    } else if (bare.empty()) {
      text = {};
    } else if (bare.rfind("//", 0) == 0) {
      text = undecorate(line);
    } else if (bare.rfind("/*", 0) == 0) {
      text = undecorate(line);
      inBlock = bare.find("*/") == std::string::npos;
    } else {
      inHeader = false;  // the first line of code closes the header
      continue;
    }
    if (text.empty()) {
      if (!current.empty()) paragraphs.push_back(std::exchange(current, {}));
    } else {
      current.push_back(text);
    }
  }
  if (!current.empty()) paragraphs.push_back(current);

  // The subject: past the title, then headings until something says
  // something.
  for (size_t i = 1; i < paragraphs.size(); ++i) {
    if (!header.subject.empty()) header.subject += '\n';
    header.subject += unwrap(paragraphs[i]);
    if (!isHeading(paragraphs[i])) break;
  }
  for (const std::vector<std::string>& paragraph : paragraphs) {
    if (trimmed(paragraph.front()) != "EDIT THESE FIRST") continue;
    header.editFirst = knobs({paragraph.begin() + 1, paragraph.end()});
    break;
  }
  return header;
}

/** One row, with everything the file can say filled in and the canvas
 *  left for a session to answer. The plate is filled from the store
 *  afterward, and re-filled as the worker renders one. */
QVariantMap rowFor(int index, const std::string& name, const std::string& key,
                   const QString& folder, const QString& blurb,
                   const fs::path& file) {
  const Header header = readHeader(file);
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
  // ONE LINE OUT OF A RUN THAT PRINTS MANY. Each action answers on a line
  // of its own — `--frame` and `--video` name the file they wrote, and
  // `--bench` prefixes its verdict so a collector can find it — so the panel
  // keeps the marked line and drops the rest rather than growing a log pane.
  connect(&m_task, &QProcess::finished, this, [this](int code) {
    const QStringList output =
        QString::fromUtf8(m_task.readAll()).split(QLatin1Char('\n'));
    QString found;
    for (const QString& line : output)
      if (line.startsWith(m_taskPrefix)) found = line.trimmed();
    m_taskLine = found.isEmpty()
                     ? QStringLiteral("no answer (exit %1)").arg(code)
                     : found;
    emit taskChanged();
  });
  connect(&m_task, &QProcess::stateChanged, this,
          [this] { emit taskChanged(); });

  const auto& entries = sketch::registry();
  m_rows.reserve((qsizetype)entries.size() +
                 (qsizetype)SketchCatalog::externals.size());
  for (int i = 0; i < (int)entries.size(); ++i) {
    const sketch::Entry& entry = entries[i];
    // The bare file, or the entry of a directory sketch: what the row
    // reads its header and its line count from, and what a click opens.
    const fs::path file =
        sketch::sourceOf(SketchCatalog::sketchDir, entry.key);
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
    if (!SketchCatalog::thumbnailDir.empty()) {
      const std::string k = book::thumbnailKey(file);
      const fs::path fresh =
          book::freshThumbnail(SketchCatalog::thumbnailDir, entry.name, k);
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

  m_worker = std::thread(&SketchCatalog::renderLoop, this);
}

SketchCatalog::~SketchCatalog() {
  {
    const std::lock_guard lock(m_mutex);
    m_stop = true;
  }
  m_wake.notify_all();
  if (m_worker.joinable()) m_worker.join();
}

QVariantMap SketchCatalog::learn(int index, const QString& canvas,
                                 double moment, const QString& background,
                                 const QString& runtime) {
  if (index < 0 || index >= m_rows.size()) return {};
  QVariantMap row = m_rows[index].toMap();
  const bool kindKnown =
      !row.value(QStringLiteral("kind")).toString().isEmpty();
  const bool learnKind = !kindKnown && !runtime.isEmpty();
  if (!learnKind &&
      row.value(QStringLiteral("canvas")).toString() == canvas &&
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
  if (SketchCatalog::thumbnailDir.empty()) return false;
  const sketch::Entry& entry = sketch::registry()[index];
  const fs::path file = sketch::sourceOf(SketchCatalog::sketchDir, entry.key);
  const std::string key = book::thumbnailKey(file);
  const fs::path fresh =
      book::freshThumbnail(SketchCatalog::thumbnailDir, entry.name, key);
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

void SketchCatalog::requestThumbnail(int index) {
  const auto& entries = sketch::registry();
  // A file opened by path would have to be built to be rendered; the row
  // keeps its runtime glyph until it is presented.
  if (index < 0 || index >= (int)entries.size()) return;
  if (!entries[index].available()) return;
  if (fillFromDisk(index)) return;  // already on disk
  if (SketchCatalog::thumbnailDir.empty() ||
      SketchCatalog::thumbnailFonts == nullptr ||
      SketchCatalog::thumbnailAssets == nullptr)
    return;
  const std::lock_guard lock(m_mutex);
  if (m_failed.count(index) || m_queued.count(index)) return;
  m_queued.insert(index);
  m_pending.push_back(index);
  m_wake.notify_one();
}

void SketchCatalog::cancelThumbnail(int index) {
  const std::lock_guard lock(m_mutex);
  if (m_inFlight == index) return;  // one already rendering finishes
  const auto at = std::find(m_pending.begin(), m_pending.end(), index);
  if (at != m_pending.end()) m_pending.erase(at);
  m_queued.erase(index);
}

void SketchCatalog::renderLoop() {
  for (;;) {
    int index = -1;
    {
      std::unique_lock lock(m_mutex);
      m_wake.wait(lock, [this] { return m_stop || !m_pending.empty(); });
      if (m_stop) return;
      index = m_pending.front();
      m_pending.pop_front();
      m_inFlight = index;
    }

    const sketch::Entry& entry = sketch::registry()[index];
    const fs::path file = sketch::sourceOf(SketchCatalog::sketchDir, entry.key);
    const std::string key = book::thumbnailKey(file);
    const fs::path out =
        book::thumbnailFile(SketchCatalog::thumbnailDir, entry.name, key);
    const bool ok = book::renderThumbnail(entry, *SketchCatalog::thumbnailFonts,
                                          *SketchCatalog::thumbnailAssets, out,
                                          book::kThumbnailWidth);
    {
      const std::lock_guard lock(m_mutex);
      m_inFlight = -1;
      m_queued.erase(index);
      if (!ok) m_failed.insert(index);
    }
    const QString name = QString::fromUtf8(entry.name);
    // Back to the GUI thread to touch the model.
    QMetaObject::invokeMethod(
        this,
        [this, index, ok, name] {
          if (ok)
            fillFromDisk(index);
          else
            emit thumbnailFailed(name);
        },
        Qt::QueuedConnection);
  }
}

void SketchCatalog::frame(int index) {
  if (index < 0 || index >= m_rows.size()) return;
  const fs::path file = m_rows[index]
                            .toMap()
                            .value(QStringLiteral("path"))
                            .toString()
                            .toStdString();
  const fs::path out =
      file.parent_path() / "captures" / (file.stem().string() + ".png");
  std::error_code code;
  fs::create_directories(out.parent_path(), code);
  run(m_rows[index].toMap().value(QStringLiteral("name")).toString(),
      {QString::fromStdString(file.string()), QStringLiteral("--frame"),
       QString::fromStdString(out.string())},
      QStringLiteral("wrote "));
}

QUrl SketchCatalog::videoDefault(int index) const {
  if (index >= 0 && index < m_rows.size()) {
    const fs::path file = m_rows[index]
                              .toMap()
                              .value(QStringLiteral("path"))
                              .toString()
                              .toStdString();
    return QUrl::fromLocalFile(QString::fromStdString(
        (file.parent_path() / "captures" / (file.stem().string() + ".mp4"))
            .string()));
  }
  const fs::path movies =
      QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
          .toStdString();
  return QUrl::fromLocalFile(
      QString::fromStdString((movies / "sigil-sketchbook.mp4").string()));
}

void SketchCatalog::video(int index, const QUrl& output) {
  if (m_task.state() != QProcess::NotRunning || !output.isLocalFile()) return;
  if (index < -1 || index >= m_rows.size()) return;

  fs::path out = output.toLocalFile().toStdString();
  if (out.extension() != ".mp4") out += ".mp4";
  std::error_code code;
  fs::create_directories(out.parent_path(), code);

  QString label = QStringLiteral("All sketches");
  QStringList arguments{QStringLiteral("--video"),
                        QString::fromStdString(out.string())};
  if (index >= 0) {
    const QVariantMap row = m_rows[index].toMap();
    if (!row.value(QStringLiteral("videoExportable")).toBool()) return;
    label = row.value(QStringLiteral("name")).toString();
    arguments << QStringLiteral("--sketch")
              << row.value(QStringLiteral("key")).toString();
  }
  run(label, arguments, QStringLiteral("wrote "));
}

void SketchCatalog::bench(int index) {
  if (index < 0 || index >= m_rows.size()) return;
  const QString file =
      m_rows[index].toMap().value(QStringLiteral("path")).toString();
  run(m_rows[index].toMap().value(QStringLiteral("name")).toString(),
      {file, QStringLiteral("--bench")}, QStringLiteral("BENCH"));
}

void SketchCatalog::reveal(int index) {
  if (index < 0 || index >= m_rows.size()) return;
  const QString file =
      m_rows[index].toMap().value(QStringLiteral("path")).toString();
  QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), file});
}

void SketchCatalog::run(const QString& label, const QStringList& arguments,
                        const QString& prefix) {
  if (m_task.state() != QProcess::NotRunning) return;
  m_taskPrefix = prefix;
  m_taskLine = label + QStringLiteral(" — running…");
  emit taskChanged();
  // THE SAME BINARY, on the same file. A run through the app's own
  // headless flags is the one that answers for what the app is showing:
  // a second executable could have been built from other sources.
  m_task.setProcessChannelMode(QProcess::MergedChannels);
  m_task.start(QCoreApplication::applicationFilePath(), arguments);
}
