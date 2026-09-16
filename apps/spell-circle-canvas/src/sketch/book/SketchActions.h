#pragma once

#include <QtQml/qqmlregistration.h>

#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

#include "Workspace.h"

/** Commands over a sketch's catalog row, and opening another workspace.
 * Environment preparation and the current window's exports run independently.
 */
class SketchActions : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString taskLine READ taskLine NOTIFY taskChanged)
  Q_PROPERTY(bool taskRunning READ taskRunning NOTIFY taskChanged)
  Q_PROPERTY(QVariantList recents READ recents NOTIFY recentsChanged)
  Q_PROPERTY(bool opening READ opening NOTIFY openChanged)
  Q_PROPERTY(QString openStatus READ openStatus NOTIFY openChanged)
  Q_PROPERTY(QString openError READ openError NOTIFY openChanged)
  Q_PROPERTY(QString workspaceName READ workspaceName CONSTANT)
  Q_PROPERTY(QString workspacePath READ workspacePath CONSTANT)
  Q_PROPERTY(QUrl openFolder READ openFolder CONSTANT)

 public:
  static std::filesystem::path workspaceRoot;
  static std::filesystem::path pythonExecutable;
  static QString pythonAbi;
  static QString startupError;
  static bool rememberSelections;

  explicit SketchActions(QObject* parent = nullptr);
  ~SketchActions() override;

  [[nodiscard]] QString taskLine() const { return m_taskLine; }
  [[nodiscard]] bool taskRunning() const {
    return m_task.state() != QProcess::NotRunning;
  }
  [[nodiscard]] QVariantList recents() const { return m_history.recent(); }
  [[nodiscard]] bool opening() const { return m_opening; }
  [[nodiscard]] QString openStatus() const { return m_openStatus; }
  [[nodiscard]] QString openError() const { return m_openError; }
  [[nodiscard]] QString workspaceName() const;
  [[nodiscard]] QString workspacePath() const;
  [[nodiscard]] QUrl openFolder() const;

  Q_INVOKABLE void openFile(const QUrl& file);
  Q_INVOKABLE void openWorkspace(const QUrl& directory);
  Q_INVOKABLE void openRecent(const QVariantMap& recent);
  Q_INVOKABLE void clearRecents();
  Q_INVOKABLE void refreshRecents();
  Q_INVOKABLE void noteSelection(const QVariantMap& row);

  Q_INVOKABLE void frame(const QVariantMap& row);
  Q_INVOKABLE void bench(const QVariantMap& row);
  Q_INVOKABLE void reveal(const QVariantMap& row);
  /** An empty row selects the entire registry for video export. */
  Q_INVOKABLE void video(const QVariantMap& row, const QUrl& output);
  Q_INVOKABLE QUrl videoDefault(const QVariantMap& row) const;

 signals:
  void taskChanged();
  void recentsChanged();
  void openChanged();

 private:
  void open(sketchbook::WorkspaceLocation location);
  void launch(const sketchbook::WorkspaceLocation& location,
              const std::filesystem::path& executable, const QString& abi);
  void failOpen(const QString& message);
  void run(const QString& label, const QStringList& arguments,
           const QString& prefix);

  QSettings m_settings;
  sketchbook::WorkspaceHistory m_history;
  QProcess m_prepare;
  sketchbook::WorkspaceLocation m_pending;
  bool m_opening = false;
  QString m_openStatus;
  QString m_openError;
  QProcess m_task;
  QString m_taskLine;
  QString m_taskPrefix;
};
