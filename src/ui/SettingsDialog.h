#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QComboBox>
#include <QStringList>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr, const QString &currentVersion = QString());
    ~SettingsDialog() override = default;

private slots:
    void onSaveClicked();
    void onBrowseJavaClicked();
    void onAutoJavaToggled(bool checked);
    void onChooseBackgroundClicked();
    void onResetBackgroundClicked();
    void onAutoRamClicked();
    void onMemorySliderChanged(int value);
    void onClearCacheClicked();
    void onBrowseGameDirClicked();
    void onResetGameDirClicked();

private:
    void setupUi();
    void loadCurrentSettings();
    void updateMemoryLabel(int mb);
    void updateBackgroundLabel();

    QString m_currentVersion;
    QString m_customBackgroundPath;

    // Tabs
    QTabWidget *m_tabWidget{nullptr};

    // Tab 1: General
    QComboBox *m_themeCombo{nullptr};
    QLineEdit *m_gameDirEdit{nullptr};
    QPushButton *m_browseGameDirButton{nullptr};
    QPushButton *m_resetGameDirButton{nullptr};
    QCheckBox *m_fullscreenCheck{nullptr};
    QSpinBox *m_resWidthSpin{nullptr};
    QSpinBox *m_resHeightSpin{nullptr};
    QLabel *m_bgLabel{nullptr};
    QPushButton *m_chooseBgButton{nullptr};
    QPushButton *m_resetBgButton{nullptr};
    QCheckBox *m_elySkinsCheck{nullptr};

    // Tab 2: Java & RAM
    QCheckBox *m_autoJavaCheck{nullptr};
    QLineEdit *m_javaPathEdit{nullptr};
    QPushButton *m_browseJavaButton{nullptr};
    QSlider *m_memorySlider{nullptr};
    QLabel *m_memoryValueLabel{nullptr};
    QPushButton *m_autoRamButton{nullptr};
    QLineEdit *m_jvmArgsEdit{nullptr};

    // Tab 3: Advanced
    QCheckBox *m_releaseCheck{nullptr};
    QCheckBox *m_snapshotCheck{nullptr};
    QCheckBox *m_oldBetaCheck{nullptr};
    QCheckBox *m_oldAlphaCheck{nullptr};
    QCheckBox *m_showConsoleCheck{nullptr};
    QCheckBox *m_validateNickCheck{nullptr};
    QLabel *m_playtimeLabel{nullptr};
    QPushButton *m_clearCacheButton{nullptr};

    // Action buttons
    QPushButton *m_saveButton{nullptr};
    QPushButton *m_cancelButton{nullptr};
};
