#pragma once

#include <QString>
#include <QJsonObject>
#include <QMetaType>
#include <QUuid>
#include <QDateTime>

struct Account {
    QString id;          // UUID identifier
    QString username;    // Player display name / nickname
    QString type;        // "offline", "elyby", "microsoft"
    QString email;       // Email or username (for Ely.by / Microsoft)
    QString password;    // Saved password or token (for Ely.by)
    QString uuid;        // Minecraft player UUID
    QString accessToken; // Auth access token ("" or "0" for offline)
    QString refreshToken;// Refresh token
    bool useElySkins = true; // Use Ely.by skin system
    qint64 createdAt = 0;// Creation timestamp (epoch ms)

    Account() = default;

    Account(const QString &name, const QString &accType = QStringLiteral("offline"))
        : id(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , username(name)
        , type(accType)
        , useElySkins(true)
        , createdAt(QDateTime::currentMSecsSinceEpoch())
    {}

    bool isValid() const {
        return !id.isEmpty() && !username.trimmed().isEmpty();
    }

    bool isOffline() const {
        return type.compare(QStringLiteral("offline"), Qt::CaseInsensitive) == 0;
    }

    bool isElyBy() const {
        return type.compare(QStringLiteral("elyby"), Qt::CaseInsensitive) == 0;
    }

    bool isMicrosoft() const {
        return type.compare(QStringLiteral("microsoft"), Qt::CaseInsensitive) == 0;
    }

    bool operator==(const Account &other) const {
        return id == other.id;
    }

    bool operator!=(const Account &other) const {
        return id != other.id;
    }

    QJsonObject toJsonObject() const {
        QJsonObject obj;
        obj[QStringLiteral("id")] = id;
        obj[QStringLiteral("username")] = username;
        obj[QStringLiteral("type")] = type;
        obj[QStringLiteral("email")] = email;
        obj[QStringLiteral("password")] = password;
        obj[QStringLiteral("uuid")] = uuid;
        obj[QStringLiteral("accessToken")] = accessToken;
        obj[QStringLiteral("refreshToken")] = refreshToken;
        obj[QStringLiteral("useElySkins")] = useElySkins;
        obj[QStringLiteral("createdAt")] = createdAt;
        return obj;
    }

    static Account fromJsonObject(const QJsonObject &obj) {
        Account acc;
        acc.id = obj[QStringLiteral("id")].toString();
        acc.username = obj[QStringLiteral("username")].toString();
        acc.type = obj.contains(QStringLiteral("type"))
            ? obj[QStringLiteral("type")].toString(QStringLiteral("offline"))
            : QStringLiteral("offline");
        acc.email = obj[QStringLiteral("email")].toString();
        acc.password = obj[QStringLiteral("password")].toString();
        acc.uuid = obj[QStringLiteral("uuid")].toString();
        acc.accessToken = obj[QStringLiteral("accessToken")].toString();
        acc.refreshToken = obj[QStringLiteral("refreshToken")].toString();
        acc.useElySkins = obj.contains(QStringLiteral("useElySkins"))
            ? obj[QStringLiteral("useElySkins")].toBool(true)
            : true;
        if (obj.contains(QStringLiteral("createdAt"))) {
            acc.createdAt = obj[QStringLiteral("createdAt")].toVariant().toLongLong();
        }
        return acc;
    }
};

Q_DECLARE_METATYPE(Account)
