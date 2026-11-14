#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <optional>

struct VersionEntry {
    QString id;
    QString type; // "release", "snapshot", "old_beta", "old_alpha"
    QString url;
    QString time;
    QString releaseTime;
    QString sha1;
    int complianceLevel = 0;
};

struct VersionManifestData {
    QString latestRelease;
    QString latestSnapshot;
    QList<VersionEntry> versions;
};

class VersionManifest : public QObject {
    Q_OBJECT

public:
    explicit VersionManifest(QObject *parent = nullptr);
    ~VersionManifest() override = default;

    // Asynchronously fetch version manifest
    void fetchManifest(bool forceRefresh = false, const QStringList &typeFilters = QStringList{QStringLiteral("release")});

    // Query methods
    bool isLoaded() const;
    QList<VersionEntry> allVersions() const;
    QList<VersionEntry> filteredVersions(const QStringList &typeFilters) const;
    QStringList filteredVersionIds(const QStringList &typeFilters) const;
    std::optional<VersionEntry> findVersion(const QString &versionId) const;
    QString versionUrl(const QString &versionId) const;
    QString latestRelease() const;
    QString latestSnapshot() const;

    // Cache path
    QString cacheFilePath() const;
    void setCacheFilePath(const QString &path);

signals:
    void manifestLoaded(const QList<VersionEntry> &versions);
    void manifestDataLoaded(const VersionManifestData &data);
    void errorOccurred(const QString &errorMessage);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    bool loadFromCache(const QString &filePath, const QStringList &typeFilters);
    bool parseJson(const QByteArray &data, const QStringList &typeFilters, bool saveToCache);

    QNetworkAccessManager *m_nam = nullptr;
    QString m_cacheFilePath;
    QStringList m_activeFilters;
    VersionManifestData m_manifestData;
    bool m_isLoaded = false;
};
