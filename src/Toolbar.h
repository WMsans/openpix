#pragma once

#include <QWidget>
#include <QColor>

class OverlayWidget;
class QPushButton;
class QHBoxLayout;

class Toolbar : public QWidget
{
    Q_OBJECT

public:
    enum class Mode {
        Main,
        Annotation
    };
    enum class AnnotationTool {
        Freehand,
        Rectangle
    };

    explicit Toolbar(OverlayWidget *overlay, QWidget *parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void showMainToolbar();
    void showAnnotationToolbar();

    QColor annotationColor() const { return m_annotationColor; }
    int annotationThickness() const { return m_annotationThickness; }
    AnnotationTool annotationTool() const { return m_annotationTool; }

signals:
    void saveRequested();
    void copyRequested();
    void ocrRequested();
    void annotateRequested();
    void scrollCaptureRequested();
    void pinClicked();
    void annotationToolChanged(AnnotationTool tool);
    void undoRequested();
    void annotationDone();
    void quitRequested();

private:
    void setupMainToolbar();
    void setupAnnotationToolbar();
    void applyStyles();
    void cycleColor();
    void cycleThickness();
    void onFreehandClicked();
    void onRectClicked();
    void onUndoClicked();
    void onDoneClicked();
    void onSaveClicked();
    void onCopyClicked();
    void onOcr();
    void updateColorButton();
    void updateThicknessButton();

    OverlayWidget *m_overlay;
    QHBoxLayout *m_mainLayout = nullptr;
    QHBoxLayout *m_annotationLayout = nullptr;
    QWidget *m_mainWidget = nullptr;
    QWidget *m_annotationWidget = nullptr;

    QPushButton *m_freehandBtn = nullptr;
    QPushButton *m_rectBtn = nullptr;
    QPushButton *m_colorBtn = nullptr;
    QPushButton *m_thicknessBtn = nullptr;
    QPushButton *m_undoBtn = nullptr;
    QPushButton *m_doneBtn = nullptr;

    Mode m_mode = Mode::Main;
    AnnotationTool m_annotationTool = AnnotationTool::Freehand;

    QColor m_annotationColor;
    int m_annotationThickness = 2;
    int m_colorIndex = 0;
    int m_thicknessIndex = 0;

    static const QColor Colors[6];
    static constexpr int Thicknesses[] = {2, 4, 6};
    static constexpr int ColorCount = 6;
    static constexpr int ThicknessCount = 3;
};
