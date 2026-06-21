#pragma once
#include "FeedModel.h"
#include "NetworkManager.h"
#include <QObject>
#include <QtQml/QtQml>

/**
 * QML singleton that owns the application's FeedModel and NetworkManager,
 * wiring received UDP packets into the feed.
 */
class Models : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(Models)
  QML_SINGLETON

  Q_PROPERTY(FeedModel *feedModel READ feedModel)
public:
  Models(QObject *parent = nullptr);

  /** Returns the singleton FeedModel instance. */
  FeedModel *feedModel() const { return m_feedModel; }

signals:

private:
  FeedModel *m_feedModel;
  NetworkManager *m_networkManager;
};
