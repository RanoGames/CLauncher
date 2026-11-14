#include "Settings.h"
#include "Constants.h"
#include "Utils.h"
#include "Logger.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <algorithm>

Settings &Settings::instance() {
    static Settings s_instance;
    return s_instance;
}

Settings::Settings(QObject *parent)
    : QObject(parent) {
    resetToDefaults();
}

void Settings::resetToDefaults() {
    QMutexLocker locker(&m_mutex);
    m_filters = QStringList{QStringLiteral("release")};
    m_memory = static_cast<int>(Utils::getOptimalRamMb());
    m_lastVersion.clear();
    m_showConsole = false;
    m_javaPath.clear();
    m_nicknames.clear();
    m_lastNickname.clear();
    m_language = Constants::DEFAULT_LANGUAGE;
    m_jvmArgs.clear();
    m_validateNickname = true;
    m_autoJava = true;
    m_fullscreen = false;
    m_resolutionWidth = Constants::DEFAULT_RESOLUTION_WIDTH;
    m_resolutionHeight = Constants::DEFAULT_RESOLUTION_HEIGHT;
    m_customBackground.clear();
    m_elyBySkins = true;
    m_uiTheme = Constants::DEFAULT_UI_THEME;
    m_autoCheckUpdates = true;
    m_gameDir.clear();
}

void Settings::load(const QString &filePath) {
    QString targetPath = filePath.isEmpty() ? Constants::defaultSettingsPath() : filePath;

    {
        QMutexLocker locker(&m_mutex);
        m_filePath = targetPath;
    }

    QFile file(targetPath);
    if (!file.exists()) {
        LOG_INFO(QStringLiteral("Settings file does not exist, creating with defaults: %1").arg(targetPath));
        resetToDefaults();
        save(targetPath);
        emit settingsLoaded();
        emit settingsChanged();
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR(QStringLiteral("Failed to open settings file for reading: %1").arg(targetPath));
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_ERROR(QStringLiteral("Failed to parse settings JSON: %1").arg(parseError.errorString()));
        return;
    }

    fromJsonObject(doc.object());
    LOG_INFO(QStringLiteral("Settings loaded successfully from: %1").arg(targetPath));

    emit settingsLoaded();
    emit settingsChanged();
}

bool Settings::save(const QString &filePath) const {
    QString targetPath;
    QJsonObject obj;

    {
        QMutexLocker locker(&m_mutex);
        targetPath = filePath.isEmpty() ? (m_filePath.isEmpty() ? Constants::defaultSettingsPath() : m_filePath) : filePath;
        obj = toJsonObject();
    }

    QFileInfo fileInfo(targetPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        LOG_ERROR(QStringLiteral("Failed to open settings file for writing: %1 (%2)").arg(targetPath, file.errorString()));
        return false;
    }

    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.flush();
    file.close();

    LOG_INFO(QStringLiteral("Settings saved successfully to: %1").arg(targetPath));
    const_cast<Settings *>(this)->settingsSaved();
    return true;
}

QJsonObject Settings::toJsonObject() const {
    QJsonObject obj;

    QJsonArray filterArray;
    for (const QString &filter : m_filters) {
        filterArray.append(filter);
    }
    obj[QStringLiteral("filters")] = filterArray;

    obj[QStringLiteral("memory")] = m_memory;
    obj[QStringLiteral("last_version")] = m_lastVersion.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(m_lastVersion);
    obj[QStringLiteral("show_console")] = m_showConsole;
    obj[QStringLiteral("java_path")] = m_javaPath.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(m_javaPath);

    QJsonArray nickArray;
    for (const QString &nick : m_nicknames) {
        nickArray.append(nick);
    }
    obj[QStringLiteral("nicknames")] = nickArray;

    obj[QStringLiteral("last_nickname")] = m_lastNickname.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(m_lastNickname);
    obj[QStringLiteral("language")] = m_language;
    obj[QStringLiteral("jvm_args")] = m_jvmArgs;
    obj[QStringLiteral("validate_nickname")] = m_validateNickname;
    obj[QStringLiteral("auto_java")] = m_autoJava;
    obj[QStringLiteral("fullscreen")] = m_fullscreen;
    obj[QStringLiteral("resolution_width")] = m_resolutionWidth;
    obj[QStringLiteral("resolution_height")] = m_resolutionHeight;
    obj[QStringLiteral("custom_background")] = m_customBackground.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(m_customBackground);
    obj[QStringLiteral("ely_by_skins")] = m_elyBySkins;
    obj[QStringLiteral("ui_theme")] = m_uiTheme;
    obj[QStringLiteral("auto_check_updates")] = m_autoCheckUpdates;

    if (!m_gameDir.isEmpty()) {
        obj[QStringLiteral("game_dir")] = m_gameDir;
    }

    return obj;
}

void Settings::fromJsonObject(const QJsonObject &obj) {
    QMutexLocker locker(&m_mutex);

    if (obj.contains(QStringLiteral("filters")) && obj[QStringLiteral("filters")].isArray()) {
        m_filters.clear();
        const QJsonArray arr = obj[QStringLiteral("filters")].toArray();
        for (const auto &val : arr) {
            m_filters.append(val.toString());
        }
    }

    if (obj.contains(QStringLiteral("memory"))) {
        m_memory = std::clamp(obj[QStringLiteral("memory")].toInt(2048), Constants::MIN_MEMORY_MB, Constants::MAX_MEMORY_MB);
    }

    if (obj.contains(QStringLiteral("last_version")) && !obj[QStringLiteral("last_version")].isNull()) {
        m_lastVersion = obj[QStringLiteral("last_version")].toString();
    }

    if (obj.contains(QStringLiteral("show_console"))) {
        m_showConsole = obj[QStringLiteral("show_console")].toBool(false);
    }

    if (obj.contains(QStringLiteral("java_path")) && !obj[QStringLiteral("java_path")].isNull()) {
        m_javaPath = obj[QStringLiteral("java_path")].toString();
    }

    if (obj.contains(QStringLiteral("nicknames")) && obj[QStringLiteral("nicknames")].isArray()) {
        m_nicknames.clear();
        const QJsonArray arr = obj[QStringLiteral("nicknames")].toArray();
        for (const auto &val : arr) {
            QString nick = val.toString();
            if (!nick.isEmpty()) {
                m_nicknames.append(nick);
            }
        }
    }

    if (obj.contains(QStringLiteral("last_nickname")) && !obj[QStringLiteral("last_nickname")].isNull()) {
        m_lastNickname = obj[QStringLiteral("last_nickname")].toString();
    }

    if (obj.contains(QStringLiteral("language")) && !obj[QStringLiteral("language")].isNull()) {
        m_language = obj[QStringLiteral("language")].toString();
    }

    if (obj.contains(QStringLiteral("jvm_args")) && !obj[QStringLiteral("jvm_args")].isNull()) {
        m_jvmArgs = obj[QStringLiteral("jvm_args")].toString();
    }

    if (obj.contains(QStringLiteral("validate_nickname"))) {
        m_validateNickname = obj[QStringLiteral("validate_nickname")].toBool(true);
    }

    if (obj.contains(QStringLiteral("auto_java"))) {
        m_autoJava = obj[QStringLiteral("auto_java")].toBool(true);
    }

    if (obj.contains(QStringLiteral("fullscreen"))) {
        m_fullscreen = obj[QStringLiteral("fullscreen")].toBool(false);
    }

    if (obj.contains(QStringLiteral("resolution_width"))) {
        m_resolutionWidth = std::max(640, obj[QStringLiteral("resolution_width")].toInt(Constants::DEFAULT_RESOLUTION_WIDTH));
    }

    if (obj.contains(QStringLiteral("resolution_height"))) {
        m_resolutionHeight = std::max(480, obj[QStringLiteral("resolution_height")].toInt(Constants::DEFAULT_RESOLUTION_HEIGHT));
    }

    if (obj.contains(QStringLiteral("custom_background")) && !obj[QStringLiteral("custom_background")].isNull()) {
        m_customBackground = obj[QStringLiteral("custom_background")].toString();
    }

    if (obj.contains(QStringLiteral("ely_by_skins"))) {
        m_elyBySkins = obj[QStringLiteral("ely_by_skins")].toBool(true);
    }

    if (obj.contains(QStringLiteral("ui_theme")) && !obj[QStringLiteral("ui_theme")].isNull()) {
        m_uiTheme = obj[QStringLiteral("ui_theme")].toString();
    }

    if (obj.contains(QStringLiteral("auto_check_updates"))) {
        m_autoCheckUpdates = obj[QStringLiteral("auto_check_updates")].toBool(true);
    }

    if (obj.contains(QStringLiteral("game_dir")) && !obj[QStringLiteral("game_dir")].isNull()) {
        m_gameDir = obj[QStringLiteral("game_dir")].toString();
    }
}

// Getters
QStringList Settings::filters() const {
    QMutexLocker locker(&m_mutex);
    return m_filters;
}

int Settings::memory() const {
    QMutexLocker locker(&m_mutex);
    return m_memory;
}

QString Settings::lastVersion() const {
    QMutexLocker locker(&m_mutex);
    return m_lastVersion;
}

bool Settings::showConsole() const {
    QMutexLocker locker(&m_mutex);
    return m_showConsole;
}

QString Settings::javaPath() const {
    QMutexLocker locker(&m_mutex);
    return m_javaPath;
}

QStringList Settings::nicknames() const {
    QMutexLocker locker(&m_mutex);
    return m_nicknames;
}

QString Settings::lastNickname() const {
    QMutexLocker locker(&m_mutex);
    return m_lastNickname;
}

QString Settings::language() const {
    QMutexLocker locker(&m_mutex);
    return m_language;
}

QString Settings::jvmArgs() const {
    QMutexLocker locker(&m_mutex);
    return m_jvmArgs;
}

bool Settings::validateNickname() const {
    QMutexLocker locker(&m_mutex);
    return m_validateNickname;
}

bool Settings::autoJava() const {
    QMutexLocker locker(&m_mutex);
    return m_autoJava;
}

bool Settings::fullscreen() const {
    QMutexLocker locker(&m_mutex);
    return m_fullscreen;
}

int Settings::resolutionWidth() const {
    QMutexLocker locker(&m_mutex);
    return m_resolutionWidth;
}

int Settings::resolutionHeight() const {
    QMutexLocker locker(&m_mutex);
    return m_resolutionHeight;
}

QString Settings::customBackground() const {
    QMutexLocker locker(&m_mutex);
    return m_customBackground;
}

bool Settings::elyBySkins() const {
    QMutexLocker locker(&m_mutex);
    return m_elyBySkins;
}

QString Settings::uiTheme() const {
    QMutexLocker locker(&m_mutex);
    return m_uiTheme;
}

bool Settings::autoCheckUpdates() const {
    QMutexLocker locker(&m_mutex);
    return m_autoCheckUpdates;
}

QString Settings::gameDir() const {
    QMutexLocker locker(&m_mutex);
    return m_gameDir;
}

QString Settings::effectiveGameDir() const {
    QMutexLocker locker(&m_mutex);
    return m_gameDir.isEmpty() ? Constants::defaultMinecraftDirectory() : m_gameDir;
}

QString Settings::settingsFilePath() const {
    QMutexLocker locker(&m_mutex);
    return m_filePath.isEmpty() ? Constants::defaultSettingsPath() : m_filePath;
}

// Setters
void Settings::setFilters(const QStringList &filters) {
    {
        QMutexLocker locker(&m_mutex);
        m_filters = filters;
    }
    emit settingsChanged();
}

void Settings::setMemory(int memoryMb) {
    {
        QMutexLocker locker(&m_mutex);
        m_memory = std::clamp(memoryMb, Constants::MIN_MEMORY_MB, Constants::MAX_MEMORY_MB);
    }
    emit settingsChanged();
}

void Settings::setLastVersion(const QString &version) {
    {
        QMutexLocker locker(&m_mutex);
        m_lastVersion = version;
    }
    emit settingsChanged();
}

void Settings::setShowConsole(bool show) {
    {
        QMutexLocker locker(&m_mutex);
        m_showConsole = show;
    }
    emit settingsChanged();
}

void Settings::setJavaPath(const QString &path) {
    {
        QMutexLocker locker(&m_mutex);
        m_javaPath = path;
    }
    emit settingsChanged();
}

void Settings::setNicknames(const QStringList &nicknames) {
    {
        QMutexLocker locker(&m_mutex);
        m_nicknames = nicknames;
    }
    emit settingsChanged();
}

void Settings::setLastNickname(const QString &nickname) {
    {
        QMutexLocker locker(&m_mutex);
        m_lastNickname = nickname;
    }
    emit settingsChanged();
}

void Settings::setLanguage(const QString &lang) {
    {
        QMutexLocker locker(&m_mutex);
        m_language = lang;
    }
    emit settingsChanged();
}

void Settings::setJvmArgs(const QString &args) {
    {
        QMutexLocker locker(&m_mutex);
        m_jvmArgs = args;
    }
    emit settingsChanged();
}

void Settings::setValidateNickname(bool validate) {
    {
        QMutexLocker locker(&m_mutex);
        m_validateNickname = validate;
    }
    emit settingsChanged();
}

void Settings::setAutoJava(bool autoJava) {
    {
        QMutexLocker locker(&m_mutex);
        m_autoJava = autoJava;
    }
    emit settingsChanged();
}

void Settings::setFullscreen(bool fullscreen) {
    {
        QMutexLocker locker(&m_mutex);
        m_fullscreen = fullscreen;
    }
    emit settingsChanged();
}

void Settings::setResolutionWidth(int width) {
    {
        QMutexLocker locker(&m_mutex);
        m_resolutionWidth = std::max(640, width);
    }
    emit settingsChanged();
}

void Settings::setResolutionHeight(int height) {
    {
        QMutexLocker locker(&m_mutex);
        m_resolutionHeight = std::max(480, height);
    }
    emit settingsChanged();
}

void Settings::setResolution(int width, int height) {
    {
        QMutexLocker locker(&m_mutex);
        m_resolutionWidth = std::max(640, width);
        m_resolutionHeight = std::max(480, height);
    }
    emit settingsChanged();
}

void Settings::setCustomBackground(const QString &path) {
    {
        QMutexLocker locker(&m_mutex);
        m_customBackground = path;
    }
    emit settingsChanged();
}

void Settings::setElyBySkins(bool enabled) {
    {
        QMutexLocker locker(&m_mutex);
        m_elyBySkins = enabled;
    }
    emit settingsChanged();
}

void Settings::setUiTheme(const QString &theme) {
    {
        QMutexLocker locker(&m_mutex);
        m_uiTheme = theme;
    }
    emit settingsChanged();
}

void Settings::setAutoCheckUpdates(bool check) {
    {
        QMutexLocker locker(&m_mutex);
        m_autoCheckUpdates = check;
    }
    emit settingsChanged();
}

void Settings::setGameDir(const QString &dir) {
    {
        QMutexLocker locker(&m_mutex);
        m_gameDir = dir;
    }
    emit settingsChanged();
}

void Settings::addNickname(const QString &nickname) {
    QString trimmed = nickname.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    {
        QMutexLocker locker(&m_mutex);
        m_nicknames.removeAll(trimmed);
        m_nicknames.prepend(trimmed);
        m_lastNickname = trimmed;
    }
    emit settingsChanged();
}

void Settings::removeNickname(const QString &nickname) {
    QString trimmed = nickname.trimmed();
    {
        QMutexLocker locker(&m_mutex);
        m_nicknames.removeAll(trimmed);
        if (m_lastNickname == trimmed) {
            m_lastNickname = m_nicknames.isEmpty() ? QString() : m_nicknames.first();
        }
    }
    emit settingsChanged();
}
