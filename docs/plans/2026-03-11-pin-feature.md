# Pin Feature Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a "pin" feature that spawns floating, draggable, resizable windows for cropped screenshots.

**Architecture:** Create a new `PinnedImageWidget` class (custom QWidget) that displays the image, handles drag anywhere, scroll-to-resize toward mouse cursor, and has a close button. Integrate via a new Pin button in the Toolbar.

**Tech Stack:** Qt6 (QWidget, QImage, QPixmap, QPainter, mouse/wheel events)

---

## Task 1: Create PinnedImageWidget Header

**Files:**
- Create: `src/PinnedImageWidget.h`

**Step 1: Create the header file**

```cpp
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
```

**Step 2: Commit**

```bash
git add src/PinnedImageWidget.h
git commit -m "feat(pin): add PinnedImageWidget header"
```

---

## Task 2: Implement PinnedImageWidget Constructor and Basic Setup

**Files:**
- Create: `src/PinnedImageWidget.cpp`

**Step 1: Create implementation file with constructor**

```cpp
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

    if (width > screenGeometry.width() * 0.8) {
        qreal ratio = (screenGeometry.width() * 0.8) / width;
        width = static_cast<int>(width * ratio);
        height = static_cast<int>(height * ratio);
        m_scale = ratio;
    }
    if (height > screenGeometry.height() * 0.8) {
        qreal ratio = (screenGeometry.height() * 0.8) / height;
        width = static_cast<int>(width * ratio);
        height = static_cast<int>(height * ratio);
        m_scale = ratio;
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
```

**Step 2: Implement updateScaledPixmap helper**

Add to `src/PinnedImageWidget.cpp`:

```cpp
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
```

**Step 3: Implement adjustWindowToContent helper**

Add to `src/PinnedImageWidget.cpp`:

```cpp
void PinnedImageWidget::adjustWindowToContent()
{
    if (m_scaledPixmap.isNull()) {
        return;
    }

    setFixedSize(m_scaledPixmap.size());
    m_closeButtonRect = QRect(width() - CloseButtonSize - 4, 4, CloseButtonSize, CloseButtonSize);
}
```

**Step 4: Commit**

```bash
git add src/PinnedImageWidget.cpp
git commit -m "feat(pin): implement PinnedImageWidget constructor and helpers"
```

---

## Task 3: Implement Paint Event and Close Button

**Files:**
- Modify: `src/PinnedImageWidget.cpp`

**Step 1: Implement paintEvent**

Add to `src/PinnedImageWidget.cpp`:

```cpp
void PinnedImageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!m_scaledPixmap.isNull()) {
        painter.drawPixmap(0, 0, m_scaledPixmap);
    }

    QColor bgColor = m_closeButtonHovered ? QColor(220, 60, 60) : QColor(180, 180, 180, 200);
    painter.setBrush(bgColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(m_closeButtonRect.adjusted(2, 2, -2, -2));

    painter.setPen(Qt::white);
    painter.drawLine(m_closeButtonRect.center() - QPoint(4, 4),
                     m_closeButtonRect.center() + QPoint(4, 4));
    painter.drawLine(m_closeButtonRect.center() + QPoint(-4, 4),
                     m_closeButtonRect.center() + QPoint(4, -4));
}
```

**Step 2: Implement updateCloseButtonHover**

Add to `src/PinnedImageWidget.cpp`:

```cpp
void PinnedImageWidget::updateCloseButtonHover(const QPoint &pos)
{
    bool hovered = m_closeButtonRect.contains(pos);
    if (hovered != m_closeButtonHovered) {
        m_closeButtonHovered = hovered;
        update(m_closeButtonRect);
    }
}
```

**Step 3: Commit**

```bash
git add src/PinnedImageWidget.cpp
git commit -m "feat(pin): implement paint event and close button rendering"
```

---

## Task 4: Implement Mouse Events for Drag and Close

**Files:**
- Modify: `src/PinnedImageWidget.cpp`

**Step 1: Implement mousePressEvent**

Add to `src/PinnedImageWidget.cpp`:

```cpp
void PinnedImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_closeButtonRect.contains(event->pos())) {
            close();
            return;
        }

        m_dragging = true;
        m_dragStart = event->globalPosition().toPoint();
        m_windowStart = pos();
        setCursor(Qt::ClosedHandCursor);
    }
}
```

**Step 2: Implement mouseMoveEvent**

Add to `src/PinnedImageWidget.cpp`:

```cpp
void PinnedImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();
    updateCloseButtonHover(pos);

    if (m_dragging) {
        QPoint delta = event->globalPosition().toPoint() - m_dragStart;
        move(m_windowStart + delta);
    }
}
```

**Step 3: Implement mouseReleaseEvent**

Add to `src/PinnedImageWidget.cpp`:

```cpp
void PinnedImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}
```

**Step 4: Implement leaveEvent**

Add to `src/PinnedImageWidget.cpp`:

```cpp
void PinnedImageWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    if (m_closeButtonHovered) {
        m_closeButtonHovered = false;
        update(m_closeButtonRect);
    }
}
```

**Step 5: Commit**

```bash
git add src/PinnedImageWidget.cpp
git commit -m "feat(pin): implement mouse events for drag and close"
```

---

## Task 5: Implement Wheel Event for Resize

**Files:**
- Modify: `src/PinnedImageWidget.cpp`

**Step 1: Implement wheelEvent**

Add to `src/PinnedImageWidget.cpp`:

```cpp
void PinnedImageWidget::wheelEvent(QWheelEvent *event)
{
    QPoint angleDelta = event->angleDelta();
    if (angleDelta.y() == 0) {
        return;
    }

    qreal delta = (angleDelta.y() > 0) ? ScaleStep : -ScaleStep;
    qreal newScale = qBound(MinScale, m_scale + delta, MaxScale);

    if (qFuzzyCompare(newScale, m_scale)) {
        return;
    }

    QPointF localPos = event->position();
    QPointF globalPos = event->globalPosition();

    qreal scaleRatio = newScale / m_scale;
    m_scale = newScale;

    updateScaledPixmap();
    adjustWindowToContent();

    QPointF newLocalPos(localPos.x() * scaleRatio, localPos.y() * scaleRatio);
    QPoint newWindowPos(static_cast<int>(globalPos.x() - newLocalPos.x()),
                        static_cast<int>(globalPos.y() - newLocalPos.y()));
    move(newWindowPos);
}
```

**Step 2: Commit**

```bash
git add src/PinnedImageWidget.cpp
git commit -m "feat(pin): implement scroll wheel resize toward cursor"
```

---

## Task 6: Update CMakeLists.txt

**Files:**
- Modify: `src/CMakeLists.txt`

**Step 1: Add PinnedImageWidget sources**

Find the source file list in `src/CMakeLists.txt` and add:
- `PinnedImageWidget.h` to headers
- `PinnedImageWidget.cpp` to sources

**Step 2: Build to verify**

```bash
cmake --build build -j$(nproc)
```

Expected: Build succeeds with no errors

**Step 3: Commit**

```bash
git add src/CMakeLists.txt
git commit -m "build(pin): add PinnedImageWidget to CMakeLists"
```

---

## Task 7: Add Pin Button to Toolbar

**Files:**
- Modify: `src/Toolbar.h`
- Modify: `src/Toolbar.cpp`

**Step 1: Add pinClicked signal to Toolbar.h**

In the `signals:` section of `src/Toolbar.h`, add:

```cpp
void pinClicked();
```

**Step 2: Add Pin button to Toolbar.cpp**

In the toolbar button setup section, add a pin button alongside the existing buttons (Save, Copy, OCR, etc.):

```cpp
auto *pinBtn = createButton("Pin", "pin");
connect(pinBtn, &QPushButton::clicked, this, &Toolbar::pinClicked);
m_layout->addWidget(pinBtn);
```

Note: Adjust the button creation pattern to match existing code style in Toolbar.cpp. May need to use an appropriate icon or text label.

**Step 3: Commit**

```bash
git add src/Toolbar.h src/Toolbar.cpp
git commit -m "feat(pin): add Pin button to Toolbar"
```

---

## Task 8: Connect Pin Action in OverlayWidget

**Files:**
- Modify: `src/OverlayWidget.h`
- Modify: `src/OverlayWidget.cpp`

**Step 1: Add onPinClicked slot to OverlayWidget.h**

In the `private slots:` section, add:

```cpp
void onPinClicked();
```

**Step 2: Connect signal in OverlayWidget.cpp constructor**

Find where other toolbar signals are connected and add:

```cpp
connect(m_toolbar, &Toolbar::pinClicked, this, &OverlayWidget::onPinClicked);
```

**Step 3: Implement onPinClicked in OverlayWidget.cpp**

```cpp
void OverlayWidget::onPinClicked()
{
    QImage cropped = croppedImage();
    if (cropped.isNull() || cropped.size().isEmpty()) {
        return;
    }

    auto *pinned = new PinnedImageWidget(cropped);
    pinned->show();

    close();
}
```

**Step 4: Add include at top of OverlayWidget.cpp**

```cpp
#include "PinnedImageWidget.h"
```

**Step 5: Commit**

```bash
git add src/OverlayWidget.h src/OverlayWidget.cpp
git commit -m "feat(pin): connect Pin button to create pinned window"
```

---

## Task 9: Update App Lifecycle in main.cpp

**Files:**
- Modify: `src/main.cpp`

**Step 1: Modify quit behavior**

The current app quits when overlay closes. Change the connection from `cancelled` and after pin actions to only quit when no pinned windows exist.

Option A: Check for PinnedImageWidget instances before quitting:

```cpp
auto shouldQuit = []() {
    const auto &widgets = QApplication::topLevelWidgets();
    for (QWidget *w : widgets) {
        if (qobject_cast<PinnedImageWidget*>(w)) {
            return false;
        }
    }
    return true;
};

QObject::connect(overlay, &OverlayWidget::cancelled, [=]() {
    if (shouldQuit()) {
        qApp->quit();
    }
});
```

Update similar logic for other close paths.

**Step 2: Add include**

```cpp
#include "PinnedImageWidget.h"
```

**Step 3: Build and verify**

```bash
cmake --build build -j$(nproc)
```

Expected: Build succeeds

**Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat(pin): update app lifecycle to keep running with pinned windows"
```

---

## Task 10: Build and Manual Test

**Step 1: Clean build**

```bash
rm -rf build && cmake -B build && cmake --build build -j$(nproc)
```

Expected: Build succeeds with no errors

**Step 2: Manual testing checklist**

Run the application and verify:
- [ ] Screenshot capture works as before
- [ ] Pin button appears in toolbar
- [ ] Clicking Pin creates a floating window with cropped image
- [ ] Overlay closes after pinning
- [ ] App stays running after overlay closes
- [ ] Pinned window can be dragged anywhere
- [ ] Scroll wheel resizes window toward cursor
- [ ] Close button (X) closes the pinned window
- [ ] Multiple pins can be created simultaneously
- [ ] App quits when last pinned window is closed

**Step 3: Final commit (if any fixes needed)**

```bash
git add -A
git commit -m "fix(pin): address issues from manual testing"
```

---

## Summary

| Task | Description | Files |
|------|-------------|-------|
| 1 | Create header | `src/PinnedImageWidget.h` |
| 2 | Constructor + helpers | `src/PinnedImageWidget.cpp` |
| 3 | Paint + close button | `src/PinnedImageWidget.cpp` |
| 4 | Mouse events | `src/PinnedImageWidget.cpp` |
| 5 | Wheel event | `src/PinnedImageWidget.cpp` |
| 6 | CMakeLists | `src/CMakeLists.txt` |
| 7 | Toolbar button | `src/Toolbar.h`, `src/Toolbar.cpp` |
| 8 | OverlayWidget connection | `src/OverlayWidget.h`, `src/OverlayWidget.cpp` |
| 9 | App lifecycle | `src/main.cpp` |
| 10 | Build + test | All files |
