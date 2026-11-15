#pragma once

#include <QDialog>
#include <QString>
#include <QList>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QScrollArea>
#include <QFrame>

#include "../core/Account.h"
#include "../core/AccountManager.h"
#include "SkinPreviewWidget.h"

class AccountManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit AccountManagerDialog(QWidget *parent = nullptr);
    ~AccountManagerDialog() override = default;

public slots:
    void refreshAccounts();

private slots:
    void onToggleAddForm();
    void onConfirmAddAccount();
    void onCancelAddAccount();
    void onSelectAccount(const QString &accountId);
    void onDeleteAccount(const QString &accountId, const QString &username);
    void onNewUsernameChanged(const QString &text);

private:
    void setupUi();
    QWidget *createAccountCard(const Account &account, bool isActive);

    // Header
    QLabel *m_headerTitle{nullptr};
    QLabel *m_accountCountLabel{nullptr};
    QPushButton *m_toggleAddButton{nullptr};

    // Add Account Form
    QFrame *m_addFormFrame{nullptr};
    QComboBox *m_typeCombo{nullptr};
    QLineEdit *m_usernameInput{nullptr};
    SkinPreviewWidget *m_livePreview{nullptr};
    QPushButton *m_confirmAddBtn{nullptr};
    QPushButton *m_cancelAddBtn{nullptr};
    QLabel *m_validationHint{nullptr};

    // Card List Area
    QScrollArea *m_scrollArea{nullptr};
    QWidget *m_cardsContainer{nullptr};
    QVBoxLayout *m_cardsLayout{nullptr};
    QLabel *m_emptyLabel{nullptr};

    // Bottom Bar
    QPushButton *m_closeButton{nullptr};
};
