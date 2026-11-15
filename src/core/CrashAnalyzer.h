#pragma once

#include <QString>
#include <QStringList>
#include <optional>

struct CrashDiagnosis {
    QString title;          // Short summary of the crash
    QString cause;          // Human-readable root cause explanation
    QString solution;       // Actionable recommendations for the user
    QString reportFile;     // Path to crash report file or log file
    QString rawSnippet;     // The excerpt from log/crash report where the error was identified
    bool isCrash = false;   // True if an error was detected
};

class CrashAnalyzer {
public:
    // Analyzes the most recent crash report or log in game directory
    static std::optional<CrashDiagnosis> analyzeLatestCrash(const QString &gameDir = QString());

    // Analyzes a given crash report file directly
    static std::optional<CrashDiagnosis> analyzeFile(const QString &filePath);

    // Analyzes raw log or crash text
    static std::optional<CrashDiagnosis> analyzeText(const QString &text, const QString &sourceFile = QString());

    // Helpers to find the latest crash report or log file
    static QString findLatestCrashReport(const QString &gameDir = QString());
    static QString findLatestLog(const QString &gameDir = QString());
};
