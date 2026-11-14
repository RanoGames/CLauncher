#include "SettingsDialog.h"
#include "../core/Settings.h"
#include "../core/Constants.h"
#include "../core/Utils.h"
#include "../core/PlaytimeTracker.h"
#include "../core/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QFileDialog>
#include <QMessageBox>
#include <QIcon>
#include <QFileInfo>
#include <QDir>

SettingsDialog::SettingsDialog(QWidget *parent, const QString &currentVersion)
    : QDialog(parent)
    , m_currentVersion(currentVersion)
{
    setupUi();
    loadCurrentSettings();
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("Настройки CLauncher"));
    resize(600, 530);
    setMinimumSize(540, 480);
    setWindowIcon(QIcon(":/mclovin.ico"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(14);

    m_tabWidget = new QTabWidget(this);

    // ==========================================
    // Tab 1: General (Интерфейс и игра)
    // ==========================================
    auto *tabGeneral = new QWidget();
    auto *genLayout = new QVBoxLayout(tabGeneral);
    genLayout->setContentsMargins(16, 16, 16, 16);
    genLayout->setSpacing(12);

    // Background image
    auto *bgTitle = new QLabel(tr("<b>Пользовательский фон главного окна:</b>"), tabGeneral);
    genLayout->addWidget(bgTitle);

    auto *bgLayout = new QHBoxLayout();
    m_bgLabel = new QLabel(tr("Стандартный фон"), tabGeneral);
    m_bgLabel->setStyleSheet("color: #94a3b8; font-weight: 500;");
    m_chooseBgButton = new QPushButton(tr("Выбрать..."), tabGeneral);
    connect(m_chooseBgButton, &QPushButton::clicked, this, &SettingsDialog::onChooseBackgroundClicked);
    m_resetBgButton = new QPushButton(tr("Сбросить"), tabGeneral);
    connect(m_resetBgButton, &QPushButton::clicked, this, &SettingsDialog::onResetBackgroundClicked);

    bgLayout->addWidget(m_bgLabel, 2);
    bgLayout->addWidget(m_chooseBgButton);
    bgLayout->addWidget(m_resetBgButton);
    genLayout->addLayout(bgLayout);

    // Game screen resolution
    auto *resTitle = new QLabel(tr("<b>Разрешение экрана в игре:</b>"), tabGeneral);
    genLayout->addWidget(resTitle);

    m_fullscreenCheck = new QCheckBox(tr("Полноэкранный режим"), tabGeneral);
    genLayout->addWidget(m_fullscreenCheck);

    auto *resLayout = new QHBoxLayout();
    m_resWidthSpin = new QSpinBox(tabGeneral);
    m_resWidthSpin->setRange(640, 3840);
    m_resWidthSpin->setSuffix(" px");

    m_resHeightSpin = new QSpinBox(tabGeneral);
    m_resHeightSpin->setRange(480, 2160);
    m_resHeightSpin->setSuffix(" px");

    resLayout->addWidget(new QLabel(tr("Ширина:"), tabGeneral));
    resLayout->addWidget(m_resWidthSpin);
    resLayout->addSpacing(16);
    resLayout->addWidget(new QLabel(tr("Высота:"), tabGeneral));
    resLayout->addWidget(m_resHeightSpin);
    resLayout->addStretch();
    genLayout->addLayout(resLayout);

    // Skins
    m_elySkinsCheck = new QCheckBox(tr("Использовать скины Ely.by"), tabGeneral);
    genLayout->addWidget(m_elySkinsCheck);

    genLayout->addStretch();
    m_tabWidget->addTab(tabGeneral, tr("🎮 Основные"));

    // ==========================================
    // Tab 2: Java & RAM (Java и Память)
    // ==========================================
    auto *tabJava = new QWidget();
    auto *javaLayout = new QVBoxLayout(tabJava);
    javaLayout->setContentsMargins(16, 16, 16, 16);
    javaLayout->setSpacing(12);

    auto *javaTitle = new QLabel(tr("<b>Среда выполнения Java:</b>"), tabJava);
    javaLayout->addWidget(javaTitle);

    m_autoJavaCheck = new QCheckBox(tr("Автоматически выбирать подходящую Java"), tabJava);
    connect(m_autoJavaCheck, &QCheckBox::toggled, this, &SettingsDialog::onAutoJavaToggled);
    javaLayout->addWidget(m_autoJavaCheck);

    auto *javaPathLayout = new QHBoxLayout();
    m_javaPathEdit = new QLineEdit(tabJava);
    m_javaPathEdit->setPlaceholderText(tr("Путь к исполняемому файлу java / javaw"));
    m_browseJavaButton = new QPushButton(tr("Обзор..."), tabJava);
    connect(m_browseJavaButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseJavaClicked);
    javaPathLayout->addWidget(m_javaPathEdit, 3);
    javaPathLayout->addWidget(m_browseJavaButton);
    javaLayout->addLayout(javaPathLayout);

    // Separator
    auto *sep = new QFrame(tabJava);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: rgba(255, 255, 255, 0.1);");
    javaLayout->addWidget(sep);

    // RAM allocation
    auto *ramHeaderLayout = new QHBoxLayout();
    ramHeaderLayout->addWidget(new QLabel(tr("<b>Выделение оперативной памяти (RAM):</b>"), tabJava));
    ramHeaderLayout->addStretch();
    m_memoryValueLabel = new QLabel(tabJava);
    m_memoryValueLabel->setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 13px;");
    ramHeaderLayout->addWidget(m_memoryValueLabel);
    javaLayout->addLayout(ramHeaderLayout);

    m_memorySlider = new QSlider(Qt::Horizontal, tabJava);
    m_memorySlider->setMinimum(Constants::MIN_MEMORY_MB);
    quint64 totalSysMb = Utils::getTotalSystemMemoryMb();
    int maxMem = (totalSysMb > Constants::MIN_MEMORY_MB) ? static_cast<int>(totalSysMb) : 8192;
    m_memorySlider->setMaximum(maxMem);
    m_memorySlider->setSingleStep(256);
    m_memorySlider->setPageStep(1024);
    connect(m_memorySlider, &QSlider::valueChanged, this, &SettingsDialog::onMemorySliderChanged);
    javaLayout->addWidget(m_memorySlider);

    quint64 optMb = Utils::getOptimalRamMb();
    m_autoRamButton = new QPushButton(QString(tr("⚡ Авто (%1 МБ)")).arg(optMb), tabJava);
    connect(m_autoRamButton, &QPushButton::clicked, this, &SettingsDialog::onAutoRamClicked);
    javaLayout->addWidget(m_autoRamButton);

    // JVM Args
    auto *jvmTitle = new QLabel(tr("<b>Дополнительные аргументы JVM:</b>"), tabJava);
    javaLayout->addWidget(jvmTitle);
    m_jvmArgsEdit = new QLineEdit(tabJava);
    m_jvmArgsEdit->setPlaceholderText(tr("-XX:+UseG1GC -XX:-UseAdaptiveSizePolicy"));
    javaLayout->addWidget(m_jvmArgsEdit);

    javaLayout->addStretch();
    m_tabWidget->addTab(tabJava, tr("☕ Java и Память"));

    // ==========================================
    // Tab 3: Advanced (Дополнительно)
    // ==========================================
    auto *tabAdv = new QWidget();
    auto *advLayout = new QVBoxLayout(tabAdv);
    advLayout->setContentsMargins(16, 16, 16, 16);
    advLayout->setSpacing(12);

    auto *filterTitle = new QLabel(tr("<b>Отображаемые типы версий:</b>"), tabAdv);
    advLayout->addWidget(filterTitle);

    m_releaseCheck = new QCheckBox(tr("Релизы (Release)"), tabAdv);
    m_snapshotCheck = new QCheckBox(tr("Снапшоты (Snapshot)"), tabAdv);
    m_oldBetaCheck = new QCheckBox(tr("Старые бета-версии (Old Beta)"), tabAdv);
    m_oldAlphaCheck = new QCheckBox(tr("Старые альфа-версии (Old Alpha)"), tabAdv);

    advLayout->addWidget(m_releaseCheck);
    advLayout->addWidget(m_snapshotCheck);
    advLayout->addWidget(m_oldBetaCheck);
    advLayout->addWidget(m_oldAlphaCheck);

    auto *moreTitle = new QLabel(tr("<b>Дополнительно:</b>"), tabAdv);
    advLayout->addWidget(moreTitle);

    m_showConsoleCheck = new QCheckBox(tr("Открывать консоль при запуске игры"), tabAdv);
    m_validateNickCheck = new QCheckBox(tr("Проверять корректность никнейма"), tabAdv);
    advLayout->addWidget(m_showConsoleCheck);
    advLayout->addWidget(m_validateNickCheck);

    // Playtime tracker
    auto stats = PlaytimeTracker::instance().getPlaytimeStats(m_currentVersion);
    QString statText = QString(tr("<b>Общее время в игре:</b> <span style='color:#4ade80;'>%1</span>"))
                           .arg(stats.first);
    if (!m_currentVersion.isEmpty() && stats.second != QStringLiteral("0 мин.")) {
        statText += QString(tr(" <span style='color:#94a3b8; font-size:11px;'>(версия %1: %2)</span>"))
                        .arg(m_currentVersion, stats.second);
    }
    m_playtimeLabel = new QLabel(statText, tabAdv);
    advLayout->addWidget(m_playtimeLabel);

    // Clear Cache button
    m_clearCacheButton = new QPushButton(tr("🗑 Очистить кэш лаунчера"), tabAdv);
    m_clearCacheButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(239, 68, 68, 0.18);"
        "  border: 1px solid rgba(239, 68, 68, 0.45);"
        "  border-radius: 8px;"
        "  color: #fca5a5;"
        "  font-weight: 600;"
        "  padding: 8px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(239, 68, 68, 0.35);"
        "  color: #ffffff;"
        "}"
    );
    connect(m_clearCacheButton, &QPushButton::clicked, this, &SettingsDialog::onClearCacheClicked);
    advLayout->addWidget(m_clearCacheButton);

    advLayout->addStretch();
    m_tabWidget->addTab(tabAdv, tr("🛠 Дополнительно"));

    mainLayout->addWidget(m_tabWidget);

    // Bottom Action Buttons
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();

    m_cancelButton = new QPushButton(tr("Отмена"), this);
    m_cancelButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(255, 255, 255, 0.08);"
        "  border: 1px solid rgba(255, 255, 255, 0.18);"
        "  border-radius: 8px;"
        "  color: #cbd5e1;"
        "  padding: 8px 20px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255, 255, 255, 0.16);"
        "  color: #ffffff;"
        "}"
    );
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(m_cancelButton);

    m_saveButton = new QPushButton(tr("✓ Сохранить"), this);
    m_saveButton->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #059669);"
        "  border: 1px solid rgba(255, 255, 255, 0.25);"
        "  border-radius: 8px;"
        "  color: #ffffff;"
        "  padding: 8px 26px;"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #34d399, stop:1 #10b981);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #059669, stop:1 #047857);"
        "}"
    );
    connect(m_saveButton, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);
    bottomLayout->addWidget(m_saveButton);

    mainLayout->addLayout(bottomLayout);
}

void SettingsDialog::loadCurrentSettings()
{
    const auto &s = Settings::instance();

    m_fullscreenCheck->setChecked(s.fullscreen());
    m_resWidthSpin->setValue(s.resolutionWidth());
    m_resHeightSpin->setValue(s.resolutionHeight());

    m_customBackgroundPath = s.customBackground();
    updateBackgroundLabel();

    m_elySkinsCheck->setChecked(s.elyBySkins());

    m_autoJavaCheck->setChecked(s.autoJava());
    m_javaPathEdit->setText(s.javaPath());
    onAutoJavaToggled(s.autoJava());

    int mem = s.memory();
    if (mem < Constants::MIN_MEMORY_MB) mem = Constants::DEFAULT_MEMORY_MB;
    m_memorySlider->setValue(mem);
    updateMemoryLabel(mem);

    m_jvmArgsEdit->setText(s.jvmArgs());

    const QStringList f = s.filters();
    m_releaseCheck->setChecked(f.contains(QStringLiteral("release")));
    m_snapshotCheck->setChecked(f.contains(QStringLiteral("snapshot")));
    m_oldBetaCheck->setChecked(f.contains(QStringLiteral("old_beta")));
    m_oldAlphaCheck->setChecked(f.contains(QStringLiteral("old_alpha")));

    m_showConsoleCheck->setChecked(s.showConsole());
    m_validateNickCheck->setChecked(s.validateNickname());
}

void SettingsDialog::updateMemoryLabel(int mb)
{
    m_memoryValueLabel->setText(QString("%1 МБ (%2 ГБ)")
        .arg(mb)
        .arg(QString::number(mb / 1024.0, 'f', 1)));
}

void SettingsDialog::updateBackgroundLabel()
{
    if (m_customBackgroundPath.isEmpty() || !QFile::exists(m_customBackgroundPath)) {
        m_bgLabel->setText(tr("Стандартный фон"));
    } else {
        m_bgLabel->setText(QFileInfo(m_customBackgroundPath).fileName());
    }
}

void SettingsDialog::onMemorySliderChanged(int value)
{
    updateMemoryLabel(value);
}

void SettingsDialog::onAutoRamClicked()
{
    quint64 opt = Utils::getOptimalRamMb();
    m_memorySlider->setValue(static_cast<int>(opt));
}

void SettingsDialog::onAutoJavaToggled(bool checked)
{
    m_javaPathEdit->setEnabled(!checked);
    m_browseJavaButton->setEnabled(!checked);
}

void SettingsDialog::onBrowseJavaClicked()
{
#if defined(Q_OS_WIN)
    QString filter = tr("Java Executable (javaw.exe java.exe);;All Files (*.*)");
#else
    QString filter = tr("Java Executable (java);;All Files (*)");
#endif
    QString current = m_javaPathEdit->text().trimmed();
    QString startDir = current.isEmpty() ? QDir::rootPath() : QFileInfo(current).absolutePath();

    QString path = QFileDialog::getOpenFileName(this, tr("Выберите Java"), startDir, filter);
    if (!path.isEmpty()) {
        m_javaPathEdit->setText(path);
    }
}

void SettingsDialog::onChooseBackgroundClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Выберите фоновое изображение"),
        QDir::homePath(),
        tr("Изображения (*.png *.jpg *.jpeg *.bmp *.webp);;Все файлы (*.*)")
    );
    if (!path.isEmpty()) {
        m_customBackgroundPath = path;
        updateBackgroundLabel();
    }
}

void SettingsDialog::onResetBackgroundClicked()
{
    m_customBackgroundPath.clear();
    updateBackgroundLabel();
}

void SettingsDialog::onClearCacheClicked()
{
    auto reply = QMessageBox::question(
        this,
        tr("Очистка кэша"),
        tr("Вы действительно хотите удалить временные файлы и кэш лаунчера?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        QString cacheDir = Constants::defaultCacheDirectory();
        QDir dir(cacheDir);
        if (dir.exists()) {
            dir.removeRecursively();
            dir.mkpath(QStringLiteral("."));
        }
        QMessageBox::information(this, tr("Очистка кэша"), tr("Кэш успешно очищен!"));
    }
}

void SettingsDialog::onSaveClicked()
{
    auto &s = Settings::instance();

    s.setFullscreen(m_fullscreenCheck->isChecked());
    s.setResolution(m_resWidthSpin->value(), m_resHeightSpin->value());
    s.setCustomBackground(m_customBackgroundPath);
    s.setElyBySkins(m_elySkinsCheck->isChecked());

    s.setAutoJava(m_autoJavaCheck->isChecked());
    s.setJavaPath(m_javaPathEdit->text().trimmed());
    s.setMemory(m_memorySlider->value());
    s.setJvmArgs(m_jvmArgsEdit->text().trimmed());

    QStringList filters;
    if (m_releaseCheck->isChecked()) filters.append(QStringLiteral("release"));
    if (m_snapshotCheck->isChecked()) filters.append(QStringLiteral("snapshot"));
    if (m_oldBetaCheck->isChecked()) filters.append(QStringLiteral("old_beta"));
    if (m_oldAlphaCheck->isChecked()) filters.append(QStringLiteral("old_alpha"));
    s.setFilters(filters);

    s.setShowConsole(m_showConsoleCheck->isChecked());
    s.setValidateNickname(m_validateNickCheck->isChecked());

    s.save();
    accept();
}
