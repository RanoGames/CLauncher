#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

struct DownloadTask {
    QUrl url;
    QString destinationPath;
    qint64 expectedSize = -1;
    QString expectedSha1;
    bool isNative = false;
    QString description;
    int retryCount = 0;
};

struct ActiveDownload {
    DownloadTask task;
    QString tempFilePath;
    qint64 bytesReceived = 0;
};

class Downloader : public QObject {
    Q_OBJECT

public:
    explicit Downloader(QObject *parent = nullptr);
    ~Downloader() override;

    // Start downloading a version
    void downloadVersion(const QString &versionId, const QString &customMinecraftDir = QString());

    // Cancel current download operation
    void cancel();

    // Check if downloading is active
    bool isBusy() const;

signals:
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void progressPercent(int percent);
    void statusChanged(const QString &status);
    void finished();
    void error(const QString &errorMessage);

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onReplyProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    void startDownloadChain(const QString &versionId);
    void fetchVersionJson(const QString &versionId, const QString &jsonUrl = QString());
    void handleVersionJsonDownloaded(const QString &versionId, const QByteArray &data);
    bool processVersionHierarchy(const QString &versionId, QJsonObject &mergedObj);
    void prepareDownloadQueue(const QString &versionId, const QJsonObject &versionData);
    void processQueue();
    void finishInstallation();
    void extractAllNatives();
    bool extractNativeJar(const QString &jarPath, const QString &targetDir);
    void abortAll(const QString &reason);

    // Rules matching
    bool isRuleAllowed(const QJsonArray &rules) const;
    QString parseMavenPath(const QString &coordinates) const;

    QNetworkAccessManager *m_nam = nullptr;
    QString m_minecraftDir;
    QString m_targetVersionId;
    bool m_cancelled = false;
    bool m_busy = false;

    // Download state
    QList<DownloadTask> m_queue;
    QMap<QNetworkReply*, ActiveDownload> m_activeDownloads;
    QStringList m_nativeJarsToExtract;

    qint64 m_totalBytes = 0;
    qint64 m_completedBytes = 0;
    int m_totalTasks = 0;
    int m_completedTasks = 0;

    static constexpr int MAX_CONCURRENT_DOWNLOADS = 8;
    static constexpr int MAX_RETRIES = 3;
};
