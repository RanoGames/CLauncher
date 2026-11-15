#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QFileInfo>
#include <QMutex>

struct ModInfo {
    QString id;              // e.g. "sodium"
    QString name;            // Display title, e.g. "Sodium"
    QString version;         // e.g. "0.5.8"
    QString description;     // Short description
    QString filename;        // File basename, e.g. "sodium-fabric-0.5.8.jar"
    QString filePath;        // Full absolute path
    qint64 fileSize = 0;     // In bytes
    bool enabled = true;     // true if .jar, false if .jar.disabled / .disabled
    QString modLoader;       // "fabric", "forge", "neoforge", "quilt", or "unknown"
};

class ModManager : public QObject {
    Q_OBJECT

public:
    explicit ModManager(const QString &modsDir = QString(), QObject *parent = nullptr);

    QString modsDirectory() const;
    void setModsDirectory(const QString &path);

    // Lists all installed mods and inspects metadata
    QList<ModInfo> listMods();

    // Inspects a single mod file without modifying anything
    static ModInfo inspectModFile(const QString &filePath);

    // Mod state operations
    bool toggleMod(const QString &filePath, bool enable);
    bool enableMod(const QString &filePath);
    bool disableMod(const QString &filePath);
    bool deleteMod(const QString &filePath);

    // Batch operations
    int enableAllMods();
    int disableAllMods();

    // Copy / add an external mod jar into mods directory
    bool addModFile(const QString &sourceFilePath);

    // Utility helpers
    static QString formatSize(qint64 sizeBytes);
    static QString cleanFilenameTitle(const QString &filename);
    static QString extractVersionFromFilename(const QString &filename);

signals:
    void modsChanged();
    void modInspected(const ModInfo &info);

private:
    QString m_modsDir;
    mutable QMutex m_mutex;

    // Helper functions for ZIP inspection
    static QByteArray readZipEntry(const QString &zipPath, const QString &entryName);
    static QByteArray readZipEntryProcessFallback(const QString &zipPath, const QString &entryName);
};
