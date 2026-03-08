#pragma once

#include <QWidget>
#include <QImage>
#include <QRect>
#include <QPoint>

class Toolbar;

class OverlayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OverlayWidget(const QImage &screenshot, QWidget *parent = nullptr);

    QImage croppedImage() const;

    enum class State {
        Idle,
        Drawing,
        Moving,
        Resizing,
    };

    enum class Handle {
        None,
        TopLeft, Top, TopRight,
        Left, Right,
        BottomLeft, Bottom, BottomRight,
    };

signals:
    void regionSelected(const QRect &region);
    void cancelled();
    void saveRequested();
    void copyRequested();
    void ocrRequested();
    void annotateRequested();
    void scrollCaptureRequested();

private slots:
    void onSaveRequested();
    void onCopyRequested();
    void onOcrRequested();
    void onAnnotateRequested();
    void onScrollCaptureRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    Handle handleAt(const QPoint &pos) const;
    Qt::CursorShape cursorForHandle(Handle handle) const;
    QRect handleRect(Handle handle) const;
    void updateToolbarPosition();

    QImage m_screenshot;
    QRect m_selection;
    State m_state = State::Idle;
    Handle m_activeHandle = Handle::None;
    QPoint m_dragStart;
    QPoint m_selectionStartPos;

    Toolbar *m_toolbar = nullptr;

    static constexpr int HandleSize = 8;
    static constexpr int MinSelectionSize = 5;
};
