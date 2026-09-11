#pragma once

#include <QtQml/qqmlregistration.h>

#include <QtCore/QObject>
#include <QtCore/QProcess>
#include <QtCore/QUrl>
#include <QtCore/QVariantMap>

/** Commands over a sketch's catalog row. Owns one subprocess at a time;
 *  the row is input data, independent of browser selection and thumbnails. */
class SketchActions : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString taskLine READ taskLine NOTIFY taskChanged)
  Q_PROPERTY(bool taskRunning READ taskRunning NOTIFY taskChanged)

 public:
  explicit SketchActions(QObject* parent = nullptr);
  ~SketchActions() override;

  [[nodiscard]] QString taskLine() const { return m_taskLine; }
  [[nodiscard]] bool taskRunning() const {
    return m_task.state() != QProcess::NotRunning;
  }

  Q_INVOKABLE void frame(const QVariantMap& row);
  Q_INVOKABLE void bench(const QVariantMap& row);
  Q_INVOKABLE void reveal(const QVariantMap& row);
  /** An empty row selects the entire registry for video export. */
  Q_INVOKABLE void video(const QVariantMap& row, const QUrl& output);
  Q_INVOKABLE QUrl videoDefault(const QVariantMap& row) const;

 signals:
  void taskChanged();

 private:
  void run(const QString& label, const QStringList& arguments,
           const QString& prefix);

  QProcess m_task;
  QString m_taskLine;
  QString m_taskPrefix;
};
