#include "MainWindow.h"
#include "../core/Constants.h"
#include "../core/Settings.h"
#include "../core/Utils.h"
#include "../core/Logger.h"
#include "../core/PlaytimeTracker.h"
#include "../mc/JavaManager.h"

#include <QPainter>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QLineEdit>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupConnections();
    loadBackground();
    populateNicknames();
    loadVersions(false);
    updateActionButtonState();
}

MainWindow::~MainWindow()
{
    if (m_consoleWindow) {
        delete m_consoleWindow;
        m_consoleWindow = nullptr;
    }
}

void MainWindow::setupUi()
{
    setObjectName(QStringLiteral("MainWindow"));
    setWindowTitle(QStringLiteral("CLauncher - Modern Minecraft Launcher"));
    setFixedSize(780, 570);
    setWindowIcon(QIcon(QStringLiteral(":/mclovin.ico")));

    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName(QStringLiteral("CentralWidget"));
    setCentralWidget(m_centralWidget);

    QFont defaultFont(QStringLiteral("Segoe UI"));
    defaultFont.setPointSize(11);
    m_centralWidget->setFont(defaultFont);

    // 1. Version Badge (top left)
    m_versionBadgeLabel = new QLabel(m_centralWidget);
    m_versionBadgeLabel->setGeometry(16, 14, 140, 26);
    m_versionBadgeLabel->setObjectName(QStringLiteral("JavaInfoLabel"));
    m_versionBadgeLabel->setAlignment(Qt::AlignCenter);
    m_versionBadgeLabel->setText(Constants::LAUNCHER_NAME + QStringLiteral(" v") + Constants::LAUNCHER_VERSION);
    m_versionBadgeLabel->setToolTip(tr("CLauncher - лёгкий и современный лаунчер Minecraft"));
    m_versionBadgeLabel->setStyleSheet(
        "QLabel#JavaInfoLabel {"
        "  background-color: rgba(18, 20, 26, 0.75);"
        "  color: #b0b0b0;"
        "  border: 1px solid rgba(255, 255, 255, 0.1);"
        "  border-radius: 8px;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
        "QLabel#JavaInfoLabel:hover {"
        "  background-color: rgba(30, 35, 45, 0.95);"
        "  color: #10b981;"
        "  border: 1px solid rgba(16, 185, 129, 0.5);"
        "}"
    );

    // 2. Status Label & Progress Bar (center)
    m_statusLabel = new QLabel(m_centralWidget);
    m_statusLabel->setGeometry(140, 215, 500, 44);
    m_statusLabel->setObjectName(QStringLiteral("Progress"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    QFont statusFont(QStringLiteral("Segoe UI"), 12, QFont::Bold);
    m_statusLabel->setFont(statusFont);
    m_statusLabel->setStyleSheet(
        "QLabel#Progress {"
        "  color: #ffffff;"
        "  background-color: rgba(18, 20, 26, 0.85);"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 12px;"
        "  padding: 4px 14px;"
        "}"
    );
    m_statusLabel->hide();

    m_progressBar = new QProgressBar(m_centralWidget);
    m_progressBar->setGeometry(180, 272, 420, 10);
    m_progressBar->setObjectName(QStringLiteral("InstallProgressBar"));
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();

    // 3. Floating Glass Dock Card
    m_dockCard = new QFrame(m_centralWidget);
    m_dockCard->setGeometry(20, 385, 740, 168);
    m_dockCard->setObjectName(QStringLiteral("DockCard"));
    m_dockCard->setStyleSheet(
        "QFrame#DockCard {"
        "  background-color: rgba(18, 20, 28, 0.88);"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 16px;"
        "}"
    );

    // Dock Top Row: Nickname
    m_nicknameTitleLabel = new QLabel(tr("👤 ИМЯ ИГРОКА"), m_dockCard);
    m_nicknameTitleLabel->setGeometry(16, 10, 220, 18);
    m_nicknameTitleLabel->setStyleSheet(QStringLiteral("color: #8e8e93; font-size: 11px; font-weight: bold; letter-spacing: 0.5px;"));

    m_nicknameCombo = new QComboBox(m_dockCard);
    m_nicknameCombo->setEditable(true);
    m_nicknameCombo->setGeometry(16, 32, 175, 48);
    m_nicknameCombo->setObjectName(QStringLiteral("Nickname"));

    m_deleteNickButton = new QPushButton(QStringLiteral("✕"), m_dockCard);
    m_deleteNickButton->setGeometry(196, 32, 42, 48);
    m_deleteNickButton->setProperty("class", "miniBtn");
    m_deleteNickButton->setToolTip(tr("Удалить никнейм из списка"));

    // Dock Top Row: Version
    m_versionTitleLabel = new QLabel(tr("📦 ВЕРСИЯ ИГРЫ"), m_dockCard);
    m_versionTitleLabel->setGeometry(250, 10, 245, 18);
    m_versionTitleLabel->setStyleSheet(QStringLiteral("color: #8e8e93; font-size: 11px; font-weight: bold; letter-spacing: 0.5px;"));

    m_versionCombo = new QComboBox(m_dockCard);
    m_versionCombo->setEditable(false);
    m_versionCombo->setGeometry(250, 32, 200, 48);
    m_versionCombo->setObjectName(QStringLiteral("VersionComboBox"));

    m_refreshVersionsButton = new QPushButton(QStringLiteral("🔄"), m_dockCard);
    m_refreshVersionsButton->setGeometry(455, 32, 44, 48);
    m_refreshVersionsButton->setProperty("class", "miniBtn");
    m_refreshVersionsButton->setToolTip(tr("Обновить список версий"));

    // Dock Top Row: Hero Action Buttons
    m_playButton = new QPushButton(tr("▶ ИГРАТЬ"), m_dockCard);
    m_playButton->setGeometry(514, 20, 210, 60);
    m_playButton->setObjectName(QStringLiteral("PlayMinecraft"));
    m_playButton->setCursor(Qt::PointingHandCursor);

    m_downloadButton = new QPushButton(tr("⬇ СКАЧАТЬ"), m_dockCard);
    m_downloadButton->setGeometry(514, 20, 210, 60);
    m_downloadButton->setObjectName(QStringLiteral("DownLoadMinecraft"));
    m_downloadButton->setCursor(Qt::PointingHandCursor);
    m_downloadButton->hide();

    m_cancelButton = new QPushButton(tr("✕ ОТМЕНА"), m_dockCard);
    m_cancelButton->setGeometry(514, 20, 210, 60);
    m_cancelButton->setObjectName(QStringLiteral("CancelInstallButton"));
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    m_cancelButton->hide();

    // Dock Bottom Row: Quick Action Pills
    const int btnY = 108;
    const int btnH = 44;

    m_settingsButton = new QPushButton(tr("⚙ Настройки"), m_dockCard);
    m_settingsButton->setGeometry(16, btnY, 134, btnH);
    m_settingsButton->setProperty("class", "actionPill");
    m_settingsButton->setToolTip(tr("Настройки лаунчера и игры"));

    m_consoleButton = new QPushButton(tr("💻 Консоль"), m_dockCard);
    m_consoleButton->setGeometry(158, btnY, 134, btnH);
    m_consoleButton->setProperty("class", "actionPill");
    m_consoleButton->setToolTip(tr("Открыть консоль отладки и логов"));

    m_folderButton = new QPushButton(tr("📁 Папка"), m_dockCard);
    m_folderButton->setGeometry(300, btnY, 134, btnH);
    m_folderButton->setProperty("class", "actionPill");
    m_folderButton->setToolTip(tr("Открыть папку Minecraft"));

    m_deleteVersionButton = new QPushButton(tr("🗑 Удалить"), m_dockCard);
    m_deleteVersionButton->setGeometry(442, btnY, 134, btnH);
    m_deleteVersionButton->setProperty("class", "actionPill");
    m_deleteVersionButton->setToolTip(tr("Удалить выбранную версию с диска"));

    m_gitHubButton = new QPushButton(tr("🐙 GitHub"), m_dockCard);
    m_gitHubButton->setGeometry(584, btnY, 140, btnH);
    m_gitHubButton->setProperty("class", "actionPill");
    m_gitHubButton->setToolTip(tr("Открыть страницу проекта на GitHub"));

    // Console window
    m_consoleWindow = new ConsoleWindow();

    // Also connect Logger global signals to Console
    connect(&Logger::instance(), &Logger::logMessageReceived,
            m_consoleWindow, [this](const QString &formattedMessage, Logger::LogLevel level) {
                Q_UNUSED(level);
                m_consoleWindow->appendLog(formattedMessage);
            });
}

void MainWindow::setupConnections()
{
    // Buttons
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::onPlayOrDownloadClicked);
    connect(m_downloadButton, &QPushButton::clicked, this, &MainWindow::onPlayOrDownloadClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &MainWindow::onCancelDownloadClicked);
    connect(m_refreshVersionsButton, &QPushButton::clicked, this, &MainWindow::onRefreshVersionsClicked);
    connect(m_deleteNickButton, &QPushButton::clicked, this, &MainWindow::onDeleteNicknameClicked);

    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::onOpenSettingsClicked);
    connect(m_consoleButton, &QPushButton::clicked, this, &MainWindow::onOpenConsoleClicked);
    connect(m_folderButton, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);
    connect(m_deleteVersionButton, &QPushButton::clicked, this, &MainWindow::onDeleteVersionClicked);
    connect(m_gitHubButton, &QPushButton::clicked, this, &MainWindow::onOpenGitHubClicked);

    // Combos
    connect(m_versionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onVersionIndexChanged);

    if (m_nicknameCombo->lineEdit()) {
        connect(m_nicknameCombo->lineEdit(), &QLineEdit::editingFinished,
                this, &MainWindow::onNicknameChanged);
    }
    connect(m_nicknameCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onNicknameChanged);

    // VersionManifest
    connect(&m_manifest, &VersionManifest::manifestLoaded, this, &MainWindow::onManifestLoaded);
    connect(&m_manifest, &VersionManifest::errorOccurred, this, &MainWindow::onManifestError);

    // Downloader
    connect(&m_downloader, &Downloader::downloadProgress, this, &MainWindow::onDownloadProgress);
    connect(&m_downloader, &Downloader::progressPercent, this, &MainWindow::onDownloadPercent);
    connect(&m_downloader, &Downloader::statusChanged, this, &MainWindow::onDownloadStatusChanged);
    connect(&m_downloader, &Downloader::finished, this, &MainWindow::onDownloadFinished);
    connect(&m_downloader, &Downloader::error, this, &MainWindow::onDownloadError);

    // GameLauncher
    connect(&m_gameLauncher, &GameLauncher::gameStarted, this, &MainWindow::onGameStarted);
    connect(&m_gameLauncher, &GameLauncher::gameExited, this, &MainWindow::onGameExited);
    connect(&m_gameLauncher, &GameLauncher::logReceived, this, &MainWindow::onGameLogReceived);
    connect(&m_gameLauncher, &GameLauncher::launchError, this, &MainWindow::onGameLaunchError);
}

void MainWindow::loadBackground()
{
    QString customBg = Settings::instance().customBackground();
    if (!customBg.isEmpty() && QFile::exists(customBg)) {
        m_bgPixmap.load(customBg);
    } else {
        m_bgPixmap.load(QStringLiteral(":/background.png"));
    }
    update();
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    if (!m_bgPixmap.isNull()) {
        painter.drawPixmap(rect(), m_bgPixmap.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    } else {
        painter.fillRect(rect(), QColor(18, 20, 28));
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
}

void MainWindow::populateNicknames()
{
    m_nicknameCombo->blockSignals(true);
    m_nicknameCombo->clear();

    const QStringList nicks = Settings::instance().nicknames();
    for (const QString &n : nicks) {
        m_nicknameCombo->addItem(n);
    }

    const QString last = Settings::instance().lastNickname();
    if (!last.isEmpty()) {
        m_nicknameCombo->setEditText(last);
    } else if (m_nicknameCombo->count() > 0) {
        m_nicknameCombo->setCurrentIndex(0);
    }
    m_nicknameCombo->blockSignals(false);
}

void MainWindow::saveCurrentNickname()
{
    QString nick = m_nicknameCombo->currentText().trimmed();
    if (nick.isEmpty()) return;

    auto &s = Settings::instance();
    s.addNickname(nick);
    s.setLastNickname(nick);
    s.save();
}

void MainWindow::loadVersions(bool forceRefresh)
{
    m_versionCombo->blockSignals(true);
    m_versionCombo->clear();
    m_versionCombo->setPlaceholderText(tr("Загрузка списка версий..."));
    m_versionCombo->blockSignals(false);

    m_statusLabel->setText(tr("Получение списка версий от Mojang..."));
    m_statusLabel->show();

    m_manifest.fetchManifest(forceRefresh, Settings::instance().filters());
}

void MainWindow::onManifestLoaded(const QList<VersionEntry> &versions)
{
    m_statusLabel->hide();
    m_versionCombo->blockSignals(true);
    m_versionCombo->clear();

    // First collect local versions that are on disk
    QString versionsDir = Constants::defaultVersionsDirectory();
    QDir dir(versionsDir);
    QStringList localVersions;
    if (dir.exists()) {
        const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &v : entries) {
            if (isVersionInstalled(v)) {
                localVersions.append(v);
            }
        }
    }

    // Add local versions if not in remote list
    QSet<QString> addedVersions;
    for (const auto &v : versions) {
        m_versionCombo->addItem(v.id, v.id);
        addedVersions.insert(v.id);
    }

    for (const QString &local : localVersions) {
        if (!addedVersions.contains(local)) {
            m_versionCombo->addItem(local + tr(" (локальная)"), local);
            addedVersions.insert(local);
        }
    }

    // Restore last selected version
    QString lastVer = Settings::instance().lastVersion();
    int idx = -1;
    if (!lastVer.isEmpty()) {
        idx = m_versionCombo->findData(lastVer);
    }
    if (idx < 0 && !m_manifest.latestRelease().isEmpty()) {
        idx = m_versionCombo->findData(m_manifest.latestRelease());
    }
    if (idx >= 0) {
        m_versionCombo->setCurrentIndex(idx);
    } else if (m_versionCombo->count() > 0) {
        m_versionCombo->setCurrentIndex(0);
    }

    m_versionCombo->blockSignals(false);
    updateActionButtonState();
}

void MainWindow::onManifestError(const QString &errorMessage)
{
    m_statusLabel->setText(tr("Ошибка сети: список версий офлайн"));
    m_statusLabel->show();
    LOG_WARN(QStringLiteral("Manifest fetch error: ") + errorMessage);

    // Fallback: populate with installed local versions
    m_versionCombo->blockSignals(true);
    m_versionCombo->clear();

    QString versionsDir = Constants::defaultVersionsDirectory();
    QDir dir(versionsDir);
    if (dir.exists()) {
        const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &v : entries) {
            if (isVersionInstalled(v)) {
                m_versionCombo->addItem(v, v);
            }
        }
    }

    QString lastVer = Settings::instance().lastVersion();
    int idx = m_versionCombo->findData(lastVer);
    if (idx >= 0) {
        m_versionCombo->setCurrentIndex(idx);
    } else if (m_versionCombo->count() > 0) {
        m_versionCombo->setCurrentIndex(0);
    }

    m_versionCombo->blockSignals(false);
    updateActionButtonState();
}

bool MainWindow::isVersionInstalled(const QString &versionId) const
{
    if (versionId.isEmpty()) return false;
    QString dirPath = Constants::defaultVersionsDirectory() + QStringLiteral("/") + versionId;
    QString jarPath = dirPath + QStringLiteral("/") + versionId + QStringLiteral(".jar");
    QString jsonPath = dirPath + QStringLiteral("/") + versionId + QStringLiteral(".json");
    return QFile::exists(jarPath) || QFile::exists(jsonPath);
}

void MainWindow::updateActionButtonState()
{
    if (m_isDownloading) {
        m_playButton->hide();
        m_downloadButton->hide();
        m_cancelButton->show();
        m_versionCombo->setEnabled(false);
        m_nicknameCombo->setEnabled(false);
        m_refreshVersionsButton->setEnabled(false);
        m_settingsButton->setEnabled(false);
        return;
    }

    m_cancelButton->hide();
    m_versionCombo->setEnabled(true);
    m_nicknameCombo->setEnabled(true);
    m_refreshVersionsButton->setEnabled(true);
    m_settingsButton->setEnabled(true);

    if (m_isGameRunning) {
        m_playButton->show();
        m_downloadButton->hide();
        m_playButton->setText(tr("▶ ИГРАЕТ..."));
        m_playButton->setEnabled(false);
        return;
    }

    QString currentVersion = m_versionCombo->currentData().toString();
    if (currentVersion.isEmpty()) {
        currentVersion = m_versionCombo->currentText().trimmed();
    }

    if (isVersionInstalled(currentVersion)) {
        m_playButton->setText(tr("▶ ИГРАТЬ"));
        m_playButton->setEnabled(true);
        m_playButton->show();
        m_downloadButton->hide();
    } else {
        m_downloadButton->setText(tr("⬇ СКАЧАТЬ"));
        m_downloadButton->setEnabled(true);
        m_downloadButton->show();
        m_playButton->hide();
    }
}

void MainWindow::onVersionIndexChanged(int index)
{
    Q_UNUSED(index);
    QString ver = m_versionCombo->currentData().toString();
    if (!ver.isEmpty()) {
        Settings::instance().setLastVersion(ver);
        Settings::instance().save();
    }
    updateActionButtonState();
}

void MainWindow::onNicknameChanged()
{
    saveCurrentNickname();
}

void MainWindow::onDeleteNicknameClicked()
{
    QString nick = m_nicknameCombo->currentText().trimmed();
    if (nick.isEmpty()) return;

    Settings::instance().removeNickname(nick);
    Settings::instance().save();
    populateNicknames();
}

void MainWindow::onPlayOrDownloadClicked()
{
    QString version = m_versionCombo->currentData().toString();
    if (version.isEmpty()) {
        version = m_versionCombo->currentText().trimmed();
    }
    QString username = m_nicknameCombo->currentText().trimmed();

    if (version.isEmpty()) {
        QMessageBox::warning(this, tr("Внимание"), tr("Пожалуйста, выберите версию игры."));
        return;
    }

    if (username.isEmpty()) {
        QMessageBox::warning(this, tr("Внимание"), tr("Пожалуйста, введите никнейм игрока."));
        return;
    }

    if (Settings::instance().validateNickname() && !Utils::isValidNickname(username)) {
        QMessageBox::warning(this, tr("Некорректный никнейм"),
                             tr("Никнейм может содержать от 3 до 16 символов (буквы, цифры и знак подчеркивания)."));
        return;
    }

    saveCurrentNickname();

    if (!isVersionInstalled(version)) {
        // Confirmation before download
        auto [freeMb, enough] = Utils::checkDiskSpace(Settings::instance().effectiveGameDir(), 500);
        if (!enough) {
            QMessageBox::warning(this, tr("Недостаточно места"),
                                 QString(tr("На диске свободно %1 МБ, требуется минимум 500 МБ.")).arg(freeMb));
            return;
        }

        auto reply = QMessageBox::question(
            this,
            tr("Установка версии"),
            QString(tr("Версия %1 ещё не установлена.\nСкачать и установить её сейчас?")).arg(version),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );

        if (reply != QMessageBox::Yes) {
            return;
        }

        // Start download
        m_isDownloading = true;
        updateActionButtonState();

        m_statusLabel->setText(QString(tr("Подготовка к загрузке %1...")).arg(version));
        m_statusLabel->show();
        m_progressBar->setValue(0);
        m_progressBar->show();

        LOG_INFO(QStringLiteral("Starting download for version: ") + version);
        m_downloader.downloadVersion(version, Settings::instance().effectiveGameDir());
        return;
    }

    // Launch game
    LOG_INFO(QStringLiteral("Preparing launch for version: ") + version + QStringLiteral(", player: ") + username);

    auto javaResult = JavaManager::selectBestJava(
        version,
        Settings::instance().javaPath(),
        Settings::instance().autoJava(),
        Settings::instance().effectiveGameDir()
    );

    if (!javaResult.isCompatible || javaResult.javaPath.isEmpty()) {
        QMessageBox::critical(
            this,
            tr("Ошибка Java"),
            QString(tr("Не удалось найти подходящую версию Java для Minecraft %1.\n\n%2\n\nУкажите путь к Java в настройках."))
                .arg(version, javaResult.warning)
        );
        return;
    }

    if (!javaResult.warning.isEmpty()) {
        LOG_WARN(QStringLiteral("Java selection warning: ") + javaResult.warning);
    }

    LaunchOptions opt;
    opt.username = username;
    opt.uuid = Utils::generateOfflinePlayerUuid(username);
    opt.versionId = version;
    opt.gameDir = Settings::instance().effectiveGameDir();
    opt.javaExecutable = javaResult.javaPath;
    opt.memoryMb = Settings::instance().memory();
    opt.customJvmArgs = Settings::instance().jvmArgs();
    opt.fullscreen = Settings::instance().fullscreen();
    opt.customResolution = true;
    opt.resolutionWidth = Settings::instance().resolutionWidth();
    opt.resolutionHeight = Settings::instance().resolutionHeight();
    opt.elyBySkins = Settings::instance().elyBySkins();
    opt.launcherName = Constants::LAUNCHER_NAME;
    opt.launcherVersion = Constants::LAUNCHER_VERSION;

    if (Settings::instance().showConsole()) {
        m_consoleWindow->show();
        m_consoleWindow->raise();
        m_consoleWindow->activateWindow();
    }

    m_statusLabel->setText(tr("Запуск Minecraft..."));
    m_statusLabel->show();

    PlaytimeTracker::instance().startSession(version);

    QString launchErr;
    if (!m_gameLauncher.launch(opt, &launchErr)) {
        PlaytimeTracker::instance().stopSession();
        m_statusLabel->hide();
        QMessageBox::critical(this, tr("Ошибка запуска Minecraft"), launchErr);
        return;
    }

    m_isGameRunning = true;
    updateActionButtonState();
}

void MainWindow::onCancelDownloadClicked()
{
    if (m_isDownloading) {
        m_downloader.cancel();
        m_isDownloading = false;
        m_progressBar->hide();
        m_statusLabel->setText(tr("Загрузка отменена"));
        updateActionButtonState();
    }
}

void MainWindow::onRefreshVersionsClicked()
{
    loadVersions(true);
}

void MainWindow::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_progressBar->setValue(percent);
    }
}

void MainWindow::onDownloadPercent(int percent)
{
    m_progressBar->setValue(percent);
}

void MainWindow::onDownloadStatusChanged(const QString &status)
{
    m_statusLabel->setText(status);
}

void MainWindow::onDownloadFinished()
{
    m_isDownloading = false;
    m_progressBar->hide();
    m_statusLabel->setText(tr("✓ Установка успешно завершена!"));
    updateActionButtonState();

    LOG_INFO(QStringLiteral("Version installation finished successfully."));
    QMessageBox::information(this, tr("Установка завершена"), tr("Версия успешно скачана и установлена!"));
}

void MainWindow::onDownloadError(const QString &errorMessage)
{
    m_isDownloading = false;
    m_progressBar->hide();
    m_statusLabel->setText(tr("Ошибка установки"));
    updateActionButtonState();

    LOG_ERROR(QStringLiteral("Downloader error: ") + errorMessage);
    QMessageBox::critical(this, tr("Ошибка скачивания"), errorMessage);
}

void MainWindow::onGameStarted()
{
    m_statusLabel->setText(tr("Minecraft запущен!"));
    LOG_INFO(QStringLiteral("Minecraft process started successfully."));
}

void MainWindow::onGameExited(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_isGameRunning = false;
    PlaytimeTracker::instance().stopSession();
    updateActionButtonState();

    m_statusLabel->setText(tr("Minecraft закрыт (код %1)").arg(exitCode));
    LOG_INFO(QStringLiteral("Minecraft process exited with code ") + QString::number(exitCode));

    if (exitCode != 0 && exitStatus == QProcess::NormalExit) {
        LOG_WARN(QStringLiteral("Minecraft exited with non-zero code ") + QString::number(exitCode));
        if (!m_consoleWindow->isVisible()) {
            auto reply = QMessageBox::question(
                this,
                tr("Игра завершилась с ошибкой"),
                QString(tr("Minecraft завершился с кодом ошибки %1.\nОткрыть консоль отладки для просмотра логов?")).arg(exitCode),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes
            );
            if (reply == QMessageBox::Yes) {
                m_consoleWindow->show();
                m_consoleWindow->raise();
            }
        }
    }
}

void MainWindow::onGameLogReceived(const QString &line)
{
    m_consoleWindow->appendLog(line);
}

void MainWindow::onGameLaunchError(const QString &errorMessage)
{
    m_isGameRunning = false;
    PlaytimeTracker::instance().stopSession();
    updateActionButtonState();

    m_statusLabel->hide();
    LOG_ERROR(QStringLiteral("Minecraft launch error: ") + errorMessage);
    QMessageBox::critical(this, tr("Ошибка запуска"), errorMessage);
}

void MainWindow::onOpenSettingsClicked()
{
    QString curVer = m_versionCombo->currentData().toString();
    SettingsDialog dlg(this, curVer);
    if (dlg.exec() == QDialog::Accepted) {
        loadBackground();
        loadVersions(false);
    }
}

void MainWindow::onOpenConsoleClicked()
{
    if (m_consoleWindow->isVisible()) {
        m_consoleWindow->hide();
    } else {
        m_consoleWindow->show();
        m_consoleWindow->raise();
        m_consoleWindow->activateWindow();
    }
}

void MainWindow::onOpenFolderClicked()
{
    QString mcDir = Settings::instance().effectiveGameDir();
    Utils::ensureDirectoryExists(mcDir);
    Utils::openInFileManager(mcDir);
}

void MainWindow::onDeleteVersionClicked()
{
    QString version = m_versionCombo->currentData().toString();
    if (version.isEmpty()) {
        version = m_versionCombo->currentText().trimmed();
    }

    if (!isVersionInstalled(version)) {
        QMessageBox::information(this, tr("Удаление версии"), tr("Данная версия не установлена на компьютере."));
        return;
    }

    auto reply = QMessageBox::question(
        this,
        tr("Удаление версии"),
        QString(tr("Вы уверены, что хотите удалить версию %1 с компьютера?")).arg(version),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        QString verDir = Constants::defaultVersionsDirectory() + QStringLiteral("/") + version;
        QDir dir(verDir);
        if (dir.exists()) {
            dir.removeRecursively();
        }
        updateActionButtonState();
        QMessageBox::information(this, tr("Удаление"), tr("Версия успешно удалена!"));
    }
}

void MainWindow::onOpenGitHubClicked()
{
    QDesktopServices::openUrl(QUrl(Constants::GITHUB_REPO_URL));
}
