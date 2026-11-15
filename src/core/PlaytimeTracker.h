#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QPair>
#include <QMutex>
#include <QElapsedTimer>

class PlaytimeTracker : public QObject {
    Q_OBJECT

public:
    static PlaytimeTracker &instance();

    void load(const QString &filePath = QString());
    bool save(const QString &filePath = QString()) const;

    // Statistics
    void addPlaytime(const QString &version, qint64 elapsedSeconds);
    qint64 totalSeconds() const;
    qint64 versionSeconds(const QString &version) const;
    QMap<QString, qint64> versionPlaytimes() const;

    // Formatting
    static QString formatSeconds(qint64 seconds);
    QPair<QString, QString> getPlaytimeStats(const QString &version = QString()) const;
    inline QPair<QString, QString> formattedStats(const QString &version = QString()) const { return getPlaytimeStats(version); }

    // Active session tracking
    void startSession(const QString &version);
    void stopSession();
    bool isSessionActive() const;
    qint64 currentSessionSeconds() const;
    QString currentSessionVersion() const;

signals:
    void playtimeUpdated(qint64 totalSeconds, const QString &version, qint64 versionSeconds);

private:
    explicit PlaytimeTracker(QObject *parent = nullptr);
    ~PlaytimeTracker() override = default;
    PlaytimeTracker(const PlaytimeTracker &) = delete;
    PlaytimeTracker &operator=(const PlaytimeTracker &) = delete;

    mutable QMutex m_mutex;
    QString m_filePath;
    qint64 m_totalSeconds = 0;
    QMap<QString, qint64> m_versions;

    // Session state
    bool m_sessionActive = false;
    QString m_sessionVersion;
    QElapsedTimer m_sessionTimer;
};
