#pragma once

#include <QWidget>
#include <QImage>
#include <QPointF>
#include <QPoint>
#include <QColor>
#include <memory>

class AnnotationManager;
class QPushButton;
class QHBoxLayout;

class LongImageWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LongImageWidget(const QImage &image, QWidget *parent = nullptr);

    QImage compositeImage() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QPointF widgetToImage(const QPointF &widgetPos) const;
    QPointF imageToWidget(const QPointF &imagePos) const;
    void clampOffset();
    void updateZoomAtPoint(double newZoom, const QPointF &anchor);

    void setupToolbar();
    void onSaveClicked();
    void onCopyClicked();
    void onAnnotateClicked();
    void onAnnotationDone();
    void onUndoClicked();
    void onFreehandClicked();
    void onRectClicked();
    void onColorClicked();
    void onThicknessClicked();

    QImage m_image;
    double m_zoom = 1.0;
    QPointF m_offset;

    bool m_dragging = false;
    QPoint m_dragStart;
    QPointF m_offsetAtDragStart;

    bool m_annotating = false;
    std::unique_ptr<AnnotationManager> m_annotationManager;

    QWidget *m_toolbar = nullptr;
    QWidget *m_mainBar = nullptr;
    QWidget *m_annotationBar = nullptr;
    QPushButton *m_colorBtn = nullptr;
    QPushButton *m_thicknessBtn = nullptr;

    QColor m_annotationColor;
    int m_annotationThickness = 2;
    int m_colorIndex = 0;
    int m_thicknessIndex = 0;

    static const QColor Colors[6];
    static constexpr int Thicknesses[] = {2, 4, 6};
    static constexpr int ColorCount = 6;
    static constexpr int ThicknessCount = 3;

    static constexpr double MinZoomFactor = 0.1;
    static constexpr double MaxZoomMultiplier = 5.0;
};
