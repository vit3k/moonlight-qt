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
    reply->deleteLater();

    // Ignore replies from POST /games/launch and other non-GET requests
    if (reply->operation() != QNetworkAccessManager::GetOperation) {
        return;
    }

    setLoading(false);

    if (reply->error() != QNetworkReply::NoError) {
        setErrorString(reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setErrorString(QString("JSON parse error: %1").arg(parseError.errorString()));
        return;
    }

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
