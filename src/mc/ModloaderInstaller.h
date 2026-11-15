#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>

class ModloaderInstaller : public QObject {
    Q_OBJECT

public:
    explicit ModloaderInstaller(QObject *parent = nullptr);
    ~ModloaderInstaller() override;

    // Fetch list of available Fabric loader versions for a game version
    void fetchLoaders(const QString &gameVersion);

    // Install Fabric loader for given game version and optional loader version
    void installFabric(const QString &gameVersion,
                       const QString &loaderVersion = QString(),
                       const QString &customMinecraftDir = QString());

    bool isBusy() const { return m_busy; }
    void cancel();

signals:
    void statusChanged(const QString &status);
    void progressPercent(int percent);
    void loadersFetched(const QStringList &loaderVersions);
    void installFinished(const QString &installedVersionId);
    void error(const QString &errorMessage);

private slots:
    void onLoadersReplyFinished(QNetworkReply *reply);
    void onProfileJsonReplyFinished(QNetworkReply *reply);

private:
    void fetchProfileJson(const QString &gameVersion, const QString &loaderVersion);
    void saveVersionJson(const QString &versionId, const QByteArray &jsonData);

    QNetworkAccessManager *m_nam = nullptr;
    QString m_gameVersion;
    QString m_loaderVersion;
    QString m_minecraftDir;
    bool m_busy = false;
    bool m_cancelled = false;
    QNetworkReply *m_currentReply = nullptr;
};
