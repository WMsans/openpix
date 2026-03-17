#include "AnnotationManager.h"

AnnotationManager::AnnotationManager(QObject *parent)
    : QObject(parent)
{
}

void AnnotationManager::setTool(Tool tool)
{
    m_tool = tool;
}

void AnnotationManager::setColor(const QColor &color)
{
    m_color = color;
}

void AnnotationManager::setThickness(int thickness)
{
    m_thickness = thickness;
}

void AnnotationManager::startStroke(const QPoint &pos)
{
    m_drawing = true;
    m_currentAnnotation = Annotation();
    m_currentAnnotation.type = m_tool;
    m_currentAnnotation.color = m_color;
    m_currentAnnotation.thickness = m_thickness;
    m_currentAnnotation.isPreview = true;

    if (m_tool == Tool::Freehand) {
        m_currentAnnotation.path = QPainterPath();
        m_currentAnnotation.path.moveTo(pos);
    } else {
        m_currentAnnotation.rect = QRect(pos, pos);
    }
}

void AnnotationManager::continueStroke(const QPoint &pos)
{
    if (!m_drawing) return;

    if (m_tool == Tool::Freehand) {
        m_currentAnnotation.path.lineTo(pos);
    } else {
        m_currentAnnotation.rect.setBottomRight(pos);
    }
}

void AnnotationManager::endStroke(const QPoint &pos)
{
    if (!m_drawing) return;

    if (m_tool == Tool::Freehand) {
        m_currentAnnotation.path.lineTo(pos);
    } else {
        m_currentAnnotation.rect.setBottomRight(pos);
    }

    m_currentAnnotation.isPreview = false;
    m_annotations.push_back(m_currentAnnotation);
    m_drawing = false;
}

void AnnotationManager::undo()
{
    if (!m_annotations.empty()) {
        m_annotations.pop_back();
    }
}

void AnnotationManager::clear()
{
    m_annotations.clear();
    m_drawing = false;
}

bool AnnotationManager::hasAnnotations() const
{
    return !m_annotations.empty();
}

int AnnotationManager::annotationCount() const
{
    return static_cast<int>(m_annotations.size());
}

void AnnotationManager::paint(QPainter &painter, const QRect &clipRect) const
{
    if (clipRect.isValid()) {
        painter.setClipRect(clipRect);
    }

    for (const auto &annotation : m_annotations) {
        QPen pen(annotation.color, annotation.thickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        if (annotation.type == Tool::Freehand) {
            painter.drawPath(annotation.path);
        } else {
            painter.drawRect(annotation.rect);
        }
    }

    if (m_drawing) {
        QPen pen(m_currentAnnotation.color, m_currentAnnotation.thickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        if (m_currentAnnotation.type == Tool::Freehand) {
            painter.drawPath(m_currentAnnotation.path);
        } else {
            painter.drawRect(m_currentAnnotation.rect);
        }
    }
}

QImage AnnotationManager::composite(const QImage &baseImage, const QRect &region, double scaleX, double scaleY) const
{
    if (baseImage.isNull()) return QImage();

    QImage result = baseImage.copy(region);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);

    for (const auto &annotation : m_annotations) {
        int scaledThickness = static_cast<int>(annotation.thickness * qMax(scaleX, scaleY));
        if (scaledThickness < 1) scaledThickness = 1;
        
        QPen pen(annotation.color, scaledThickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        if (annotation.type == Tool::Freehand) {
            QPainterPath scaledPath;
            for (int i = 0; i < annotation.path.elementCount(); ++i) {
                QPainterPath::Element e = annotation.path.elementAt(i);
                qreal sx = e.x * scaleX - region.x();
                qreal sy = e.y * scaleY - region.y();
                if (i == 0) {
                    scaledPath.moveTo(sx, sy);
                } else if (e.type == QPainterPath::LineToElement) {
                    scaledPath.lineTo(sx, sy);
                } else if (e.type == QPainterPath::MoveToElement) {
                    scaledPath.moveTo(sx, sy);
                }
            }
            painter.drawPath(scaledPath);
        } else {
            QRect scaledRect(
                static_cast<int>(annotation.rect.x() * scaleX - region.x()),
                static_cast<int>(annotation.rect.y() * scaleY - region.y()),
                static_cast<int>(annotation.rect.width() * scaleX),
                static_cast<int>(annotation.rect.height() * scaleY)
            );
            painter.drawRect(scaledRect);
        }
    }

    painter.end();
    return result;
}
