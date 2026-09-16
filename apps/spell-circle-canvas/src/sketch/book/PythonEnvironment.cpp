#include "PythonEnvironment.h"

#include <sigilsketch/python/Python.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSysInfo>

namespace sketchbook {

QStringList pythonEnvironmentArguments(const std::filesystem::path& source) {
  const auto [major, minor] = sigil::sketch::python::interpreterVersion();
  const auto abi = sigil::sketch::python::interpreterAbi();
  QStringList arguments{QStringLiteral("-I"),
                        QStringLiteral(SIGIL_PYTHON_ENVIRONMENT_SCRIPT),
                        QString::fromStdString(source.string()),
                        QStringLiteral("--version"),
                        QStringLiteral("%1.%2").arg(major).arg(minor),
                        QStringLiteral("--abi"),
                        QString::fromUtf8(abi.data(), (qsizetype)abi.size()),
                        QStringLiteral("--machine"),
                        QSysInfo::currentCpuArchitecture(),
                        QStringLiteral("--pointer-bits"),
                        QString::number(sizeof(void*) * 8)};
  std::error_code error;
  const auto absolute = std::filesystem::weakly_canonical(source, error);
  const auto relative = absolute.lexically_relative(SIGIL_SKETCH_DIR);
  if (!error && !relative.empty() && *relative.begin() != "..")
    arguments << QStringLiteral("--boundary")
              << QStringLiteral(SIGIL_SKETCH_DIR);
  return arguments;
}

bool readPythonEnvironment(const QByteArray& output, PythonEnvironment& result,
                           QString& error) {
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(output, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    error = QStringLiteral("Could not read the Python environment: %1")
                .arg(parseError.errorString());
    return false;
  }
  const auto object = document.object();
  const auto executable = object.value(QStringLiteral("executable"));
  const auto abi = object.value(QStringLiteral("abi"));
  if (!executable.isString() || !abi.isString() ||
      executable.toString().isEmpty() != abi.toString().isEmpty()) {
    error = QStringLiteral("The Python environment response is incomplete.");
    return false;
  }
  result.executable = executable.toString().toStdString();
  result.abi = abi.toString().toStdString();
  return true;
}

}  // namespace sketchbook
