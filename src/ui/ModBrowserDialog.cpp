#include "ModBrowserDialog.h"
#include "../core/Constants.h"
#include "../core/Settings.h"
#include "../core/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QIcon>
#include <QLocale>

namespace {

class ModCardWidget : public QWidget {
public:
    ModCardWidget(const ModSearchResult &res, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(12);

        auto *infoLayout = new QVBoxLayout();
        infoLayout->setSpacing(4);

        // Title and author
        auto *titleLabel = new QLabel(this);
        QString authorStr = res.author.isEmpty() ? QString() : QString(tr(" <span style='color: #94a3b8; font-weight: normal; font-size: 11px;'>от %1</span>")).arg(res.author);
        titleLabel->setText(QStringLiteral("<b style='color: #38bdf8; font-size: 14px;'>") + res.title.toHtmlEscaped() + QStringLiteral("</b>") + authorStr);
        titleLabel->setTextFormat(Qt::RichText);
        infoLayout->addWidget(titleLabel);

        // Description
        auto *descLabel = new QLabel(res.description, this);
        descLabel->setWordWrap(true);
        descLabel->setStyleSheet(QStringLiteral("color: #cbd5e1; font-size: 11px;"));
        infoLayout->addWidget(descLabel);

        // Stats & Categories
        auto *metaLayout = new QHBoxLayout();
        metaLayout->setSpacing(8);

        QLocale locale(QLocale::Russian);
        QString dlCount = locale.toString(res.downloads);
        auto *dlLabel = new QLabel(QString(tr("<span style='color: #4ade80; font-size: 11px;'>⬇ %1 скачиваний</span>")).arg(dlCount), this);
        dlLabel->setTextFormat(Qt::RichText);
        metaLayout->addWidget(dlLabel);

        if (!res.categories.isEmpty()) {
            QString cats = res.categories.join(QStringLiteral(", "));
            auto *catLabel = new QLabel(QString(tr("<span style='color: #94a3b8; font-size: 10px;'>[%1]</span>")).arg(cats), this);
            catLabel->setTextFormat(Qt::RichText);
            metaLayout->addWidget(catLabel);
        }

        metaLayout->addStretch();
        infoLayout->addLayout(metaLayout);

        layout->addLayout(infoLayout, 1);

        // Install Button
        installButton = new QPushButton(tr("Скачать"), this);
        installButton->setFixedSize(92, 38);
        installButton->setStyleSheet(
            "QPushButton {"
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #10b981, stop:1 #059669);"
            "  border: 1px solid rgba(255, 255, 255, 0.2);"
            "  border-radius: 8px;"
            "  color: #ffffff;"
            "  font-weight: bold;"
            "  font-size: 12px;"
            "}"
            "QPushButton:hover {"
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #34d399, stop:1 #10b981);"
            "}"
            "QPushButton:disabled {"
            "  background: rgba(255, 255, 255, 0.1);"
            "  border: 1px solid rgba(255, 255, 255, 0.08);"
            "  color: #94a3b8;"
            "}"
        );
        layout->addWidget(installButton, 0, Qt::AlignVCenter);
    }

    QPushButton *installButton{nullptr};
};

} // namespace

ModBrowserDialog::ModBrowserDialog(QWidget *parent,
                                   const QString &mcVersion,
                                   const QString &gameDir)
    : QDialog(parent)
    , m_mcVersion(mcVersion)
    , m_gameDir(gameDir.isEmpty() ? Settings::instance().effectiveGameDir() : gameDir)
{
    setupUi();

    // Connect ModBrowser signals
    connect(&m_browser, &ModBrowser::searchStarted, this, &ModBrowserDialog::onSearchStarted);
    connect(&m_browser, &ModBrowser::searchFinished, this, &ModBrowserDialog::onSearchFinished);
    connect(&m_browser, &ModBrowser::searchError, this, &ModBrowserDialog::onSearchError);

    connect(&m_browser, &ModBrowser::downloadStarted, this, &ModBrowserDialog::onDownloadStarted);
    connect(&m_browser, &ModBrowser::downloadProgress, this, &ModBrowserDialog::onDownloadProgress);
    connect(&m_browser, &ModBrowser::downloadFinished, this, &ModBrowserDialog::onDownloadFinished);
    connect(&m_browser, &ModBrowser::downloadError, this, &ModBrowserDialog::onDownloadError);
    connect(&m_browser, &ModBrowser::statusMessage, this, &ModBrowserDialog::onStatusMessage);

    // Initial search
    onSearchClicked();
}

QString ModBrowserDialog::cleanMinecraftVersion(const QString &ver) const
{
    static QRegularExpression re(QStringLiteral(R"(1\.\d+(\.\d+)?)"));
    auto match = re.match(ver);
    if (match.hasMatch()) {
        return match.captured(0);
    }
    return ver.trimmed();
}

void ModBrowserDialog::setupUi()
{
    setWindowTitle(tr("Каталог модов (Modrinth)"));
    resize(760, 560);
    setMinimumSize(680, 460);
    setWindowIcon(QIcon(":/mclovin.ico"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    // --- Top Search & Filter Bar ---
    auto *topLayout = new QHBoxLayout();
    topLayout->setSpacing(8);

    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(tr("🔍 Поиск модов (Sodium, Iris, JEI, JourneyMap...)..."));
    m_searchInput->setClearButtonEnabled(true);
    connect(m_searchInput, &QLineEdit::returnPressed, this, &ModBrowserDialog::onSearchClicked);
    topLayout->addWidget(m_searchInput, 3);

    m_loaderCombo = new QComboBox(this);
    m_loaderCombo->addItem(tr("Все лоадеры"), QString());
    m_loaderCombo->addItem(QStringLiteral("Fabric"), QStringLiteral("fabric"));
    m_loaderCombo->addItem(QStringLiteral("Forge"), QStringLiteral("forge"));
    m_loaderCombo->addItem(QStringLiteral("NeoForge"), QStringLiteral("neoforge"));
    m_loaderCombo->addItem(QStringLiteral("Quilt"), QStringLiteral("quilt"));
    // Try to auto-select fabric if current version contains fabric
    if (m_mcVersion.contains(QStringLiteral("fabric"), Qt::CaseInsensitive)) {
        m_loaderCombo->setCurrentIndex(1);
    } else if (m_mcVersion.contains(QStringLiteral("forge"), Qt::CaseInsensitive)) {
        m_loaderCombo->setCurrentIndex(2);
    }
    connect(m_loaderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModBrowserDialog::onSearchClicked);
    topLayout->addWidget(m_loaderCombo, 1);

    m_versionEdit = new QLineEdit(this);
    m_versionEdit->setPlaceholderText(tr("Версия MC"));
    m_versionEdit->setFixedWidth(90);
    m_versionEdit->setText(cleanMinecraftVersion(m_mcVersion));
    connect(m_versionEdit, &QLineEdit::returnPressed, this, &ModBrowserDialog::onSearchClicked);
    topLayout->addWidget(m_versionEdit);

    m_searchButton = new QPushButton(tr("Найти"), this);
    m_searchButton->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b82f6, stop:1 #2563eb);"
        "  border: 1px solid rgba(255, 255, 255, 0.2);"
        "  border-radius: 8px;"
        "  color: #ffffff;"
        "  font-weight: bold;"
        "  padding: 6px 16px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #60a5fa, stop:1 #3b82f6);"
        "}"
    );
    connect(m_searchButton, &QPushButton::clicked, this, &ModBrowserDialog::onSearchClicked);
    topLayout->addWidget(m_searchButton);

    mainLayout->addLayout(topLayout);

    // --- Results List ---
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_listWidget->setStyleSheet(
        "QListWidget {"
        "  background-color: rgba(18, 20, 28, 0.75);"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 10px;"
        "}"
        "QListWidget::item {"
        "  border-bottom: 1px solid rgba(255, 255, 255, 0.06);"
        "  padding: 2px;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: rgba(255, 255, 255, 0.03);"
        "}"
    );
    mainLayout->addWidget(m_listWidget);

    // Progress Bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    // --- Bottom Status & Actions ---
    auto *bottomLayout = new QHBoxLayout();

    m_statusLabel = new QLabel(tr("Введите название мода и нажмите «Найти»"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px;"));
    bottomLayout->addWidget(m_statusLabel, 1);

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

void ModBrowserDialog::onSearchClicked()
{
    QString query = m_searchInput->text().trimmed();
    QString loader = m_loaderCombo->currentData().toString();
    QString ver = m_versionEdit->text().trimmed();

    m_browser.searchMods(query, ver, loader, 25, 0);
}

void ModBrowserDialog::onSearchStarted()
{
    m_searchButton->setEnabled(false);
    m_statusLabel->setText(tr("Поиск на Modrinth..."));
    m_progressBar->setRange(0, 0); // indeterminate
    m_progressBar->show();
}

void ModBrowserDialog::onSearchFinished(const QList<ModSearchResult> &results, int totalHits)
{
    m_searchButton->setEnabled(true);
    m_progressBar->hide();
    m_listWidget->clear();
    m_installingButtons.clear();

    if (results.isEmpty()) {
        m_statusLabel->setText(tr("Ничего не найдено по заданным критериям."));
        return;
    }

    m_statusLabel->setText(QString(tr("Найдено модов: %1 (всего в каталоге: %2)")).arg(results.size()).arg(totalHits));

    for (const auto &res : results) {
        auto *item = new QListWidgetItem(m_listWidget);
        auto *widget = new ModCardWidget(res, m_listWidget);

        connect(widget->installButton, &QPushButton::clicked, this, [this, id = res.projectId, title = res.title, btn = widget->installButton]() {
            onInstallRequested(id, title, btn);
        });

        item->setSizeHint(widget->sizeHint());
        m_listWidget->addItem(item);
        m_listWidget->setItemWidget(item, widget);
    }
}

void ModBrowserDialog::onSearchError(const QString &errorMessage)
{
    m_searchButton->setEnabled(true);
    m_progressBar->hide();
    m_statusLabel->setText(QString(tr("Ошибка поиска: %1")).arg(errorMessage));
}

void ModBrowserDialog::onInstallRequested(const QString &projectId, const QString &title, QPushButton *button)
{
    button->setEnabled(false);
    button->setText(tr("Загрузка..."));
    m_installingButtons[projectId] = button;

    QString loader = m_loaderCombo->currentData().toString();
    QString ver = m_versionEdit->text().trimmed();
    QString modsDir = m_gameDir + QStringLiteral("/mods");

    m_statusLabel->setText(QString(tr("Поиск совместимого файла для «%1»...")).arg(title));
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->show();

    m_browser.installMod(projectId, title, ver, loader, modsDir);
}

void ModBrowserDialog::onDownloadStarted(const QString &fileName)
{
    m_statusLabel->setText(QString(tr("Скачивание файла %1...")).arg(fileName));
    m_progressBar->setValue(10);
}

void ModBrowserDialog::onDownloadProgress(const QString &fileName, qint64 bytesReceived, qint64 bytesTotal, int percent)
{
    Q_UNUSED(fileName);
    Q_UNUSED(bytesReceived);
    Q_UNUSED(bytesTotal);
    m_progressBar->setValue(percent);
}

void ModBrowserDialog::onDownloadFinished(const QString &fileName, const QString &savedPath)
{
    Q_UNUSED(savedPath);
    m_progressBar->hide();
    m_statusLabel->setText(QString(tr("✅ Мод «%1» успешно установлен в папку mods!")).arg(fileName));

    for (auto it = m_installingButtons.begin(); it != m_installingButtons.end(); ++it) {
        QPushButton *btn = it.value();
        if (btn && btn->text() == tr("Загрузка...")) {
            btn->setText(tr("✓ Установлен"));
            btn->setStyleSheet(
                "QPushButton {"
                "  background-color: rgba(16, 185, 129, 0.25);"
                "  border: 1px solid #10b981;"
                "  border-radius: 8px;"
                "  color: #34d399;"
                "  font-weight: bold;"
                "  font-size: 11px;"
                "}"
            );
            btn->setEnabled(false);
            break;
        }
    }
}

void ModBrowserDialog::onDownloadError(const QString &fileName, const QString &errorMessage)
{
    Q_UNUSED(fileName);
    m_progressBar->hide();
    m_statusLabel->setText(QString(tr("❌ Ошибка установки: %1")).arg(errorMessage));

    for (auto it = m_installingButtons.begin(); it != m_installingButtons.end(); ++it) {
        QPushButton *btn = it.value();
        if (btn && btn->text() == tr("Загрузка...")) {
            btn->setText(tr("Скачать"));
            btn->setEnabled(true);
            break;
        }
    }
}

void ModBrowserDialog::onStatusMessage(const QString &message)
{
    m_statusLabel->setText(message);
}
