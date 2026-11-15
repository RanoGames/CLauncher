#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

class JavaDownloader : public QObject {
    Q_OBJECT

public:
    explicit JavaDownloader(QObject *parent = nullptr);
    ~JavaDownloader() override;

    // Download portable JRE for given major version (e.g. 8, 17, 21)
    void downloadJava(int majorVersion, const QString &customMinecraftDir = QString());

    // Helper: download portable JRE required for a specific Minecraft version
    void downloadJavaForMcVersion(const QString &mcVersion, const QString &customMinecraftDir = QString());

    bool isBusy() const { return m_busy; }
    void cancel();

signals:
    void statusChanged(const QString &status);
    void progressPercent(int percent);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void installFinished(const QString &installedJavaPath);
    void error(const QString &errorMessage);

private slots:
    void onReplyFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    QString determineOs() const;
    QString determineArch() const;
    bool extractArchive(const QString &archivePath, const QString &targetDir);
    QString findJavaExecutable(const QString &targetDir) const;

    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_currentReply = nullptr;
    QFile *m_tempFile = nullptr;
    QString m_tempFilePath;
    QString m_targetDir;
    QString m_minecraftDir;
    int m_majorVersion = 0;
    bool m_busy = false;
    bool m_cancelled = false;
};
