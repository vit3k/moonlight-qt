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
    Q_PROPERTY(bool checkingRunningGame READ isCheckingRunningGame NOTIFY checkingRunningGameChanged)
    Q_PROPERTY(bool hasRunningGame READ hasRunningGame NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGameId READ runningGameId NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGameName READ runningGameName NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGameSource READ runningGameSource NOTIFY runningGameChanged)
    Q_PROPERTY(QString runningGamePosterUrl READ runningGamePosterUrl NOTIFY runningGameChanged)

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
    Q_INVOKABLE Session* createSessionForRunningGame();

    Q_INVOKABLE void postGameLaunch(const QString& gameId);
    Q_INVOKABLE void fetchRunningGame();
    Q_INVOKABLE void stopRunningGame();

    bool isLoading() const { return m_Loading; }
    QString errorString() const { return m_ErrorString; }
    bool isCheckingRunningGame() const { return m_CheckingRunningGame; }
    bool hasRunningGame() const { return !m_RunningGame.id.isEmpty(); }
    QString runningGameId() const { return m_RunningGame.id; }
    QString runningGameName() const { return m_RunningGame.name; }
    QString runningGameSource() const { return m_RunningGame.source; }
    QString runningGamePosterUrl() const { return m_RunningGame.posterUrl; }

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void loadingChanged();
    void errorStringChanged();
    void checkingRunningGameChanged();
    void runningGameChanged();
    void runningGameStopCompleted(bool success, const QString& errorString);

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
    QString posterUrlForGameId(const QString& gameId) const;
    void setLoading(bool loading);
    void setErrorString(const QString& error);
    void setCheckingRunningGame(bool checking);
    void setRunningGame(const GameEntry& game);
    void clearRunningGame();

    QNetworkAccessManager* m_Nam;
    QVector<GameEntry> m_Games;
    GameEntry m_RunningGame;
    QString m_HostAddress;
    NvComputer* m_Computer = nullptr;
    bool m_Loading = false;
    bool m_CheckingRunningGame = false;
    QString m_ErrorString;
};
