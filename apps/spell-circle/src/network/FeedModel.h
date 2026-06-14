#pragma once
#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QList>

struct FeedItem {
    QDateTime timestamp;
    QString   source;
    QString   message;
};

struct CircleGeometry {
    QString  name;
    float    x      = 0.0f;
    float    y      = 0.0f;
    uint32_t radius = 0;
};

class FeedModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TimestampRole = Qt::UserRole + 1,
        SourceRole,
        MessageRole,
    };
    Q_ENUM(Roles)

    explicit FeedModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QList<CircleGeometry>& circles() const { return m_circles; }

signals:
    void circlesChanged();

public slots:
    void onSpellCircleReceived(const QString& source, const QByteArray& payload);

private:
    QList<FeedItem>       m_items;
    QList<CircleGeometry> m_circles;
};
