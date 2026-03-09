# Long Image Viewer Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the broken scroll-capture result display with a dedicated `LongImageWidget` that supports scrolling, zooming, panning, annotations, save, and copy.

**Architecture:** A new `LongImageWidget` class handles tall stitched images. It manages its own viewport (zoom + offset), renders only the visible portion of the image, stores annotations in image-space coordinates, and builds a simple inline toolbar. The `OverlayWidget::startScrollCapture()` lambda is modified to create a `LongImageWidget` instead of re-showing the overlay.

**Tech Stack:** C++17, Qt6 (Widgets, Gui), existing `AnnotationManager`

---

### Task 1: Create LongImageWidget header

**Files:**
- Create: `src/LongImageWidget.h`

**Step 1: Write the header file**

```cpp
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
```

**Step 2: Verify the file compiles as a header**

No standalone compilation needed; this will be verified in Task 2.

**Step 3: Commit**

```bash
git add src/LongImageWidget.h
git commit -m "feat: add LongImageWidget header for scroll capture viewer"
```

---

### Task 2: Implement LongImageWidget core — constructor, viewport, painting

**Files:**
- Create: `src/LongImageWidget.cpp`
- Modify: `CMakeLists.txt:89-98` (add source file)

**Step 1: Write the initial implementation with constructor and paintEvent**

Create `src/LongImageWidget.cpp`:

```cpp
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
```

**Step 2: Add LongImageWidget.cpp to CMakeLists.txt**

In `CMakeLists.txt`, add `src/LongImageWidget.cpp` to the `add_executable` list after `src/ScrollCapture.cpp` (line 97):

Change line 97 from:
```
    src/ScrollCapture.cpp
```
to:
```
    src/ScrollCapture.cpp
    src/LongImageWidget.cpp
```

**Step 3: Build to verify compilation**

Run: `cmake --build build -j$(nproc)`
Expected: Compiles successfully (linker errors for missing methods are OK at this stage since we haven't written all methods yet)

**Step 4: Commit**

```bash
git add src/LongImageWidget.cpp CMakeLists.txt
git commit -m "feat: implement LongImageWidget core - constructor, viewport, painting"
```

---

### Task 3: Implement navigation — scroll, pan, zoom, keyboard

**Files:**
- Modify: `src/LongImageWidget.cpp` (add mouse/wheel/key event handlers)

**Step 1: Add the event handlers**

Append to `src/LongImageWidget.cpp`:

```cpp
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
```

**Step 2: Build to verify**

Run: `cmake --build build -j$(nproc)`
Expected: Compiles (linker errors for toolbar methods still expected)

**Step 3: Commit**

```bash
git add src/LongImageWidget.cpp
git commit -m "feat: implement LongImageWidget navigation - scroll, pan, zoom, keyboard"
```

---

### Task 4: Implement toolbar and save/copy/annotate actions

**Files:**
- Modify: `src/LongImageWidget.cpp` (add setupToolbar and action methods)

**Step 1: Add toolbar setup and action implementations**

Append to `src/LongImageWidget.cpp`:

```cpp
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
        return m_annotationManager->composite(m_image, m_image.rect());
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
```

**Step 2: Build to verify full compilation**

Run: `cmake --build build -j$(nproc)`
Expected: Compiles and links successfully with no errors.

**Step 3: Commit**

```bash
git add src/LongImageWidget.cpp
git commit -m "feat: implement LongImageWidget toolbar, save, copy, and annotation actions"
```

---

### Task 5: Wire ScrollCapture to LongImageWidget

**Files:**
- Modify: `src/OverlayWidget.cpp:1-5` (add include)
- Modify: `src/OverlayWidget.cpp:491-498` (change finished lambda)

**Step 1: Add include for LongImageWidget**

At the top of `src/OverlayWidget.cpp`, add after the existing includes (after line 4 `#include "ScrollCapture.h"`):

```cpp
#include "LongImageWidget.h"
```

**Step 2: Replace the ScrollCapture::finished lambda**

In `src/OverlayWidget.cpp`, replace lines 491-498:

```cpp
    connect(scrollCapture, &ScrollCapture::finished, this, [this](const QImage &stitchedImage) {
        m_screenshot = stitchedImage;
        m_selection = QRect(0, 0, width(), height());
        show();
        setFocus();
        updateToolbarPosition();
        update();
    });
```

With:

```cpp
    connect(scrollCapture, &ScrollCapture::finished, this, [this](const QImage &stitchedImage) {
        auto *viewer = new LongImageWidget(stitchedImage);
        connect(viewer, &QWidget::destroyed, this, []() {
            qApp->quit();
        });
        close();
    });
```

This creates a `LongImageWidget` with the stitched image and closes the overlay. When the viewer is destroyed (on quit), the app exits.

**Step 3: Build and verify**

Run: `cmake --build build -j$(nproc)`
Expected: Compiles and links successfully.

**Step 4: Commit**

```bash
git add src/OverlayWidget.cpp
git commit -m "feat: wire scroll capture result to LongImageWidget instead of OverlayWidget"
```

---

### Task 6: Manual smoke test and fix any issues

**Files:**
- Possibly: `src/LongImageWidget.cpp` (fixes based on testing)

**Step 1: Build the project**

Run: `cmake --build build -j$(nproc)`
Expected: Clean build with no errors or warnings.

**Step 2: Run existing tests**

Run: `cd build && ctest --output-on-failure`
Expected: All existing tests pass (test_stitcher).

**Step 3: Manual test checklist**

Run the app: `./build/openpix`

Test the following:
1. Normal screenshot flow still works (select region, save, copy) -- should be unchanged
2. Scroll capture: select region, click Scroll, scroll the target app, click Finish
3. Long image viewer appears with the stitched image
4. Scroll wheel moves through the image vertically
5. Ctrl+scroll zooms in/out anchored to cursor
6. Click-and-drag pans the image
7. Click Annotate, draw freehand and rectangles on the image
8. Annotations stay in correct position when scrolling/zooming
9. Undo removes the last annotation
10. Done exits annotation mode
11. Save As saves the full image with annotations
12. Copy copies the full image with annotations
13. Escape exits annotation mode first, then quits the app
14. Quit button exits

**Step 4: Fix any issues found during testing**

Address any bugs found. Common issues to watch for:
- Annotation coordinates not mapping correctly through zoom/pan
- Toolbar not visible or positioned incorrectly
- Offset clamping causing jumpy behavior at image boundaries
- Zoom anchor point calculation off

**Step 5: Commit fixes**

```bash
git add -A
git commit -m "fix: address issues found during long image viewer testing"
```
