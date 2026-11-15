#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QMap>

struct ModSearchResult {
    QString projectId;
    QString slug;
    QString title;
    QString description;
    QString author;
    qint64 downloads = 0;
    qint64 follows = 0;
    QString iconUrl;
    QStringList categories;
    QStringList versions;
};

struct ModVersionFile {
    QString url;
    QString filename;
    qint64 size = 0;
    bool primary = false;
    QString sha1;
    QString sha512;
};

struct ModVersionInfo {
    QString id;
    QString name;
    QString versionNumber;
    QStringList gameVersions;
    QStringList loaders;
    QList<ModVersionFile> files;
};

class ModBrowser : public QObject {
    Q_OBJECT

public:
    explicit ModBrowser(QObject *parent = nullptr);
    ~ModBrowser() override;

    // Search mods on Modrinth v2 API
    void searchMods(const QString &query,
                    const QString &gameVersion = QString(),
                    const QString &loader = QStringLiteral("fabric"),
                    int limit = 20,
                    int offset = 0);

    // Fetch version files for a given mod project
    void fetchProjectVersions(const QString &projectId,
                             const QString &gameVersion = QString(),
                             const QString &loader = QString());

    // Downloads the best matching mod version jar directly into the mods directory
    void installMod(const QString &projectId,
                    const QString &modTitle,
                    const QString &gameVersion,
                    const QString &loader,
                    const QString &targetModsDir = QString());

    // Downloads a specific version file directly
    void downloadFile(const QString &fileUrl,
                      const QString &fileName,
                      const QString &targetModsDir = QString());

    void cancelSearch();
    void cancelDownloads();

    bool isSearching() const { return m_isSearching; }

signals:
    void searchStarted();
    void searchFinished(const QList<ModSearchResult> &results, int totalHits);
    void searchError(const QString &errorMessage);

    void versionsLoaded(const QString &projectId, const QList<ModVersionInfo> &versions);
    void versionsError(const QString &projectId, const QString &errorMessage);

    void downloadStarted(const QString &fileName);
    void downloadProgress(const QString &fileName, qint64 bytesReceived, qint64 bytesTotal, int percent);
    void downloadFinished(const QString &fileName, const QString &savedPath);
    void downloadError(const QString &fileName, const QString &errorMessage);

    void statusMessage(const QString &message);

private slots:
    void onSearchReplyFinished();
    void onVersionsReplyFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadReadyRead();
    void onDownloadReplyFinished();

private:
    QNetworkAccessManager *m_netManager;
    QNetworkReply *m_searchReply = nullptr;
    bool m_isSearching = false;

    // Tracking pending install requests awaiting versions
    struct PendingInstall {
        QString projectId;
        QString modTitle;
        QString targetModsDir;
    };
    QMap<QNetworkReply*, PendingInstall> m_pendingInstalls;

    // Tracking active downloads
    struct ActiveDownload {
        QString fileName;
        QString targetPath;
        QFile *file = nullptr;
    };
    QMap<QNetworkReply*, ActiveDownload> m_activeDownloads;

    static QString sanitizeVersion(const QString &version);
};
