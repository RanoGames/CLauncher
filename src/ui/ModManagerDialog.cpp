#include "ModManagerDialog.h"
#include "../core/Constants.h"
#include "../core/Settings.h"
#include "../core/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QMimeData>
#include <QFileInfo>
#include <QIcon>

ModManagerDialog::ModManagerDialog(QWidget *parent, const QString &gameDir)
    : QDialog(parent)
{
    QString baseDir = gameDir.isEmpty() ? Settings::instance().effectiveGameDir() : gameDir;
    m_modManager.setModsDirectory(baseDir + QStringLiteral("/mods"));

    setAcceptDrops(true);
    setupUi();
    refreshMods();
}

void ModManagerDialog::setupUi()
{
    setWindowTitle(tr("Менеджер установленных модов"));
    resize(740, 520);
    setMinimumSize(640, 420);
    setWindowIcon(QIcon(":/mclovin.ico"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    // --- Top Toolbar ---
    auto *topLayout = new QHBoxLayout();
    topLayout->setSpacing(8);

    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(tr("🔍 Поиск по названию или файлу..."));
    m_searchInput->setClearButtonEnabled(true);
    connect(m_searchInput, &QLineEdit::textChanged, this, &ModManagerDialog::onSearchTextChanged);
    topLayout->addWidget(m_searchInput, 3);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem(tr("Все моды"), QStringLiteral("all"));
    m_filterCombo->addItem(tr("Только активные"), QStringLiteral("active"));
    m_filterCombo->addItem(tr("Только отключенные"), QStringLiteral("disabled"));
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModManagerDialog::onFilterComboChanged);
    topLayout->addWidget(m_filterCombo, 1);

    m_addModButton = new QPushButton(tr("➕ Добавить"), this);
    m_addModButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(16, 185, 129, 0.2);"
        "  border: 1px solid #10b981;"
        "  border-radius: 8px;"
        "  color: #34d399;"
        "  font-weight: 600;"
        "  padding: 6px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(16, 185, 129, 0.35);"
        "  color: #ffffff;"
        "}"
    );
    connect(m_addModButton, &QPushButton::clicked, this, &ModManagerDialog::onAddModClicked);
    topLayout->addWidget(m_addModButton);

    m_openFolderButton = new QPushButton(tr("📁 Папка"), this);
    m_openFolderButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(59, 130, 246, 0.18);"
        "  border: 1px solid #3b82f6;"
        "  border-radius: 8px;"
        "  color: #60a5fa;"
        "  font-weight: 600;"
        "  padding: 6px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(59, 130, 246, 0.32);"
        "  color: #ffffff;"
        "}"
    );
    connect(m_openFolderButton, &QPushButton::clicked, this, &ModManagerDialog::onOpenFolderClicked);
    topLayout->addWidget(m_openFolderButton);

    m_refreshButton = new QPushButton(QStringLiteral("🔄"), this);
    m_refreshButton->setToolTip(tr("Обновить список модов"));
    m_refreshButton->setFixedWidth(38);
    m_refreshButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(255, 255, 255, 0.08);"
        "  border: 1px solid rgba(255, 255, 255, 0.18);"
        "  border-radius: 8px;"
        "  padding: 6px;"
        "  font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255, 255, 255, 0.2);"
        "}"
    );
    connect(m_refreshButton, &QPushButton::clicked, this, &ModManagerDialog::refreshMods);
    topLayout->addWidget(m_refreshButton);

    mainLayout->addLayout(topLayout);

    // --- Stats & Batch Actions Bar ---
    auto *statsLayout = new QHBoxLayout();

    m_statsLabel = new QLabel(this);
    m_statsLabel->setStyleSheet("color: #94a3b8; font-size: 12px; font-weight: 600;");
    statsLayout->addWidget(m_statsLabel);

    statsLayout->addStretch();

    m_enableAllButton = new QPushButton(tr("✓ Включить все"), this);
    m_enableAllButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(255, 255, 255, 0.06);"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 6px;"
        "  color: #cbd5e1;"
        "  font-size: 11px;"
        "  padding: 4px 10px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 0.18); color: #ffffff; }"
    );
    connect(m_enableAllButton, &QPushButton::clicked, this, &ModManagerDialog::onEnableAllClicked);
    statsLayout->addWidget(m_enableAllButton);

    m_disableAllButton = new QPushButton(tr("✕ Отключить все"), this);
    m_disableAllButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(255, 255, 255, 0.06);"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 6px;"
        "  color: #cbd5e1;"
        "  font-size: 11px;"
        "  padding: 4px 10px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 0.18); color: #ffffff; }"
    );
    connect(m_disableAllButton, &QPushButton::clicked, this, &ModManagerDialog::onDisableAllClicked);
    statsLayout->addWidget(m_disableAllButton);

    mainLayout->addLayout(statsLayout);

    // --- Mod Table ---
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        tr("Вкл"),
        tr("Название / Файл"),
        tr("Размер"),
        tr("Удалить")
    });

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);

    m_table->setStyleSheet(
        "QTableWidget {"
        "  background-color: rgba(18, 20, 28, 0.75);"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 10px;"
        "  color: #f1f5f9;"
        "}"
        "QTableWidget::item {"
        "  padding: 6px 8px;"
        "  border-bottom: 1px solid rgba(255, 255, 255, 0.05);"
        "}"
        "QTableWidget::item:selected {"
        "  background-color: rgba(59, 130, 246, 0.2);"
        "}"
        "QHeaderView::section {"
        "  background-color: rgba(26, 32, 44, 0.85);"
        "  color: #94a3b8;"
        "  font-weight: bold;"
        "  border: none;"
        "  padding: 8px;"
        "}"
    );

    mainLayout->addWidget(m_table);

    // --- Bottom Row ---
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();

    m_closeButton = new QPushButton(tr("Закрыть"), this);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(255, 255, 255, 0.08);"
        "  border: 1px solid rgba(255, 255, 255, 0.18);"
        "  border-radius: 8px;"
        "  color: #cbd5e1;"
        "  padding: 7px 22px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 0.18); color: #ffffff; }"
    );
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_closeButton);

    mainLayout->addLayout(bottomLayout);
}

void ModManagerDialog::refreshMods()
{
    m_cachedMods = m_modManager.listMods();

    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    m_table->setRowCount(m_cachedMods.size());

    int activeCount = 0;
    int disabledCount = 0;

    for (int row = 0; row < m_cachedMods.size(); ++row) {
        const auto &mod = m_cachedMods.at(row);
        if (mod.enabled) {
            activeCount++;
        } else {
            disabledCount++;
        }

        // Col 0: Checkbox
        auto *cbWidget = new QWidget(this);
        auto *cbLayout = new QHBoxLayout(cbWidget);
        cbLayout->setContentsMargins(6, 0, 6, 0);
        cbLayout->setAlignment(Qt::AlignCenter);

        auto *cb = new QCheckBox(cbWidget);
        cb->setChecked(mod.enabled);
        connect(cb, &QCheckBox::toggled, this, [this, path = mod.filePath](bool checked) {
            onModToggled(path, checked);
        });
        cbLayout->addWidget(cb);
        m_table->setCellWidget(row, 0, cbWidget);

        // Col 1: Title & File
        QString titleText = mod.name;
        if (!mod.version.isEmpty()) {
            titleText += QStringLiteral(" ") + mod.version;
        }
        if (!mod.modLoader.isEmpty() && mod.modLoader != QStringLiteral("unknown")) {
            titleText += QStringLiteral(" [") + mod.modLoader + QStringLiteral("]");
        }
        titleText += QStringLiteral("  —  ") + mod.filename;

        auto *itemTitle = new QTableWidgetItem(titleText);
        itemTitle->setData(Qt::UserRole, mod.filePath);
        itemTitle->setData(Qt::UserRole + 1, mod.name);
        itemTitle->setData(Qt::UserRole + 2, mod.filename);
        itemTitle->setData(Qt::UserRole + 3, mod.enabled);

        QString tooltip = QString(tr("Файл: %1\nВерсия: %2\nЛоадер: %3\nПуть: %4"))
                              .arg(mod.filename,
                                   mod.version.isEmpty() ? QStringLiteral("-") : mod.version,
                                   mod.modLoader.isEmpty() ? QStringLiteral("-") : mod.modLoader,
                                   mod.filePath);
        if (!mod.description.isEmpty()) {
            tooltip += QStringLiteral("\n\n") + mod.description;
        }
        itemTitle->setToolTip(tooltip);

        if (!mod.enabled) {
            itemTitle->setForeground(QColor(148, 163, 184, 150));
        }
        m_table->setItem(row, 1, itemTitle);

        // Col 2: Size
        auto *itemSize = new QTableWidgetItem(ModManager::formatSize(mod.fileSize));
        itemSize->setTextAlignment(Qt::AlignCenter);
        itemSize->setForeground(QColor(148, 163, 184));
        m_table->setItem(row, 2, itemSize);

        // Col 3: Delete Button
        auto *actWidget = new QWidget(this);
        auto *actLayout = new QHBoxLayout(actWidget);
        actLayout->setContentsMargins(4, 0, 4, 0);
        actLayout->setAlignment(Qt::AlignCenter);

        auto *btnDel = new QPushButton(QStringLiteral("🗑"), actWidget);
        btnDel->setToolTip(tr("Удалить мод"));
        btnDel->setStyleSheet(
            "QPushButton {"
            "  background-color: transparent;"
            "  color: #ef4444;"
            "  font-size: 13px;"
            "  border: none;"
            "  padding: 3px 6px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(239, 68, 68, 0.2);"
            "  border-radius: 4px;"
            "}"
        );
        connect(btnDel, &QPushButton::clicked, this, [this, path = mod.filePath, name = mod.name]() {
            onDeleteModClicked(path, name);
        });
        actLayout->addWidget(btnDel);
        m_table->setCellWidget(row, 3, actWidget);
    }

    updateStats(m_cachedMods.size(), activeCount, disabledCount);
    applyFilter();
}

void ModManagerDialog::updateStats(int total, int active, int disabled)
{
    m_statsLabel->setText(
        QString(tr("📦 Всего модов: %1  (🟢 %2 активных, ⚪ %3 отключенных)"))
            .arg(total)
            .arg(active)
            .arg(disabled)
    );
}

void ModManagerDialog::onSearchTextChanged(const QString &text)
{
    Q_UNUSED(text);
    applyFilter();
}

void ModManagerDialog::onFilterComboChanged(int index)
{
    Q_UNUSED(index);
    applyFilter();
}

void ModManagerDialog::applyFilter()
{
    QString query = m_searchInput->text().trimmed().toLower();
    QString filterType = m_filterCombo->currentData().toString();

    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto *item = m_table->item(row, 1);
        if (!item) continue;

        QString title = item->data(Qt::UserRole + 1).toString().toLower();
        QString filename = item->data(Qt::UserRole + 2).toString().toLower();
        bool isEnabled = item->data(Qt::UserRole + 3).toBool();

        bool matchesQuery = query.isEmpty() || title.contains(query) || filename.contains(query);
        bool matchesFilter = true;

        if (filterType == QStringLiteral("active") && !isEnabled) {
            matchesFilter = false;
        } else if (filterType == QStringLiteral("disabled") && isEnabled) {
            matchesFilter = false;
        }

        m_table->setRowHidden(row, !(matchesQuery && matchesFilter));
    }
}

void ModManagerDialog::onAddModClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Выберите файлы модов для установки"),
        QString(),
        tr("Minecraft Mods (*.jar);;Все файлы (*.*)")
    );

    if (files.isEmpty()) return;

    int added = 0;
    for (const QString &file : files) {
        if (m_modManager.addModFile(file)) {
            added++;
        }
    }

    if (added > 0) {
        refreshMods();
    }
}

void ModManagerDialog::onOpenFolderClicked()
{
    QString dir = m_modManager.modsDirectory();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void ModManagerDialog::onEnableAllClicked()
{
    m_modManager.enableAllMods();
    refreshMods();
}

void ModManagerDialog::onDisableAllClicked()
{
    m_modManager.disableAllMods();
    refreshMods();
}

void ModManagerDialog::onModToggled(const QString &filePath, bool checked)
{
    m_modManager.toggleMod(filePath, checked);
    refreshMods();
}

void ModManagerDialog::onDeleteModClicked(const QString &filePath, const QString &displayName)
{
    auto reply = QMessageBox::question(
        this,
        tr("Удаление мода"),
        QString(tr("Вы действительно хотите удалить мод «%1»?")).arg(displayName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        if (m_modManager.deleteMod(filePath)) {
            refreshMods();
        } else {
            QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось удалить файл мода."));
        }
    }
}

void ModManagerDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        for (const QUrl &url : urls) {
            QString path = url.toLocalFile();
            if (path.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive) ||
                path.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    QDialog::dragEnterEvent(event);
}

void ModManagerDialog::dropEvent(QDropEvent *event)
{
    int added = 0;
    const auto urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        QString path = url.toLocalFile();
        if (path.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive) ||
            path.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive)) {
            if (m_modManager.addModFile(path)) {
                added++;
            }
        }
    }

    if (added > 0) {
        refreshMods();
        event->acceptProposedAction();
    } else {
        QDialog::dropEvent(event);
    }
}
