#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFile>
#include <QIcon>
#include <iostream>

#include "core/Constants.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include "core/AccountManager.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    // Install Qt message handler early
    Logger::installQtMessageHandler();

    QApplication app(argc, argv);
    app.setApplicationName(Constants::LAUNCHER_NAME);
    app.setApplicationVersion(Constants::LAUNCHER_VERSION);
    app.setOrganizationName(QStringLiteral("RanoGames"));
    app.setWindowIcon(QIcon(QStringLiteral(":/mclovin.ico")));

    // Command-line parser
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("CLauncher - Modern, fast, and sleek Minecraft launcher built in C++ and Qt6"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption gameDirOption(QStringList() << QStringLiteral("d") << QStringLiteral("game-dir"),
                                     QStringLiteral("Custom Minecraft game directory path."),
                                     QStringLiteral("directory"));
    parser.addOption(gameDirOption);

    parser.process(app);

    // Initialize core subsystems
    Logger::instance().init();
    LOG_INFO(QStringLiteral("Starting CLauncher v") + Constants::LAUNCHER_VERSION);

    Settings::instance().load();

    if (parser.isSet(gameDirOption)) {
        Settings::instance().setGameDir(parser.value(gameDirOption));
    }

    AccountManager::instance().load();

    // Apply global stylesheet
    QFile styleFile(QStringLiteral(":/style.qss"));
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    } else {
        LOG_WARN(QStringLiteral("Could not load global stylesheet from :/style.qss"));
    }

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
