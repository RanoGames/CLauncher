#include "AccountManagerDialog.h"
#include "../core/Settings.h"
#include "../core/Utils.h"
#include "../core/Logger.h"
#include "../mc/SkinManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QIcon>
#include <QDesktopServices>
#include <QUrl>

AccountManagerDialog::AccountManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();

    connect(&AccountManager::instance(), &AccountManager::accountAdded,
            this, &AccountManagerDialog::refreshAccounts);
    connect(&AccountManager::instance(), &AccountManager::accountRemoved,
            this, &AccountManagerDialog::refreshAccounts);
    connect(&AccountManager::instance(), &AccountManager::activeAccountChanged,
            this, &AccountManagerDialog::refreshAccounts);

    connect(&SkinManager::instance(), &SkinManager::avatarLoaded,
            this, [this](const QString &username, const QPixmap &avatar) {
        if (!m_accountsList) return;
        for (int i = 0; i < m_accountsList->count(); ++i) {
            QListWidgetItem *it = m_accountsList->item(i);
            QString accId = it->data(Qt::UserRole).toString();
            Account a = AccountManager::instance().getAccount(accId);
            if (a.isValid() && a.username.compare(username, Qt::CaseInsensitive) == 0) {
                it->setIcon(QIcon(avatar.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            }
        }
    });

    refreshAccounts();
}

void AccountManagerDialog::setupUi()
{
    setWindowTitle(tr("Управление аккаунтами"));
    resize(780, 560);
    setMinimumSize(700, 480);
    setWindowIcon(QIcon(QStringLiteral(":/mclovin.ico")));

    setStyleSheet(
        "QDialog {"
        "  background-color: #141721;"
        "  color: #e2e8f0;"
        "}"
    );

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    // ==========================================
    // 1. Header Toolbar
    // ==========================================
    auto *headerLayout = new QVBoxLayout();
    headerLayout->setSpacing(2);

    m_headerTitle = new QLabel(tr("👥 Управление аккаунтами"), this);
    m_headerTitle->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 16px; font-weight: bold;"));
    headerLayout->addWidget(m_headerTitle);

    m_headerSubtitle = new QLabel(tr("Настройка учетных записей: Оффлайн, Ely.by и Microsoft (Xbox)"), this);
    m_headerSubtitle->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 11px;"));
    headerLayout->addWidget(m_headerSubtitle);

    mainLayout->addLayout(headerLayout);

    // ==========================================
    // 2. Center Two-Pane Body
    // ==========================================
    auto *panesLayout = new QHBoxLayout();
    panesLayout->setSpacing(14);

    // ------------------------------------------
    // LEFT PANE: Accounts List & Controls (270px)
    // ------------------------------------------
    auto *leftPaneWidget = new QWidget(this);
    leftPaneWidget->setFixedWidth(270);
    auto *leftLayout = new QVBoxLayout(leftPaneWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    m_leftHeaderLabel = new QLabel(tr("Сохранённые аккаунты:"), leftPaneWidget);
    m_leftHeaderLabel->setStyleSheet(QStringLiteral("color: #cbd5e1; font-size: 12px; font-weight: 600;"));
    leftLayout->addWidget(m_leftHeaderLabel);

    m_accountsList = new QListWidget(leftPaneWidget);
    m_accountsList->setIconSize(QSize(32, 32));
    m_accountsList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_accountsList->setStyleSheet(
        "QListWidget {"
        "  background-color: rgba(18, 20, 28, 0.9);"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 10px;"
        "  padding: 4px;"
        "}"
        "QListWidget::item {"
        "  padding: 8px 10px;"
        "  border-radius: 8px;"
        "  color: #f1f5f9;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  margin-bottom: 3px;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: rgba(255, 255, 255, 0.06);"
        "}"
        "QListWidget::item:selected {"
        "  background-color: rgba(16, 185, 129, 0.22);"
        "  border: 1px solid #10b981;"
        "  color: #ffffff;"
        "  font-weight: bold;"
        "}"
    );
    connect(m_accountsList, &QListWidget::itemSelectionChanged,
            this, &AccountManagerDialog::onAccountListSelectionChanged);
    leftLayout->addWidget(m_accountsList, 1);

    // Left pane buttons row 1: Add & Delete
    auto *leftBtnRow1 = new QHBoxLayout();
    leftBtnRow1->setSpacing(8);

    m_addBtn = new QPushButton(tr("➕ Добавить"), leftPaneWidget);
    m_addBtn->setCursor(Qt::PointingHandCursor);
    m_addBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(16, 185, 129, 0.18);"
        "  border: 1px solid #10b981;"
        "  border-radius: 7px;"
        "  color: #34d399;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  padding: 7px 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #10b981;"
        "  color: #ffffff;"
        "}"
    );
    connect(m_addBtn, &QPushButton::clicked, this, &AccountManagerDialog::onAddNewAccountClicked);
    leftBtnRow1->addWidget(m_addBtn);

    m_deleteBtn = new QPushButton(tr("🗑 Удалить"), leftPaneWidget);
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(239, 68, 68, 0.15);"
        "  border: 1px solid rgba(239, 68, 68, 0.5);"
        "  border-radius: 7px;"
        "  color: #f87171;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  padding: 7px 12px;"
        "}"
        "QPushButton:hover:enabled {"
        "  background-color: #ef4444;"
        "  color: #ffffff;"
        "}"
        "QPushButton:disabled {"
        "  background-color: rgba(255, 255, 255, 0.04);"
        "  border: 1px solid rgba(255, 255, 255, 0.08);"
        "  color: #64748b;"
        "}"
    );
    connect(m_deleteBtn, &QPushButton::clicked, this, &AccountManagerDialog::onDeleteAccountClicked);
    leftBtnRow1->addWidget(m_deleteBtn);

    leftLayout->addLayout(leftBtnRow1);

    // Left pane button row 2: Make Active
    m_makeActiveBtn = new QPushButton(tr("✓ Сделать активным"), leftPaneWidget);
    m_makeActiveBtn->setCursor(Qt::PointingHandCursor);
    m_makeActiveBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(14, 165, 233, 0.18);"
        "  border: 1px solid #0ea5e9;"
        "  border-radius: 7px;"
        "  color: #38bdf8;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  padding: 8px 12px;"
        "}"
        "QPushButton:hover:enabled {"
        "  background-color: #0ea5e9;"
        "  color: #ffffff;"
        "}"
        "QPushButton:disabled {"
        "  background-color: rgba(255, 255, 255, 0.04);"
        "  border: 1px solid rgba(255, 255, 255, 0.08);"
        "  color: #64748b;"
        "}"
    );
    connect(m_makeActiveBtn, &QPushButton::clicked, this, &AccountManagerDialog::onMakeActiveClicked);
    leftLayout->addWidget(m_makeActiveBtn);

    panesLayout->addWidget(leftPaneWidget);

    // ------------------------------------------
    // RIGHT PANE: Account Configuration Panel
    // ------------------------------------------
    m_configPanel = new QFrame(this);
    m_configPanel->setObjectName(QStringLiteral("ConfigPanel"));
    m_configPanel->setStyleSheet(
        "QFrame#ConfigPanel {"
        "  background-color: rgba(20, 24, 36, 0.95);"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 12px;"
        "  padding: 14px;"
        "}"
    );

    auto *configLayout = new QVBoxLayout(m_configPanel);
    configLayout->setContentsMargins(14, 12, 14, 12);
    configLayout->setSpacing(12);

    m_panelTitleLabel = new QLabel(tr("Настройка аккаунта"), m_configPanel);
    m_panelTitleLabel->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 14px; font-weight: bold;"));
    configLayout->addWidget(m_panelTitleLabel);

    // Type Selector Radio Buttons
    auto *typeLabel = new QLabel(tr("Тип авторизации:"), m_configPanel);
    typeLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px; font-weight: 600;"));
    configLayout->addWidget(typeLabel);

    auto *radiosLayout = new QHBoxLayout();
    radiosLayout->setSpacing(10);

    m_typeGroup = new QButtonGroup(this);

    m_radioOffline = new QRadioButton(tr("👤 Без пароля"), m_configPanel);
    m_radioOffline->setChecked(true);
    m_radioElyBy = new QRadioButton(tr("🌐 Ely.by"), m_configPanel);
    m_radioMicrosoft = new QRadioButton(tr("🎮 Microsoft"), m_configPanel);

    const QString radioStyle = QStringLiteral(
        "QRadioButton {"
        "  color: #cbd5e1;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  spacing: 6px;"
        "}"
        "QRadioButton:hover {"
        "  color: #38bdf8;"
        "}"
        "QRadioButton::indicator {"
        "  width: 15px;"
        "  height: 15px;"
        "  border-radius: 7px;"
        "  border: 1px solid rgba(255, 255, 255, 0.35);"
        "  background-color: rgba(15, 17, 26, 0.8);"
        "}"
        "QRadioButton::indicator:checked {"
        "  background-color: #10b981;"
        "  border-color: #10b981;"
        "}"
    );

    m_radioOffline->setStyleSheet(radioStyle);
    m_radioElyBy->setStyleSheet(radioStyle);
    m_radioMicrosoft->setStyleSheet(radioStyle);

    m_typeGroup->addButton(m_radioOffline, 0);
    m_typeGroup->addButton(m_radioElyBy, 1);
    m_typeGroup->addButton(m_radioMicrosoft, 2);

    radiosLayout->addWidget(m_radioOffline);
    radiosLayout->addWidget(m_radioElyBy);
    radiosLayout->addWidget(m_radioMicrosoft);
    radiosLayout->addStretch(1);

    connect(m_radioOffline, &QRadioButton::toggled, this, &AccountManagerDialog::onTypeRadioToggled);
    connect(m_radioElyBy, &QRadioButton::toggled, this, &AccountManagerDialog::onTypeRadioToggled);
    connect(m_radioMicrosoft, &QRadioButton::toggled, this, &AccountManagerDialog::onTypeRadioToggled);

    configLayout->addLayout(radiosLayout);

    // Form Pages Stack
    m_typeStack = new QStackedWidget(m_configPanel);
    m_typeStack->setStyleSheet(QStringLiteral("background: transparent;"));

    const QString inputStyle = QStringLiteral(
        "QLineEdit {"
        "  background-color: rgba(15, 17, 26, 0.85);"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 7px;"
        "  color: #f1f5f9;"
        "  padding: 6px 12px;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid #10b981;"
        "}"
    );

    // 1. Page Offline
    m_offlinePage = new QWidget();
    auto *offlineLayout = new QVBoxLayout(m_offlinePage);
    offlineLayout->setContentsMargins(0, 4, 0, 4);
    offlineLayout->setSpacing(6);

    auto *offNickLabel = new QLabel(tr("Имя пользователя / Никнейм:"), m_offlinePage);
    offNickLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px; font-weight: 600;"));
    offlineLayout->addWidget(offNickLabel);

    m_offlineNickInput = new QLineEdit(m_offlinePage);
    m_offlineNickInput->setPlaceholderText(tr("Введите никнейм (3-16 симв.)..."));
    m_offlineNickInput->setClearButtonEnabled(true);
    m_offlineNickInput->setStyleSheet(inputStyle);
    auto *regexValidator = new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^[a-zA-Z0-9_]{3,16}$")), this);
    m_offlineNickInput->setValidator(regexValidator);
    connect(m_offlineNickInput, &QLineEdit::textChanged, this, &AccountManagerDialog::onInputTextChanged);
    connect(m_offlineNickInput, &QLineEdit::returnPressed, this, &AccountManagerDialog::onSaveAccountClicked);
    offlineLayout->addWidget(m_offlineNickInput);

    m_offlineHintLabel = new QLabel(tr("💡 Стандартный оффлайн режим. Не требует пароля. Скин подгружается по никнейму."), m_offlinePage);
    m_offlineHintLabel->setWordWrap(true);
    m_offlineHintLabel->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    offlineLayout->addWidget(m_offlineHintLabel);
    offlineLayout->addStretch(1);

    m_typeStack->addWidget(m_offlinePage);

    // 2. Page Ely.by
    m_elyPage = new QWidget();
    auto *elyLayout = new QVBoxLayout(m_elyPage);
    elyLayout->setContentsMargins(0, 4, 0, 4);
    elyLayout->setSpacing(6);

    auto *elyLoginLabel = new QLabel(tr("Электронная почта или логин Ely.by:"), m_elyPage);
    elyLoginLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px; font-weight: 600;"));
    elyLayout->addWidget(elyLoginLabel);

    m_elyLoginInput = new QLineEdit(m_elyPage);
    m_elyLoginInput->setPlaceholderText(tr("user@example.com или логин..."));
    m_elyLoginInput->setClearButtonEnabled(true);
    m_elyLoginInput->setStyleSheet(inputStyle);
    connect(m_elyLoginInput, &QLineEdit::textChanged, this, &AccountManagerDialog::onInputTextChanged);
    connect(m_elyLoginInput, &QLineEdit::returnPressed, this, &AccountManagerDialog::onSaveAccountClicked);
    elyLayout->addWidget(m_elyLoginInput);

    auto *elyPassLabel = new QLabel(tr("Пароль учетной записи Ely.by:"), m_elyPage);
    elyPassLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px; font-weight: 600;"));
    elyLayout->addWidget(elyPassLabel);

    m_elyPasswordInput = new QLineEdit(m_elyPage);
    m_elyPasswordInput->setEchoMode(QLineEdit::Password);
    m_elyPasswordInput->setPlaceholderText(tr("Пароль аккаунта..."));
    m_elyPasswordInput->setClearButtonEnabled(true);
    m_elyPasswordInput->setStyleSheet(inputStyle);
    connect(m_elyPasswordInput, &QLineEdit::returnPressed, this, &AccountManagerDialog::onSaveAccountClicked);
    elyLayout->addWidget(m_elyPasswordInput);

    m_elySkinsCheck = new QCheckBox(tr("Использовать систему скинов Ely.by"), m_elyPage);
    m_elySkinsCheck->setChecked(true);
    m_elySkinsCheck->setStyleSheet(
        "QCheckBox {"
        "  color: #38bdf8;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  spacing: 6px;"
        "}"
    );
    elyLayout->addWidget(m_elySkinsCheck);

    m_elyHintLabel = new QLabel(tr("💡 Авторизация через Ely.by позволяет отображать скины и плащи в мультиплеере."), m_elyPage);
    m_elyHintLabel->setWordWrap(true);
    m_elyHintLabel->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    elyLayout->addWidget(m_elyHintLabel);
    elyLayout->addStretch(1);

    m_typeStack->addWidget(m_elyPage);

    // 3. Page Microsoft
    m_msPage = new QWidget();
    auto *msLayout = new QVBoxLayout(m_msPage);
    msLayout->setContentsMargins(0, 4, 0, 4);
    msLayout->setSpacing(6);

    auto *msEmailLabel = new QLabel(tr("Электронная почта / Геймертег Microsoft:"), m_msPage);
    msEmailLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px; font-weight: 600;"));
    msLayout->addWidget(msEmailLabel);

    m_msEmailInput = new QLineEdit(m_msPage);
    m_msEmailInput->setPlaceholderText(tr("user@outlook.com или геймертег..."));
    m_msEmailInput->setClearButtonEnabled(true);
    m_msEmailInput->setStyleSheet(inputStyle);
    connect(m_msEmailInput, &QLineEdit::textChanged, this, &AccountManagerDialog::onInputTextChanged);
    connect(m_msEmailInput, &QLineEdit::returnPressed, this, &AccountManagerDialog::onSaveAccountClicked);
    msLayout->addWidget(m_msEmailInput);

    m_msLoginBtn = new QPushButton(tr("🔑 Авторизоваться через браузер (Microsoft OAuth)"), m_msPage);
    m_msLoginBtn->setCursor(Qt::PointingHandCursor);
    m_msLoginBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(37, 99, 235, 0.2);"
        "  border: 1px solid #3b82f6;"
        "  border-radius: 7px;"
        "  color: #60a5fa;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  padding: 8px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "}"
    );
    connect(m_msLoginBtn, &QPushButton::clicked, this, &AccountManagerDialog::onMicrosoftLoginClicked);
    msLayout->addWidget(m_msLoginBtn);

    m_msStatusLabel = new QLabel(tr("Статус: Готов к авторизации"), m_msPage);
    m_msStatusLabel->setStyleSheet(QStringLiteral("color: #10b981; font-size: 11px; font-weight: 600;"));
    msLayout->addWidget(m_msStatusLabel);

    m_msHintLabel = new QLabel(tr("💡 Официальная лицензия Minecraft для доступа к лицензионным серверам."), m_msPage);
    m_msHintLabel->setWordWrap(true);
    m_msHintLabel->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    msLayout->addWidget(m_msHintLabel);
    msLayout->addStretch(1);

    m_typeStack->addWidget(m_msPage);

    configLayout->addWidget(m_typeStack, 1);

    // Live Skin Preview Card
    m_previewCard = new QFrame(m_configPanel);
    m_previewCard->setStyleSheet(
        "QFrame {"
        "  background-color: rgba(15, 17, 26, 0.65);"
        "  border: 1px solid rgba(255, 255, 255, 0.08);"
        "  border-radius: 10px;"
        "  padding: 8px;"
        "}"
    );
    auto *previewCardLayout = new QHBoxLayout(m_previewCard);
    previewCardLayout->setContentsMargins(8, 6, 8, 6);
    previewCardLayout->setSpacing(12);

    m_livePreview = new SkinPreviewWidget(m_previewCard);
    m_livePreview->setFixedSize(48, 48);
    previewCardLayout->addWidget(m_livePreview, 0, Qt::AlignVCenter);

    auto *previewInfoLayout = new QVBoxLayout();
    previewInfoLayout->setSpacing(2);

    m_previewNickLabel = new QLabel(tr("Игрок: Player"), m_previewCard);
    m_previewNickLabel->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 13px; font-weight: bold;"));
    previewInfoLayout->addWidget(m_previewNickLabel);

    m_previewSubLabel = new QLabel(tr("Предпросмотр скина головы игрока"), m_previewCard);
    m_previewSubLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 11px;"));
    previewInfoLayout->addWidget(m_previewSubLabel);

    previewCardLayout->addLayout(previewInfoLayout, 1);
    configLayout->addWidget(m_previewCard);

    // Right Pane Action Buttons (Save & Cancel)
    auto *configBtnLayout = new QHBoxLayout();
    configBtnLayout->setSpacing(10);

    m_saveBtn = new QPushButton(tr("💾 Сохранить аккаунт"), m_configPanel);
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    m_saveBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #10b981;"
        "  border: 1px solid #10b981;"
        "  border-radius: 8px;"
        "  color: #ffffff;"
        "  font-weight: bold;"
        "  font-size: 13px;"
        "  padding: 8px 18px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #059669;"
        "}"
    );
    connect(m_saveBtn, &QPushButton::clicked, this, &AccountManagerDialog::onSaveAccountClicked);
    configBtnLayout->addWidget(m_saveBtn);

    m_cancelBtn = new QPushButton(tr("✕ Отмена"), m_configPanel);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(255, 255, 255, 0.08);"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 8px;"
        "  color: #cbd5e1;"
        "  font-size: 12px;"
        "  padding: 8px 16px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255, 255, 255, 0.16);"
        "  color: #ffffff;"
        "}"
    );
    connect(m_cancelBtn, &QPushButton::clicked, this, &AccountManagerDialog::onCancelEditClicked);
    configBtnLayout->addWidget(m_cancelBtn);

    configLayout->addLayout(configBtnLayout);

    panesLayout->addWidget(m_configPanel, 1);
    mainLayout->addLayout(panesLayout, 1);

    // ==========================================
    // 3. Bottom Bar
    // ==========================================
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(10);

    auto *bottomNote = new QLabel(tr("💡 Выбранный активный аккаунт используется при запуске Minecraft"), this);
    bottomNote->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    bottomLayout->addWidget(bottomNote);

    bottomLayout->addStretch(1);

    m_closeBtn = new QPushButton(tr("Закрыть"), this);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(255, 255, 255, 0.08);"
        "  border: 1px solid rgba(255, 255, 255, 0.18);"
        "  border-radius: 8px;"
        "  color: #f1f5f9;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  padding: 8px 24px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255, 255, 255, 0.18);"
        "}"
    );
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(bottomLayout);
}

void AccountManagerDialog::refreshAccounts()
{
    m_accountsList->blockSignals(true);
    m_accountsList->clear();

    const QList<Account> accounts = AccountManager::instance().accounts();
    const Account active = AccountManager::instance().activeAccount();

    int activeRow = -1;
    for (int i = 0; i < accounts.size(); ++i) {
        const Account &acc = accounts[i];
        auto *item = new QListWidgetItem();
        QPixmap avatar = SkinManager::instance().getAvatar(acc.username, 32);
        item->setIcon(QIcon(avatar));

        QString typeBadge;
        if (acc.type.compare(QStringLiteral("elyby"), Qt::CaseInsensitive) == 0) {
            typeBadge = QStringLiteral("[Ely.by]");
        } else if (acc.type.compare(QStringLiteral("microsoft"), Qt::CaseInsensitive) == 0) {
            typeBadge = QStringLiteral("[MS]");
        } else {
            typeBadge = QStringLiteral("[Оффлайн]");
        }

        QString itemText = acc.username + QStringLiteral(" ") + typeBadge;
        if (active.isValid() && acc.id == active.id) {
            itemText += QStringLiteral("  ✓");
            activeRow = i;
        }

        item->setText(itemText);
        item->setData(Qt::UserRole, acc.id);
        m_accountsList->addItem(item);
    }

    m_accountsList->blockSignals(false);

    if (m_accountsList->count() > 0) {
        int rowToSelect = activeRow >= 0 ? activeRow : 0;
        m_accountsList->setCurrentRow(rowToSelect);
    } else {
        resetFormToNew();
    }
}

void AccountManagerDialog::onAccountListSelectionChanged()
{
    QListWidgetItem *cur = m_accountsList->currentItem();
    if (!cur) {
        m_deleteBtn->setEnabled(false);
        m_makeActiveBtn->setEnabled(false);
        return;
    }

    QString accId = cur->data(Qt::UserRole).toString();
    Account acc = AccountManager::instance().getAccount(accId);
    if (!acc.isValid()) return;

    m_selectedAccountId = acc.id;
    m_isCreatingNew = false;
    populateAccountDetails(acc);

    Account active = AccountManager::instance().activeAccount();
    bool isActive = (active.isValid() && active.id == acc.id);
    m_makeActiveBtn->setEnabled(!isActive);
    m_makeActiveBtn->setText(isActive ? tr("✓ Уже активен") : tr("✓ Сделать активным"));
    m_deleteBtn->setEnabled(true);
}

void AccountManagerDialog::populateAccountDetails(const Account &acc)
{
    m_panelTitleLabel->setText(tr("⚙ Настройка аккаунта: %1").arg(acc.username));

    m_typeGroup->blockSignals(true);
    if (acc.type.compare(QStringLiteral("elyby"), Qt::CaseInsensitive) == 0) {
        m_radioElyBy->setChecked(true);
        m_typeStack->setCurrentIndex(1);
        m_elyLoginInput->setText(acc.email.isEmpty() ? acc.username : acc.email);
        m_elyPasswordInput->setText(acc.password);
        m_elySkinsCheck->setChecked(acc.useElySkins);
    } else if (acc.type.compare(QStringLiteral("microsoft"), Qt::CaseInsensitive) == 0) {
        m_radioMicrosoft->setChecked(true);
        m_typeStack->setCurrentIndex(2);
        m_msEmailInput->setText(acc.email.isEmpty() ? acc.username : acc.email);
    } else {
        m_radioOffline->setChecked(true);
        m_typeStack->setCurrentIndex(0);
        m_offlineNickInput->setText(acc.username);
    }
    m_typeGroup->blockSignals(false);

    updateLiveSkinPreview();
}

void AccountManagerDialog::resetFormToNew()
{
    m_selectedAccountId.clear();
    m_isCreatingNew = true;
    m_panelTitleLabel->setText(tr("➕ Создание нового аккаунта"));

    m_typeGroup->blockSignals(true);
    m_radioOffline->setChecked(true);
    m_typeStack->setCurrentIndex(0);
    m_typeGroup->blockSignals(false);

    m_offlineNickInput->clear();
    m_elyLoginInput->clear();
    m_elyPasswordInput->clear();
    m_elySkinsCheck->setChecked(true);
    m_msEmailInput->clear();

    m_makeActiveBtn->setEnabled(false);
    m_makeActiveBtn->setText(tr("✓ Сделать активным"));
    m_deleteBtn->setEnabled(false);

    updateLiveSkinPreview();
    m_offlineNickInput->setFocus();
}

void AccountManagerDialog::onAddNewAccountClicked()
{
    m_accountsList->blockSignals(true);
    m_accountsList->clearSelection();
    m_accountsList->blockSignals(false);

    resetFormToNew();
}

void AccountManagerDialog::onDeleteAccountClicked()
{
    if (m_selectedAccountId.isEmpty()) return;

    Account acc = AccountManager::instance().getAccount(m_selectedAccountId);
    if (!acc.isValid()) return;

    auto reply = QMessageBox::question(
        this,
        tr("Удаление аккаунта"),
        tr("Вы действительно хотите удалить аккаунт \"%1\"?").arg(acc.username),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        AccountManager::instance().removeAccount(m_selectedAccountId);
        AccountManager::instance().save();
        m_selectedAccountId.clear();
        refreshAccounts();
    }
}

void AccountManagerDialog::onMakeActiveClicked()
{
    if (m_selectedAccountId.isEmpty()) return;

    AccountManager::instance().setActiveAccount(m_selectedAccountId);
    AccountManager::instance().save();
    refreshAccounts();
}

void AccountManagerDialog::onTypeRadioToggled()
{
    if (m_radioOffline->isChecked()) {
        m_typeStack->setCurrentIndex(0);
    } else if (m_radioElyBy->isChecked()) {
        m_typeStack->setCurrentIndex(1);
    } else if (m_radioMicrosoft->isChecked()) {
        m_typeStack->setCurrentIndex(2);
    }
    updateLiveSkinPreview();
}

void AccountManagerDialog::onInputTextChanged()
{
    updateLiveSkinPreview();
}

void AccountManagerDialog::onMicrosoftLoginClicked()
{
    m_msStatusLabel->setText(tr("Статус: Открытие страницы авторизации Microsoft..."));
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://login.live.com/oauth20_authorize.srf?client_id=00000000402b5328&response_type=code&scope=service::user.auth.xboxlive.com::MBI_SSL&redirect_uri=https://login.live.com/oauth20_desktop.srf")));
    QMessageBox::information(this, tr("Авторизация Microsoft"),
                             tr("Браузер открыт для авторизации Microsoft.\nВведите ваш логин или email в поле выше и нажмите «Сохранить аккаунт»."));
    m_msStatusLabel->setText(tr("Статус: Готов к сохранению"));
}

QString AccountManagerDialog::getEffectiveUsernameFromInputs() const
{
    if (m_radioElyBy->isChecked()) {
        QString val = m_elyLoginInput->text().trimmed();
        if (val.contains(QLatin1Char('@'))) {
            val = val.split(QLatin1Char('@')).first().trimmed();
        }
        return val.isEmpty() ? QStringLiteral("ElyUser") : val;
    } else if (m_radioMicrosoft->isChecked()) {
        QString val = m_msEmailInput->text().trimmed();
        if (val.contains(QLatin1Char('@'))) {
            val = val.split(QLatin1Char('@')).first().trimmed();
        }
        return val.isEmpty() ? QStringLiteral("Player") : val;
    } else {
        QString val = m_offlineNickInput->text().trimmed();
        return val.isEmpty() ? QStringLiteral("Steve") : val;
    }
}

void AccountManagerDialog::updateLiveSkinPreview()
{
    QString name = getEffectiveUsernameFromInputs();
    m_livePreview->setUsername(name);
    m_previewNickLabel->setText(tr("Игрок: %1").arg(name));
}

void AccountManagerDialog::onCancelEditClicked()
{
    if (!m_selectedAccountId.isEmpty()) {
        Account acc = AccountManager::instance().getAccount(m_selectedAccountId);
        if (acc.isValid()) {
            populateAccountDetails(acc);
            return;
        }
    }
    refreshAccounts();
}

void AccountManagerDialog::onSaveAccountClicked()
{
    QString targetId;

    if (m_radioOffline->isChecked()) {
        QString nick = m_offlineNickInput->text().trimmed();
        if (nick.length() < 3 || !Utils::isValidNickname(nick)) {
            QMessageBox::warning(this, tr("Некорректный никнейм"),
                                 tr("Никнейм должен содержать от 3 до 16 символов (только латинские буквы, цифры и _)."));
            m_offlineNickInput->setFocus();
            return;
        }

        if (!m_isCreatingNew && !m_selectedAccountId.isEmpty()) {
            Account acc = AccountManager::instance().getAccount(m_selectedAccountId);
            acc.username = nick;
            acc.type = QStringLiteral("offline");
            acc.email.clear();
            acc.password.clear();
            acc.uuid = Utils::generateOfflinePlayerUuid(nick);
            AccountManager::instance().updateAccount(acc);
            targetId = acc.id;
        } else {
            Account acc(nick, QStringLiteral("offline"));
            AccountManager::instance().addAccount(acc);
            targetId = acc.id;
        }
        Settings::instance().addNickname(nick);
    } else if (m_radioElyBy->isChecked()) {
        QString login = m_elyLoginInput->text().trimmed();
        if (login.isEmpty()) {
            QMessageBox::warning(this, tr("Внимание"), tr("Введите логин или адрес электронной почты аккаунта Ely.by."));
            m_elyLoginInput->setFocus();
            return;
        }

        QString username = login;
        if (login.contains(QLatin1Char('@'))) {
            username = login.split(QLatin1Char('@')).first().trimmed();
        }
        if (!Utils::isValidNickname(username)) {
            username = Utils::sanitizeNickname(username);
            if (username.length() < 3) {
                username = QStringLiteral("Player_") + QString::number(QDateTime::currentMSecsSinceEpoch() % 10000);
            }
        }

        QString password = m_elyPasswordInput->text();
        bool useSkins = m_elySkinsCheck->isChecked();

        if (!m_isCreatingNew && !m_selectedAccountId.isEmpty()) {
            Account acc = AccountManager::instance().getAccount(m_selectedAccountId);
            acc.username = username;
            acc.type = QStringLiteral("elyby");
            acc.email = login;
            acc.password = password;
            acc.useElySkins = useSkins;
            AccountManager::instance().updateAccount(acc);
            targetId = acc.id;
        } else {
            Account acc(username, QStringLiteral("elyby"));
            acc.email = login;
            acc.password = password;
            acc.useElySkins = useSkins;
            AccountManager::instance().addAccount(acc);
            targetId = acc.id;
        }
        Settings::instance().addNickname(username);
    } else { // Microsoft
        QString email = m_msEmailInput->text().trimmed();
        if (email.isEmpty()) {
            QMessageBox::warning(this, tr("Внимание"), tr("Введите адрес электронной почты или геймертег Microsoft."));
            m_msEmailInput->setFocus();
            return;
        }

        QString username = email;
        if (email.contains(QLatin1Char('@'))) {
            username = email.split(QLatin1Char('@')).first().trimmed();
        }
        if (!Utils::isValidNickname(username)) {
            username = Utils::sanitizeNickname(username);
            if (username.length() < 3) {
                username = QStringLiteral("Player_") + QString::number(QDateTime::currentMSecsSinceEpoch() % 10000);
            }
        }

        if (!m_isCreatingNew && !m_selectedAccountId.isEmpty()) {
            Account acc = AccountManager::instance().getAccount(m_selectedAccountId);
            acc.username = username;
            acc.type = QStringLiteral("microsoft");
            acc.email = email;
            AccountManager::instance().updateAccount(acc);
            targetId = acc.id;
        } else {
            Account acc(username, QStringLiteral("microsoft"));
            acc.email = email;
            AccountManager::instance().addAccount(acc);
            targetId = acc.id;
        }
        Settings::instance().addNickname(username);
    }

    AccountManager::instance().save();
    Settings::instance().save();

    // If only one account or active is empty, set active
    if (!targetId.isEmpty()) {
        Account active = AccountManager::instance().activeAccount();
        if (!active.isValid() || AccountManager::instance().accounts().size() == 1) {
            AccountManager::instance().setActiveAccount(targetId);
            AccountManager::instance().save();
        }
    }

    refreshAccounts();

    // Select the saved item
    for (int i = 0; i < m_accountsList->count(); ++i) {
        if (m_accountsList->item(i)->data(Qt::UserRole).toString() == targetId) {
            m_accountsList->setCurrentRow(i);
            break;
        }
    }
}
