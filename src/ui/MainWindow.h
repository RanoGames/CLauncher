#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QFrame>
#include <QPixmap>
#include <QProcess>
#include <QSize>

#include "../mc/VersionManifest.h"
#include "../mc/Downloader.h"
#include "../mc/GameLauncher.h"
#include "ConsoleWindow.h"
#include "SettingsDialog.h"
#include "SkinPreviewWidget.h"
#include "ModManagerDialog.h"
#include "ModBrowserDialog.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onPlayOrDownloadClicked();
    void onCancelDownloadClicked();
    void onRefreshVersionsClicked();
    void onDeleteNicknameClicked();
    void onOpenSettingsClicked();
    void onOpenModsClicked();
    void onOpenModBrowserClicked();
    void onOpenConsoleClicked();
    void onOpenFolderClicked();
    void onDeleteVersionClicked();
    void onOpenGitHubClicked();

    // Manifest signals
    void onManifestLoaded(const QList<VersionEntry> &versions);
    void onManifestError(const QString &errorMessage);

    // Downloader signals
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadPercent(int percent);
    void onDownloadStatusChanged(const QString &status);
    void onDownloadFinished();
    void onDownloadError(const QString &errorMessage);

    // GameLauncher signals
    void onGameStarted();
    void onGameExited(int exitCode, QProcess::ExitStatus exitStatus);
    void onGameLogReceived(const QString &line);
    void onGameLaunchError(const QString &errorMessage);

    // Combo boxes
    void onVersionIndexChanged(int index);
    void onNicknameChanged();

private:
    void setupUi();
    void setupConnections();
    void loadBackground();
    void populateNicknames();
    void loadVersions(bool forceRefresh = false);
    void updateActionButtonState();
    bool isVersionInstalled(const QString &versionId) const;
    void saveCurrentNickname();

    // Central & Background
    QWidget *m_centralWidget{nullptr};
    QPixmap m_bgPixmap;
    QPixmap m_cachedScaledBg;
    QSize m_cachedBgSize;

    // Header / Badges
    QLabel *m_versionBadgeLabel{nullptr};

    // Center Progress & Status
    QLabel *m_statusLabel{nullptr};
    QProgressBar *m_progressBar{nullptr};

    // Dock Card
    QFrame *m_dockCard{nullptr};

    // Nickname & Skin
    QLabel *m_nicknameTitleLabel{nullptr};
    SkinPreviewWidget *m_skinPreview{nullptr};
    QComboBox *m_nicknameCombo{nullptr};
    QPushButton *m_deleteNickButton{nullptr};

    // Version
    QLabel *m_versionTitleLabel{nullptr};
    QComboBox *m_versionCombo{nullptr};
    QPushButton *m_refreshVersionsButton{nullptr};

    // Hero buttons
    QPushButton *m_playButton{nullptr};
    QPushButton *m_downloadButton{nullptr};
    QPushButton *m_cancelButton{nullptr};

    // Quick action pills
    QPushButton *m_settingsButton{nullptr};
    QPushButton *m_modsButton{nullptr};
    QPushButton *m_modBrowserButton{nullptr};
    QPushButton *m_folderButton{nullptr};
    QPushButton *m_consoleButton{nullptr};
    QPushButton *m_deleteVersionButton{nullptr};
    QPushButton *m_gitHubButton{nullptr};

    // Subsystems
    VersionManifest m_manifest;
    Downloader m_downloader;
    GameLauncher m_gameLauncher;
    ConsoleWindow *m_consoleWindow{nullptr};

    bool m_isDownloading{false};
    bool m_isGameRunning{false};
};
