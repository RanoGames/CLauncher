#pragma once

#include <QWidget>
#include <QString>
#include <QPixmap>

class SkinPreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit SkinPreviewWidget(QWidget *parent = nullptr);
    ~SkinPreviewWidget() override = default;

    void setUsername(const QString &username);
    QString username() const { return m_username; }

public slots:
    void refresh();

signals:
    void avatarLoaded(const QString &username);
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onAvatarLoaded(const QString &username, const QPixmap &avatar);

private:
    void updateAvatar();

    QString m_username;
    QPixmap m_avatarPixmap;
};
