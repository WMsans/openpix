#include "OverlayWidget.h"
#include "Toolbar.h"
#include "AnnotationManager.h"
#include "ScrollCapture.h"
#include "LongImageWidget.h"
#include "CaptureManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>
#include <QtMath>
#include <iostream>

OverlayWidget::OverlayWidget(const QImage &screenshot, CaptureManager *captureManager,
                             QWidget *parent)
    : QWidget(parent)
    , m_screenshot(screenshot)
    , m_annotationManager(std::make_unique<AnnotationManager>())
    , m_captureManager(captureManager)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    auto screens = QGuiApplication::screens();
    QRect totalGeometry;
    for (const auto &screen : screens) {
        totalGeometry = totalGeometry.united(screen->geometry());
    }
    setGeometry(totalGeometry);

    m_toolbar = new Toolbar(this, this);
    connect(m_toolbar, &Toolbar::saveRequested, this, &OverlayWidget::onSaveRequested);
    connect(m_toolbar, &Toolbar::copyRequested, this, &OverlayWidget::onCopyRequested);
    connect(m_toolbar, &Toolbar::ocrRequested, this, &OverlayWidget::onOcrRequested);
    connect(m_toolbar, &Toolbar::annotateRequested, this, &OverlayWidget::onAnnotateRequested);
    connect(m_toolbar, &Toolbar::scrollCaptureRequested, this, &OverlayWidget::onScrollCaptureRequested);
    connect(m_toolbar, &Toolbar::annotationToolChanged, this, &OverlayWidget::onAnnotationToolChanged);
    connect(m_toolbar, &Toolbar::undoRequested, this, &OverlayWidget::onUndoRequested);
    connect(m_toolbar, &Toolbar::annotationDone, this, &OverlayWidget::onAnnotationDone);
    connect(m_toolbar, &Toolbar::quitRequested, this, []{
        qApp->quit();
    });
    m_toolbar->hide();

    show();
    setFocus();
}

QImage OverlayWidget::croppedImage() const
{
    std::cout << "=== croppedImage() called ===" << std::endl;
    std::cout << "Screenshot size: " << m_screenshot.width() << "x" << m_screenshot.height() << std::endl;
    std::cout << "Selection valid: " << m_selection.isValid() << std::endl;
    std::cout << "Selection rect (widget coords): " << m_selection.x() << "," << m_selection.y() << " " << m_selection.width() << "x" << m_selection.height() << std::endl;
    std::cout << "Widget geometry: " << geometry().x() << "," << geometry().y() << " " << geometry().width() << "x" << geometry().height() << std::endl;
    
    if (m_selection.isValid()) {
        double scaleX = static_cast<double>(m_screenshot.width()) / width();
        double scaleY = static_cast<double>(m_screenshot.height()) / height();
        
        std::cout << "Scale factors: X=" << scaleX << " Y=" << scaleY << std::endl;
        
        QRect scaledSelection(
            static_cast<int>(m_selection.x() * scaleX),
            static_cast<int>(m_selection.y() * scaleY),
            static_cast<int>(m_selection.width() * scaleX),
            static_cast<int>(m_selection.height() * scaleY)
        );
        
        std::cout << "Scaled selection rect (screenshot coords): " << scaledSelection.x() << "," << scaledSelection.y() << " " << scaledSelection.width() << "x" << scaledSelection.height() << std::endl;
        
        QImage base = m_screenshot.copy(scaledSelection);
        std::cout << "Cropped image size: " << base.width() << "x" << base.height() << std::endl;
        base.save("/tmp/selection_debug.png");
        std::cout << "Saved selection to /tmp/selection_debug.png" << std::endl;
        
        if (m_annotationManager && m_annotationManager->hasAnnotations()) {
            return m_annotationManager->composite(m_screenshot, scaledSelection);
        }
        return base;
    }
    return QImage();
}

void OverlayWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.drawImage(rect(), m_screenshot);

    if (m_selection.isNull() || !m_selection.isValid()) {
        painter.fillRect(rect(), QColor(0, 0, 0, 128));
        return;
    }

    QPainterPath path;
    path.addRect(rect());
    path.addRect(m_selection);
    painter.setClipPath(path);
    painter.fillRect(rect(), QColor(0, 0, 0, 128));
    painter.setClipping(false);

    if (m_annotationManager && m_annotationMode) {
        m_annotationManager->paint(painter, m_selection);
    }

    QPen borderPen(QColor(255, 255, 255), 1, Qt::DashLine);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(m_selection);

    for (int i = 1; i <= 8; ++i) {
        Handle h = static_cast<Handle>(i);
        QRect hr = handleRect(h);
        painter.setBrush(QColor(255, 255, 255));
        painter.setPen(QPen(QColor(0, 0, 0), 1));
        painter.drawRect(hr);
    }
}

OverlayWidget::Handle OverlayWidget::handleAt(const QPoint &pos) const
{
    for (int i = 1; i <= 8; ++i) {
        Handle h = static_cast<Handle>(i);
        if (handleRect(h).contains(pos)) {
            return h;
        }
    }
    return Handle::None;
}

Qt::CursorShape OverlayWidget::cursorForHandle(Handle handle) const
{
    switch (handle) {
    case Handle::TopLeft:
    case Handle::BottomRight:
        return Qt::SizeFDiagCursor;
    case Handle::TopRight:
    case Handle::BottomLeft:
        return Qt::SizeBDiagCursor;
    case Handle::Top:
    case Handle::Bottom:
        return Qt::SizeVerCursor;
    case Handle::Left:
    case Handle::Right:
        return Qt::SizeHorCursor;
    default:
        return Qt::ArrowCursor;
    }
}

QRect OverlayWidget::handleRect(Handle handle) const
{
    if (handle == Handle::None || m_selection.isNull()) {
        return QRect();
    }

    int hs = HandleSize;
    int x = m_selection.x();
    int y = m_selection.y();
    int w = m_selection.width();
    int h = m_selection.height();
    int cx = x + w / 2;
    int cy = y + h / 2;

    switch (handle) {
    case Handle::TopLeft:
        return QRect(x - hs, y - hs, hs * 2, hs * 2);
    case Handle::Top:
        return QRect(cx - hs, y - hs, hs * 2, hs * 2);
    case Handle::TopRight:
        return QRect(x + w - hs, y - hs, hs * 2, hs * 2);
    case Handle::Left:
        return QRect(x - hs, cy - hs, hs * 2, hs * 2);
    case Handle::Right:
        return QRect(x + w - hs, cy - hs, hs * 2, hs * 2);
    case Handle::BottomLeft:
        return QRect(x - hs, y + h - hs, hs * 2, hs * 2);
    case Handle::Bottom:
        return QRect(cx - hs, y + h - hs, hs * 2, hs * 2);
    case Handle::BottomRight:
        return QRect(x + w - hs, y + h - hs, hs * 2, hs * 2);
    default:
        return QRect();
    }
}

void OverlayWidget::updateToolbarPosition()
{
    if (!m_toolbar) {
        return;
    }

    if (!m_selection.isValid() || m_selection.width() < MinSelectionSize || m_selection.height() < MinSelectionSize) {
        m_toolbar->hide();
        return;
    }

    QSize toolbarSize = m_toolbar->sizeHint();
    int margin = 4;

    int x = m_selection.right() - toolbarSize.width() + 1;
    int y = m_selection.bottom() + margin + 1;

    QRect screenRect = rect();
    int bottomSpace = screenRect.bottom() - m_selection.bottom() - margin;

    if (bottomSpace < toolbarSize.height()) {
        y = m_selection.top() - toolbarSize.height() - margin;
    }

    x = qMax(screenRect.left(), qMin(x, screenRect.right() - toolbarSize.width()));
    y = qMax(screenRect.top(), qMin(y, screenRect.bottom() - toolbarSize.height()));

    m_toolbar->move(x, y);
    m_toolbar->show();
}

void OverlayWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (m_annotationMode && m_selection.contains(event->pos())) {
        m_state = State::Annotating;
        if (m_annotationManager && m_toolbar) {
            m_annotationManager->setColor(m_toolbar->annotationColor());
            m_annotationManager->setThickness(m_toolbar->annotationThickness());
            m_annotationManager->startStroke(event->pos());
        }
        return;
    }

    m_dragStart = event->pos();

    Handle h = handleAt(event->pos());
    if (h != Handle::None) {
        m_state = State::Resizing;
        m_activeHandle = h;
        m_selectionStartPos = m_selection.topLeft();
        m_dragStart = event->pos();
        return;
    }

    if (m_selection.isValid() && m_selection.contains(event->pos())) {
        m_state = State::Moving;
        m_dragStart = event->pos();
        m_selectionStartPos = m_selection.topLeft();
        return;
    }

    m_state = State::Drawing;
    m_selectionStartPos = event->pos();
    m_selection = QRect();
}

void OverlayWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();

    switch (m_state) {
    case State::Idle: {
        Handle h = handleAt(pos);
        setCursor(cursorForHandle(h));
        break;
    }
    case State::Drawing: {
        QRect newSelection(m_selectionStartPos, pos);
        m_selection = newSelection.normalized();
        update();
        break;
    }
    case State::Moving: {
        QPoint delta = pos - m_dragStart;
        QRect newSelection(m_selectionStartPos + delta, m_selection.size());
        m_selection = newSelection.intersected(rect());
        updateToolbarPosition();
        update();
        break;
    }
    case State::Resizing: {
        QPoint delta = pos - m_dragStart;
        QRect r;

        switch (m_activeHandle) {
        case Handle::TopLeft:
            r = QRect(m_selectionStartPos + delta, m_selection.bottomRight());
            break;
        case Handle::Top:
            r = QRect(QPoint(m_selection.left(), m_selectionStartPos.y() + delta.y()), m_selection.bottomRight());
            break;
        case Handle::TopRight:
            r = QRect(QPoint(m_selection.right() + delta.x(), m_selectionStartPos.y() + delta.y()), m_selection.bottomLeft());
            break;
        case Handle::Left:
            r = QRect(QPoint(m_selectionStartPos.x() + delta.x(), m_selection.top()), m_selection.bottomRight());
            break;
        case Handle::Right:
            r = QRect(m_selection.topLeft(), QPoint(m_selection.right() + delta.x(), m_selection.bottom()));
            break;
        case Handle::BottomLeft:
            r = QRect(QPoint(m_selectionStartPos.x() + delta.x(), m_selection.bottom() + delta.y()), m_selection.topRight());
            break;
        case Handle::Bottom:
            r = QRect(m_selection.topLeft(), QPoint(m_selection.right(), m_selection.bottom() + delta.y()));
            break;
        case Handle::BottomRight:
            r = QRect(m_selection.topLeft(), m_selection.bottomRight() + delta);
            break;
        default:
            r = m_selection;
            break;
        }

        r = r.normalized();
        r = r.intersected(rect());
        m_selection = r;
        updateToolbarPosition();
        update();
        break;
    }
    case State::Annotating: {
        if (m_annotationManager) {
            m_annotationManager->continueStroke(pos);
            update();
        }
        break;
    }
    }
}

void OverlayWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (m_state == State::Annotating) {
        if (m_annotationManager) {
            m_annotationManager->endStroke(event->pos());
        }
        m_state = State::Idle;
        update();
        return;
    }

    if (m_state == State::Drawing) {
        if (m_selection.width() < MinSelectionSize || m_selection.height() < MinSelectionSize) {
            m_selection = QRect();
        }
    }

    m_state = State::Idle;
    m_activeHandle = Handle::None;

    updateToolbarPosition();
    update();
}

void OverlayWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        emit cancelled();
        close();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_selection.isValid() && m_selection.width() >= MinSelectionSize && m_selection.height() >= MinSelectionSize) {
            emit regionSelected(m_selection);
            close();
        }
        break;
    case Qt::Key_Left:
        if (m_selection.isValid()) {
            m_selection.translate(event->modifiers() & Qt::ShiftModifier ? -10 : -1, 0);
            updateToolbarPosition();
            update();
        }
        break;
    case Qt::Key_Right:
        if (m_selection.isValid()) {
            m_selection.translate(event->modifiers() & Qt::ShiftModifier ? 10 : 1, 0);
            updateToolbarPosition();
            update();
        }
        break;
    case Qt::Key_Up:
        if (m_selection.isValid()) {
            m_selection.translate(0, event->modifiers() & Qt::ShiftModifier ? -10 : -1);
            updateToolbarPosition();
            update();
        }
        break;
    case Qt::Key_Down:
        if (m_selection.isValid()) {
            m_selection.translate(0, event->modifiers() & Qt::ShiftModifier ? 10 : 1);
            updateToolbarPosition();
            update();
        }
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void OverlayWidget::onSaveRequested()
{
    emit saveRequested();
}

void OverlayWidget::onCopyRequested()
{
    emit copyRequested();
}

void OverlayWidget::onOcrRequested()
{
    emit ocrRequested();
}

void OverlayWidget::onAnnotateRequested()
{
    m_annotationMode = true;
    if (m_toolbar && m_annotationManager) {
        m_annotationManager->setTool(static_cast<AnnotationManager::Tool>(m_toolbar->annotationTool()));
        m_annotationManager->setColor(m_toolbar->annotationColor());
        m_annotationManager->setThickness(m_toolbar->annotationThickness());
        m_toolbar->showAnnotationToolbar();
    }
    emit annotateRequested();
}

void OverlayWidget::onScrollCaptureRequested()
{
    startScrollCapture();
}

void OverlayWidget::onAnnotationToolChanged(Toolbar::AnnotationTool tool)
{
    if (m_annotationManager) {
        m_annotationManager->setTool(static_cast<AnnotationManager::Tool>(tool));
    }
}

void OverlayWidget::onUndoRequested()
{
    if (m_annotationManager) {
        m_annotationManager->undo();
        update();
    }
}

void OverlayWidget::onAnnotationDone()
{
    m_annotationMode = false;
    if (m_toolbar) {
        m_toolbar->showMainToolbar();
    }
    updateToolbarPosition();
    update();
}

void OverlayWidget::startScrollCapture()
{
    if (!m_selection.isValid() || !m_captureManager) {
        return;
    }

    double scaleX = static_cast<double>(m_screenshot.width()) / width();
    double scaleY = static_cast<double>(m_screenshot.height()) / height();
    QRect scaledRegion(
        static_cast<int>(m_selection.x() * scaleX),
        static_cast<int>(m_selection.y() * scaleY),
        static_cast<int>(m_selection.width() * scaleX),
        static_cast<int>(m_selection.height() * scaleY)
    );

    hide();

    ScrollCapture *scrollCapture = new ScrollCapture(m_captureManager, scaledRegion);

    connect(scrollCapture, &ScrollCapture::finished, this, [this](const QImage &stitchedImage) {
        auto *viewer = new LongImageWidget(stitchedImage);
        connect(viewer, &QWidget::destroyed, this, []() {
            qApp->quit();
        });
        close();
    });

    connect(scrollCapture, &ScrollCapture::cancelled, this, [this]() {
        show();
        setFocus();
    });
}
