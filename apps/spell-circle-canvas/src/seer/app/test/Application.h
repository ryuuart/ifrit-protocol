#pragma once

#include <QtGui/QGuiApplication>

inline void ensureSeerTestApplication() {
  static int argc = 1;
  static char name[] = "seer_qt_test";
  static char* argv[] = {name, nullptr};
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  static QGuiApplication application(argc, argv);
}
