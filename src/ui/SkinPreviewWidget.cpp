#include "SkinPreviewWidget.h"
#include "../mc/SkinManager.h"
#include "../core/Logger.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

SkinPreviewWidget::SkinPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(48, 48);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Аватар игрока"));

    connect(&SkinManager::instance(), &SkinManager::avatarLoaded,
            this, &SkinPreviewWidget::onAvatarLoaded);

    updateAvatar();
}

void SkinPreviewWidget::setUsername(const QString &username)
{
    QString clean = username.trimmed();
    if (m_username == clean) {
        return;
    }

    m_username = clean;
    updateAvatar();
}

void SkinPreviewWidget::refresh()
{
    if (!m_username.isEmpty()) {
        SkinManager::instance().loadAvatarAsync(m_username, 48);
    }
    updateAvatar();
}

void SkinPreviewWidget::updateAvatar()
{
    if (m_username.isEmpty()) {
        m_avatarPixmap = SkinManager::defaultAvatar(48);
    } else {
        m_avatarPixmap = SkinManager::instance().getAvatar(m_username, 48);
    }
    update();
}

void SkinPreviewWidget::onAvatarLoaded(const QString &username, const QPixmap &avatar)
{
    if (username.compare(m_username, Qt::CaseInsensitive) == 0) {
        m_avatarPixmap = avatar;
        update();
        emit avatarLoaded(m_username);
    }
}

void SkinPreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect r = rect().adjusted(2, 2, -2, -2);
    const qreal radius = 8.0;

    QPainterPath clipPath;
    clipPath.addRoundedRect(r, radius, radius);

    painter.save();
    painter.setClipPath(clipPath);

    if (!m_avatarPixmap.isNull()) {
        painter.drawPixmap(r, m_avatarPixmap.scaled(r.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    } else {
        painter.drawPixmap(r, SkinManager::defaultAvatar(r.width()));
    }
    painter.restore();

    // Subtle border
    painter.setPen(QPen(QColor(255, 255, 255, 35), 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(r, radius, radius);
}

void SkinPreviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        emit clicked();
    }
    QWidget::mouseReleaseEvent(event);
}
