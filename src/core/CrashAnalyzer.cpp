#include "CrashAnalyzer.h"
#include "Constants.h"
#include "Logger.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QTextStream>

QString CrashAnalyzer::findLatestCrashReport(const QString &gameDir) {
    QString baseDir = gameDir.isEmpty() ? Constants::defaultMinecraftDirectory() : gameDir;
    QDir crashReportsDir(QDir::cleanPath(baseDir + QStringLiteral("/crash-reports")));
    if (!crashReportsDir.exists()) {
        return QString();
    }

    QFileInfoList list = crashReportsDir.entryInfoList(
        QStringList{QStringLiteral("crash-*.txt")},
        QDir::Files,
        QDir::Time // newest first
    );

    if (list.isEmpty()) {
        return QString();
    }

    return list.first().absoluteFilePath();
}

QString CrashAnalyzer::findLatestLog(const QString &gameDir) {
    QString baseDir = gameDir.isEmpty() ? Constants::defaultMinecraftDirectory() : gameDir;
    QString logPath = QDir::cleanPath(baseDir + QStringLiteral("/logs/latest.log"));
    if (QFile::exists(logPath)) {
        return logPath;
    }
    return QString();
}

std::optional<CrashDiagnosis> CrashAnalyzer::analyzeLatestCrash(const QString &gameDir) {
    // 1. First look in crash-reports/
    QString crashFile = findLatestCrashReport(gameDir);
    if (!crashFile.isEmpty()) {
        auto result = analyzeFile(crashFile);
        if (result.has_value()) {
            return result;
        }
    }

    // 2. If nothing found in crash-reports, analyze the last 300 lines of logs/latest.log
    QString logFile = findLatestLog(gameDir);
    if (!logFile.isEmpty()) {
        QFile file(logFile);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QStringList lines;
            while (!in.atEnd()) {
                lines.append(in.readLine());
                if (lines.size() > 500) {
                    lines.removeFirst();
                }
            }
            QString logTail = lines.join(QLatin1Char('\n'));
            return analyzeText(logTail, logFile);
        }
    }

    return std::nullopt;
}

std::optional<CrashDiagnosis> CrashAnalyzer::analyzeFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_WARN(QStringLiteral("Failed to open crash report file: %1").arg(filePath));
        return std::nullopt;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    return analyzeText(content, filePath);
}

std::optional<CrashDiagnosis> CrashAnalyzer::analyzeText(const QString &text, const QString &sourceFile) {
    if (text.trimmed().isEmpty()) {
        return std::nullopt;
    }

    CrashDiagnosis diag;
    diag.reportFile = sourceFile;

    // Helper to find snippet around matching text
    auto extractSnippet = [&](const QString &keyword, int maxChars = 250) -> QString {
        int idx = text.indexOf(keyword, 0, Qt::CaseInsensitive);
        if (idx == -1) return QString();
        int start = std::max(0, idx - 50);
        int length = std::min(static_cast<int>(text.size() - start), maxChars);
        return text.mid(start, length).trimmed();
    };

    // 1. Out of Memory Error
    if (text.contains(QStringLiteral("OutOfMemoryError"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("java.lang.OutOfMemoryError"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("There is insufficient memory for the Java Runtime"), Qt::CaseInsensitive)) {
        diag.title = QStringLiteral("Недостаточно оперативной памяти (OutOfMemoryError)");
        diag.cause = QStringLiteral("Minecraft превысил выделенный лимит оперативной памяти.");
        diag.solution = QStringLiteral("Откройте Настройки лаунчера и увеличьте ползунок «Память (RAM)» (например, до 4096 МБ или 6144 МБ для модов).");
        diag.rawSnippet = extractSnippet(QStringLiteral("OutOfMemoryError"));
        diag.isCrash = true;
        return diag;
    }

    // 2. Incompatible or Missing Java Runtime
    if (text.contains(QStringLiteral("UnsupportedClassVersionError"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("has been compiled by a more recent version"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("class file version"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("java: command not found"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("No Java runtime present"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("Unable to locate a Java Runtime"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("Java executable not found"), Qt::CaseInsensitive)) {
        diag.title = QStringLiteral("Несовместимая версия Java или отсутствует среда выполнения");
        diag.cause = QStringLiteral("Игра или один из модов собраны под другую версию Java, либо среда выполнения Java не найдена в системе.");
        diag.solution = QStringLiteral("Включите «Автоопределение Java» в настройках лаунчера или выберите совместимую версию Java (Java 8 для 1.16-, Java 17 для 1.18-1.20.4, Java 21 для 1.20.5+).");
        diag.rawSnippet = extractSnippet(QStringLiteral("UnsupportedClassVersionError"));
        if (diag.rawSnippet.isEmpty()) diag.rawSnippet = extractSnippet(QStringLiteral("Java"));
        diag.isCrash = true;
        return diag;
    }

    // 3. Missing Mod Dependencies (Fabric / Forge / NeoForge / Quilt)
    static const QRegularExpression depRe(
        QStringLiteral(R"((?:requires\s+['"]([^'"]+)['"])|(?:missing\s+dependency:?\s+([^\n\r]+))|(?:Mod\s+([^\s]+)\s+requires))"),
        QRegularExpression::CaseInsensitiveOption
    );
    auto depMatch = depRe.match(text);
    if (text.contains(QStringLiteral("ModResolutionException"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("IncompatibleModsException"), Qt::CaseInsensitive) ||
        depMatch.hasMatch()) {
        QString missingMod = QStringLiteral("неизвестный мод");
        if (depMatch.hasMatch()) {
            for (int i = 1; i <= 3; ++i) {
                QString cap = depMatch.captured(i).trimmed();
                if (!cap.isEmpty()) {
                    missingMod = cap;
                    break;
                }
            }
        }
        diag.title = QStringLiteral("Отсутствует зависимость мода");
        diag.cause = QStringLiteral("Один из установленных модов требует для своей работы библиотеку или мод: %1.").arg(missingMod);
        diag.solution = QStringLiteral("Установите недостающий мод (%1) в папку mods через каталог модов или вручную.").arg(missingMod);
        diag.rawSnippet = depMatch.hasMatch() ? depMatch.captured(0) : extractSnippet(QStringLiteral("ModResolutionException"));
        diag.isCrash = true;
        return diag;
    }

    // 4. Duplicate Mods
    if (text.contains(QStringLiteral("DuplicateModsException"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("found duplicate mods"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("Duplicate mod ID found"), Qt::CaseInsensitive)) {
        diag.title = QStringLiteral("Конфликт: найдены дубликаты модов");
        diag.cause = QStringLiteral("В папке mods установлено несколько версий одного и того же мода.");
        diag.solution = QStringLiteral("Откройте папку mods и удалите устаревшие копии повторяющихся модов.");
        diag.rawSnippet = extractSnippet(QStringLiteral("Duplicate"));
        diag.isCrash = true;
        return diag;
    }

    // 5. Video driver / OpenGL / GLFW Errors
    if (text.contains(QStringLiteral("Pixel format not accelerated"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("GLFW error 65542"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("WGL: The driver does not appear to support OpenGL"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("org.lwjgl.LWJGLException: Could not create context"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("GLX: Failed to create context"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("Failed to initialize OpenGL"), Qt::CaseInsensitive)) {
        diag.title = QStringLiteral("Ошибка видеодрайвера (OpenGL / GLFW)");
        diag.cause = QStringLiteral("Видеокарта или графический драйвер не поддерживают требуемый профиль OpenGL.");
        diag.solution = QStringLiteral("Обновите драйвер видеокарты (NVIDIA, AMD или Intel) с официального сайта производителя или переключите дискретную/встроенную графику.");
        diag.rawSnippet = extractSnippet(QStringLiteral("GLFW error"));
        if (diag.rawSnippet.isEmpty()) diag.rawSnippet = extractSnippet(QStringLiteral("OpenGL"));
        diag.isCrash = true;
        return diag;
    }

    // 6. Corrupted Mods / Mixin / ASM Errors
    if (text.contains(QStringLiteral("MixinApplyError"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("org.spongepowered.asm.mixin"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("ZipException"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("invalid END header"), Qt::CaseInsensitive) ||
        text.contains(QStringLiteral("Error parsing metadata"), Qt::CaseInsensitive)) {
        diag.title = QStringLiteral("Конфликт модов (Mixin Error / Повреждённый файл)");
        diag.cause = QStringLiteral("Два или более мода пытаются несовместимым образом модифицировать один и тот же код игры, либо JAR-файл мода повреждён.");
        diag.solution = QStringLiteral("Попробуйте временно отключить последние добавленные моды по одному, чтобы найти проблемный мод.");
        diag.rawSnippet = extractSnippet(QStringLiteral("Mixin"));
        if (diag.rawSnippet.isEmpty()) diag.rawSnippet = extractSnippet(QStringLiteral("ZipException"));
        diag.isCrash = true;
        return diag;
    }

    // 7. Generic Minecraft Crash Report detection
    if (text.contains(QStringLiteral("---- Minecraft Crash Report ----")) ||
        text.contains(QStringLiteral("A fatal error has been detected by the Java Runtime Environment")) ||
        text.contains(QStringLiteral("Exception in thread \"main\""))) {
        
        static const QRegularExpression descRe(QStringLiteral(R"(Description:\s*([^\n\r]+))"));
        auto match = descRe.match(text);
        QString descText = match.hasMatch() ? match.captured(1).trimmed() : QStringLiteral("Непредвиденное исключение при выполнении игры");

        diag.title = QStringLiteral("Сбой Minecraft (%1)").arg(descText.left(50));
        diag.cause = descText;
        diag.solution = QStringLiteral("Проверьте установленные моды на совместимость с версией игры, либо изучите полный лог в окне консоли.");
        diag.rawSnippet = match.hasMatch() ? match.captured(0) : extractSnippet(QStringLiteral("Exception in thread"));
        diag.isCrash = true;
        return diag;
    }

    return std::nullopt;
}
