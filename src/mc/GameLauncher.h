#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>

struct LaunchOptions {
    QString username;
    QString uuid;
    QString versionId;
    QString gameDir;
    QString javaExecutable;
    int memoryMb = 2048;
    QString customJvmArgs;
    bool fullscreen = false;
    bool customResolution = false;
    int resolutionWidth = 1280;
    int resolutionHeight = 720;
    bool elyBySkins = true;
    QString authlibJarPath;
    QString launcherName;
    QString launcherVersion;
};

class GameLauncher : public QObject {
    Q_OBJECT

public:
    explicit GameLauncher(QObject *parent = nullptr);
    ~GameLauncher() override;

    // Build the launch command without running it
    bool buildLaunchCommand(const LaunchOptions &options,
                            QString &outProgram,
                            QStringList &outArguments,
                            QString *outError = nullptr);

    // Launch Minecraft
    bool launch(const LaunchOptions &options, QString *outError = nullptr);

    // Process control
    bool isRunning() const;
    qint64 processId() const;
    void killGame();
    QProcess *process() const;

signals:
    void gameStarted();
    void gameExited(int exitCode, QProcess::ExitStatus exitStatus);
    void logReceived(const QString &line);
    void launchError(const QString &errorMessage);

private slots:
    void onReadyRead();
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    QJsonObject loadAndMergeVersionJson(const QString &versionId, const QString &mcDir);
    QString buildClasspath(const QJsonObject &mergedVersion, const QString &versionId, const QString &mcDir);
    bool isRuleAllowed(const QJsonArray &rules, const LaunchOptions &options) const;
    QString parseMavenPath(const QString &coordinates) const;
    QString expandPlaceholders(const QString &input, const QMap<QString, QString> &placeholders) const;
    QStringList expandArgumentList(const QJsonArray &argArray,
                                   const QMap<QString, QString> &placeholders,
                                   const LaunchOptions &options);

    QProcess *m_process = nullptr;
    QString m_currentVersionId;
};
