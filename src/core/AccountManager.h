#pragma once

#include <QObject>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>

#include "Account.h"

class AccountManager : public QObject {
    Q_OBJECT

public:
    static AccountManager &instance();

    // Data access
    QList<Account> accounts() const;
    Account activeAccount() const;
    Account getAccount(const QString &accountId) const;
    Account getAccountByUsername(const QString &username) const;
    bool hasAccount(const QString &accountId) const;

    // Mutators
    bool addAccount(const Account &account);
    bool removeAccount(const QString &accountId);
    bool setActiveAccount(const QString &accountId);
    bool updateAccount(const Account &account);

    // Persistence
    void load(const QString &filePath = QString());
    bool save(const QString &filePath = QString()) const;
    QString accountsFilePath() const;

    // Cleanup helper
    static QStringList purgeNicknameSpam(const QStringList &rawList, const QString &lastNick = QString());

signals:
    void accountAdded(const Account &account);
    void accountRemoved(const QString &accountId);
    void activeAccountChanged(const Account &account);

private:
    explicit AccountManager(QObject *parent = nullptr);
    ~AccountManager() override = default;
    AccountManager(const AccountManager &) = delete;
    AccountManager &operator=(const AccountManager &) = delete;

    void autoCleanupAndMigrate(bool fileLoaded);
    bool writeToFile(const QString &filePath, const QJsonObject &root) const;
    QString accountsFilePathInternal() const;

    mutable QMutex m_mutex;
    QList<Account> m_accounts;
    QString m_activeAccountId;
    QString m_filePath;
};
