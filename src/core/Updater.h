#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

struct UpdateInfo {
    bool updateAvailable = false;
    QString currentVersion;
    QString latestVersion;
    QString changelog;
    QString releaseUrl;
    QString downloadUrl;
    QString assetName;
    qint64 assetSize = 0;
};

class Updater : public QObject {
    Q_OBJECT

public:
    explicit Updater(QObject *parent = nullptr);
    ~Updater() override;

    // Checks GitHub releases asynchronously
    void checkForUpdates(const QString &apiUrl = QString());

    // Starts downloading the update binary
    void downloadUpdate(const QString &downloadUrl, const QString &destinationFilePath = QString());

    // Aborts any ongoing check or download
    void abort();

    bool isChecking() const { return m_isChecking; }
    bool isDownloading() const { return m_isDownloading; }
    const UpdateInfo &lastUpdateInfo() const { return m_lastInfo; }

signals:
    void checkingStarted();
    void updateAvailable(const UpdateInfo &info);
    void noUpdateAvailable(const QString &currentVersion);
    void checkFailed(const QString &errorMessage);

    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal, int percent);
    void downloadFinished(const QString &savedFilePath);
    void downloadFailed(const QString &errorMessage);

private slots:
    void onCheckFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadReadyRead();
    void onDownloadFinished();

private:
    QNetworkAccessManager *m_netManager;
    QNetworkReply *m_checkReply = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QFile *m_downloadFile = nullptr;
    QString m_downloadTarget;

    bool m_isChecking = false;
    bool m_isDownloading = false;
    UpdateInfo m_lastInfo;
};
