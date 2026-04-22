#pragma once

#include "backend/computermanager.h"
#include "streaming/session.h"

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class CustomGameModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

    enum Roles
    {
        GameIdRole = Qt::UserRole,
        NameRole,
        SourceRole,
        PosterUrlRole,
    };

public:
    explicit CustomGameModel(QObject *parent = nullptr);

    Q_INVOKABLE void initialize(ComputerManager* computerManager, int computerIndex);

    Q_INVOKABLE void refresh();

    Q_INVOKABLE QString getGameIdAt(int index) const;
    Q_INVOKABLE QString getGameNameAt(int index) const;

    Q_INVOKABLE Session* createSessionForGame(int gameIndex);
    Q_INVOKABLE Session* createDesktopSession();

    Q_INVOKABLE void postGameLaunch(const QString& gameId);

    bool isLoading() const { return m_Loading; }
    QString errorString() const { return m_ErrorString; }

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void loadingChanged();
    void errorStringChanged();

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    struct GameEntry {
        QString id;
        QString name;
        QString source;
        QString posterUrl;
    };

    void fetchGames();
    NvApp resolveDesktopApp() const;
    void setLoading(bool loading);
    void setErrorString(const QString& error);

    QNetworkAccessManager* m_Nam;
    QVector<GameEntry> m_Games;
    QString m_HostAddress;
    NvComputer* m_Computer = nullptr;
    bool m_Loading = false;
    QString m_ErrorString;
};
