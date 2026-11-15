#pragma once

#include <QDialog>
#include <QString>
#include <QList>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QListWidget>
#include <QStackedWidget>
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
    void onAccountListSelectionChanged();
    void onAddNewAccountClicked();
    void onDeleteAccountClicked();
    void onMakeActiveClicked();
    void onSaveAccountClicked();
    void onCancelEditClicked();
    void onTypeRadioToggled();
    void onInputTextChanged();
    void onMicrosoftLoginClicked();

private:
    void setupUi();
    void populateAccountDetails(const Account &acc);
    void resetFormToNew();
    QString getEffectiveUsernameFromInputs() const;
    void updateLiveSkinPreview();

    // Top Header
    QLabel *m_headerTitle{nullptr};
    QLabel *m_headerSubtitle{nullptr};

    // Left Pane
    QLabel *m_leftHeaderLabel{nullptr};
    QListWidget *m_accountsList{nullptr};
    QPushButton *m_addBtn{nullptr};
    QPushButton *m_deleteBtn{nullptr};
    QPushButton *m_makeActiveBtn{nullptr};

    // Right Pane - Config
    QFrame *m_configPanel{nullptr};
    QLabel *m_panelTitleLabel{nullptr};

    // Type Radios
    QButtonGroup *m_typeGroup{nullptr};
    QRadioButton *m_radioOffline{nullptr};
    QRadioButton *m_radioElyBy{nullptr};
    QRadioButton *m_radioMicrosoft{nullptr};

    // Form Pages
    QStackedWidget *m_typeStack{nullptr};

    // 1. Offline
    QWidget *m_offlinePage{nullptr};
    QLineEdit *m_offlineNickInput{nullptr};
    QLabel *m_offlineHintLabel{nullptr};

    // 2. Ely.by
    QWidget *m_elyPage{nullptr};
    QLineEdit *m_elyLoginInput{nullptr};
    QLineEdit *m_elyPasswordInput{nullptr};
    QCheckBox *m_elySkinsCheck{nullptr};
    QLabel *m_elyHintLabel{nullptr};

    // 3. Microsoft
    QWidget *m_msPage{nullptr};
    QLineEdit *m_msEmailInput{nullptr};
    QPushButton *m_msLoginBtn{nullptr};
    QLabel *m_msStatusLabel{nullptr};
    QLabel *m_msHintLabel{nullptr};

    // Live Skin Preview Card
    QFrame *m_previewCard{nullptr};
    SkinPreviewWidget *m_livePreview{nullptr};
    QLabel *m_previewNickLabel{nullptr};
    QLabel *m_previewSubLabel{nullptr};

    // Right Pane Buttons
    QPushButton *m_saveBtn{nullptr};
    QPushButton *m_cancelBtn{nullptr};

    // Bottom Bar
    QPushButton *m_closeBtn{nullptr};

    // State
    QString m_selectedAccountId;
    bool m_isCreatingNew = false;
};
