#include "SketchActions.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>
#include <QtCore/QStringList>
#include <filesystem>

namespace fs = std::filesystem;

SketchActions::SketchActions(QObject* parent) : QObject(parent) {
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

  connect(&m_task, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError) {
            m_taskLine = m_task.errorString();
            emit taskChanged();
          });
}

SketchActions::~SketchActions() {
  // The child render is this window's too: started from a button here,
  // it has nothing to report to once the window is gone, so it is ended
  // rather than left for QProcess to kill with a warning.
  if (m_task.state() != QProcess::NotRunning) {
    m_task.kill();
    m_task.waitForFinished();
  }
}

void SketchActions::frame(const QVariantMap& row) {
  if (row.value(QStringLiteral("path")).toString().isEmpty()) return;
  const fs::path file =
      row.value(QStringLiteral("path")).toString().toStdString();
  const fs::path out =
      file.parent_path() / "captures" / (file.stem().string() + ".png");
  std::error_code code;
  fs::create_directories(out.parent_path(), code);
  run(row.value(QStringLiteral("name")).toString(),
      {QString::fromStdString(file.string()), QStringLiteral("--frame"),
       QString::fromStdString(out.string())},
      QStringLiteral("wrote "));
}

QUrl SketchActions::videoDefault(const QVariantMap& row) const {
  if (!row.value(QStringLiteral("path")).toString().isEmpty()) {
    const fs::path file =
        row.value(QStringLiteral("path")).toString().toStdString();
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

void SketchActions::video(const QVariantMap& row, const QUrl& output) {
  if (m_task.state() != QProcess::NotRunning || !output.isLocalFile()) return;

  if (!row.isEmpty() && !row.value(QStringLiteral("videoExportable")).toBool())
    return;

  fs::path out = output.toLocalFile().toStdString();
  if (out.extension() != ".mp4") out += ".mp4";
  std::error_code code;
  fs::create_directories(out.parent_path(), code);

  QString label = QStringLiteral("All sketches");
  // The video path opens a device only when the selection needs one.
  QStringList arguments{QStringLiteral("--gpu"), QStringLiteral("--video"),
                        QString::fromStdString(out.string())};
  if (!row.isEmpty()) {
    label = row.value(QStringLiteral("name")).toString();
    arguments << QStringLiteral("--sketch")
              << row.value(QStringLiteral("key")).toString();
  }
  run(label, arguments, QStringLiteral("wrote "));
}

void SketchActions::bench(const QVariantMap& row) {
  if (row.value(QStringLiteral("path")).toString().isEmpty()) return;
  const QString file = row.value(QStringLiteral("path")).toString();
  run(row.value(QStringLiteral("name")).toString(),
      {file, QStringLiteral("--bench")}, QStringLiteral("BENCH"));
}

void SketchActions::reveal(const QVariantMap& row) {
  if (row.value(QStringLiteral("path")).toString().isEmpty()) return;
  const QString file = row.value(QStringLiteral("path")).toString();
  QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), file});
}

void SketchActions::run(const QString& label, const QStringList& arguments,
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
