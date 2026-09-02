/** @file
 * The registry, the files behind it and the plates beside it, read once
 * into the rows a browser shows.
 */

#include "SketchCatalog.h"

#include <sigilsketch/core/Registry.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "SketchbookView.h"

namespace fs = std::filesystem;
namespace sketch = sigil::sketch;

// Set by main() before QML loads: the build's own quick-tier baseline,
// or whatever `--plates` named instead.
fs::path SketchCatalog::platesDir;

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

/** The plate the quick tier photographed this sketch as, or nothing
 *  where no sweep has run on this machine. A plate is named the way the
 *  sketch is FILED, which is what the baseline holds it under. */
QString plateFor(const std::string& name) {
  if (SketchCatalog::platesDir.empty()) return {};
  const fs::path plate = SketchCatalog::platesDir / ("plate_" + name + ".png");
  std::error_code code;
  if (!fs::exists(plate, code)) return {};
  return QUrl::fromLocalFile(QString::fromStdString(plate.string())).toString();
}

/** One row, with everything the file and the plate can say filled in and
 *  the canvas left for a session to answer. */
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
  row.insert(QStringLiteral("plate"), plateFor(name));
  // Answered by a running session, and empty until one has run.
  row.insert(QStringLiteral("canvas"), QString());
  row.insert(QStringLiteral("background"), QString());
  row.insert(QStringLiteral("moment"), -1.0);
  return row;
}

}  // namespace

SketchCatalog::SketchCatalog(QObject* parent) : QObject(parent) {
  // ONE LINE OUT OF A RUN THAT PRINTS MANY. Both flags answer on a line
  // of their own — `--frame` names the file it wrote, `--bench` prefixes
  // its verdict so a collector can find it — so the panel keeps the
  // marked line and drops the rest rather than growing a log pane.
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

  const auto& entries = sketch::registry();
  m_rows.reserve((qsizetype)entries.size() +
                 (qsizetype)SketchbookView::externals.size());
  for (int i = 0; i < (int)entries.size(); ++i) {
    const sketch::Entry& entry = entries[i];
    const fs::path file =
        SketchbookView::sketchDir / (std::string(entry.key) + ".cpp");
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
    m_rows.push_back(row);
  }
  // …and the files this session was pointed at, under their own stems.
  // Their directory stands in for a folder: two drafts may share a stem,
  // and where they stand is the only thing that tells them apart.
  for (int i = 0; i < (int)SketchbookView::externals.size(); ++i) {
    const fs::path& file = SketchbookView::externals[i];
    const std::string stem = file.stem().string();
    QVariantMap row =
        rowFor((int)entries.size() + i, stem, stem, QStringLiteral("Workspace"),
               QString::fromStdString(file.parent_path().string()), file);
    // A file opened by path is compiled when it is opened, so which
    // runtime it draws through is not known until it has been.
    row.insert(QStringLiteral("kind"), QString());
    row.insert(QStringLiteral("available"), true);
    row.insert(QStringLiteral("reason"), QString());
    m_rows.push_back(row);
  }
}

void SketchCatalog::learn(int index, const QString& canvas, double moment,
                          const QString& background) {
  if (index < 0 || index >= m_rows.size()) return;
  QVariantMap row = m_rows[index].toMap();
  if (row.value(QStringLiteral("canvas")).toString() == canvas &&
      row.value(QStringLiteral("moment")).toDouble() == moment)
    return;
  row.insert(QStringLiteral("canvas"), canvas);
  row.insert(QStringLiteral("moment"), moment);
  row.insert(QStringLiteral("background"), background);
  m_rows[index] = row;
  emit sketchesChanged();
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
  run(index,
      {QString::fromStdString(file.string()), QStringLiteral("--frame"),
       QString::fromStdString(out.string())},
      QStringLiteral("wrote "));
}

void SketchCatalog::bench(int index) {
  if (index < 0 || index >= m_rows.size()) return;
  const QString file =
      m_rows[index].toMap().value(QStringLiteral("path")).toString();
  run(index, {file, QStringLiteral("--bench")}, QStringLiteral("BENCH"));
}

void SketchCatalog::reveal(int index) {
  if (index < 0 || index >= m_rows.size()) return;
  const QString file =
      m_rows[index].toMap().value(QStringLiteral("path")).toString();
  QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), file});
}

void SketchCatalog::run(int index, const QStringList& arguments,
                        const QString& prefix) {
  if (m_task.state() != QProcess::NotRunning) return;
  m_taskPrefix = prefix;
  m_taskLine = m_rows[index].toMap().value(QStringLiteral("name")).toString() +
               QStringLiteral(" — running…");
  emit taskChanged();
  // THE SAME BINARY, on the same file. A run through the app's own
  // headless flags is the one that answers for what the app is showing:
  // a second executable could have been built from other sources.
  m_task.setProcessChannelMode(QProcess::MergedChannels);
  m_task.start(QCoreApplication::applicationFilePath(), arguments);
}
