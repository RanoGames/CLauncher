#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QMutex>

class Settings : public QObject {
    Q_OBJECT

public:
    static Settings &instance();

    void load(const QString &filePath = QString());
    bool save(const QString &filePath = QString()) const;
    void resetToDefaults();

    // Getters
    QStringList filters() const;
    int memory() const;
    QString lastVersion() const;
    bool showConsole() const;
    QString javaPath() const;
    QStringList nicknames() const;
    QString lastNickname() const;
    QString language() const;
    QString jvmArgs() const;
    bool validateNickname() const;
    bool autoJava() const;
    bool fullscreen() const;
    int resolutionWidth() const;
    int resolutionHeight() const;
    QString customBackground() const;
    bool elyBySkins() const;
    QString uiTheme() const;
    bool autoCheckUpdates() const;
    QString gameDir() const;
    QString effectiveGameDir() const;
    QString settingsFilePath() const;

    // Setters
    void setFilters(const QStringList &filters);
    void setMemory(int memoryMb);
    void setLastVersion(const QString &version);
    void setShowConsole(bool show);
    void setJavaPath(const QString &path);
    void setNicknames(const QStringList &nicknames);
    void setLastNickname(const QString &nickname);
    void setLanguage(const QString &lang);
    void setJvmArgs(const QString &args);
    void setValidateNickname(bool validate);
    void setAutoJava(bool autoJava);
    void setFullscreen(bool fullscreen);
    void setResolutionWidth(int width);
    void setResolutionHeight(int height);
    void setResolution(int width, int height);
    void setCustomBackground(const QString &path);
    void setElyBySkins(bool enabled);
    void setUiTheme(const QString &theme);
    void setAutoCheckUpdates(bool check);
    void setGameDir(const QString &dir);

    // Helpers
    void addNickname(const QString &nickname);
    void removeNickname(const QString &nickname);

    // Serialization
    QJsonObject toJsonObject() const;
    void fromJsonObject(const QJsonObject &obj);

signals:
    void settingsLoaded();
    void settingsSaved();
    void settingsChanged();

private:
    explicit Settings(QObject *parent = nullptr);
    ~Settings() override = default;
    Settings(const Settings &) = delete;
    Settings &operator=(const Settings &) = delete;

    mutable QMutex m_mutex;
    QString m_filePath;

    // Fields
    QStringList m_filters;
    int m_memory = 2048;
    QString m_lastVersion;
    bool m_showConsole = false;
    QString m_javaPath;
    QStringList m_nicknames;
    QString m_lastNickname;
    QString m_language;
    QString m_jvmArgs;
    bool m_validateNickname = true;
    bool m_autoJava = true;
    bool m_fullscreen = false;
    int m_resolutionWidth = 1280;
    int m_resolutionHeight = 720;
    QString m_customBackground;
    bool m_elyBySkins = true;
    QString m_uiTheme;
    bool m_autoCheckUpdates = true;
    QString m_gameDir;
};
