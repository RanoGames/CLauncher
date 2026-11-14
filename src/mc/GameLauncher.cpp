#include "mc/GameLauncher.h"
#include "mc/JavaManager.h"
#include "core/Constants.h"
#include "core/Utils.h"
#include "core/Logger.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

GameLauncher::GameLauncher(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &GameLauncher::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &GameLauncher::onReadyRead);
    connect(m_process, &QProcess::started, this, &GameLauncher::onProcessStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GameLauncher::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &GameLauncher::onProcessError);
}

GameLauncher::~GameLauncher() {
    killGame();
}

bool GameLauncher::isRunning() const {
    return m_process && (m_process->state() != QProcess::NotRunning);
}

qint64 GameLauncher::processId() const {
    return m_process ? m_process->processId() : 0;
}

void GameLauncher::killGame() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        LOG_INFO(QStringLiteral("Terminating Minecraft process..."));
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }
}

QProcess *GameLauncher::process() const {
    return m_process;
}

void GameLauncher::onProcessStarted() {
    LOG_INFO(QStringLiteral("Minecraft process started successfully (PID: %1)").arg(processId()));
    emit gameStarted();
}

void GameLauncher::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    LOG_INFO(QStringLiteral("Minecraft process exited with code %1 (Status: %2)").arg(exitCode).arg(exitStatus));
    emit gameExited(exitCode, exitStatus);
}

void GameLauncher::onProcessError(QProcess::ProcessError error) {
    const QString errString = m_process ? m_process->errorString() : QStringLiteral("Unknown process error");
    LOG_ERROR(QStringLiteral("Minecraft process error (%1): %2").arg(error).arg(errString));
    emit launchError(errString);
}

void GameLauncher::onReadyRead() {
    if (!m_process) return;

    const QByteArray data = m_process->readAll();
    const QString text = QString::fromUtf8(data);
    const QStringList lines = text.split(QRegularExpression(QStringLiteral(R"(\r?\n)")));

    for (const QString &line : lines) {
        if (!line.isEmpty()) {
            emit logReceived(line);
        }
    }
}

QJsonObject GameLauncher::loadAndMergeVersionJson(const QString &versionId, const QString &mcDir) {
    QList<QJsonObject> chain;
    QString currentId = versionId;
    QSet<QString> seen;

    while (!currentId.isEmpty() && !seen.contains(currentId)) {
        seen.insert(currentId);
        const QString jsonPath = QDir(mcDir).filePath(
            QStringLiteral("versions/%1/%1.json").arg(currentId));

        QFile file(jsonPath);
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_WARN(QStringLiteral("Could not open version JSON at %1").arg(jsonPath));
            break;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (!doc.isObject()) {
            break;
        }

        const QJsonObject obj = doc.object();
        chain.append(obj);

        if (obj.contains(QStringLiteral("inheritsFrom"))) {
            currentId = obj.value(QStringLiteral("inheritsFrom")).toString();
        } else {
            break;
        }
    }

    if (chain.isEmpty()) {
        return QJsonObject();
    }

    // Merge chain from root (oldest parent) to leaf (target version)
    QJsonObject merged = chain.last();
    for (int i = chain.size() - 2; i >= 0; --i) {
        const QJsonObject child = chain[i];

        // 1. Scalar fields: child overrides parent
        for (const QString &key : {QStringLiteral("mainClass"), QStringLiteral("assets"),
                                   QStringLiteral("type"), QStringLiteral("releaseTime")}) {
            if (child.contains(key) && !child[key].isNull()) {
                merged[key] = child[key];
            }
        }

        // 2. minecraftArguments: concatenate strings
        if (child.contains(QStringLiteral("minecraftArguments"))) {
            const QString childArgs = child.value(QStringLiteral("minecraftArguments")).toString().trimmed();
            const QString parentArgs = merged.value(QStringLiteral("minecraftArguments")).toString().trimmed();
            if (!childArgs.isEmpty()) {
                merged[QStringLiteral("minecraftArguments")] = parentArgs.isEmpty()
                    ? childArgs
                    : QStringLiteral("%1 %2").arg(parentArgs, childArgs);
            }
        }

        // 3. arguments { game, jvm }: concatenate arrays
        if (child.contains(QStringLiteral("arguments")) && child[QStringLiteral("arguments")].isObject()) {
            QJsonObject mergedArgs = merged.value(QStringLiteral("arguments")).toObject();
            const QJsonObject childArgs = child.value(QStringLiteral("arguments")).toObject();

            for (const QString &key : {QStringLiteral("game"), QStringLiteral("jvm")}) {
                QJsonArray items = mergedArgs.value(key).toArray();
                const QJsonArray add = childArgs.value(key).toArray();
                for (const auto &item : add) {
                    items.append(item);
                }
                mergedArgs[key] = items;
            }
            merged[QStringLiteral("arguments")] = mergedArgs;
        }

        // 4. Libraries: concatenate arrays (parent libraries + child libraries)
        QJsonArray libs = merged.value(QStringLiteral("libraries")).toArray();
        const QJsonArray childLibs = child.value(QStringLiteral("libraries")).toArray();
        for (const auto &lib : childLibs) {
            libs.append(lib);
        }
        merged[QStringLiteral("libraries")] = libs;
    }

    return merged;
}

bool GameLauncher::isRuleAllowed(const QJsonArray &rules, const LaunchOptions &options) const {
    if (rules.isEmpty()) {
        return true;
    }

    const QString osName = Utils::currentOSName(); // "windows", "linux", "osx"
    const QString arch = Utils::currentArch();

    bool allowed = false;
    bool matchedAny = false;

    for (const auto &rVal : rules) {
        if (!rVal.isObject()) continue;
        const QJsonObject rule = rVal.toObject();

        bool ruleMatches = true;

        if (rule.contains(QStringLiteral("os")) && rule[QStringLiteral("os")].isObject()) {
            const QJsonObject osObj = rule[QStringLiteral("os")].toObject();
            if (osObj.contains(QStringLiteral("name"))) {
                if (osObj.value(QStringLiteral("name")).toString() != osName) {
                    ruleMatches = false;
                }
            }
            if (ruleMatches && osObj.contains(QStringLiteral("arch"))) {
                const QString ruleArch = osObj.value(QStringLiteral("arch")).toString();
                const bool wantX64 = (ruleArch == QStringLiteral("x64") || ruleArch == QStringLiteral("amd64") || ruleArch == QStringLiteral("x86_64"));
                const bool isX64 = (arch == QStringLiteral("x64"));
                if (wantX64 != isX64) {
                    ruleMatches = false;
                }
            }
        }

        if (ruleMatches && rule.contains(QStringLiteral("features")) && rule[QStringLiteral("features")].isObject()) {
            const QJsonObject features = rule[QStringLiteral("features")].toObject();
            for (auto it = features.begin(); it != features.end(); ++it) {
                const QString feat = it.key();
                const bool val = it.value().toBool(false);
                if (feat == QStringLiteral("has_custom_resolution")) {
                    if (val != (options.customResolution && !options.fullscreen)) {
                        ruleMatches = false;
                    }
                } else if (feat == QStringLiteral("is_demo_user")) {
                    if (val != false) {
                        ruleMatches = false;
                    }
                } else {
                    if (val) ruleMatches = false;
                }
            }
        }

        if (ruleMatches) {
            matchedAny = true;
            allowed = (rule.value(QStringLiteral("action")).toString() == QStringLiteral("allow"));
        }
    }

    return matchedAny ? allowed : false;
}

QString GameLauncher::parseMavenPath(const QString &coordinates) const {
    const QStringList parts = coordinates.split(QLatin1Char(':'));
    if (parts.size() < 3) {
        return QString();
    }

    const QString group = QString(parts[0]).replace(QLatin1Char('.'), QLatin1Char('/'));
    const QString artifact = parts[1];
    const QString version = parts[2];
    QString classifier;
    QString ext = QStringLiteral("jar");

    if (parts.size() >= 4) {
        const QString rest = parts[3];
        const int atIdx = rest.indexOf(QLatin1Char('@'));
        if (atIdx != -1) {
            classifier = rest.left(atIdx);
            ext = rest.mid(atIdx + 1);
        } else {
            classifier = rest;
        }
    }

    if (!classifier.isEmpty()) {
        return QStringLiteral("%1/%2/%3/%2-%3-%4.%5").arg(group, artifact, version, classifier, ext);
    }
    return QStringLiteral("%1/%2/%3/%2-%3.%4").arg(group, artifact, version, ext);
}

QString GameLauncher::buildClasspath(const QJsonObject &mergedVersion, const QString &versionId, const QString &mcDir) {
    QStringList cpEntries;
    const QString librariesDir = QDir(mcDir).filePath(QStringLiteral("libraries"));

    // 1. Libraries
    if (mergedVersion.contains(QStringLiteral("libraries")) && mergedVersion[QStringLiteral("libraries")].isArray()) {
        const QJsonArray libs = mergedVersion[QStringLiteral("libraries")].toArray();
        LaunchOptions dummyOpts;
        dummyOpts.customResolution = true;

        for (const auto &lVal : libs) {
            if (!lVal.isObject()) continue;
            const QJsonObject lib = lVal.toObject();

            if (lib.contains(QStringLiteral("rules")) && !isRuleAllowed(lib[QStringLiteral("rules")].toArray(), dummyOpts)) {
                continue;
            }

            QString relPath;
            if (lib.contains(QStringLiteral("downloads")) && lib[QStringLiteral("downloads")].isObject()) {
                const QJsonObject dls = lib[QStringLiteral("downloads")].toObject();
                if (dls.contains(QStringLiteral("artifact")) && dls[QStringLiteral("artifact")].isObject()) {
                    relPath = dls[QStringLiteral("artifact")].toObject().value(QStringLiteral("path")).toString();
                }
            } else if (lib.contains(QStringLiteral("name"))) {
                relPath = parseMavenPath(lib.value(QStringLiteral("name")).toString());
            }

            if (!relPath.isEmpty()) {
                const QString fullPath = QDir(librariesDir).filePath(relPath);
                if (QFile::exists(fullPath) && !cpEntries.contains(fullPath)) {
                    cpEntries.append(fullPath);
                }
            }
        }
    }

    // 2. Client JAR (or parent client jar)
    QString clientJarPath = QDir(mcDir).filePath(QStringLiteral("versions/%1/%1.jar").arg(versionId));
    if (!QFile::exists(clientJarPath) && mergedVersion.contains(QStringLiteral("inheritsFrom"))) {
        const QString parentId = mergedVersion.value(QStringLiteral("inheritsFrom")).toString();
        const QString parentJar = QDir(mcDir).filePath(QStringLiteral("versions/%1/%1.jar").arg(parentId));
        if (QFile::exists(parentJar)) {
            clientJarPath = parentJar;
        }
    }

    if (QFile::exists(clientJarPath)) {
        cpEntries.append(clientJarPath);
    }

#if defined(Q_OS_WIN)
    return cpEntries.join(QLatin1Char(';'));
#else
    return cpEntries.join(QLatin1Char(':'));
#endif
}

QString GameLauncher::expandPlaceholders(const QString &input, const QMap<QString, QString> &placeholders) const {
    QString out = input;
    for (auto it = placeholders.begin(); it != placeholders.end(); ++it) {
        const QString token = QStringLiteral("${%1}").arg(it.key());
        if (out.contains(token)) {
            out.replace(token, it.value());
        }
    }
    return out;
}

QStringList GameLauncher::expandArgumentList(const QJsonArray &argArray,
                                            const QMap<QString, QString> &placeholders,
                                            const LaunchOptions &options) {
    QStringList out;

    for (const auto &item : argArray) {
        if (item.isString()) {
            out.append(expandPlaceholders(item.toString(), placeholders));
        } else if (item.isObject()) {
            const QJsonObject obj = item.toObject();
            if (obj.contains(QStringLiteral("rules")) && !isRuleAllowed(obj[QStringLiteral("rules")].toArray(), options)) {
                continue;
            }

            const QJsonValue val = obj.value(QStringLiteral("value"));
            if (val.isString()) {
                out.append(expandPlaceholders(val.toString(), placeholders));
            } else if (val.isArray()) {
                for (const auto &subVal : val.toArray()) {
                    if (subVal.isString()) {
                        out.append(expandPlaceholders(subVal.toString(), placeholders));
                    }
                }
            }
        }
    }

    return out;
}

bool GameLauncher::buildLaunchCommand(const LaunchOptions &options,
                                      QString &outProgram,
                                      QStringList &outArguments,
                                      QString *outError) {
    if (options.username.trimmed().isEmpty()) {
        if (outError) *outError = QStringLiteral("Username cannot be empty.");
        return false;
    }
    if (options.versionId.trimmed().isEmpty()) {
        if (outError) *outError = QStringLiteral("Version ID cannot be empty.");
        return false;
    }

    const QString mcDir = options.gameDir.isEmpty() ? Constants::defaultMinecraftDirectory() : options.gameDir;
    const QJsonObject merged = loadAndMergeVersionJson(options.versionId, mcDir);
    if (merged.isEmpty()) {
        if (outError) *outError = QStringLiteral("Version JSON not found or invalid for %1").arg(options.versionId);
        return false;
    }

    const QString mainClass = merged.value(QStringLiteral("mainClass")).toString();
    if (mainClass.isEmpty()) {
        if (outError) *outError = QStringLiteral("No mainClass found in version definition.");
        return false;
    }

    // Determine Java executable
    QString javaExe = options.javaExecutable;
    if (javaExe.isEmpty() || !QFile::exists(javaExe)) {
        const auto javaResult = JavaManager::selectBestJava(options.versionId, QString(), true, mcDir);
        javaExe = javaResult.javaPath;
    }

    if (javaExe.isEmpty() || !QFile::exists(javaExe)) {
        if (outError) *outError = QStringLiteral("Java runtime not found on system.");
        return false;
    }

    // Build Paths & Placeholders
    const QString nativesDir = QDir(mcDir).filePath(QStringLiteral("versions/%1/natives").arg(options.versionId));
    const QString assetsDir = QDir(mcDir).filePath(QStringLiteral("assets"));
    const QString librariesDir = QDir(mcDir).filePath(QStringLiteral("libraries"));
    const QString assetsIndex = merged.value(QStringLiteral("assets")).toString(QStringLiteral("legacy"));
    const QString classpath = buildClasspath(merged, options.versionId, mcDir);
    const QString uuid = options.uuid.trimmed().isEmpty()
        ? Utils::generateOfflinePlayerUuid(options.username)
        : options.uuid;

    QMap<QString, QString> placeholders;
    placeholders[QStringLiteral("auth_player_name")] = options.username;
    placeholders[QStringLiteral("version_name")] = options.versionId;
    placeholders[QStringLiteral("version_type")] = merged.value(QStringLiteral("type")).toString(Constants::LAUNCHER_NAME);
    placeholders[QStringLiteral("game_directory")] = mcDir;
    placeholders[QStringLiteral("assets_root")] = assetsDir;
    placeholders[QStringLiteral("assets_index_name")] = assetsIndex;
    placeholders[QStringLiteral("auth_uuid")] = uuid;
    placeholders[QStringLiteral("auth_access_token")] = QStringLiteral("0");
    placeholders[QStringLiteral("user_type")] = QStringLiteral("legacy");
    placeholders[QStringLiteral("user_properties")] = QStringLiteral("{}");
    placeholders[QStringLiteral("auth_xuid")] = QString();
    placeholders[QStringLiteral("clientid")] = QString();
    placeholders[QStringLiteral("natives_directory")] = nativesDir;
    placeholders[QStringLiteral("launcher_name")] = options.launcherName.isEmpty() ? Constants::LAUNCHER_NAME : options.launcherName;
    placeholders[QStringLiteral("launcher_version")] = options.launcherVersion.isEmpty() ? Constants::LAUNCHER_VERSION : options.launcherVersion;
    placeholders[QStringLiteral("classpath")] = classpath;
#if defined(Q_OS_WIN)
    placeholders[QStringLiteral("classpath_separator")] = QStringLiteral(";");
#else
    placeholders[QStringLiteral("classpath_separator")] = QStringLiteral(":");
#endif
    placeholders[QStringLiteral("library_directory")] = librariesDir;
    placeholders[QStringLiteral("resolution_width")] = QString::number(options.resolutionWidth);
    placeholders[QStringLiteral("resolution_height")] = QString::number(options.resolutionHeight);
    placeholders[QStringLiteral("profile_name")] = options.username;

    // --- JVM Arguments ---
    QStringList jvmArgs;

    // 1. Memory limit
    const int mem = (options.memoryMb >= 512) ? options.memoryMb : Constants::DEFAULT_MEMORY_MB;
    jvmArgs.append(QStringLiteral("-Xmx%1M").arg(mem));

    // 2. Ely.by skins support if authlib-injector is present
    if (options.elyBySkins && !options.authlibJarPath.isEmpty() && QFile::exists(options.authlibJarPath)) {
        jvmArgs.append(QStringLiteral("-javaagent:%1=%2").arg(options.authlibJarPath, Constants::ELY_BY_SKIN_URL));
    }

    // 3. User custom JVM arguments
    if (!options.customJvmArgs.trimmed().isEmpty()) {
        const QStringList customList = options.customJvmArgs.split(QRegularExpression(QStringLiteral(R"(\s+)")), Qt::SkipEmptyParts);
        jvmArgs.append(customList);
    }

    // 4. Version JVM arguments
    bool hasModernJvmArgs = false;
    if (merged.contains(QStringLiteral("arguments")) && merged[QStringLiteral("arguments")].isObject()) {
        const QJsonObject argsObj = merged[QStringLiteral("arguments")].toObject();
        if (argsObj.contains(QStringLiteral("jvm")) && argsObj[QStringLiteral("jvm")].isArray()) {
            hasModernJvmArgs = true;
            const QStringList versionJvm = expandArgumentList(argsObj[QStringLiteral("jvm")].toArray(), placeholders, options);
            jvmArgs.append(versionJvm);
        }
    }

    // Fallback legacy JVM arguments (< 1.13)
    if (!hasModernJvmArgs) {
        jvmArgs.append(QStringLiteral("-Djava.library.path=%1").arg(nativesDir));
#if defined(Q_OS_WIN)
        jvmArgs.append(QStringLiteral("-Dos.name=Windows 10"));
        jvmArgs.append(QStringLiteral("-Dos.version=10.0"));
#endif
        jvmArgs.append(QStringLiteral("-XX:HeapDumpPath=MojangTricksIntelDriversForPerformance_javaw.exe_minecraft.exe.heap"));
        jvmArgs.append(QStringLiteral("-cp"));
        jvmArgs.append(classpath);
    }

    // --- Game Arguments ---
    QStringList gameArgs;

    if (merged.contains(QStringLiteral("arguments")) && merged[QStringLiteral("arguments")].isObject()) {
        const QJsonObject argsObj = merged[QStringLiteral("arguments")].toObject();
        if (argsObj.contains(QStringLiteral("game")) && argsObj[QStringLiteral("game")].isArray()) {
            gameArgs = expandArgumentList(argsObj[QStringLiteral("game")].toArray(), placeholders, options);
        }
    }

    if (gameArgs.isEmpty() && merged.contains(QStringLiteral("minecraftArguments"))) {
        const QString rawArgs = merged.value(QStringLiteral("minecraftArguments")).toString();
        const QStringList splitArgs = rawArgs.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString &arg : splitArgs) {
            gameArgs.append(expandPlaceholders(arg, placeholders));
        }
    }

    // Extra launcher arguments
    if (options.fullscreen) {
        gameArgs.append(QStringLiteral("--fullscreen"));
    } else if (options.customResolution) {
        gameArgs.append(QStringLiteral("--width"));
        gameArgs.append(QString::number(options.resolutionWidth));
        gameArgs.append(QStringLiteral("--height"));
        gameArgs.append(QString::number(options.resolutionHeight));
    }

    outProgram = javaExe;
    outArguments = jvmArgs;
    outArguments.append(mainClass);
    outArguments.append(gameArgs);

    return true;
}

bool GameLauncher::launch(const LaunchOptions &options, QString *outError) {
    if (isRunning()) {
        if (outError) *outError = QStringLiteral("A Minecraft instance is already running.");
        return false;
    }

    QString program;
    QStringList arguments;
    if (!buildLaunchCommand(options, program, arguments, outError)) {
        return false;
    }

    const QString mcDir = options.gameDir.isEmpty() ? Constants::defaultMinecraftDirectory() : options.gameDir;
    m_currentVersionId = options.versionId;

    LOG_INFO(QStringLiteral("Launching Minecraft %1 with %2").arg(options.versionId, program));
    LOG_DEBUG(QStringLiteral("Arguments: %1").arg(arguments.join(QLatin1Char(' '))));

    m_process->setProgram(program);
    m_process->setArguments(arguments);
    m_process->setWorkingDirectory(mcDir);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    m_process->start();
    return true;
}
