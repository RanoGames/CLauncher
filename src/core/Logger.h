#pragma once

#include <QObject>
#include <QString>
#include <QFile>
#include <QMutex>
#include <QtGlobal>

class Logger : public QObject {
    Q_OBJECT

public:
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error
    };
    Q_ENUM(LogLevel)

    static Logger &instance();

    // Initialize logger with an optional custom log file path
    void init(const QString &customLogFilePath = QString());

    // Logging methods
    void debug(const QString &message);
    void info(const QString &message);
    void warn(const QString &message);
    void error(const QString &message);
    void log(LogLevel level, const QString &message);

    // Qt message handler hook
    static void installQtMessageHandler();
    static void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    // Get current log file path
    QString logFilePath() const;

signals:
    void logMessageReceived(const QString &formattedMessage, Logger::LogLevel level);

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger() override;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void writeLog(LogLevel level, const QString &message);
    static QString levelToString(LogLevel level);

    mutable QMutex m_mutex;
    QFile m_logFile;
    QString m_logFilePath;
    bool m_initialized = false;
};

// Convenience macros
#define LOG_DEBUG(msg) Logger::instance().debug(msg)
#define LOG_INFO(msg)  Logger::instance().info(msg)
#define LOG_WARN(msg)  Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)
