#include "SketchActions.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QStandardPaths>
#include <QtCore/QStringList>
#include <algorithm>
#include <filesystem>
#include <utility>

#include "PythonEnvironment.h"
#include "SketchCatalog.h"

namespace fs = std::filesystem;

fs::path SketchActions::workspaceRoot;
fs::path SketchActions::pythonExecutable;
QString SketchActions::pythonAbi;
QString SketchActions::startupError;
bool SketchActions::rememberSelections = true;

namespace {

QString pathString(const fs::path& path) {
  return QString::fromStdString(path.string());
}

fs::path localPath(const QUrl& url) {
  if (!url.isLocalFile()) return {};
  return QFileInfo(url.toLocalFile()).absoluteFilePath().toStdString();
}

bool within(const fs::path& file, const fs::path& root) {
  const auto relative = file.lexically_relative(root);
  return !relative.empty() && !relative.is_absolute() &&
         *relative.begin() != "..";
}

void appendPython(QStringList& arguments, const fs::path& executable,
                  const QString& abi) {
  if (executable.empty()) return;
  arguments << QStringLiteral("--python-executable") << pathString(executable)
            << QStringLiteral("--python-abi") << abi;
}

QProcessEnvironment childEnvironment(const fs::path& executable = {}) {
  auto environment = QProcessEnvironment::systemEnvironment();
  for (const auto* name :
       {"PYTHONEXECUTABLE", "__PYVENV_LAUNCHER__", "PYTHONHOME", "PYTHONPATH",
        "VIRTUAL_ENV", "UV_PROJECT", "UV_PROJECT_ENVIRONMENT", "UV_PYTHON",
        "UV_WORKING_DIR"})
    environment.remove(QString::fromLatin1(name));
  if (!executable.empty()) {
    const auto binaryDirectory = executable.parent_path();
    std::error_code error;
    if (fs::is_regular_file(binaryDirectory.parent_path() / "pyvenv.cfg",
                            error))
      environment.insert(QStringLiteral("VIRTUAL_ENV"),
                         pathString(binaryDirectory.parent_path()));
    environment.insert(QStringLiteral("PATH"),
                       pathString(binaryDirectory) + QDir::listSeparator() +
                           environment.value(QStringLiteral("PATH")));
  }
  return environment;
}

}  // namespace

SketchActions::SketchActions(QObject* parent)
    : QObject(parent), m_history(m_settings), m_openError(startupError) {
  connect(&m_prepare, &QProcess::finished, this,
          [this](int code, QProcess::ExitStatus status) {
            if (!m_opening) return;
            const QByteArray output = m_prepare.readAllStandardOutput();
            const QString error =
                QString::fromUtf8(m_prepare.readAllStandardError()).trimmed();
            if (status != QProcess::NormalExit || code != 0) {
              failOpen(error.isEmpty()
                           ? QStringLiteral("Python environment preparation "
                                            "failed (exit %1).")
                                 .arg(code)
                           : error);
              return;
            }
            sketchbook::PythonEnvironment environment;
            QString why;
            if (!sketchbook::readPythonEnvironment(output, environment, why)) {
              failOpen(why);
              return;
            }
            launch(m_pending, environment.executable,
                   QString::fromStdString(environment.abi));
          });
  connect(&m_prepare, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
              failOpen(QStringLiteral("Could not prepare the Python "
                                      "environment: %1")
                           .arg(m_prepare.errorString()));
          });

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
  if (m_prepare.state() != QProcess::NotRunning) {
    m_prepare.kill();
    m_prepare.waitForFinished();
  }
  // The child render is this window's too: started from a button here,
  // it has nothing to report to once the window is gone, so it is ended
  // rather than left for QProcess to kill with a warning.
  if (m_task.state() != QProcess::NotRunning) {
    m_task.kill();
    m_task.waitForFinished();
  }
}

QString SketchActions::workspaceName() const {
  return workspaceRoot.empty() ? QString()
                               : pathString(workspaceRoot.filename());
}

QString SketchActions::workspacePath() const {
  return pathString(workspaceRoot);
}

QUrl SketchActions::openFolder() const {
  return QUrl::fromLocalFile(
      workspaceRoot.empty()
          ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
          : pathString(workspaceRoot));
}

void SketchActions::openFile(const QUrl& file) {
  if (m_opening) return;
  const fs::path path = localPath(file);
  if (path.empty()) {
    failOpen(QStringLiteral("Choose a local C++ or Python sketch file."));
    return;
  }
  open({.file = path});
}

void SketchActions::openWorkspace(const QUrl& directory) {
  if (m_opening) return;
  const fs::path path = localPath(directory);
  if (path.empty()) {
    failOpen(QStringLiteral("Choose a local workspace folder."));
    return;
  }
  open({.root = path});
}

void SketchActions::openRecent(const QVariantMap& recent) {
  if (m_opening) return;
  const fs::path path =
      recent.value(QStringLiteral("path")).toString().toStdString();
  if (path.empty()) return;
  if (recent.value(QStringLiteral("kind")).toString() ==
      QStringLiteral("folder"))
    open({.root = path});
  else
    open({.file = path});
}

void SketchActions::clearRecents() {
  m_history.clear();
  emit recentsChanged();
}

void SketchActions::refreshRecents() {
  m_settings.sync();
  emit recentsChanged();
}

void SketchActions::noteSelection(const QVariantMap& row) {
  if (!rememberSelections) return;
  const fs::path path =
      row.value(QStringLiteral("path")).toString().toStdString();
  if (path.empty() || std::ranges::find(SketchCatalog::externals, path) ==
                          SketchCatalog::externals.end())
    return;
  if (!workspaceRoot.empty() && !within(path, workspaceRoot)) return;
  m_history.remember({.root = workspaceRoot, .file = path});
  emit recentsChanged();
}

void SketchActions::failOpen(const QString& message) {
  m_opening = false;
  m_openStatus.clear();
  m_openError = message;
  emit openChanged();
}

void SketchActions::open(sketchbook::WorkspaceLocation location) {
  if (m_opening) return;
  std::error_code error;
  const bool folder = !location.root.empty();
  const fs::path target = folder ? location.root : location.file;
  const bool exists = folder ? fs::is_directory(target, error)
                             : fs::is_regular_file(target, error);
  if (!exists) {
    failOpen(
        QStringLiteral("The %1 is no longer available: %2")
            .arg(folder ? QStringLiteral("workspace") : QStringLiteral("file"),
                 pathString(target)));
    refreshRecents();
    return;
  }
  if (!folder && target.extension() != ".py" && target.extension() != ".cpp") {
    failOpen(QStringLiteral("Choose a .cpp or .py sketch file."));
    return;
  }
  bool python = target.extension() == ".py";
  if (folder) {
    const auto files = sketchbook::workspaceFiles(target);
    location.file =
        sketchbook::workspaceEntry(files, m_history.forRoot(location.root));
    python = std::ranges::any_of(
        files, [](const fs::path& file) { return file.extension() == ".py"; });
  }
  m_pending = std::move(location);
  m_opening = true;
  m_openError.clear();
  m_openStatus =
      QStringLiteral("Opening %1…").arg(pathString(target.filename()));
  emit openChanged();

  if (!python) {
    launch(m_pending, {}, {});
    return;
  }
  m_openStatus = QStringLiteral("Preparing Python environment for %1…")
                     .arg(pathString(target.filename()));
  emit openChanged();
  m_prepare.setProcessEnvironment(childEnvironment());
  m_prepare.setWorkingDirectory(
      pathString(folder ? target : target.parent_path()));
  m_prepare.start(QString::fromUtf8(SIGIL_PYTHON_EXECUTABLE),
                  sketchbook::pythonEnvironmentArguments(target));
}

void SketchActions::launch(const sketchbook::WorkspaceLocation& location,
                           const fs::path& executable, const QString& abi) {
  QStringList arguments;
  if (!location.file.empty()) arguments << pathString(location.file);
  if (!location.root.empty())
    arguments << QStringLiteral("--workspace") << pathString(location.root);
  appendPython(arguments, executable, abi);
  QProcess child;
  child.setProgram(QCoreApplication::applicationFilePath());
  child.setArguments(arguments);
  child.setProcessEnvironment(childEnvironment(executable));
  child.setWorkingDirectory(pathString(
      location.root.empty() ? location.file.parent_path() : location.root));
  if (!child.startDetached()) {
    failOpen(QStringLiteral("Could not open Sketchbook: %1")
                 .arg(child.errorString()));
    return;
  }
  m_history.remember(location);
  emit recentsChanged();
  m_opening = false;
  m_openStatus = QStringLiteral("Opened %1 in a new window.")
                     .arg(pathString(
                         (location.root.empty() ? location.file : location.root)
                             .filename()));
  emit openChanged();
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
  QStringList configured = arguments;
  appendPython(configured, pythonExecutable, pythonAbi);
  m_task.setProcessEnvironment(childEnvironment(pythonExecutable));
  if (!workspaceRoot.empty())
    m_task.setWorkingDirectory(pathString(workspaceRoot));
  m_task.start(QCoreApplication::applicationFilePath(), configured);
}
