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
    setWindowTitle("OpenPix Long Screenshot");
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

void LongImageWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        double factor = event->angleDelta().y() > 0 ? 1.1 : 0.9;
        updateZoomAtPoint(m_zoom * factor, event->position());
    } else {
        double scrollAmount = -event->angleDelta().y() / m_zoom;
        m_offset.setY(m_offset.y() + scrollAmount);
        clampOffset();
        update();
    }
}

void LongImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (m_annotating) {
        if (m_annotationManager) {
            m_annotationManager->setColor(m_annotationColor);
            m_annotationManager->setThickness(m_annotationThickness);
            QPointF imagePos = widgetToImage(event->position());
            m_annotationManager->startStroke(imagePos.toPoint());
        }
        return;
    }

    m_dragging = true;
    m_dragStart = event->pos();
    m_offsetAtDragStart = m_offset;
    setCursor(Qt::ClosedHandCursor);
}

void LongImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_annotating && m_annotationManager) {
        QPointF imagePos = widgetToImage(event->position());
        m_annotationManager->continueStroke(imagePos.toPoint());
        update();
        return;
    }

    if (m_dragging) {
        QPoint delta = event->pos() - m_dragStart;
        m_offset.setX(m_offsetAtDragStart.x() - delta.x() / m_zoom);
        m_offset.setY(m_offsetAtDragStart.y() - delta.y() / m_zoom);
        clampOffset();
        update();
    } else if (!m_annotating) {
        setCursor(Qt::OpenHandCursor);
    }
}

void LongImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    if (m_annotating && m_annotationManager) {
        QPointF imagePos = widgetToImage(event->position());
        m_annotationManager->endStroke(imagePos.toPoint());
        update();
        return;
    }

    m_dragging = false;
    setCursor(m_annotating ? Qt::CrossCursor : Qt::OpenHandCursor);
}

void LongImageWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_annotating) {
            onAnnotationDone();
        } else {
            close();
            qApp->quit();
        }
    } else {
        QWidget::keyPressEvent(event);
    }
}

void LongImageWidget::setupToolbar()
{
    m_toolbar = new QWidget(this);
    m_toolbar->setStyleSheet(R"(
        QWidget {
            background-color: rgba(45, 45, 45, 230);
            border-radius: 6px;
            border: 1px solid #444;
        }
        QPushButton {
            background-color: #3d3d3d;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 6px 12px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #4d4d4d;
        }
        QPushButton:pressed {
            background-color: #5d5d5d;
        }
    )");

    auto *toolbarLayout = new QVBoxLayout(m_toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(0);

    m_mainBar = new QWidget(m_toolbar);
    auto *mainLayout = new QHBoxLayout(m_mainBar);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(2);

    auto *saveBtn = new QPushButton("Save As", m_mainBar);
    auto *copyBtn = new QPushButton("Copy", m_mainBar);
    auto *annotateBtn = new QPushButton("Annotate", m_mainBar);
    auto *quitBtn = new QPushButton("Quit", m_mainBar);

    connect(saveBtn, &QPushButton::clicked, this, &LongImageWidget::onSaveClicked);
    connect(copyBtn, &QPushButton::clicked, this, &LongImageWidget::onCopyClicked);
    connect(annotateBtn, &QPushButton::clicked, this, &LongImageWidget::onAnnotateClicked);
    connect(quitBtn, &QPushButton::clicked, this, []{ qApp->quit(); });

    mainLayout->addWidget(saveBtn);
    mainLayout->addWidget(copyBtn);
    mainLayout->addWidget(annotateBtn);
    mainLayout->addWidget(quitBtn);

    m_annotationBar = new QWidget(m_toolbar);
    auto *annLayout = new QHBoxLayout(m_annotationBar);
    annLayout->setContentsMargins(4, 4, 4, 4);
    annLayout->setSpacing(2);

    auto *freehandBtn = new QPushButton("Freehand", m_annotationBar);
    auto *rectBtn = new QPushButton("Rectangle", m_annotationBar);
    m_colorBtn = new QPushButton("Color", m_annotationBar);
    m_thicknessBtn = new QPushButton("2px", m_annotationBar);
    auto *undoBtn = new QPushButton("Undo", m_annotationBar);
    auto *doneBtn = new QPushButton("Done", m_annotationBar);

    connect(freehandBtn, &QPushButton::clicked, this, &LongImageWidget::onFreehandClicked);
    connect(rectBtn, &QPushButton::clicked, this, &LongImageWidget::onRectClicked);
    connect(m_colorBtn, &QPushButton::clicked, this, &LongImageWidget::onColorClicked);
    connect(m_thicknessBtn, &QPushButton::clicked, this, &LongImageWidget::onThicknessClicked);
    connect(undoBtn, &QPushButton::clicked, this, &LongImageWidget::onUndoClicked);
    connect(doneBtn, &QPushButton::clicked, this, &LongImageWidget::onAnnotationDone);

    annLayout->addWidget(freehandBtn);
    annLayout->addWidget(rectBtn);
    annLayout->addWidget(m_colorBtn);
    annLayout->addWidget(m_thicknessBtn);
    annLayout->addWidget(undoBtn);
    annLayout->addWidget(doneBtn);

    toolbarLayout->addWidget(m_mainBar);
    toolbarLayout->addWidget(m_annotationBar);
    m_annotationBar->hide();

    m_toolbar->adjustSize();

    auto *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeom = screen->geometry();
        m_toolbar->move(screenGeom.center().x() - m_toolbar->width() / 2, screenGeom.top() + 10);
    }
}

QImage LongImageWidget::compositeImage() const
{
    if (m_annotationManager && m_annotationManager->hasAnnotations()) {
        return m_annotationManager->composite(m_image, m_image.rect(), 1.0, 1.0);
    }
    return m_image;
}

void LongImageWidget::onSaveClicked()
{
    QImage image = compositeImage();
    if (image.isNull()) return;

    QString defaultPath = QDir::homePath() + "/Pictures/screenshot_" +
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";

    QString fileName = QFileDialog::getSaveFileName(
        nullptr, "Save Screenshot", defaultPath,
        "PNG Images (*.png);;All Files (*)");

    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".png", Qt::CaseInsensitive)) {
            fileName += ".png";
        }
        if (image.save(fileName, "PNG")) {
            qApp->quit();
        } else {
            qWarning() << "Failed to save screenshot to" << fileName;
        }
    }
}

void LongImageWidget::onCopyClicked()
{
    QImage image = compositeImage();
    if (image.isNull()) return;

    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();

    QProcess wlCopy;
    wlCopy.start("wl-copy", QStringList() << "-t" << "image/png");
    wlCopy.write(imageData);
    wlCopy.closeWriteChannel();
    wlCopy.waitForFinished(1000);

    if (wlCopy.exitCode() != 0) {
        qWarning() << "wl-copy failed:" << wlCopy.readAllStandardError();
        QMessageBox::warning(this, "Copy Failed", "Failed to copy image to clipboard");
        return;
    }

    qApp->quit();
}

void LongImageWidget::onAnnotateClicked()
{
    m_annotating = true;
    setCursor(Qt::CrossCursor);
    if (m_annotationManager) {
        m_annotationManager->setTool(AnnotationManager::Tool::Freehand);
        m_annotationManager->setColor(m_annotationColor);
        m_annotationManager->setThickness(m_annotationThickness);
    }
    m_mainBar->hide();
    m_annotationBar->show();
    m_toolbar->adjustSize();
}

void LongImageWidget::onAnnotationDone()
{
    m_annotating = false;
    setCursor(Qt::OpenHandCursor);
    m_annotationBar->hide();
    m_mainBar->show();
    m_toolbar->adjustSize();
    update();
}

void LongImageWidget::onUndoClicked()
{
    if (m_annotationManager) {
        m_annotationManager->undo();
        update();
    }
}

void LongImageWidget::onFreehandClicked()
{
    if (m_annotationManager) {
        m_annotationManager->setTool(AnnotationManager::Tool::Freehand);
    }
}

void LongImageWidget::onRectClicked()
{
    if (m_annotationManager) {
        m_annotationManager->setTool(AnnotationManager::Tool::Rectangle);
    }
}

void LongImageWidget::onColorClicked()
{
    m_colorIndex = (m_colorIndex + 1) % ColorCount;
    m_annotationColor = Colors[m_colorIndex];
    if (m_annotationManager) {
        m_annotationManager->setColor(m_annotationColor);
    }
    if (m_colorBtn) {
        QString colorName = m_annotationColor.name();
        m_colorBtn->setStyleSheet(QString("background-color: %1; color: %2;")
            .arg(colorName)
            .arg(m_annotationColor.lightness() > 128 ? "#000000" : "#ffffff"));
    }
}

void LongImageWidget::onThicknessClicked()
{
    m_thicknessIndex = (m_thicknessIndex + 1) % ThicknessCount;
    m_annotationThickness = Thicknesses[m_thicknessIndex];
    if (m_annotationManager) {
        m_annotationManager->setThickness(m_annotationThickness);
    }
    if (m_thicknessBtn) {
        m_thicknessBtn->setText(QString("%1px").arg(m_annotationThickness));
    }
}
