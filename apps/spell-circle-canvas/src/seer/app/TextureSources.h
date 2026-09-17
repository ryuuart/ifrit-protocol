#pragma once

#include <QtQml/qqmlregistration.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>

class TextureSources : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Texture sources are owned by the application")
  Q_PROPERTY(QVariantList publications READ publications NOTIFY changed)
  Q_PROPERTY(bool supported READ supported CONSTANT)
 public:
  explicit TextureSources(QObject* parent = nullptr);
  QVariantList publications() const { return m_publications; }
  bool supported() const;
  Q_INVOKABLE void refresh();
 signals:
  void changed();

 private:
  QVariantList m_publications;
  QTimer m_discovery{this};
};
