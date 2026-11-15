#include "AccountManager.h"
#include "Settings.h"
#include "Utils.h"
#include "Logger.h"

#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <algorithm>

AccountManager &AccountManager::instance() {
    static AccountManager s_instance;
    return s_instance;
}

AccountManager::AccountManager(QObject *parent)
    : QObject(parent) {
    load();
}

QString AccountManager::accountsFilePath() const {
    QMutexLocker locker(&m_mutex);
    return accountsFilePathInternal();
}

QString AccountManager::accountsFilePathInternal() const {
    if (!m_filePath.isEmpty()) {
        return m_filePath;
    }
    return QDir(Settings::instance().effectiveGameDir()).filePath(QStringLiteral("accounts.json"));
}

QList<Account> AccountManager::accounts() const {
    QMutexLocker locker(&m_mutex);
    return m_accounts;
}

Account AccountManager::activeAccount() const {
    QMutexLocker locker(&m_mutex);
    for (const Account &acc : m_accounts) {
        if (acc.id == m_activeAccountId) {
            return acc;
        }
    }
    if (!m_accounts.isEmpty()) {
        return m_accounts.first();
    }
    return Account();
}

Account AccountManager::getAccount(const QString &accountId) const {
    QMutexLocker locker(&m_mutex);
    for (const Account &acc : m_accounts) {
        if (acc.id == accountId) {
            return acc;
        }
    }
    return Account();
}

Account AccountManager::getAccountByUsername(const QString &username) const {
    QMutexLocker locker(&m_mutex);
    for (const Account &acc : m_accounts) {
        if (acc.username.compare(username.trimmed(), Qt::CaseInsensitive) == 0) {
            return acc;
        }
    }
    return Account();
}

bool AccountManager::hasAccount(const QString &accountId) const {
    QMutexLocker locker(&m_mutex);
    for (const Account &acc : m_accounts) {
        if (acc.id == accountId) {
            return true;
        }
    }
    return false;
}

bool AccountManager::addAccount(const Account &account) {
    if (!account.isValid()) {
        return false;
    }

    Account acc = account;
    acc.username = acc.username.trimmed();
    if (acc.id.isEmpty()) {
        acc.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (acc.createdAt == 0) {
        acc.createdAt = QDateTime::currentMSecsSinceEpoch();
    }
    if (acc.type.isEmpty()) {
        acc.type = QStringLiteral("offline");
    }
    if (acc.uuid.isEmpty() && acc.type == QStringLiteral("offline")) {
        acc.uuid = Utils::generateOfflinePlayerUuid(acc.username);
    }

    bool activeChanged = false;
    Account activeAcc;
    QJsonObject rootToSave;
    QString targetPath;

    {
        QMutexLocker locker(&m_mutex);
        bool updated = false;

        // Check if an account with the same ID already exists
        for (int i = 0; i < m_accounts.size(); ++i) {
            if (m_accounts[i].id == acc.id) {
                m_accounts[i] = acc;
                updated = true;
                if (m_activeAccountId == acc.id) {
                    activeChanged = true;
                    activeAcc = acc;
                }
                break;
            }
        }

        // Check if an account with the same username and type already exists
        if (!updated) {
            for (int i = 0; i < m_accounts.size(); ++i) {
                if (m_accounts[i].username.compare(acc.username, Qt::CaseInsensitive) == 0 &&
                    m_accounts[i].type == acc.type) {
                    acc.id = m_accounts[i].id; // Keep existing id
                    m_accounts[i] = acc;
                    updated = true;
                    if (m_activeAccountId == acc.id) {
                        activeChanged = true;
                        activeAcc = acc;
                    }
                    break;
                }
            }
        }

        if (!updated) {
            m_accounts.append(acc);
            if (m_activeAccountId.isEmpty() || m_accounts.size() == 1) {
                m_activeAccountId = acc.id;
                activeChanged = true;
                activeAcc = acc;
            }
        }

        targetPath = accountsFilePathInternal();
        rootToSave[QStringLiteral("activeAccountId")] = m_activeAccountId;
        QJsonArray accArray;
        for (const Account &a : m_accounts) {
            accArray.append(a.toJsonObject());
        }
        rootToSave[QStringLiteral("accounts")] = accArray;
    }

    writeToFile(targetPath, rootToSave);

    // Keep Settings in sync
    Settings::instance().addNickname(acc.username);
    if (activeChanged) {
        Settings::instance().setLastNickname(acc.username);
    }
    Settings::instance().save();

    emit accountAdded(acc);
    if (activeChanged) {
        emit activeAccountChanged(activeAcc);
    }
    return true;
}

bool AccountManager::removeAccount(const QString &accountId) {
    if (accountId.isEmpty()) {
        return false;
    }

    bool found = false;
    bool activeChanged = false;
    Account newActive;
    QString removedUsername;
    QJsonObject rootToSave;
    QString targetPath;

    {
        QMutexLocker locker(&m_mutex);
        for (int i = 0; i < m_accounts.size(); ++i) {
            if (m_accounts[i].id == accountId) {
                removedUsername = m_accounts[i].username;
                m_accounts.removeAt(i);
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }

        if (m_activeAccountId == accountId) {
            if (!m_accounts.isEmpty()) {
                m_activeAccountId = m_accounts.first().id;
                newActive = m_accounts.first();
            } else {
                m_activeAccountId.clear();
                newActive = Account();
            }
            activeChanged = true;
        }

        targetPath = accountsFilePathInternal();
        rootToSave[QStringLiteral("activeAccountId")] = m_activeAccountId;
        QJsonArray accArray;
        for (const Account &a : m_accounts) {
            accArray.append(a.toJsonObject());
        }
        rootToSave[QStringLiteral("accounts")] = accArray;
    }

    writeToFile(targetPath, rootToSave);

    if (!removedUsername.isEmpty()) {
        Settings::instance().removeNickname(removedUsername);
        if (activeChanged && newActive.isValid()) {
            Settings::instance().setLastNickname(newActive.username);
        }
        Settings::instance().save();
    }

    emit accountRemoved(accountId);
    if (activeChanged) {
        emit activeAccountChanged(newActive);
    }
    return true;
}

bool AccountManager::setActiveAccount(const QString &accountId) {
    Account target;
    QJsonObject rootToSave;
    QString targetPath;

    {
        QMutexLocker locker(&m_mutex);
        if (m_activeAccountId == accountId && !accountId.isEmpty()) {
            return true;
        }
        for (const Account &acc : m_accounts) {
            if (acc.id == accountId) {
                target = acc;
                break;
            }
        }
        if (!target.isValid()) {
            return false;
        }

        m_activeAccountId = accountId;
        targetPath = accountsFilePathInternal();
        rootToSave[QStringLiteral("activeAccountId")] = m_activeAccountId;
        QJsonArray accArray;
        for (const Account &a : m_accounts) {
            accArray.append(a.toJsonObject());
        }
        rootToSave[QStringLiteral("accounts")] = accArray;
    }

    writeToFile(targetPath, rootToSave);

    Settings::instance().setLastNickname(target.username);
    Settings::instance().save();

    emit activeAccountChanged(target);
    return true;
}

bool AccountManager::updateAccount(const Account &account) {
    return addAccount(account);
}

bool AccountManager::writeToFile(const QString &filePath, const QJsonObject &root) const {
    QFileInfo fi(filePath);
    QDir().mkpath(fi.absolutePath());

    QJsonDocument doc(root);
    QByteArray data = doc.toJson(QJsonDocument::Indented);

    QSaveFile saveFile(filePath);
    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(QStringLiteral("Failed to open %1 for saving accounts: %2")
            .arg(filePath, saveFile.errorString()));
        return false;
    }

    if (saveFile.write(data) != data.size()) {
        LOG_ERROR(QStringLiteral("Failed to write accounts data to %1").arg(filePath));
        saveFile.cancelWriting();
        return false;
    }

    if (!saveFile.commit()) {
        LOG_ERROR(QStringLiteral("Failed to commit accounts save file %1").arg(filePath));
        return false;
    }

    return true;
}

bool AccountManager::save(const QString &filePath) const {
    QString targetPath = filePath;
    QJsonObject root;
    {
        QMutexLocker locker(&m_mutex);
        if (targetPath.isEmpty()) {
            targetPath = accountsFilePathInternal();
        }
        root[QStringLiteral("activeAccountId")] = m_activeAccountId;
        QJsonArray accArray;
        for (const Account &acc : m_accounts) {
            accArray.append(acc.toJsonObject());
        }
        root[QStringLiteral("accounts")] = accArray;
    }
    return writeToFile(targetPath, root);
}

void AccountManager::load(const QString &filePath) {
    QString targetPath = filePath;
    {
        QMutexLocker locker(&m_mutex);
        if (!targetPath.isEmpty()) {
            m_filePath = targetPath;
        }
        targetPath = accountsFilePathInternal();
    }

    QFile file(targetPath);
    bool fileLoaded = false;
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject root = doc.object();
            QList<Account> loadedAccounts;
            QString activeId;

            if (root.contains(QStringLiteral("activeAccountId"))) {
                activeId = root[QStringLiteral("activeAccountId")].toString();
            } else if (root.contains(QStringLiteral("active_account_id"))) {
                activeId = root[QStringLiteral("active_account_id")].toString();
            }

            if (root.contains(QStringLiteral("accounts")) && root[QStringLiteral("accounts")].isArray()) {
                const QJsonArray arr = root[QStringLiteral("accounts")].toArray();
                for (const QJsonValue &val : arr) {
                    if (val.isObject()) {
                        Account acc = Account::fromJsonObject(val.toObject());
                        if (acc.isValid()) {
                            loadedAccounts.append(acc);
                        }
                    }
                }
            }

            {
                QMutexLocker locker(&m_mutex);
                m_accounts = loadedAccounts;
                m_activeAccountId = activeId;
                if (m_activeAccountId.isEmpty() && !m_accounts.isEmpty()) {
                    m_activeAccountId = m_accounts.first().id;
                }
            }
            fileLoaded = true;
        }
    }

    autoCleanupAndMigrate(fileLoaded);
}

QStringList AccountManager::purgeNicknameSpam(const QStringList &rawList, const QString &lastNick) {
    // 1. Filter valid nicknames (length 3-16, valid characters) and deduplicate preserving order
    QStringList validNicks;
    QSet<QString> seen;
    for (const QString &raw : rawList) {
        QString trimmed = raw.trimmed();
        if (trimmed.length() >= 3 && trimmed.length() <= 16 && Utils::isValidNickname(trimmed)) {
            QString lower = trimmed.toLower();
            if (!seen.contains(lower)) {
                seen.insert(lower);
                validNicks.append(trimmed);
            }
        }
    }

    // 2. Remove strict prefixes (if A is a strict prefix of B, A is typing keystroke spam)
    QStringList noPrefixes;
    for (const QString &a : validNicks) {
        bool isPrefix = false;
        for (const QString &b : validNicks) {
            if (a.compare(b, Qt::CaseInsensitive) != 0 &&
                b.startsWith(a, Qt::CaseInsensitive)) {
                isPrefix = true;
                break;
            }
        }
        if (!isPrefix) {
            noPrefixes.append(a);
        }
    }

    // 3. Purge typo branches against lastNick if present
    QString targetNick = lastNick.trimmed();
    if (!targetNick.isEmpty() && Utils::isValidNickname(targetNick)) {
        bool targetFound = false;
        for (const QString &s : noPrefixes) {
            if (s.compare(targetNick, Qt::CaseInsensitive) == 0) {
                targetFound = true;
                break;
            }
        }

        if (targetFound) {
            QStringList cleaned;
            for (const QString &a : noPrefixes) {
                if (a.compare(targetNick, Qt::CaseInsensitive) == 0) {
                    cleaned.append(a);
                    continue;
                }
                // Common prefix length
                int commonLen = 0;
                int maxLen = std::min(a.length(), targetNick.length());
                for (int i = 0; i < maxLen; ++i) {
                    if (a.at(i).toLower() == targetNick.at(i).toLower()) {
                        commonLen++;
                    } else {
                        break;
                    }
                }
                // If sharing 3 or more leading characters with targetNick, it's an abandoned typo branch
                if (commonLen >= 3) {
                    continue;
                }
                cleaned.append(a);
            }
            return cleaned;
        }
    }

    // If targetNick wasn't in noPrefixes or wasn't provided, check pairwise typo branches
    QSet<QString> toDrop;
    for (int i = 0; i < noPrefixes.size(); ++i) {
        for (int j = 0; j < noPrefixes.size(); ++j) {
            if (i == j) continue;
            const QString &a = noPrefixes.at(i);
            const QString &b = noPrefixes.at(j);
            int commonLen = 0;
            int maxLen = std::min(a.length(), b.length());
            for (int k = 0; k < maxLen; ++k) {
                if (a.at(k).toLower() == b.at(k).toLower()) {
                    commonLen++;
                } else {
                    break;
                }
            }
            if (commonLen >= 4 && b.length() > a.length()) {
                toDrop.insert(a);
            }
        }
    }

    QStringList result;
    for (const QString &s : noPrefixes) {
        if (!toDrop.contains(s)) {
            result.append(s);
        }
    }
    return result;
}

void AccountManager::autoCleanupAndMigrate(bool fileLoaded) {
    Q_UNUSED(fileLoaded);
    Settings &s = Settings::instance();
    const QStringList rawNicks = s.nicknames();
    const QString lastNick = s.lastNickname();

    // Purge prefix spam and typo scraps
    QStringList cleanedNicks = purgeNicknameSpam(rawNicks, lastNick);

    // If settings has spam or differs from cleaned, permanently clean settings.json
    if (rawNicks != cleanedNicks) {
        LOG_INFO(QStringLiteral("AccountManager: Purging nickname spam from settings (%1 entries -> %2 entries)")
            .arg(rawNicks.size()).arg(cleanedNicks.size()));
        s.setNicknames(cleanedNicks);
        if (!cleanedNicks.isEmpty() && !cleanedNicks.contains(s.lastNickname())) {
            s.setLastNickname(cleanedNicks.first());
        }
        s.save();
    }

    // Migrate into accounts if m_accounts is empty or we have cleaned nicknames not yet in accounts
    bool needSave = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_accounts.isEmpty()) {
            for (const QString &nick : cleanedNicks) {
                Account acc;
                acc.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                acc.username = nick;
                acc.type = QStringLiteral("offline");
                acc.uuid = Utils::generateOfflinePlayerUuid(nick);
                acc.accessToken = QString();
                acc.createdAt = QDateTime::currentMSecsSinceEpoch();
                m_accounts.append(acc);

                if (m_activeAccountId.isEmpty() ||
                    nick.compare(s.lastNickname(), Qt::CaseInsensitive) == 0) {
                    m_activeAccountId = acc.id;
                }
            }
            if (!m_accounts.isEmpty()) {
                needSave = true;
            }
        } else {
            // Check if any cleaned nickname is missing in accounts
            for (const QString &nick : cleanedNicks) {
                bool found = false;
                for (const Account &existing : m_accounts) {
                    if (existing.username.compare(nick, Qt::CaseInsensitive) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    Account acc;
                    acc.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                    acc.username = nick;
                    acc.type = QStringLiteral("offline");
                    acc.uuid = Utils::generateOfflinePlayerUuid(nick);
                    acc.accessToken = QString();
                    acc.createdAt = QDateTime::currentMSecsSinceEpoch();
                    m_accounts.append(acc);
                    needSave = true;
                }
            }
            if (m_activeAccountId.isEmpty() && !m_accounts.isEmpty()) {
                m_activeAccountId = m_accounts.first().id;
                needSave = true;
            }
        }
    }

    if (needSave) {
        save();
    }
}
