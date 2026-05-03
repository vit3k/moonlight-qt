#include "customgamemodel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

namespace {

QString normalizePosterUrl(const QString& posterUrl, const QString& hostAddress)
{
    const QString trimmedPosterUrl = posterUrl.trimmed();
    if (trimmedPosterUrl.isEmpty()) {
        return QString();
    }

    const QUrl parsedUrl(trimmedPosterUrl);
    if (parsedUrl.isValid() && !parsedUrl.scheme().isEmpty()) {
        return parsedUrl.toString();
    }

    if (hostAddress.isEmpty()) {
        return trimmedPosterUrl;
    }

    QUrl baseUrl(QString("http://%1:7878").arg(hostAddress));
    return baseUrl.resolved(QUrl(trimmedPosterUrl)).toString();
}

}

CustomGameModel::CustomGameModel(QObject *parent)
    : QAbstractListModel(parent),
      m_Nam(new QNetworkAccessManager(this))
{
    connect(m_Nam, &QNetworkAccessManager::finished,
            this, &CustomGameModel::onReplyFinished);
}

void CustomGameModel::initialize(ComputerManager* computerManager, int computerIndex)
{
    Q_ASSERT(computerIndex < computerManager->getComputers().count());
    m_Computer = computerManager->getComputers().at(computerIndex);
    QReadLocker lock(&m_Computer->lock);
    m_HostAddress = m_Computer->activeAddress.address();
    lock.unlock();

    fetchGames();
    fetchRunningGame();
}

Session* CustomGameModel::createSessionForGame(int gameIndex)
{
    Q_ASSERT(m_Computer != nullptr);
    Q_ASSERT(gameIndex >= 0 && gameIndex < m_Games.count());

    NvApp desktopApp = resolveDesktopApp();

    return new Session(m_Computer, desktopApp);
}

Session* CustomGameModel::createDesktopSession()
{
    Q_ASSERT(m_Computer != nullptr);

    NvApp desktopApp = resolveDesktopApp();
    return new Session(m_Computer, desktopApp);
}

Session* CustomGameModel::createSessionForRunningGame()
{
    Q_ASSERT(m_Computer != nullptr);

    NvApp appToResume;

    {
        QReadLocker lock(&m_Computer->lock);

        // Session asserts that the selected app ID matches the host-reported
        // currentGameId (or that currentGameId is 0). The /games/running
        // endpoint may expose a store-specific ID that doesn't match Moonlight's
        // app ID, so always resume using the host state.
        if (m_Computer->currentGameId != 0) {
            for (const NvApp& app : m_Computer->appList) {
                if (app.id == m_Computer->currentGameId) {
                    appToResume = app;
                    break;
                }
            }

            // Some hosts may report a currentGameId that isn't present in
            // appList (for example, launcher-managed titles). In that case,
            // still use the host-reported ID so Session's invariant holds.
            if (!appToResume.isInitialized()) {
                appToResume.id = m_Computer->currentGameId;
                appToResume.name = m_RunningGame.name.isEmpty() ? QStringLiteral("Running Game") : m_RunningGame.name;
            }
        }
    }

    if (!appToResume.isInitialized()) {
        appToResume = resolveDesktopApp();
    }

    return new Session(m_Computer, appToResume);
}

void CustomGameModel::postGameLaunch(const QString& gameId)
{
    if (m_HostAddress.isEmpty()) return;

    QString url = QString("http://%1:7878/games/launch").arg(m_HostAddress);
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(5000);

    QJsonObject body;
    body["id"] = gameId;
    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);

    // Fire and forget — reply handled by onReplyFinished (will be ignored for non-games calls)
    m_Nam->post(request, data);
}

void CustomGameModel::refresh()
{
    fetchGames();
    fetchRunningGame();
}

QString CustomGameModel::getGameIdAt(int index) const
{
    if (index < 0 || index >= m_Games.count()) return QString();
    return m_Games.at(index).id;
}

QString CustomGameModel::getGameNameAt(int index) const
{
    if (index < 0 || index >= m_Games.count()) return QString();
    return m_Games.at(index).name;
}

void CustomGameModel::fetchGames()
{
    if (m_HostAddress.isEmpty()) return;
    setLoading(true);
    setErrorString(QString());
    QString url = QString("http://%1:7878/games").arg(m_HostAddress);
    m_Nam->get(QNetworkRequest(QUrl(url)));
}

void CustomGameModel::fetchRunningGame()
{
    if (m_HostAddress.isEmpty()) {
        clearRunningGame();
        return;
    }

    setCheckingRunningGame(true);
    QString url = QString("http://%1:7878/games/running").arg(m_HostAddress);
    m_Nam->get(QNetworkRequest(QUrl(url)));
}

void CustomGameModel::stopRunningGame()
{
    if (m_HostAddress.isEmpty()) {
        emit runningGameStopCompleted(false, QStringLiteral("Host address is empty"));
        return;
    }

    QString url = QString("http://%1:7878/games/running/stop").arg(m_HostAddress);
    QNetworkRequest request((QUrl(url)));
    request.setTransferTimeout(5000);
    m_Nam->post(request, QByteArray());
}

NvApp CustomGameModel::resolveDesktopApp() const
{
    NvApp desktopApp;

    if (m_Computer != nullptr) {
        QReadLocker lock(&m_Computer->lock);
        for (const NvApp& app : m_Computer->appList) {
            if (app.name.compare("Desktop", Qt::CaseInsensitive) == 0) {
                desktopApp = app;
                break;
            }
        }
    }

    if (!desktopApp.isInitialized()) {
        desktopApp.id = 0;
        desktopApp.name = "Desktop";
    }

    return desktopApp;
}

void CustomGameModel::onReplyFinished(QNetworkReply* reply)
{
    const QUrl requestUrl = reply->request().url();
    const QString requestPath = requestUrl.path();

    reply->deleteLater();

    if (reply->operation() == QNetworkAccessManager::PostOperation) {
        if (requestPath.endsWith("/games/running/stop")) {
            if (reply->error() != QNetworkReply::NoError) {
                emit runningGameStopCompleted(false, reply->errorString());
            }
            else {
                clearRunningGame();
                emit runningGameStopCompleted(true, QString());
            }
        }

        // Ignore POST /games/launch and any other POST responses
        return;
    }

    if (reply->operation() != QNetworkAccessManager::GetOperation) {
        return;
    }

    const bool isGamesRequest = requestPath.endsWith("/games") &&
                                !requestPath.endsWith("/games/running") &&
                                !requestPath.endsWith("/games/running/stop");
    const bool isRunningGameRequest = requestPath.endsWith("/games/running");

    if (!isGamesRequest && !isRunningGameRequest) {
        return;
    }

    if (isGamesRequest) {
        setLoading(false);
    }

    if (isRunningGameRequest) {
        setCheckingRunningGame(false);
    }

    if (reply->error() != QNetworkReply::NoError) {
        if (isGamesRequest) {
            setErrorString(reply->errorString());
        }
        else {
            clearRunningGame();
        }
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (isGamesRequest) {
            setErrorString(QString("JSON parse error: %1").arg(parseError.errorString()));
        }
        else {
            clearRunningGame();
        }
        return;
    }

    if (isGamesRequest) {
        if (!doc.isArray()) {
            setErrorString("Expected JSON array");
            return;
        }

        QVector<GameEntry> newGames;
        for (const QJsonValue& val : doc.array()) {
            QJsonObject obj = val.toObject();
            GameEntry entry;
            entry.id = obj["id"].toString();
            entry.name = obj["name"].toString();
            entry.source = obj["source"].toString();
            entry.posterUrl = normalizePosterUrl(obj["poster_url"].toString(), m_HostAddress);
            newGames.append(entry);
        }

        beginResetModel();
        m_Games = newGames;
        endResetModel();

        // If we already detected a running game but it had no poster URL,
        // backfill it from the full games list.
        if (!m_RunningGame.id.isEmpty() && m_RunningGame.posterUrl.isEmpty()) {
            const QString posterUrl = posterUrlForGameId(m_RunningGame.id);
            if (!posterUrl.isEmpty()) {
                GameEntry updatedRunningGame = m_RunningGame;
                updatedRunningGame.posterUrl = posterUrl;
                setRunningGame(updatedRunningGame);
            }
        }

        return;
    }

    // /games/running endpoint
    QJsonObject runningObj;
    if (doc.isArray()) {
        QJsonArray arr = doc.array();
        if (arr.isEmpty()) {
            clearRunningGame();
            return;
        }
        runningObj = arr.first().toObject();
    }
    else if (doc.isObject()) {
        QJsonObject rootObj = doc.object();
        if (rootObj.contains("id") || rootObj.contains("name")) {
            runningObj = rootObj;
        }
        else if (rootObj.value("running").isArray()) {
            QJsonArray arr = rootObj.value("running").toArray();
            if (arr.isEmpty()) {
                clearRunningGame();
                return;
            }
            runningObj = arr.first().toObject();
        }
        else if (rootObj.value("games").isArray()) {
            QJsonArray arr = rootObj.value("games").toArray();
            if (arr.isEmpty()) {
                clearRunningGame();
                return;
            }
            runningObj = arr.first().toObject();
        }
    }
    else {
        clearRunningGame();
        return;
    }

    if (runningObj.isEmpty()) {
        clearRunningGame();
        return;
    }

    GameEntry runningGame;
    runningGame.id = runningObj["id"].toString();
    runningGame.name = runningObj["name"].toString();
    runningGame.source = runningObj["source"].toString();
    runningGame.posterUrl = normalizePosterUrl(runningObj["poster_url"].toString(), m_HostAddress);

    if (runningGame.posterUrl.isEmpty() && !runningGame.id.isEmpty()) {
        runningGame.posterUrl = posterUrlForGameId(runningGame.id);
    }

    setRunningGame(runningGame);
}

int CustomGameModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_Games.count();
}

QVariant CustomGameModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_Games.count())
        return QVariant();

    const GameEntry& entry = m_Games.at(index.row());
    switch (role) {
    case GameIdRole:  return entry.id;
    case NameRole:    return entry.name;
    case SourceRole:  return entry.source;
    case PosterUrlRole: return entry.posterUrl;
    default:          return QVariant();
    }
}

QHash<int, QByteArray> CustomGameModel::roleNames() const
{
    QHash<int, QByteArray> names;
    names[GameIdRole] = "gameId";
    names[NameRole]   = "name";
    names[SourceRole] = "source";
    names[PosterUrlRole] = "posterUrl";
    return names;
}

QString CustomGameModel::posterUrlForGameId(const QString& gameId) const
{
    for (const GameEntry& entry : m_Games) {
        if (entry.id == gameId && !entry.posterUrl.isEmpty()) {
            return entry.posterUrl;
        }
    }

    return QString();
}

void CustomGameModel::setLoading(bool loading)
{
    if (m_Loading != loading) {
        m_Loading = loading;
        emit loadingChanged();
    }
}

void CustomGameModel::setErrorString(const QString& error)
{
    if (m_ErrorString != error) {
        m_ErrorString = error;
        emit errorStringChanged();
    }
}

void CustomGameModel::setCheckingRunningGame(bool checking)
{
    if (m_CheckingRunningGame != checking) {
        m_CheckingRunningGame = checking;
        emit checkingRunningGameChanged();
    }
}

void CustomGameModel::setRunningGame(const GameEntry& game)
{
    if (m_RunningGame.id == game.id &&
        m_RunningGame.name == game.name &&
        m_RunningGame.source == game.source &&
        m_RunningGame.posterUrl == game.posterUrl) {
        return;
    }

    m_RunningGame = game;
    emit runningGameChanged();
}

void CustomGameModel::clearRunningGame()
{
    if (!m_RunningGame.id.isEmpty() ||
        !m_RunningGame.name.isEmpty() ||
        !m_RunningGame.source.isEmpty() ||
        !m_RunningGame.posterUrl.isEmpty()) {
        m_RunningGame = GameEntry();
        emit runningGameChanged();
    }
}
