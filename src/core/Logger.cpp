#include "Logger.h"
#include "Constants.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <iostream>

Logger &Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

Logger::Logger(QObject *parent)
    : QObject(parent) {
}

Logger::~Logger() {
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen()) {
        m_logFile.flush();
        m_logFile.close();
    }
}

void Logger::init(const QString &customLogFilePath) {
    QMutexLocker locker(&m_mutex);

    if (m_logFile.isOpen()) {
        m_logFile.flush();
        m_logFile.close();
    }

    m_logFilePath = customLogFilePath.isEmpty() ? Constants::defaultLogPath() : customLogFilePath;

    QFileInfo fileInfo(m_logFilePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    m_logFile.setFileName(m_logFilePath);
    if (!m_logFile.open(QIODevice::Append | QIODevice::Text)) {
        std::cerr << "[Logger] Failed to open log file: "
                  << m_logFilePath.toStdString() << " ("
                  << m_logFile.errorString().toStdString() << ")" << std::endl;
    }

    m_initialized = true;
}

QString Logger::logFilePath() const {
    QMutexLocker locker(&m_mutex);
    return m_logFilePath;
}

QString Logger::levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
        return QStringLiteral("DEBUG");
    case LogLevel::Info:
        return QStringLiteral("INFO");
    case LogLevel::Warning:
        return QStringLiteral("WARN");
    case LogLevel::Error:
        return QStringLiteral("ERROR");
    }
    return QStringLiteral("UNKNOWN");
}

void Logger::debug(const QString &message) {
    log(LogLevel::Debug, message);
}

void Logger::info(const QString &message) {
    log(LogLevel::Info, message);
}

void Logger::warn(const QString &message) {
    log(LogLevel::Warning, message);
}

void Logger::error(const QString &message) {
    log(LogLevel::Error, message);
}

void Logger::log(LogLevel level, const QString &message) {
    writeLog(level, message);
}

void Logger::writeLog(LogLevel level, const QString &message) {
    QString timeStr = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    QString levelStr = levelToString(level);
    QString formatted = QStringLiteral("[%1] [%2] %3").arg(timeStr, levelStr, message);

    {
        QMutexLocker locker(&m_mutex);

        // Auto-initialize if not yet initialized
        if (!m_initialized) {
            locker.unlock();
            init();
            locker.relock();
        }

        // Output to standard console
        if (level == LogLevel::Error || level == LogLevel::Warning) {
            std::cerr << formatted.toStdString() << std::endl;
        } else {
            std::cout << formatted.toStdString() << std::endl;
        }

        // Output to file
        if (m_logFile.isOpen()) {
            QByteArray data = (formatted + QStringLiteral("\n")).toUtf8();
            m_logFile.write(data);
            m_logFile.flush();
        }
    }

    // Emit signal outside the mutex lock to avoid deadlocks with UI slots
    emit logMessageReceived(formatted, level);
}

void Logger::installQtMessageHandler() {
    qInstallMessageHandler(Logger::qtMessageHandler);
}

void Logger::qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(context);
    LogLevel level = LogLevel::Info;
    switch (type) {
    case QtDebugMsg:
        level = LogLevel::Debug;
        break;
    case QtInfoMsg:
        level = LogLevel::Info;
        break;
    case QtWarningMsg:
        level = LogLevel::Warning;
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        level = LogLevel::Error;
        break;
    }
    Logger::instance().log(level, msg);
}
