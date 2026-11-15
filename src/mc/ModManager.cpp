#include "mc/ModManager.h"
#include "core/Constants.h"
#include "core/Utils.h"
#include "core/Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QMutexLocker>
#include <zlib.h>
#include <cstring>

ModManager::ModManager(const QString &modsDir, QObject *parent)
    : QObject(parent)
    , m_modsDir(modsDir.isEmpty() ? Constants::defaultModsDirectory() : modsDir) {
    Utils::ensureDirectoryExists(m_modsDir);
}

QString ModManager::modsDirectory() const {
    QMutexLocker locker(&m_mutex);
    return m_modsDir;
}

void ModManager::setModsDirectory(const QString &path) {
    QMutexLocker locker(&m_mutex);
    m_modsDir = path;
    Utils::ensureDirectoryExists(m_modsDir);
}

QString ModManager::formatSize(qint64 sizeBytes) {
    if (sizeBytes < 1024) {
        return QStringLiteral("%1 Б").arg(sizeBytes);
    }
    if (sizeBytes < 1024 * 1024) {
        return QStringLiteral("%1 КБ").arg(QString::number(sizeBytes / 1024.0, 'f', 1));
    }
    return QStringLiteral("%1 МБ").arg(QString::number(sizeBytes / (1024.0 * 1024.0), 'f', 2));
}

QString ModManager::cleanFilenameTitle(const QString &filename) {
    QString name = filename;
    const QStringList extensions = {
        QStringLiteral(".jar.disabled"),
        QStringLiteral(".disabled"),
        QStringLiteral(".jar")
    };

    for (const QString &ext : extensions) {
        if (name.endsWith(ext, Qt::CaseInsensitive)) {
            name.chop(ext.length());
            break;
        }
    }
    return name;
}

QString ModManager::extractVersionFromFilename(const QString &filename) {
    static const QRegularExpression verRe(QStringLiteral(R"([-+._vV]([0-9]+(?:\.[0-9]+)+(?:[-+][a-zA-Z0-9_.]*)?))"));
    auto match = verRe.match(filename);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QString();
}

QByteArray ModManager::readZipEntry(const QString &zipPath, const QString &entryName) {
    QFile file(zipPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return readZipEntryProcessFallback(zipPath, entryName);
    }

    qint64 fileSize = file.size();
    if (fileSize < 22) { // Minimal ZIP size
        file.close();
        return readZipEntryProcessFallback(zipPath, entryName);
    }

    // Read end of central directory (EOCD)
    qint64 readSize = std::min(fileSize, static_cast<qint64>(65536 + 22));
    file.seek(fileSize - readSize);
    QByteArray tail = file.read(readSize);

    // Search backwards for EOCD signature 0x06054b50 (PK\x05\x06)
    int eocdIdx = -1;
    for (int i = tail.size() - 22; i >= 0; --i) {
        if (static_cast<unsigned char>(tail[i]) == 0x50 &&
            static_cast<unsigned char>(tail[i + 1]) == 0x4b &&
            static_cast<unsigned char>(tail[i + 2]) == 0x05 &&
            static_cast<unsigned char>(tail[i + 3]) == 0x06) {
            eocdIdx = i;
            break;
        }
    }

    if (eocdIdx == -1) {
        file.close();
        return readZipEntryProcessFallback(zipPath, entryName);
    }

    const char *eocdPtr = tail.constData() + eocdIdx;
    quint16 totalEntries = *reinterpret_cast<const quint16*>(eocdPtr + 10);
    quint32 cdOffset = *reinterpret_cast<const quint32*>(eocdPtr + 16);

    if (cdOffset >= static_cast<quint32>(fileSize)) {
        file.close();
        return readZipEntryProcessFallback(zipPath, entryName);
    }

    if (!file.seek(cdOffset)) {
        file.close();
        return readZipEntryProcessFallback(zipPath, entryName);
    }

    // Traverse central directory headers
    for (quint16 entry = 0; entry < totalEntries; ++entry) {
        QByteArray cdHeader = file.read(46);
        if (cdHeader.size() < 46) {
            break;
        }

        const char *cd = cdHeader.constData();
        // Central file header signature: 0x02014b50 (PK\x01\x02)
        if (static_cast<unsigned char>(cd[0]) != 0x50 ||
            static_cast<unsigned char>(cd[1]) != 0x4b ||
            static_cast<unsigned char>(cd[2]) != 0x01 ||
            static_cast<unsigned char>(cd[3]) != 0x02) {
            break;
        }

        quint16 compMethod = *reinterpret_cast<const quint16*>(cd + 10);
        quint32 compSize = *reinterpret_cast<const quint32*>(cd + 20);
        quint32 uncompSize = *reinterpret_cast<const quint32*>(cd + 24);
        quint16 nameLen = *reinterpret_cast<const quint16*>(cd + 28);
        quint16 extraLen = *reinterpret_cast<const quint16*>(cd + 30);
        quint16 commentLen = *reinterpret_cast<const quint16*>(cd + 32);
        quint32 localOffset = *reinterpret_cast<const quint32*>(cd + 42);

        QByteArray entryNameBytes = file.read(nameLen);
        if (extraLen > 0) file.seek(file.pos() + extraLen);
        if (commentLen > 0) file.seek(file.pos() + commentLen);

        QString currentName = QString::fromUtf8(entryNameBytes);
        if (currentName.compare(entryName, Qt::CaseInsensitive) == 0) {
            // Match found! Seek to local header
            if (!file.seek(localOffset)) {
                break;
            }

            QByteArray localHeader = file.read(30);
            if (localHeader.size() < 30) {
                break;
            }

            const char *lh = localHeader.constData();
            if (static_cast<unsigned char>(lh[0]) != 0x50 ||
                static_cast<unsigned char>(lh[1]) != 0x4b ||
                static_cast<unsigned char>(lh[2]) != 0x03 ||
                static_cast<unsigned char>(lh[3]) != 0x04) {
                break;
            }

            quint16 localNameLen = *reinterpret_cast<const quint16*>(lh + 26);
            quint16 localExtraLen = *reinterpret_cast<const quint16*>(lh + 28);

            file.seek(localOffset + 30 + localNameLen + localExtraLen);
            QByteArray rawData = file.read(compSize);
            file.close();

            if (compMethod == 0) {
                // STORED
                return rawData;
            } else if (compMethod == 8) {
                // DEFLATED (raw deflate)
                QByteArray uncompressed;
                uncompressed.resize(uncompSize);

                z_stream strm;
                std::memset(&strm, 0, sizeof(strm));
                strm.next_in = reinterpret_cast<Bytef*>(rawData.data());
                strm.avail_in = rawData.size();
                strm.next_out = reinterpret_cast<Bytef*>(uncompressed.data());
                strm.avail_out = uncompressed.size();

                if (inflateInit2(&strm, -MAX_WBITS) == Z_OK) {
                    int ret = inflate(&strm, Z_FINISH);
                    inflateEnd(&strm);
                    if (ret == Z_STREAM_END || ret == Z_OK) {
                        uncompressed.resize(strm.total_out);
                        return uncompressed;
                    }
                }
            }
            return readZipEntryProcessFallback(zipPath, entryName);
        }
    }

    file.close();
    return readZipEntryProcessFallback(zipPath, entryName);
}

QByteArray ModManager::readZipEntryProcessFallback(const QString &zipPath, const QString &entryName) {
    // 1. Try unzip -p
    QProcess unzipProc;
    unzipProc.start(QStringLiteral("unzip"), QStringList{QStringLiteral("-p"), zipPath, entryName});
    if (unzipProc.waitForFinished(3000) && unzipProc.exitCode() == 0) {
        QByteArray out = unzipProc.readAllStandardOutput();
        if (!out.isEmpty()) {
            return out;
        }
    }

    // 2. Try tar -xOf
    QProcess tarProc;
    tarProc.start(QStringLiteral("tar"), QStringList{QStringLiteral("-xOf"), zipPath, entryName});
    if (tarProc.waitForFinished(3000) && tarProc.exitCode() == 0) {
        QByteArray out = tarProc.readAllStandardOutput();
        if (!out.isEmpty()) {
            return out;
        }
    }

    return QByteArray();
}

ModInfo ModManager::inspectModFile(const QString &filePath) {
    QFileInfo fi(filePath);
    ModInfo info;
    info.filePath = fi.absoluteFilePath();
    info.filename = fi.fileName();
    info.fileSize = fi.size();
    info.enabled = fi.fileName().endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive);
    info.modLoader = QStringLiteral("unknown");

    QString cleanTitle = cleanFilenameTitle(fi.fileName());
    info.name = cleanTitle;
    info.version = extractVersionFromFilename(fi.fileName());

    // 1. Check Fabric (fabric.mod.json)
    QByteArray fabricData = readZipEntry(filePath, QStringLiteral("fabric.mod.json"));
    if (!fabricData.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(fabricData);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            info.id = obj[QStringLiteral("id")].toString();
            QString name = obj[QStringLiteral("name")].toString();
            if (!name.isEmpty()) info.name = name;
            QString ver = obj[QStringLiteral("version")].toString();
            if (!ver.isEmpty()) info.version = ver;
            info.description = obj[QStringLiteral("description")].toString();
            info.modLoader = QStringLiteral("fabric");
            return info;
        }
    }

    // 2. Check Quilt (quilt.mod.json)
    QByteArray quiltData = readZipEntry(filePath, QStringLiteral("quilt.mod.json"));
    if (!quiltData.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(quiltData);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QJsonObject loaderObj = obj[QStringLiteral("quilt_loader")].toObject();
            info.id = loaderObj[QStringLiteral("id")].toString();
            QJsonObject metaObj = loaderObj[QStringLiteral("metadata")].toObject();
            QString name = metaObj[QStringLiteral("name")].toString();
            if (!name.isEmpty()) info.name = name;
            QString ver = loaderObj[QStringLiteral("version")].toString();
            if (!ver.isEmpty()) info.version = ver;
            info.description = metaObj[QStringLiteral("description")].toString();
            info.modLoader = QStringLiteral("quilt");
            return info;
        }
    }

    // 3. Check NeoForge / Forge (META-INF/neoforge.mods.toml or META-INF/mods.toml)
    for (const QString &tomlPath : {QStringLiteral("META-INF/neoforge.mods.toml"), QStringLiteral("META-INF/mods.toml")}) {
        QByteArray tomlData = readZipEntry(filePath, tomlPath);
        if (!tomlData.isEmpty()) {
            QString tomlStr = QString::fromUtf8(tomlData);
            static const QRegularExpression idRe(QStringLiteral(R"(modId\s*=\s*["']([^"']+)["'])"));
            static const QRegularExpression nameRe(QStringLiteral(R"(displayName\s*=\s*["']([^"']+)["'])"));
            static const QRegularExpression verRe(QStringLiteral(R"(version\s*=\s*["']([^"']+)["'])"));
            static const QRegularExpression descRe(QStringLiteral(R"raw(description\s*=\s*(?:'''([\s\S]*?)'''|"""([\s\S]*?)"""|["']([^"']*)["']))raw"));

            auto idMatch = idRe.match(tomlStr);
            if (idMatch.hasMatch()) info.id = idMatch.captured(1).trimmed();

            auto nameMatch = nameRe.match(tomlStr);
            if (nameMatch.hasMatch()) info.name = nameMatch.captured(1).trimmed();

            auto verMatch = verRe.match(tomlStr);
            if (verMatch.hasMatch()) info.version = verMatch.captured(1).trimmed();

            auto descMatch = descRe.match(tomlStr);
            if (descMatch.hasMatch()) {
                for (int i = 1; i <= 3; ++i) {
                    if (!descMatch.captured(i).isEmpty()) {
                        info.description = descMatch.captured(i).trimmed();
                        break;
                    }
                }
            }

            info.modLoader = tomlPath.contains(QStringLiteral("neoforge")) ? QStringLiteral("neoforge") : QStringLiteral("forge");
            return info;
        }
    }

    // 4. Check Legacy Forge (mcmod.info)
    QByteArray mcmodData = readZipEntry(filePath, QStringLiteral("mcmod.info"));
    if (!mcmodData.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(mcmodData);
        QJsonArray modArray;
        if (doc.isArray()) {
            modArray = doc.array();
        } else if (doc.isObject() && doc.object().contains(QStringLiteral("modList"))) {
            modArray = doc.object()[QStringLiteral("modList")].toArray();
        }

        if (!modArray.isEmpty() && modArray.first().isObject()) {
            QJsonObject first = modArray.first().toObject();
            info.id = first[QStringLiteral("modid")].toString();
            QString name = first[QStringLiteral("name")].toString();
            if (!name.isEmpty()) info.name = name;
            QString ver = first[QStringLiteral("version")].toString();
            if (!ver.isEmpty()) info.version = ver;
            info.description = first[QStringLiteral("description")].toString();
            info.modLoader = QStringLiteral("forge");
            return info;
        }
    }

    // Default id if none found
    if (info.id.isEmpty()) {
        info.id = info.name.toLower().remove(QRegularExpression(QStringLiteral(R"([^a-z0-9_])")));
    }

    return info;
}

QList<ModInfo> ModManager::listMods() {
    QMutexLocker locker(&m_mutex);
    QList<ModInfo> result;
    QDir dir(m_modsDir);
    if (!dir.exists()) {
        return result;
    }

    QFileInfoList list = dir.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase
    );

    for (const QFileInfo &fi : list) {
        QString lowerName = fi.fileName().toLower();
        if (lowerName.endsWith(QStringLiteral(".jar")) || lowerName.endsWith(QStringLiteral(".disabled"))) {
            ModInfo info = inspectModFile(fi.absoluteFilePath());
            result.append(info);
            emit modInspected(info);
        }
    }

    return result;
}

bool ModManager::toggleMod(const QString &filePath, bool enable) {
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        LOG_WARN(QStringLiteral("Mod file does not exist: %1").arg(filePath));
        return false;
    }

    QString dirPath = fi.absolutePath();
    QString fileName = fi.fileName();
    QString newFileName;

    if (enable) {
        newFileName = fileName;
        if (newFileName.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive)) {
            newFileName.chop(9); // remove .disabled
        }
        if (!newFileName.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive)) {
            newFileName += QStringLiteral(".jar");
        }
    } else {
        newFileName = fileName;
        if (!newFileName.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive)) {
            newFileName += QStringLiteral(".disabled");
        }
    }

    QString newFilePath = QDir(dirPath).filePath(newFileName);
    if (newFilePath == filePath) {
        return true;
    }

    QFile file(filePath);
    if (!file.rename(newFilePath)) {
        LOG_ERROR(QStringLiteral("Failed to rename mod %1 to %2: %3").arg(filePath, newFilePath, file.errorString()));
        return false;
    }

    LOG_INFO(QStringLiteral("Toggled mod %1 -> %2").arg(fileName, newFileName));
    emit modsChanged();
    return true;
}

bool ModManager::enableMod(const QString &filePath) {
    return toggleMod(filePath, true);
}

bool ModManager::disableMod(const QString &filePath) {
    return toggleMod(filePath, false);
}

bool ModManager::deleteMod(const QString &filePath) {
    QFile file(filePath);
    if (!file.exists()) {
        return false;
    }

    if (file.remove()) {
        LOG_INFO(QStringLiteral("Deleted mod file: %1").arg(filePath));
        emit modsChanged();
        return true;
    }

    LOG_ERROR(QStringLiteral("Failed to delete mod file %1: %2").arg(filePath, file.errorString()));
    return false;
}

int ModManager::enableAllMods() {
    QMutexLocker locker(&m_mutex);
    QDir dir(m_modsDir);
    if (!dir.exists()) return 0;

    int changed = 0;
    QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : list) {
        if (fi.fileName().endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive)) {
            QString oldPath = fi.absoluteFilePath();
            QString newName = fi.fileName();
            newName.chop(9);
            if (!newName.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive)) {
                newName += QStringLiteral(".jar");
            }
            QString newPath = dir.filePath(newName);
            if (QFile::rename(oldPath, newPath)) {
                changed++;
            }
        }
    }

    if (changed > 0) {
        emit modsChanged();
    }
    return changed;
}

int ModManager::disableAllMods() {
    QMutexLocker locker(&m_mutex);
    QDir dir(m_modsDir);
    if (!dir.exists()) return 0;

    int changed = 0;
    QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : list) {
        if (fi.fileName().endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive)) {
            QString oldPath = fi.absoluteFilePath();
            QString newPath = dir.filePath(fi.fileName() + QStringLiteral(".disabled"));
            if (QFile::rename(oldPath, newPath)) {
                changed++;
            }
        }
    }

    if (changed > 0) {
        emit modsChanged();
    }
    return changed;
}

bool ModManager::addModFile(const QString &sourceFilePath) {
    QFileInfo fi(sourceFilePath);
    if (!fi.exists() || !fi.isFile()) {
        LOG_ERROR(QStringLiteral("Source mod file does not exist: %1").arg(sourceFilePath));
        return false;
    }

    QMutexLocker locker(&m_mutex);
    Utils::ensureDirectoryExists(m_modsDir);
    QString destPath = QDir(m_modsDir).filePath(fi.fileName());

    if (QFile::exists(destPath)) {
        QFile::remove(destPath);
    }

    if (QFile::copy(sourceFilePath, destPath)) {
        LOG_INFO(QStringLiteral("Added mod file %1 -> %2").arg(sourceFilePath, destPath));
        emit modsChanged();
        return true;
    }

    LOG_ERROR(QStringLiteral("Failed to copy mod file %1 to %2").arg(sourceFilePath, destPath));
    return false;
}
