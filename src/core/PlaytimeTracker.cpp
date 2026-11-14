#include "PlaytimeTracker.h"
#include "Constants.h"
#include "Logger.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

PlaytimeTracker &PlaytimeTracker::instance() {
    static PlaytimeTracker s_instance;
    return s_instance;
}

PlaytimeTracker::PlaytimeTracker(QObject *parent)
    : QObject(parent) {
}

void PlaytimeTracker::load(const QString &filePath) {
    QString targetPath = filePath.isEmpty() ? Constants::defaultPlaytimePath() : filePath;

    {
        QMutexLocker locker(&m_mutex);
        m_filePath = targetPath;
        m_totalSeconds = 0;
        m_versions.clear();
    }

    QFile file(targetPath);
    if (!file.exists()) {
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_WARN(QStringLiteral("Failed to open playtime file: %1").arg(targetPath));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARN(QStringLiteral("Failed to parse playtime JSON: %1").arg(parseError.errorString()));
        return;
    }

    QJsonObject obj = doc.object();
    QMutexLocker locker(&m_mutex);
    m_totalSeconds = obj.value(QStringLiteral("total_seconds")).toInteger(0);

    QJsonObject versionsObj = obj.value(QStringLiteral("versions")).toObject();
    for (auto it = versionsObj.constBegin(); it != versionsObj.constEnd(); ++it) {
        m_versions.insert(it.key(), it.value().toInteger(0));
    }
}

bool PlaytimeTracker::save(const QString &filePath) const {
    QString targetPath;
    QJsonObject obj;

    {
        QMutexLocker locker(&m_mutex);
        targetPath = filePath.isEmpty() ? (m_filePath.isEmpty() ? Constants::defaultPlaytimePath() : m_filePath) : filePath;

        obj[QStringLiteral("total_seconds")] = m_totalSeconds;
        QJsonObject versionsObj;
        for (auto it = m_versions.constBegin(); it != m_versions.constEnd(); ++it) {
            versionsObj.insert(it.key(), it.value());
        }
        obj[QStringLiteral("versions")] = versionsObj;
    }

    QFileInfo fileInfo(targetPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        LOG_WARN(QStringLiteral("Failed to save playtime file: %1").arg(targetPath));
        return false;
    }

    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.flush();
    file.close();

    return true;
}

void PlaytimeTracker::addPlaytime(const QString &version, qint64 elapsedSeconds) {
    if (elapsedSeconds <= 0) {
        return;
    }

    QString verKey = version.isEmpty() ? QStringLiteral("Unknown") : version;
    qint64 newTotal = 0;
    qint64 newVerSeconds = 0;

    {
        QMutexLocker locker(&m_mutex);
        m_totalSeconds += elapsedSeconds;
        m_versions[verKey] = m_versions.value(verKey, 0) + elapsedSeconds;

        newTotal = m_totalSeconds;
        newVerSeconds = m_versions[verKey];
    }

    save();
    emit playtimeUpdated(newTotal, verKey, newVerSeconds);
}

qint64 PlaytimeTracker::totalSeconds() const {
    QMutexLocker locker(&m_mutex);
    return m_totalSeconds;
}

qint64 PlaytimeTracker::versionSeconds(const QString &version) const {
    QMutexLocker locker(&m_mutex);
    QString verKey = version.isEmpty() ? QStringLiteral("Unknown") : version;
    return m_versions.value(verKey, 0);
}

QMap<QString, qint64> PlaytimeTracker::versionPlaytimes() const {
    QMutexLocker locker(&m_mutex);
    return m_versions;
}

QString PlaytimeTracker::formatSeconds(qint64 seconds) {
    if (seconds <= 0) {
        return QStringLiteral("0 мин.");
    }

    qint64 hours = seconds / 3600;
    qint64 minutes = (seconds % 3600) / 60;

    if (hours > 0) {
        return QStringLiteral("%1 ч. %2 мин.").arg(hours).arg(minutes);
    }
    if (minutes > 0) {
        return QStringLiteral("%1 мин.").arg(minutes);
    }
    return QStringLiteral("%1 сек.").arg(seconds);
}

QPair<QString, QString> PlaytimeTracker::getPlaytimeStats(const QString &version) const {
    QMutexLocker locker(&m_mutex);
    QString totalStr = formatSeconds(m_totalSeconds);
    QString verKey = version.isEmpty() ? QStringLiteral("Unknown") : version;
    QString verStr = formatSeconds(m_versions.value(verKey, 0));
    return qMakePair(totalStr, verStr);
}

void PlaytimeTracker::startSession(const QString &version) {
    QMutexLocker locker(&m_mutex);
    m_sessionActive = true;
    m_sessionVersion = version.isEmpty() ? QStringLiteral("Unknown") : version;
    m_sessionTimer.restart();
    LOG_INFO(QStringLiteral("Started playtime session for version: %1").arg(m_sessionVersion));
}

void PlaytimeTracker::stopSession() {
    QString version;
    qint64 elapsedSec = 0;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_sessionActive) {
            return;
        }
        m_sessionActive = false;
        elapsedSec = m_sessionTimer.elapsed() / 1000;
        version = m_sessionVersion;
        m_sessionVersion.clear();
    }

    LOG_INFO(QStringLiteral("Stopped playtime session for %1, elapsed: %2 seconds").arg(version).arg(elapsedSec));
    addPlaytime(version, elapsedSec);
}

bool PlaytimeTracker::isSessionActive() const {
    QMutexLocker locker(&m_mutex);
    return m_sessionActive;
}

qint64 PlaytimeTracker::currentSessionSeconds() const {
    QMutexLocker locker(&m_mutex);
    if (!m_sessionActive) {
        return 0;
    }
    return m_sessionTimer.elapsed() / 1000;
}

QString PlaytimeTracker::currentSessionVersion() const {
    QMutexLocker locker(&m_mutex);
    return m_sessionVersion;
}
