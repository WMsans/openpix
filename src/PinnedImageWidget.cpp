#include "PinnedImageWidget.h"

#include <QApplication>
#include <QPainter>
#include <QScreen>

PinnedImageWidget::PinnedImageWidget(const QImage &image, QWidget *parent)
    : QWidget(parent)
    , m_image(image)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);

    int width = qMax(32, m_image.width());
    int height = qMax(32, m_image.height());

    QScreen *screen = QApplication::screenAt(QCursor::pos());
    if (!screen) {
        screen = QApplication::primaryScreen();
    }
    QRect screenGeometry = screen->availableGeometry();

    qreal widthRatio = 1.0;
    qreal heightRatio = 1.0;
    if (width > screenGeometry.width() * 0.8)
        widthRatio = (screenGeometry.width() * 0.8) / width;
    if (height > screenGeometry.height() * 0.8)
        heightRatio = (screenGeometry.height() * 0.8) / height;
    m_scale = qMin(widthRatio, heightRatio);
    if (m_scale < 1.0) {
        width = static_cast<int>(m_image.width() * m_scale);
        height = static_cast<int>(m_image.height() * m_scale);
    }

    setFixedSize(width, height);

    QPoint cursorPos = QCursor::pos();
    int x = cursorPos.x() - width / 2;
    int y = cursorPos.y() - height / 2;
    x = qBound(screenGeometry.left(), x, screenGeometry.right() - width);
    y = qBound(screenGeometry.top(), y, screenGeometry.bottom() - height);
    move(x, y);

    m_closeButtonRect = QRect(width - CloseButtonSize - 4, 4, CloseButtonSize, CloseButtonSize);

    updateScaledPixmap();
}

void PinnedImageWidget::updateScaledPixmap()
{
    if (m_image.isNull()) {
        m_scaledPixmap = QPixmap();
        return;
    }

    QSize scaledSize(static_cast<int>(m_image.width() * m_scale),
                     static_cast<int>(m_image.height() * m_scale));
    m_scaledPixmap = QPixmap::fromImage(m_image.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void PinnedImageWidget::adjustWindowToContent()
{
    if (m_scaledPixmap.isNull()) {
        return;
    }

    setFixedSize(m_scaledPixmap.size());
    m_closeButtonRect = QRect(width() - CloseButtonSize - 4, 4, CloseButtonSize, CloseButtonSize);
}
