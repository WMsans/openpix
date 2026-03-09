#include "LongImageWidget.h"
#include "AnnotationManager.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QFileDialog>
#include <QDir>
#include <QDateTime>
#include <QProcess>
#include <QBuffer>
#include <QMessageBox>

const QColor LongImageWidget::Colors[6] = {Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::white, Qt::black};
constexpr int LongImageWidget::Thicknesses[];

LongImageWidget::LongImageWidget(const QImage &image, QWidget *parent)
    : QWidget(parent)
    , m_image(image)
    , m_annotationManager(std::make_unique<AnnotationManager>())
    , m_annotationColor(Colors[0])
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::BypassWindowManagerHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    auto screens = QGuiApplication::screens();
    QRect totalGeometry;
    for (const auto &screen : screens) {
        totalGeometry = totalGeometry.united(screen->geometry());
    }
    setGeometry(totalGeometry);

    if (m_image.width() > 0) {
        m_zoom = static_cast<double>(width()) / m_image.width();
    }
    m_offset = QPointF(0.0, 0.0);

    setupToolbar();
    show();
    setFocus();
}

QPointF LongImageWidget::widgetToImage(const QPointF &widgetPos) const
{
    return QPointF(
        widgetPos.x() / m_zoom + m_offset.x(),
        widgetPos.y() / m_zoom + m_offset.y()
    );
}

QPointF LongImageWidget::imageToWidget(const QPointF &imagePos) const
{
    return QPointF(
        (imagePos.x() - m_offset.x()) * m_zoom,
        (imagePos.y() - m_offset.y()) * m_zoom
    );
}

void LongImageWidget::clampOffset()
{
    double visibleW = width() / m_zoom;
    double visibleH = height() / m_zoom;

    double maxX = m_image.width() - visibleW;
    double maxY = m_image.height() - visibleH;

    if (maxX < 0) {
        m_offset.setX(maxX / 2.0);
    } else {
        m_offset.setX(qBound(0.0, m_offset.x(), maxX));
    }

    if (maxY < 0) {
        m_offset.setY(maxY / 2.0);
    } else {
        m_offset.setY(qBound(0.0, m_offset.y(), maxY));
    }
}

void LongImageWidget::updateZoomAtPoint(double newZoom, const QPointF &anchor)
{
    double fitZoom = static_cast<double>(width()) / m_image.width();
    double minZoom = qMin(fitZoom, MinZoomFactor);
    double maxZoom = MaxZoomMultiplier;
    newZoom = qBound(minZoom, newZoom, maxZoom);

    QPointF imagePoint = widgetToImage(anchor);
    m_zoom = newZoom;
    m_offset.setX(imagePoint.x() - anchor.x() / m_zoom);
    m_offset.setY(imagePoint.y() - anchor.y() / m_zoom);
    clampOffset();
    update();
}

void LongImageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), QColor(30, 30, 30));

    QRectF sourceRect(m_offset.x(), m_offset.y(),
                      width() / m_zoom, height() / m_zoom);

    sourceRect = sourceRect.intersected(QRectF(0, 0, m_image.width(), m_image.height()));

    QRectF targetRect(
        (sourceRect.x() - m_offset.x()) * m_zoom,
        (sourceRect.y() - m_offset.y()) * m_zoom,
        sourceRect.width() * m_zoom,
        sourceRect.height() * m_zoom
    );

    painter.drawImage(targetRect, m_image, sourceRect);

    if (m_annotationManager && (m_annotationManager->hasAnnotations() || m_annotating)) {
        painter.save();
        painter.translate(-m_offset.x() * m_zoom, -m_offset.y() * m_zoom);
        painter.scale(m_zoom, m_zoom);
        m_annotationManager->paint(painter);
        painter.restore();
    }
}
