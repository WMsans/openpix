#pragma once

#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QRect>
#include <QPoint>
#include <QImage>
#include <vector>

class AnnotationManager : public QObject
{
    Q_OBJECT

public:
    enum class Tool {
        Freehand,
        Rectangle
    };

    struct Annotation {
        Tool type;
        QColor color;
        int thickness;
        QPainterPath path;
        QRect rect;
        bool isPreview = false;
    };

    explicit AnnotationManager(QObject *parent = nullptr);

    void setTool(Tool tool);
    Tool tool() const { return m_tool; }

    void setColor(const QColor &color);
    QColor color() const { return m_color; }

    void setThickness(int thickness);
    int thickness() const { return m_thickness; }

    void startStroke(const QPoint &pos);
    void continueStroke(const QPoint &pos);
    void endStroke(const QPoint &pos);

    void undo();
    void clear();
    bool hasAnnotations() const;
    int annotationCount() const;

    void paint(QPainter &painter, const QRect &clipRect = QRect()) const;
    QImage composite(const QImage &baseImage, const QRect &region, double scaleX, double scaleY) const;

private:
    Tool m_tool = Tool::Freehand;
    QColor m_color = Qt::red;
    int m_thickness = 2;

    std::vector<Annotation> m_annotations;
    Annotation m_currentAnnotation;
    bool m_drawing = false;
};
