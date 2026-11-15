#include "AccountManagerDialog.h"
#include "../core/Settings.h"
#include "../core/Utils.h"
#include "../core/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QIcon>
#include <QScrollBar>

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

    refreshAccounts();
}

void AccountManagerDialog::setupUi()
{
    setWindowTitle(tr("Менеджер аккаунтов"));
    resize(620, 520);
    setMinimumSize(520, 420);
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
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);

    m_headerTitle = new QLabel(tr("👥 Управление аккаунтами"), this);
    m_headerTitle->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 16px; font-weight: bold;"));
    headerLayout->addWidget(m_headerTitle);

    m_accountCountLabel = new QLabel(this);
    m_accountCountLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px; font-weight: 500;"));
    headerLayout->addWidget(m_accountCountLabel);

    headerLayout->addStretch(1);

    m_toggleAddButton = new QPushButton(tr("➕ Добавить аккаунт"), this);
    m_toggleAddButton->setCursor(Qt::PointingHandCursor);
    m_toggleAddButton->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(16, 185, 129, 0.2);"
        "  border: 1px solid #10b981;"
        "  border-radius: 8px;"
        "  color: #34d399;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  padding: 6px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(16, 185, 129, 0.35);"
        "  color: #ffffff;"
        "}"
    );
    connect(m_toggleAddButton, &QPushButton::clicked, this, &AccountManagerDialog::onToggleAddForm);
    headerLayout->addWidget(m_toggleAddButton);

    mainLayout->addLayout(headerLayout);

    // ==========================================
    // 2. Add Account Form (Collapsible)
    // ==========================================
    m_addFormFrame = new QFrame(this);
    m_addFormFrame->setObjectName(QStringLiteral("AddFormFrame"));
    m_addFormFrame->setStyleSheet(
        "QFrame#AddFormFrame {"
        "  background-color: rgba(26, 31, 46, 0.95);"
        "  border: 1px solid rgba(16, 185, 129, 0.45);"
        "  border-radius: 12px;"
        "  padding: 10px;"
        "}"
    );
    m_addFormFrame->hide();

    auto *formMainLayout = new QVBoxLayout(m_addFormFrame);
    formMainLayout->setContentsMargins(12, 10, 12, 10);
    formMainLayout->setSpacing(8);

    auto *formTitle = new QLabel(tr("➕ Создание нового аккаунта"), m_addFormFrame);
    formTitle->setStyleSheet(QStringLiteral("color: #34d399; font-size: 12px; font-weight: bold;"));
    formMainLayout->addWidget(formTitle);

    auto *formRowLayout = new QHBoxLayout();
    formRowLayout->setSpacing(12);

    // Live Skin Preview
    m_livePreview = new SkinPreviewWidget(m_addFormFrame);
    formRowLayout->addWidget(m_livePreview, 0, Qt::AlignVCenter);

    // Inputs column
    auto *inputsLayout = new QVBoxLayout();
    inputsLayout->setSpacing(6);

    auto *fieldsRow = new QHBoxLayout();
    fieldsRow->setSpacing(8);

    auto *typeLabel = new QLabel(tr("Тип:"), m_addFormFrame);
    typeLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px; font-weight: 600;"));
    fieldsRow->addWidget(typeLabel);

    m_typeCombo = new QComboBox(m_addFormFrame);
    m_typeCombo->addItem(tr("Оффлайн"), QStringLiteral("offline"));
    m_typeCombo->addItem(tr("Ely.by"), QStringLiteral("elyby"));
    m_typeCombo->setStyleSheet(
        "QComboBox {"
        "  background-color: rgba(15, 17, 26, 0.85);"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 6px;"
        "  color: #f1f5f9;"
        "  padding: 5px 10px;"
        "  min-width: 90px;"
        "}"
    );
    fieldsRow->addWidget(m_typeCombo);

    auto *nickLabel = new QLabel(tr("Никнейм:"), m_addFormFrame);
    nickLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px; font-weight: 600;"));
    fieldsRow->addWidget(nickLabel);

    m_usernameInput = new QLineEdit(m_addFormFrame);
    m_usernameInput->setPlaceholderText(tr("Имя игрока (3-16 симв.)..."));
    m_usernameInput->setClearButtonEnabled(true);
    auto *regexValidator = new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^[a-zA-Z0-9_]{3,16}$")), this);
    m_usernameInput->setValidator(regexValidator);
    m_usernameInput->setStyleSheet(
        "QLineEdit {"
        "  background-color: rgba(15, 17, 26, 0.85);"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 6px;"
        "  color: #f1f5f9;"
        "  padding: 5px 10px;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid #10b981;"
        "}"
    );
    connect(m_usernameInput, &QLineEdit::textChanged, this, &AccountManagerDialog::onNewUsernameChanged);
    connect(m_usernameInput, &QLineEdit::returnPressed, this, &AccountManagerDialog::onConfirmAddAccount);
    fieldsRow->addWidget(m_usernameInput, 1);

    inputsLayout->addLayout(fieldsRow);

    m_validationHint = new QLabel(tr("Введите никнейм от 3 до 16 символов (только буквы A-Z, цифры и _)"), m_addFormFrame);
    m_validationHint->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    inputsLayout->addWidget(m_validationHint);

    formRowLayout->addLayout(inputsLayout, 1);

    // Form buttons
    auto *formButtonsLayout = new QVBoxLayout();
    formButtonsLayout->setSpacing(6);

    m_confirmAddBtn = new QPushButton(tr("✓ Добавить"), m_addFormFrame);
    m_confirmAddBtn->setCursor(Qt::PointingHandCursor);
    m_confirmAddBtn->setEnabled(false);
    m_confirmAddBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(16, 185, 129, 0.25);"
        "  border: 1px solid #10b981;"
        "  border-radius: 6px;"
        "  color: #34d399;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  padding: 6px 14px;"
        "}"
        "QPushButton:hover:enabled {"
        "  background-color: #10b981;"
        "  color: #ffffff;"
        "}"
        "QPushButton:disabled {"
        "  background-color: rgba(255, 255, 255, 0.05);"
        "  border: 1px solid rgba(255, 255, 255, 0.1);"
        "  color: #64748b;"
        "}"
    );
    connect(m_confirmAddBtn, &QPushButton::clicked, this, &AccountManagerDialog::onConfirmAddAccount);
    formButtonsLayout->addWidget(m_confirmAddBtn);

    m_cancelAddBtn = new QPushButton(tr("✕ Отмена"), m_addFormFrame);
    m_cancelAddBtn->setCursor(Qt::PointingHandCursor);
    m_cancelAddBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(255, 255, 255, 0.06);"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 6px;"
        "  color: #94a3b8;"
        "  font-size: 12px;"
        "  padding: 5px 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255, 255, 255, 0.15);"
        "  color: #ffffff;"
        "}"
    );
    connect(m_cancelAddBtn, &QPushButton::clicked, this, &AccountManagerDialog::onCancelAddAccount);
    formButtonsLayout->addWidget(m_cancelAddBtn);

    formRowLayout->addLayout(formButtonsLayout);

    formMainLayout->addLayout(formRowLayout);
    mainLayout->addWidget(m_addFormFrame);

    // ==========================================
    // 3. Scroll Area with Account Cards
    // ==========================================
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        "QScrollArea {"
        "  background-color: rgba(15, 17, 25, 0.6);"
        "  border: 1px solid rgba(255, 255, 255, 0.08);"
        "  border-radius: 12px;"
        "}"
    );

    m_cardsContainer = new QWidget();
    m_cardsContainer->setStyleSheet(QStringLiteral("background: transparent;"));
    m_cardsLayout = new QVBoxLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(10, 10, 10, 10);
    m_cardsLayout->setSpacing(8);

    m_emptyLabel = new QLabel(tr("Нет сохраненных аккаунтов.\nНажмите «➕ Добавить аккаунт», чтобы создать аккаунт."), m_cardsContainer);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: #64748b; font-size: 13px; line-height: 1.5; padding: 40px;"));
    m_cardsLayout->addWidget(m_emptyLabel);

    m_scrollArea->setWidget(m_cardsContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    // ==========================================
    // 4. Bottom Actions Bar
    // ==========================================
    auto *bottomLayout = new QHBoxLayout();

    auto *hintLabel = new QLabel(tr("💡 Выбранный активный аккаунт используется при запуске игры"), this);
    hintLabel->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    bottomLayout->addWidget(hintLabel);

    bottomLayout->addStretch(1);

    m_closeButton = new QPushButton(tr("Закрыть"), this);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setStyleSheet(
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
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_closeButton);

    mainLayout->addLayout(bottomLayout);
}

void AccountManagerDialog::refreshAccounts()
{
    // Clear existing cards
    QLayoutItem *child;
    while ((child = m_cardsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            if (child->widget() != m_emptyLabel) {
                child->widget()->deleteLater();
            }
        }
        delete child;
    }

    const QList<Account> accounts = AccountManager::instance().accounts();
    const Account active = AccountManager::instance().activeAccount();

    m_accountCountLabel->setText(tr("Всего: %1").arg(accounts.size()));

    if (accounts.isEmpty()) {
        m_cardsLayout->addWidget(m_emptyLabel);
        m_emptyLabel->show();
        m_addFormFrame->show();
        m_toggleAddButton->setText(tr("− Скрыть форму"));
        m_usernameInput->setFocus();
    } else {
        m_emptyLabel->hide();
        for (const Account &acc : accounts) {
            bool isActive = (acc.id == active.id);
            QWidget *card = createAccountCard(acc, isActive);
            m_cardsLayout->addWidget(card);
        }
        m_cardsLayout->addStretch(1);
    }
}

QWidget *AccountManagerDialog::createAccountCard(const Account &account, bool isActive)
{
    auto *card = new QFrame(m_cardsContainer);
    card->setObjectName(QStringLiteral("AccountCard"));

    if (isActive) {
        card->setStyleSheet(
            "QFrame#AccountCard {"
            "  background-color: rgba(16, 185, 129, 0.12);"
            "  border: 1.5px solid #10b981;"
            "  border-radius: 12px;"
            "}"
        );
    } else {
        card->setStyleSheet(
            "QFrame#AccountCard {"
            "  background-color: rgba(255, 255, 255, 0.04);"
            "  border: 1px solid rgba(255, 255, 255, 0.1);"
            "  border-radius: 12px;"
            "}"
            "QFrame#AccountCard:hover {"
            "  background-color: rgba(255, 255, 255, 0.07);"
            "  border: 1px solid rgba(255, 255, 255, 0.18);"
            "}"
        );
    }

    auto *cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(12, 10, 12, 10);
    cardLayout->setSpacing(12);

    // 1. Avatar (48x48)
    auto *avatar = new SkinPreviewWidget(card);
    avatar->setUsername(account.username);
    cardLayout->addWidget(avatar, 0, Qt::AlignVCenter);

    // 2. Info Column
    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);

    auto *nameLabel = new QLabel(account.username, card);
    if (isActive) {
        nameLabel->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 14px; font-weight: bold;"));
    } else {
        nameLabel->setStyleSheet(QStringLiteral("color: #f1f5f9; font-size: 14px; font-weight: 600;"));
    }
    infoLayout->addWidget(nameLabel);

    auto *badgesLayout = new QHBoxLayout();
    badgesLayout->setSpacing(6);

    // Type Badge ([Оффлайн], [Ely.by], [Microsoft])
    auto *typeBadge = new QLabel(card);
    if (account.type.compare(QStringLiteral("elyby"), Qt::CaseInsensitive) == 0) {
        typeBadge->setText(tr("[Ely.by]"));
        typeBadge->setStyleSheet(
            "QLabel {"
            "  background-color: rgba(14, 165, 233, 0.18);"
            "  color: #38bdf8;"
            "  border: 1px solid rgba(14, 165, 233, 0.45);"
            "  border-radius: 4px;"
            "  padding: 2px 7px;"
            "  font-size: 11px;"
            "  font-weight: bold;"
            "}"
        );
    } else if (account.type.compare(QStringLiteral("microsoft"), Qt::CaseInsensitive) == 0) {
        typeBadge->setText(tr("[Microsoft]"));
        typeBadge->setStyleSheet(
            "QLabel {"
            "  background-color: rgba(34, 197, 94, 0.18);"
            "  color: #4ade80;"
            "  border: 1px solid rgba(34, 197, 94, 0.45);"
            "  border-radius: 4px;"
            "  padding: 2px 7px;"
            "  font-size: 11px;"
            "  font-weight: bold;"
            "}"
        );
    } else {
        typeBadge->setText(tr("[Оффлайн]"));
        typeBadge->setStyleSheet(
            "QLabel {"
            "  background-color: rgba(148, 163, 184, 0.15);"
            "  color: #cbd5e1;"
            "  border: 1px solid rgba(148, 163, 184, 0.35);"
            "  border-radius: 4px;"
            "  padding: 2px 7px;"
            "  font-size: 11px;"
            "  font-weight: bold;"
            "}"
        );
    }
    badgesLayout->addWidget(typeBadge);

    // Active Checkmark Badge
    if (isActive) {
        auto *activeBadge = new QLabel(tr("✓ Активен"), card);
        activeBadge->setStyleSheet(
            "QLabel {"
            "  background-color: rgba(16, 185, 129, 0.22);"
            "  color: #10b981;"
            "  border: 1px solid #10b981;"
            "  border-radius: 4px;"
            "  padding: 2px 8px;"
            "  font-size: 11px;"
            "  font-weight: bold;"
            "}"
        );
        badgesLayout->addWidget(activeBadge);
    }

    badgesLayout->addStretch(1);
    infoLayout->addLayout(badgesLayout);

    cardLayout->addLayout(infoLayout, 1);

    // 3. Actions Column
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    if (isActive) {
        auto *activeBtn = new QPushButton(tr("✓ Выбран"), card);
        activeBtn->setEnabled(false);
        activeBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: rgba(16, 185, 129, 0.15);"
            "  border: 1px solid #10b981;"
            "  border-radius: 8px;"
            "  color: #10b981;"
            "  font-weight: 600;"
            "  font-size: 12px;"
            "  padding: 6px 14px;"
            "}"
        );
        btnLayout->addWidget(activeBtn);
    } else {
        auto *selectBtn = new QPushButton(tr("Выбрать"), card);
        selectBtn->setCursor(Qt::PointingHandCursor);
        selectBtn->setStyleSheet(
            "QPushButton {"
            "  background-color: rgba(59, 130, 246, 0.18);"
            "  border: 1px solid #3b82f6;"
            "  border-radius: 8px;"
            "  color: #60a5fa;"
            "  font-weight: 600;"
            "  font-size: 12px;"
            "  padding: 6px 14px;"
            "}"
            "QPushButton:hover {"
            "  background-color: rgba(59, 130, 246, 0.35);"
            "  color: #ffffff;"
            "}"
        );
        QString accId = account.id;
        connect(selectBtn, &QPushButton::clicked, this, [this, accId]() {
            onSelectAccount(accId);
        });
        btnLayout->addWidget(selectBtn);
    }

    auto *deleteBtn = new QPushButton(tr("Удалить"), card);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: rgba(239, 68, 68, 0.12);"
        "  border: 1px solid rgba(239, 68, 68, 0.35);"
        "  border-radius: 8px;"
        "  color: #f87171;"
        "  font-weight: 600;"
        "  font-size: 12px;"
        "  padding: 6px 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(239, 68, 68, 0.35);"
        "  color: #ffffff;"
        "}"
    );
    QString accId = account.id;
    QString accName = account.username;
    connect(deleteBtn, &QPushButton::clicked, this, [this, accId, accName]() {
        onDeleteAccount(accId, accName);
    });
    btnLayout->addWidget(deleteBtn);

    cardLayout->addLayout(btnLayout);

    return card;
}

void AccountManagerDialog::onToggleAddForm()
{
    bool visible = m_addFormFrame->isVisible();
    m_addFormFrame->setVisible(!visible);
    m_toggleAddButton->setText(!visible ? tr("− Скрыть форму") : tr("➕ Добавить аккаунт"));
    if (!visible) {
        m_usernameInput->setFocus();
    }
}

void AccountManagerDialog::onCancelAddAccount()
{
    m_usernameInput->clear();
    m_addFormFrame->hide();
    m_toggleAddButton->setText(tr("➕ Добавить аккаунт"));
}

void AccountManagerDialog::onConfirmAddAccount()
{
    QString username = m_usernameInput->text().trimmed();
    static const QRegularExpression regex(QStringLiteral("^[a-zA-Z0-9_]{3,16}$"));
    if (!regex.match(username).hasMatch()) {
        QMessageBox::warning(this, tr("Некорректный никнейм"),
                             tr("Никнейм должен содержать от 3 до 16 символов (буквы A-Z, цифры 0-9 и знак подчеркивания)."));
        return;
    }

    Account existing = AccountManager::instance().getAccountByUsername(username);
    if (existing.isValid()) {
        QMessageBox::information(this, tr("Аккаунт уже добавлен"),
                                 tr("Аккаунт с именем \"%1\" уже существует в списке.").arg(username));
        AccountManager::instance().setActiveAccount(existing.id);
        m_usernameInput->clear();
        m_addFormFrame->hide();
        m_toggleAddButton->setText(tr("➕ Добавить аккаунт"));
        refreshAccounts();
        return;
    }

    QString type = m_typeCombo->currentData().toString();
    if (type.isEmpty()) {
        type = QStringLiteral("offline");
    }

    Account newAcc(username, type);
    AccountManager::instance().addAccount(newAcc);
    AccountManager::instance().setActiveAccount(newAcc.id);

    // Sync to Settings for backward compatibility
    Settings::instance().addNickname(username);
    Settings::instance().setLastNickname(username);
    Settings::instance().save();

    m_usernameInput->clear();
    m_addFormFrame->hide();
    m_toggleAddButton->setText(tr("➕ Добавить аккаунт"));
    refreshAccounts();
}

void AccountManagerDialog::onSelectAccount(const QString &accountId)
{
    Account acc = AccountManager::instance().getAccount(accountId);
    AccountManager::instance().setActiveAccount(accountId);
    if (acc.isValid()) {
        Settings::instance().setLastNickname(acc.username);
        Settings::instance().save();
    }
    refreshAccounts();
}

void AccountManagerDialog::onDeleteAccount(const QString &accountId, const QString &username)
{
    auto reply = QMessageBox::question(
        this,
        tr("Удаление аккаунта"),
        tr("Вы действительно хотите удалить аккаунт \"%1\"?").arg(username),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        AccountManager::instance().removeAccount(accountId);
        Settings::instance().removeNickname(username);
        Settings::instance().save();
        refreshAccounts();
    }
}

void AccountManagerDialog::onNewUsernameChanged(const QString &text)
{
    QString nick = text.trimmed();
    m_livePreview->setUsername(nick);

    static const QRegularExpression regex(QStringLiteral("^[a-zA-Z0-9_]{3,16}$"));
    bool valid = regex.match(nick).hasMatch();
    m_confirmAddBtn->setEnabled(valid);

    if (nick.isEmpty()) {
        m_validationHint->setText(tr("Введите никнейм от 3 до 16 символов (только буквы A-Z, цифры и _)"));
        m_validationHint->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    } else if (valid) {
        m_validationHint->setText(tr("✓ Корректный никнейм"));
        m_validationHint->setStyleSheet(QStringLiteral("color: #34d399; font-size: 11px; font-weight: bold;"));
    } else {
        m_validationHint->setText(tr("✕ От 3 до 16 символов, только английские буквы, цифры и _"));
        m_validationHint->setStyleSheet(QStringLiteral("color: #f87171; font-size: 11px;"));
    }
}
