#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QWidget>

class PinnedImageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PinnedImageWidget(const QImage &image, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void updateScaledPixmap();
    void adjustWindowToContent();
    void updateCloseButtonHover(const QPoint &pos);

    QImage m_image;
    QPixmap m_scaledPixmap;
    qreal m_scale = 1.0;
    QPoint m_dragStart;
    QPoint m_windowStart;
    bool m_dragging = false;
    QRect m_closeButtonRect;
    bool m_closeButtonHovered = false;

    static constexpr int CloseButtonSize = 16;
    static constexpr qreal MinScale = 0.1;
    static constexpr qreal MaxScale = 10.0;
    static constexpr qreal ScaleStep = 0.1;
};
