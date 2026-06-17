#pragma once
#include "FeedModel.h"
#include "NetworkManager.h"
#include <QObject>
#include <QtQml/QtQml>

class Models : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Models)
  QML_SINGLETON

  Q_PROPERTY(FeedModel *feedModel READ feedModel)
public:
  Models(QObject *parent = nullptr);

  FeedModel *feedModel() const { return m_feedModel; }

signals:

private:
  FeedModel *m_feedModel;
  NetworkManager *m_networkManager;
};
