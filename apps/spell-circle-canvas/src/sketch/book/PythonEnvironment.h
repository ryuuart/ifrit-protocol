#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <filesystem>
#include <string>

namespace sketchbook {

struct PythonEnvironment {
  std::filesystem::path executable;
  std::string abi;
};

/** Arguments for the environment resolver, including the native host's ABI.
 * Bundled sketches stop project discovery at their catalogue directory. */
QStringList pythonEnvironmentArguments(const std::filesystem::path& source);

/** Reads the resolver's answer without initializing the embedded interpreter.
 */
bool readPythonEnvironment(const QByteArray& output, PythonEnvironment& result,
                           QString& error);

}  // namespace sketchbook
