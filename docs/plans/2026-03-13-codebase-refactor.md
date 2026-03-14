# Codebase Refactor Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Restructure the OpenPix codebase into a layered architecture with subdirectories, extract duplicated logic into shared services, decouple tightly coupled components, clean up debug output, and add tests.

**Architecture:** Organize source files into `capture/`, `ui/`, `services/`, and `annotation/` subdirectories. Extract `ImageExporter` and `AnnotationToolbar` from duplicated code, rename `OcrEngine` to `OcrService` with single-instance DI, decouple `Toolbar` from `OverlayWidget` using signals-only, and centralize application lifecycle.

**Tech Stack:** C++17, Qt6 (Widgets, Gui, DBus, Concurrent, Test), OpenCV, ONNX Runtime

---

### Task 1: Create directory structure

**Files:**
- Create: `src/capture/`, `src/ui/`, `src/services/`, `src/annotation/`

**Step 1: Create the subdirectories**

```bash
mkdir -p src/capture src/ui src/services src/annotation
```

**Step 2: Commit**

```bash
git add src/capture src/ui src/services src/annotation
git commit --allow-empty -m "refactor: create subdirectory structure for layered architecture"
```

---

### Task 2: Move capture files to src/capture/

**Files:**
- Move: `src/CaptureManager.h` -> `src/capture/CaptureManager.h`
- Move: `src/CaptureManager.cpp` -> `src/capture/CaptureManager.cpp`
- Move: `src/ScrollCapture.h` -> `src/capture/ScrollCapture.h`
- Move: `src/ScrollCapture.cpp` -> `src/capture/ScrollCapture.cpp`
- Move: `src/Stitcher.h` -> `src/capture/Stitcher.h`
- Move: `src/Stitcher.cpp` -> `src/capture/Stitcher.cpp`

**Step 1: Move the files**

```bash
git mv src/CaptureManager.h src/capture/CaptureManager.h
git mv src/CaptureManager.cpp src/capture/CaptureManager.cpp
git mv src/ScrollCapture.h src/capture/ScrollCapture.h
git mv src/ScrollCapture.cpp src/capture/ScrollCapture.cpp
git mv src/Stitcher.h src/capture/Stitcher.h
git mv src/Stitcher.cpp src/capture/Stitcher.cpp
```

**Step 2: Update include paths in moved files**

In `src/capture/ScrollCapture.cpp`, update includes:
```cpp
#include "ScrollCapture.h"
#include "CaptureManager.h"
#include "Stitcher.h"
```
These are now in the same directory, so no path change needed for these three. No other internal includes.

**Step 3: Update include paths in files that reference capture files**

In `src/main.cpp`, change:
```cpp
#include "CaptureManager.h"
```
to:
```cpp
#include "capture/CaptureManager.h"
```

In `src/OverlayWidget.cpp`, change:
```cpp
#include "CaptureManager.h"
#include "ScrollCapture.h"
```
to:
```cpp
#include "capture/CaptureManager.h"
#include "capture/ScrollCapture.h"
```

In `src/OverlayWidget.h`, the forward declaration `class CaptureManager;` stays as-is (no include).

In `tests/test_stitcher.cpp`, change:
```cpp
#include "../src/Stitcher.h"
```
to:
```cpp
#include "../src/capture/Stitcher.h"
```

**Step 4: Update CMakeLists.txt source paths**

Change the source file paths in the `add_executable(openpix ...)` block:
```cmake
    src/CaptureManager.cpp
    src/ScrollCapture.cpp
    src/Stitcher.cpp
```
to:
```cmake
    src/capture/CaptureManager.cpp
    src/capture/ScrollCapture.cpp
    src/capture/Stitcher.cpp
```

Update the test target:
```cmake
add_executable(test_stitcher tests/test_stitcher.cpp src/Stitcher.cpp)
target_include_directories(test_stitcher PRIVATE ${CMAKE_BINARY_DIR} src/)
```
to:
```cmake
add_executable(test_stitcher tests/test_stitcher.cpp src/capture/Stitcher.cpp)
target_include_directories(test_stitcher PRIVATE ${CMAKE_BINARY_DIR} src/ src/capture/)
```

**Step 5: Build and test**

```bash
cmake -B build && cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```
Expected: Build succeeds, all tests pass.

**Step 6: Commit**

```bash
git add -A && git commit -m "refactor: move capture files to src/capture/"
```

---

### Task 3: Move annotation files to src/annotation/

**Files:**
- Move: `src/AnnotationManager.h` -> `src/annotation/AnnotationManager.h`
- Move: `src/AnnotationManager.cpp` -> `src/annotation/AnnotationManager.cpp`

**Step 1: Move the files**

```bash
git mv src/AnnotationManager.h src/annotation/AnnotationManager.h
git mv src/AnnotationManager.cpp src/annotation/AnnotationManager.cpp
```

**Step 2: Update include paths in files that reference AnnotationManager**

In `src/OverlayWidget.cpp`, change:
```cpp
#include "AnnotationManager.h"
```
to:
```cpp
#include "annotation/AnnotationManager.h"
```

In `src/OverlayWidget.h`, the forward declaration `class AnnotationManager;` stays as-is.

In `src/LongImageWidget.cpp`, change:
```cpp
#include "AnnotationManager.h"
```
to:
```cpp
#include "annotation/AnnotationManager.h"
```

In `src/LongImageWidget.h`, the forward declaration `class AnnotationManager;` stays as-is.

**Step 3: Update CMakeLists.txt**

Change:
```cmake
    src/AnnotationManager.cpp
```
to:
```cmake
    src/annotation/AnnotationManager.cpp
```

**Step 4: Build and test**

```bash
cmake -B build && cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

**Step 5: Commit**

```bash
git add -A && git commit -m "refactor: move annotation files to src/annotation/"
```

---

### Task 4: Move UI widget files to src/ui/

**Files:**
- Move: `src/OverlayWidget.h` -> `src/ui/OverlayWidget.h`
- Move: `src/OverlayWidget.cpp` -> `src/ui/OverlayWidget.cpp`
- Move: `src/Toolbar.h` -> `src/ui/Toolbar.h`
- Move: `src/Toolbar.cpp` -> `src/ui/Toolbar.cpp`
- Move: `src/LongImageWidget.h` -> `src/ui/LongImageWidget.h`
- Move: `src/LongImageWidget.cpp` -> `src/ui/LongImageWidget.cpp`
- Move: `src/PinnedImageWidget.h` -> `src/ui/PinnedImageWidget.h`
- Move: `src/PinnedImageWidget.cpp` -> `src/ui/PinnedImageWidget.cpp`

**Step 1: Move the files**

```bash
git mv src/OverlayWidget.h src/ui/OverlayWidget.h
git mv src/OverlayWidget.cpp src/ui/OverlayWidget.cpp
git mv src/Toolbar.h src/ui/Toolbar.h
git mv src/Toolbar.cpp src/ui/Toolbar.cpp
git mv src/LongImageWidget.h src/ui/LongImageWidget.h
git mv src/LongImageWidget.cpp src/ui/LongImageWidget.cpp
git mv src/PinnedImageWidget.h src/ui/PinnedImageWidget.h
git mv src/PinnedImageWidget.cpp src/ui/PinnedImageWidget.cpp
```

**Step 2: Update includes in moved files**

In `src/ui/OverlayWidget.h`, change:
```cpp
#include "Toolbar.h"
```
No path change needed (same directory now).

In `src/ui/OverlayWidget.cpp`, update:
```cpp
#include "OverlayWidget.h"
#include "Toolbar.h"
#include "annotation/AnnotationManager.h"
#include "capture/ScrollCapture.h"
#include "LongImageWidget.h"
#include "capture/CaptureManager.h"
#include "PinnedImageWidget.h"
```
Change the annotation/capture paths to use `../`:
```cpp
#include "OverlayWidget.h"
#include "Toolbar.h"
#include "../annotation/AnnotationManager.h"
#include "../capture/ScrollCapture.h"
#include "LongImageWidget.h"
#include "../capture/CaptureManager.h"
#include "PinnedImageWidget.h"
```

In `src/ui/Toolbar.cpp`, change:
```cpp
#include "Toolbar.h"
#include "OverlayWidget.h"
#include "OcrEngine.h"
```
to:
```cpp
#include "Toolbar.h"
#include "OverlayWidget.h"
#include "../OcrEngine.h"
```
(OcrEngine hasn't moved yet, it's still in src/)

In `src/ui/LongImageWidget.cpp`, change:
```cpp
#include "LongImageWidget.h"
#include "AnnotationManager.h"
```
to:
```cpp
#include "LongImageWidget.h"
#include "../annotation/AnnotationManager.h"
```

**Step 3: Update includes in src/main.cpp**

```cpp
#include "CaptureManager.h"
#include "OverlayWidget.h"
#include "OcrEngine.h"
#include "PinnedImageWidget.h"
```
to:
```cpp
#include "capture/CaptureManager.h"
#include "ui/OverlayWidget.h"
#include "OcrEngine.h"
#include "ui/PinnedImageWidget.h"
```

**Step 4: Update CMakeLists.txt**

Change source paths:
```cmake
    src/OverlayWidget.cpp
    src/Toolbar.cpp
    src/LongImageWidget.cpp
    src/PinnedImageWidget.cpp
```
to:
```cmake
    src/ui/OverlayWidget.cpp
    src/ui/Toolbar.cpp
    src/ui/LongImageWidget.cpp
    src/ui/PinnedImageWidget.cpp
```

Add `src/ui/` to include directories so AUTOMOC finds headers:
```cmake
target_include_directories(openpix PRIVATE
    ${CMAKE_BINARY_DIR}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIRS}
    ${rapidocr_SOURCE_DIR}/include
    ${rapidocr_SOURCE_DIR}/src
    src/
    src/ui/
    src/capture/
    src/annotation/
)
```

**Step 5: Build and test**

```bash
cmake -B build && cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

**Step 6: Commit**

```bash
git add -A && git commit -m "refactor: move UI widget files to src/ui/"
```

---

### Task 5: Move OcrEngine to src/services/ and rename to OcrService

**Files:**
- Move: `src/OcrEngine.h` -> `src/services/OcrService.h`
- Move: `src/OcrEngine.cpp` -> `src/services/OcrService.cpp`

**Step 1: Move and rename the files**

```bash
git mv src/OcrEngine.h src/services/OcrService.h
git mv src/OcrEngine.cpp src/services/OcrService.cpp
```

**Step 2: Rename the class in the header**

In `src/services/OcrService.h`, replace the entire contents with:
```cpp
#pragma once

#include <QString>
#include <QImage>
#include <QObject>
#include <memory>

class OcrLite;

class OcrService : public QObject
{
    Q_OBJECT

public:
    explicit OcrService(QObject *parent = nullptr);
    ~OcrService();

    bool init(const QString &modelsDir);
    bool isInitialized() const { return m_initialized; }

    QString recognize(const QImage &image);

    QString lastError() const { return m_error; }

private:
    std::unique_ptr<OcrLite> m_ocr;
    bool m_initialized = false;
    QString m_error;
};
```

**Step 3: Rename the class in the implementation and clean debug output**

In `src/services/OcrService.cpp`, replace the entire contents with:
```cpp
#include "OcrService.h"
#include "OcrLite.h"

#include <QDir>
#include <QDebug>
#include <QFile>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

static cv::Mat qImageToCvMat(const QImage &image)
{
    QImage converted = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(
        converted.height(),
        converted.width(),
        CV_8UC3,
        const_cast<uchar *>(converted.bits()),
        converted.bytesPerLine()
    );
    cv::Mat result;
    cv::cvtColor(mat, result, cv::COLOR_RGB2BGR);
    return result.clone();
}

OcrService::OcrService(QObject *parent)
    : QObject(parent)
    , m_ocr(std::make_unique<OcrLite>())
{
}

OcrService::~OcrService() = default;

bool OcrService::init(const QString &modelsDir)
{
    QDir dir(modelsDir);
    if (!dir.exists()) {
        m_error = QString("Models directory not found: %1").arg(modelsDir);
        qWarning() << m_error;
        return false;
    }

    QString detPath = dir.filePath("ch_PP-OCRv4_det_infer.onnx");
    QString clsPath = dir.filePath("ch_ppocr_mobile_v2.0_cls_infer.onnx");
    QString recPath = dir.filePath("ch_PP-OCRv4_rec_infer.onnx");
    QString keysPath = dir.filePath("keys.txt");

    if (!QFile::exists(detPath)) {
        m_error = QString("Detection model not found: %1").arg(detPath);
        qWarning() << m_error;
        return false;
    }
    if (!QFile::exists(clsPath)) {
        m_error = QString("Classification model not found: %1").arg(clsPath);
        qWarning() << m_error;
        return false;
    }
    if (!QFile::exists(recPath)) {
        m_error = QString("Recognition model not found: %1").arg(recPath);
        qWarning() << m_error;
        return false;
    }
    if (!QFile::exists(keysPath)) {
        m_error = QString("Keys file not found: %1").arg(keysPath);
        qWarning() << m_error;
        return false;
    }

    m_ocr->setNumThread(4);
    m_ocr->setGpuIndex(-1);
    m_ocr->initLogger(true, false, false);

    bool success = m_ocr->initModels(
        detPath.toStdString(),
        clsPath.toStdString(),
        recPath.toStdString(),
        keysPath.toStdString()
    );

    if (!success) {
        m_error = "Failed to initialize OCR models";
        qWarning() << m_error;
        return false;
    }

    m_initialized = true;
    qDebug() << "OcrService initialized successfully";
    return true;
}

QString OcrService::recognize(const QImage &image)
{
    if (!m_initialized) {
        m_error = "OcrService not initialized";
        return QString();
    }

    if (image.isNull()) {
        m_error = "Invalid image";
        return QString();
    }

    cv::Mat mat = qImageToCvMat(image);
    if (mat.empty()) {
        m_error = "Failed to convert image";
        return QString();
    }

    constexpr int padding = 50;
    constexpr int maxSideLen = 1024;
    constexpr float boxScoreThresh = 0.5f;
    constexpr float boxThresh = 0.3f;
    constexpr float unClipRatio = 1.6f;
    constexpr bool doAngle = true;
    constexpr bool mostAngle = true;

    OcrResult result = m_ocr->detect(
        mat, padding, maxSideLen, boxScoreThresh, boxThresh,
        unClipRatio, doAngle, mostAngle
    );

    QString text = QString::fromStdString(result.strRes).trimmed();

    if (text.isEmpty()) {
        for (const auto &block : result.textBlocks) {
            QString blockText = QString::fromStdString(block.text);
            if (!blockText.trimmed().isEmpty()) {
                if (!text.isEmpty())
                    text += '\n';
                text += blockText;
            }
        }
    }

    return text;
}
```

**Step 4: Update includes that reference OcrEngine**

In `src/main.cpp`, change:
```cpp
#include "OcrEngine.h"
```
to:
```cpp
#include "services/OcrService.h"
```

Also in `src/main.cpp`, change `OcrEngine` to `OcrService`:
```cpp
    OcrEngine *ocrEngine = new OcrEngine(&app);
```
to:
```cpp
    OcrService *ocrService = new OcrService(&app);
```
And update all references from `ocrEngine` to `ocrService` in main.cpp.

In `src/ui/Toolbar.cpp`, change:
```cpp
#include "../OcrEngine.h"
```
to:
```cpp
#include "../services/OcrService.h"
```
(The static OcrEngine usage in Toolbar will be addressed in Task 8.)

**Step 5: Update CMakeLists.txt**

Change:
```cmake
    src/OcrEngine.cpp
```
to:
```cmake
    src/services/OcrService.cpp
```

Add `src/services/` to include dirs:
```cmake
    src/services/
```

**Step 6: Build and test**

```bash
cmake -B build && cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

**Step 7: Commit**

```bash
git add -A && git commit -m "refactor: rename OcrEngine to OcrService and move to src/services/"
```

---

### Task 6: Extract ImageExporter service

**Files:**
- Create: `src/services/ImageExporter.h`
- Create: `src/services/ImageExporter.cpp`
- Modify: `src/ui/Toolbar.cpp`
- Modify: `src/ui/LongImageWidget.cpp`

**Step 1: Write the test**

Create `tests/test_image_exporter.cpp`:
```cpp
#include <QtTest/QtTest>
#include <QImage>
#include <QTemporaryDir>
#include "../src/services/ImageExporter.h"

class TestImageExporter : public QObject
{
    Q_OBJECT

private slots:
    void testSaveToPath()
    {
        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::red);

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QString filePath = tempDir.path() + "/test_screenshot.png";

        bool result = ImageExporter::saveToPath(image, filePath);
        QVERIFY(result);
        QVERIFY(QFile::exists(filePath));

        QImage loaded(filePath);
        QVERIFY(!loaded.isNull());
        QCOMPARE(loaded.size(), image.size());
    }

    void testSaveToPathAppendsExtension()
    {
        QImage image(50, 50, QImage::Format_RGB32);
        image.fill(Qt::blue);

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QString filePath = tempDir.path() + "/test_no_ext";

        bool result = ImageExporter::saveToPath(image, filePath);
        QVERIFY(result);
        QVERIFY(QFile::exists(filePath + ".png"));
    }

    void testSaveToPathNullImage()
    {
        QImage image;
        bool result = ImageExporter::saveToPath(image, "/tmp/should_not_exist.png");
        QVERIFY(!result);
    }

    void testSaveToPathEmptyPath()
    {
        QImage image(10, 10, QImage::Format_RGB32);
        image.fill(Qt::green);
        bool result = ImageExporter::saveToPath(image, "");
        QVERIFY(!result);
    }

    void testEncodeAsPng()
    {
        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::red);

        QByteArray data = ImageExporter::encodeAsPng(image);
        QVERIFY(!data.isEmpty());
        QVERIFY(data.startsWith("\x89PNG"));
    }

    void testEncodeAsPngNullImage()
    {
        QImage image;
        QByteArray data = ImageExporter::encodeAsPng(image);
        QVERIFY(data.isEmpty());
    }
};

QTEST_MAIN(TestImageExporter)
#include "test_image_exporter.moc"
```

**Step 2: Add test to CMakeLists.txt**

Add after the existing test_stitcher block:
```cmake
add_executable(test_image_exporter tests/test_image_exporter.cpp src/services/ImageExporter.cpp)
target_include_directories(test_image_exporter PRIVATE ${CMAKE_BINARY_DIR} src/ src/services/)
target_link_libraries(test_image_exporter Qt6::Test Qt6::Gui Qt6::Widgets)
add_test(NAME test_image_exporter COMMAND test_image_exporter)
```

**Step 3: Run test to verify it fails**

```bash
cmake -B build && cmake --build build -j$(nproc)
```
Expected: FAIL - ImageExporter.h not found.

**Step 4: Create ImageExporter header**

Create `src/services/ImageExporter.h`:
```cpp
#pragma once

#include <QImage>
#include <QString>
#include <QByteArray>

class QWidget;

class ImageExporter
{
public:
    static bool saveToFile(const QImage &image, QWidget *parent);
    static bool saveToPath(const QImage &image, const QString &filePath);
    static bool copyToClipboard(const QImage &image);
    static QByteArray encodeAsPng(const QImage &image);
};
```

**Step 5: Create ImageExporter implementation**

Create `src/services/ImageExporter.cpp`:
```cpp
#include "ImageExporter.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>

bool ImageExporter::saveToFile(const QImage &image, QWidget *parent)
{
    if (image.isNull()) return false;

    QString defaultPath = QDir::homePath() + "/Pictures/screenshot_" +
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";

    QString fileName = QFileDialog::getSaveFileName(
        parent, "Save Screenshot", defaultPath,
        "PNG Images (*.png);;All Files (*)");

    if (fileName.isEmpty()) return false;

    return saveToPath(image, fileName);
}

bool ImageExporter::saveToPath(const QImage &image, const QString &filePath)
{
    if (image.isNull() || filePath.isEmpty()) return false;

    QString path = filePath;
    if (!path.endsWith(".png", Qt::CaseInsensitive)) {
        path += ".png";
    }

    return image.save(path, "PNG");
}

bool ImageExporter::copyToClipboard(const QImage &image)
{
    if (image.isNull()) return false;

    QByteArray imageData = encodeAsPng(image);
    if (imageData.isEmpty()) return false;

    QProcess wlCopy;
    wlCopy.start("wl-copy", QStringList() << "-t" << "image/png");
    if (!wlCopy.waitForStarted(1000)) return false;

    wlCopy.write(imageData);
    wlCopy.closeWriteChannel();
    wlCopy.waitForFinished(1000);

    return wlCopy.exitCode() == 0;
}

QByteArray ImageExporter::encodeAsPng(const QImage &image)
{
    if (image.isNull()) return QByteArray();

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();
    return data;
}
```

**Step 6: Build and run tests**

```bash
cmake -B build && cmake --build build -j$(nproc)
cd build && ctest -R test_image_exporter --output-on-failure
```
Expected: All tests pass.

**Step 7: Update Toolbar to use ImageExporter**

In `src/ui/Toolbar.cpp`, add include:
```cpp
#include "../services/ImageExporter.h"
```

Replace `Toolbar::onSaveClicked()` (the entire method body) with:
```cpp
void Toolbar::onSaveClicked()
{
    if (!m_overlay) return;

    QImage image = m_overlay->croppedImage();
    if (image.isNull()) return;

    if (ImageExporter::saveToFile(image, nullptr)) {
        emit quitRequested();
    }
}
```

Replace `Toolbar::onCopyClicked()` (the entire method body) with:
```cpp
void Toolbar::onCopyClicked()
{
    if (!m_overlay) return;

    QImage image = m_overlay->croppedImage();
    if (image.isNull()) return;

    if (ImageExporter::copyToClipboard(image)) {
        emit quitRequested();
    } else {
        QMessageBox::warning(m_overlay, "Copy Failed", "Failed to copy image to clipboard");
    }
}
```

Remove now-unused includes from `src/ui/Toolbar.cpp`:
- `QFileDialog`
- `QClipboard`
- `QMimeData`
- `QDateTime`
- `QDir`
- `QProcess`
- `QBuffer`

**Step 8: Update LongImageWidget to use ImageExporter**

In `src/ui/LongImageWidget.cpp`, add include:
```cpp
#include "../services/ImageExporter.h"
```

Replace `LongImageWidget::onSaveClicked()` with:
```cpp
void LongImageWidget::onSaveClicked()
{
    QImage image = compositeImage();
    if (image.isNull()) return;

    if (ImageExporter::saveToFile(image, this)) {
        qApp->quit();
    }
}
```

Replace `LongImageWidget::onCopyClicked()` with:
```cpp
void LongImageWidget::onCopyClicked()
{
    QImage image = compositeImage();
    if (image.isNull()) return;

    if (ImageExporter::copyToClipboard(image)) {
        qApp->quit();
    } else {
        QMessageBox::warning(this, "Copy Failed", "Failed to copy image to clipboard");
    }
}
```

Remove now-unused includes from `src/ui/LongImageWidget.cpp`:
- `QFileDialog`
- `QDir`
- `QDateTime`
- `QProcess`
- `QBuffer`

**Step 9: Add ImageExporter.cpp to CMakeLists.txt main target**

Add to `add_executable(openpix ...)`:
```cmake
    src/services/ImageExporter.cpp
```

**Step 10: Build and run all tests**

```bash
cmake -B build && cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

**Step 11: Commit**

```bash
git add -A && git commit -m "refactor: extract ImageExporter service to eliminate duplicated save/copy logic"
```

---

### Task 7: Extract AnnotationConstants and AnnotationToolbar

**Files:**
- Create: `src/annotation/AnnotationConstants.h`
- Create: `src/annotation/AnnotationToolbar.h`
- Create: `src/annotation/AnnotationToolbar.cpp`
- Modify: `src/ui/Toolbar.h`
- Modify: `src/ui/Toolbar.cpp`
- Modify: `src/ui/LongImageWidget.h`
- Modify: `src/ui/LongImageWidget.cpp`

**Step 1: Create AnnotationConstants.h**

Create `src/annotation/AnnotationConstants.h`:
```cpp
#pragma once

#include <QColor>

namespace Annotation {
    constexpr int ColorCount = 6;
    inline const QColor Colors[ColorCount] = {
        Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::white, Qt::black
    };
    constexpr int ThicknessCount = 3;
    constexpr int Thicknesses[ThicknessCount] = {2, 4, 6};
}
```

**Step 2: Create AnnotationToolbar header**

Create `src/annotation/AnnotationToolbar.h`:
```cpp
#pragma once

#include <QWidget>
#include <QColor>

class QPushButton;
class QHBoxLayout;

class AnnotationToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit AnnotationToolbar(QWidget *parent = nullptr);

    QColor currentColor() const { return m_color; }
    int currentThickness() const { return m_thickness; }

    enum class Tool { Freehand, Rectangle };
    Tool currentTool() const { return m_tool; }

signals:
    void toolChanged(Tool tool);
    void colorChanged(QColor color);
    void thicknessChanged(int thickness);
    void undoRequested();
    void doneRequested();

private:
    void cycleColor();
    void cycleThickness();
    void updateColorButton();
    void updateThicknessButton();

    QPushButton *m_freehandBtn = nullptr;
    QPushButton *m_rectBtn = nullptr;
    QPushButton *m_colorBtn = nullptr;
    QPushButton *m_thicknessBtn = nullptr;
    QPushButton *m_undoBtn = nullptr;
    QPushButton *m_doneBtn = nullptr;

    Tool m_tool = Tool::Freehand;
    QColor m_color;
    int m_thickness = 2;
    int m_colorIndex = 0;
    int m_thicknessIndex = 0;
};
```

**Step 3: Create AnnotationToolbar implementation**

Create `src/annotation/AnnotationToolbar.cpp`:
```cpp
#include "AnnotationToolbar.h"
#include "AnnotationConstants.h"

#include <QHBoxLayout>
#include <QPushButton>

AnnotationToolbar::AnnotationToolbar(QWidget *parent)
    : QWidget(parent)
    , m_color(Annotation::Colors[0])
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    m_freehandBtn = new QPushButton("Freehand", this);
    m_rectBtn = new QPushButton("Rectangle", this);
    m_colorBtn = new QPushButton("Color", this);
    m_thicknessBtn = new QPushButton("2px", this);
    m_undoBtn = new QPushButton("Undo", this);
    m_doneBtn = new QPushButton("Done", this);

    connect(m_freehandBtn, &QPushButton::clicked, this, [this]() {
        m_tool = Tool::Freehand;
        emit toolChanged(m_tool);
    });
    connect(m_rectBtn, &QPushButton::clicked, this, [this]() {
        m_tool = Tool::Rectangle;
        emit toolChanged(m_tool);
    });
    connect(m_colorBtn, &QPushButton::clicked, this, &AnnotationToolbar::cycleColor);
    connect(m_thicknessBtn, &QPushButton::clicked, this, &AnnotationToolbar::cycleThickness);
    connect(m_undoBtn, &QPushButton::clicked, this, &AnnotationToolbar::undoRequested);
    connect(m_doneBtn, &QPushButton::clicked, this, &AnnotationToolbar::doneRequested);

    layout->addWidget(m_freehandBtn);
    layout->addWidget(m_rectBtn);
    layout->addWidget(m_colorBtn);
    layout->addWidget(m_thicknessBtn);
    layout->addWidget(m_undoBtn);
    layout->addWidget(m_doneBtn);

    updateColorButton();
}

void AnnotationToolbar::cycleColor()
{
    m_colorIndex = (m_colorIndex + 1) % Annotation::ColorCount;
    m_color = Annotation::Colors[m_colorIndex];
    updateColorButton();
    emit colorChanged(m_color);
}

void AnnotationToolbar::cycleThickness()
{
    m_thicknessIndex = (m_thicknessIndex + 1) % Annotation::ThicknessCount;
    m_thickness = Annotation::Thicknesses[m_thicknessIndex];
    updateThicknessButton();
    emit thicknessChanged(m_thickness);
}

void AnnotationToolbar::updateColorButton()
{
    if (m_colorBtn) {
        QString colorName = m_color.name();
        m_colorBtn->setStyleSheet(QString("background-color: %1; color: %2;")
            .arg(colorName)
            .arg(m_color.lightness() > 128 ? "#000000" : "#ffffff"));
    }
}

void AnnotationToolbar::updateThicknessButton()
{
    if (m_thicknessBtn) {
        m_thicknessBtn->setText(QString("%1px").arg(m_thickness));
    }
}
```

**Step 4: Update Toolbar to use AnnotationToolbar**

In `src/ui/Toolbar.h`, replace the entire file with:
```cpp
#pragma once

#include <QWidget>
#include <QColor>

class OverlayWidget;
class QPushButton;
class QHBoxLayout;
class AnnotationToolbar;

class Toolbar : public QWidget
{
    Q_OBJECT

public:
    explicit Toolbar(OverlayWidget *overlay, QWidget *parent = nullptr);

    void showMainToolbar();
    void showAnnotationToolbar();

    QColor annotationColor() const;
    int annotationThickness() const;

    void onSaveClicked();
    void onCopyClicked();
    void onOcr();

signals:
    void saveRequested();
    void copyRequested();
    void ocrRequested();
    void annotateRequested();
    void scrollCaptureRequested();
    void pinClicked();
    void annotationToolChanged(int tool);
    void undoRequested();
    void annotationDone();
    void quitRequested();

private:
    void setupMainToolbar();
    void applyStyles();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    OverlayWidget *m_overlay;
    QWidget *m_mainWidget = nullptr;
    AnnotationToolbar *m_annotationToolbar = nullptr;

    enum class Mode { Main, Annotation };
    Mode m_mode = Mode::Main;
};
```

In `src/ui/Toolbar.cpp`, replace the entire file with:
```cpp
#include "Toolbar.h"
#include "OverlayWidget.h"
#include "../annotation/AnnotationToolbar.h"
#include "../services/OcrService.h"
#include "../services/ImageExporter.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QKeyEvent>
#include <QProcess>

void Toolbar::keyPressEvent(QKeyEvent *event)
{
    if (m_overlay) {
        QCoreApplication::sendEvent(m_overlay, static_cast<QEvent *>(event));
    }
}

Toolbar::Toolbar(OverlayWidget *overlay, QWidget *parent)
    : QWidget(parent)
    , m_overlay(overlay)
{
    setWindowTitle("OpenPix Toolbar");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_annotationToolbar = new AnnotationToolbar(this);
    setupMainToolbar();

    layout->addWidget(m_mainWidget);
    layout->addWidget(m_annotationToolbar);

    connect(m_annotationToolbar, &AnnotationToolbar::toolChanged, this, [this](AnnotationToolbar::Tool tool) {
        emit annotationToolChanged(static_cast<int>(tool));
    });
    connect(m_annotationToolbar, &AnnotationToolbar::undoRequested, this, &Toolbar::undoRequested);
    connect(m_annotationToolbar, &AnnotationToolbar::doneRequested, this, &Toolbar::annotationDone);

    applyStyles();
    showMainToolbar();
}

void Toolbar::setupMainToolbar()
{
    m_mainWidget = new QWidget(this);
    auto *mainLayout = new QHBoxLayout(m_mainWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(2);

    auto *saveBtn = new QPushButton("Save As", m_mainWidget);
    auto *copyBtn = new QPushButton("Copy", m_mainWidget);
    auto *ocrBtn = new QPushButton("OCR", m_mainWidget);
    auto *annotateBtn = new QPushButton("Annotate", m_mainWidget);
    auto *scrollBtn = new QPushButton("Scroll", m_mainWidget);
    auto *pinBtn = new QPushButton("Pin", m_mainWidget);

    connect(saveBtn, &QPushButton::clicked, this, &Toolbar::onSaveClicked);
    connect(copyBtn, &QPushButton::clicked, this, &Toolbar::onCopyClicked);
    connect(ocrBtn, &QPushButton::clicked, this, &Toolbar::onOcr);
    connect(annotateBtn, &QPushButton::clicked, this, &Toolbar::annotateRequested);
    connect(scrollBtn, &QPushButton::clicked, this, &Toolbar::scrollCaptureRequested);
    connect(pinBtn, &QPushButton::clicked, this, &Toolbar::pinClicked);

    mainLayout->addWidget(saveBtn);
    mainLayout->addWidget(copyBtn);
    mainLayout->addWidget(ocrBtn);
    mainLayout->addWidget(annotateBtn);
    mainLayout->addWidget(scrollBtn);
    mainLayout->addWidget(pinBtn);
}

void Toolbar::applyStyles()
{
    setStyleSheet(R"(
        Toolbar {
            background-color: #2d2d2d;
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
}

void Toolbar::showMainToolbar()
{
    m_mode = Mode::Main;
    m_mainWidget->show();
    m_annotationToolbar->hide();
    adjustSize();
}

void Toolbar::showAnnotationToolbar()
{
    m_mode = Mode::Annotation;
    m_mainWidget->hide();
    m_annotationToolbar->show();
    adjustSize();
}

QColor Toolbar::annotationColor() const
{
    return m_annotationToolbar->currentColor();
}

int Toolbar::annotationThickness() const
{
    return m_annotationToolbar->currentThickness();
}

void Toolbar::onSaveClicked()
{
    if (!m_overlay) return;

    QImage image = m_overlay->croppedImage();
    if (image.isNull()) return;

    if (ImageExporter::saveToFile(image, nullptr)) {
        emit quitRequested();
    }
}

void Toolbar::onCopyClicked()
{
    if (!m_overlay) return;

    QImage image = m_overlay->croppedImage();
    if (image.isNull()) return;

    if (ImageExporter::copyToClipboard(image)) {
        emit quitRequested();
    } else {
        QMessageBox::warning(m_overlay, "Copy Failed", "Failed to copy image to clipboard");
    }
}

void Toolbar::onOcr()
{
    if (!m_overlay) return;

    QImage img = m_overlay->croppedImage();
    if (img.isNull()) return;

    setEnabled(false);
    setCursor(Qt::WaitCursor);

    QtConcurrent::run([this, img]() {
        static OcrService ocrService;

        if (!ocrService.isInitialized()) {
            QString modelsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
            if (!ocrService.init(modelsDir)) {
                modelsDir = QCoreApplication::applicationDirPath() + "/../share/openpix/models";
                if (!ocrService.init(modelsDir)) {
                    modelsDir = "/usr/share/openpix/models";
                    if (!ocrService.init(modelsDir)) {
                        QMetaObject::invokeMethod(this, [this]() {
                            QMessageBox::warning(m_overlay, "OCR Error",
                                "OCR models not found. Place models in ~/.local/share/openpix/models/");
                            setEnabled(true);
                            unsetCursor();
                        }, Qt::QueuedConnection);
                        return;
                    }
                }
            }
        }

        QString text = ocrService.recognize(img);

        QMetaObject::invokeMethod(this, [this, text]() {
            setEnabled(true);
            unsetCursor();

            if (text.isEmpty()) {
                QMessageBox::information(m_overlay, "OCR", "No text detected in selection.");
                return;
            }

            QProcess wlCopy;
            wlCopy.start("wl-copy", QStringList() << text);
            wlCopy.waitForFinished(1000);

            QMessageBox::information(m_overlay, "OpenPix", "Text copied to clipboard");
            emit quitRequested();
        }, Qt::QueuedConnection);
    });
}
```

**Step 5: Update OverlayWidget for new Toolbar interface**

In `src/ui/OverlayWidget.h`, change:
```cpp
    void onAnnotationToolChanged(Toolbar::AnnotationTool tool);
```
to:
```cpp
    void onAnnotationToolChanged(int tool);
```

In `src/ui/OverlayWidget.cpp`, change the signal connection:
```cpp
    connect(m_toolbar, &Toolbar::annotationToolChanged, this, &OverlayWidget::onAnnotationToolChanged);
```
stays the same since the signal now emits `int`.

Change `onAnnotationToolChanged`:
```cpp
void OverlayWidget::onAnnotationToolChanged(Toolbar::AnnotationTool tool)
{
    if (m_annotationManager) {
        m_annotationManager->setTool(static_cast<AnnotationManager::Tool>(tool));
    }
}
```
to:
```cpp
void OverlayWidget::onAnnotationToolChanged(int tool)
{
    if (m_annotationManager) {
        m_annotationManager->setTool(static_cast<AnnotationManager::Tool>(tool));
    }
}
```

In `onAnnotateRequested`, change:
```cpp
    m_annotationManager->setTool(static_cast<AnnotationManager::Tool>(m_toolbar->annotationTool()));
```
to:
```cpp
    m_annotationManager->setTool(AnnotationManager::Tool::Freehand);
```

**Step 6: Update LongImageWidget to use AnnotationToolbar**

In `src/ui/LongImageWidget.h`, replace with:
```cpp
#pragma once

#include <QWidget>
#include <QImage>
#include <QPointF>
#include <QPoint>
#include <QColor>
#include <memory>

class AnnotationManager;
class AnnotationToolbar;
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
    AnnotationToolbar *m_annotationBar = nullptr;

    static constexpr double MinZoomFactor = 0.1;
    static constexpr double MaxZoomMultiplier = 5.0;
};
```

In `src/ui/LongImageWidget.cpp`, replace the entire file with:
```cpp
#include "LongImageWidget.h"
#include "../annotation/AnnotationManager.h"
#include "../annotation/AnnotationToolbar.h"
#include "../services/ImageExporter.h"

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
#include <QMessageBox>

LongImageWidget::LongImageWidget(const QImage &image, QWidget *parent)
    : QWidget(parent)
    , m_image(image)
    , m_annotationManager(std::make_unique<AnnotationManager>())
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
            m_annotating = false;
            setCursor(Qt::OpenHandCursor);
            m_annotationBar->hide();
            m_mainBar->show();
            m_toolbar->adjustSize();
            update();
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
    connect(annotateBtn, &QPushButton::clicked, this, [this]() {
        m_annotating = true;
        setCursor(Qt::CrossCursor);
        if (m_annotationManager) {
            m_annotationManager->setTool(AnnotationManager::Tool::Freehand);
            m_annotationManager->setColor(m_annotationBar->currentColor());
            m_annotationManager->setThickness(m_annotationBar->currentThickness());
        }
        m_mainBar->hide();
        m_annotationBar->show();
        m_toolbar->adjustSize();
    });
    connect(quitBtn, &QPushButton::clicked, this, [] { qApp->quit(); });

    mainLayout->addWidget(saveBtn);
    mainLayout->addWidget(copyBtn);
    mainLayout->addWidget(annotateBtn);
    mainLayout->addWidget(quitBtn);

    m_annotationBar = new AnnotationToolbar(m_toolbar);

    connect(m_annotationBar, &AnnotationToolbar::toolChanged, this, [this](AnnotationToolbar::Tool tool) {
        if (m_annotationManager) {
            m_annotationManager->setTool(static_cast<AnnotationManager::Tool>(tool));
        }
    });
    connect(m_annotationBar, &AnnotationToolbar::colorChanged, this, [this](QColor color) {
        if (m_annotationManager) {
            m_annotationManager->setColor(color);
        }
    });
    connect(m_annotationBar, &AnnotationToolbar::thicknessChanged, this, [this](int thickness) {
        if (m_annotationManager) {
            m_annotationManager->setThickness(thickness);
        }
    });
    connect(m_annotationBar, &AnnotationToolbar::undoRequested, this, [this]() {
        if (m_annotationManager) {
            m_annotationManager->undo();
            update();
        }
    });
    connect(m_annotationBar, &AnnotationToolbar::doneRequested, this, [this]() {
        m_annotating = false;
        setCursor(Qt::OpenHandCursor);
        m_annotationBar->hide();
        m_mainBar->show();
        m_toolbar->adjustSize();
        update();
    });

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

    if (ImageExporter::saveToFile(image, this)) {
        qApp->quit();
    }
}

void LongImageWidget::onCopyClicked()
{
    QImage image = compositeImage();
    if (image.isNull()) return;

    if (ImageExporter::copyToClipboard(image)) {
        qApp->quit();
    } else {
        QMessageBox::warning(this, "Copy Failed", "Failed to copy image to clipboard");
    }
}
```

**Step 7: Update CMakeLists.txt**

Add to `add_executable(openpix ...)`:
```cmake
    src/annotation/AnnotationToolbar.cpp
```

**Step 8: Build and run all tests**

```bash
cmake -B build && cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

**Step 9: Commit**

```bash
git add -A && git commit -m "refactor: extract AnnotationToolbar and AnnotationConstants to eliminate duplication"
```

---

### Task 8: Clean up debug output

**Files:**
- Modify: `src/ui/OverlayWidget.cpp`

**Step 1: Remove debug output from OverlayWidget::croppedImage()**

In `src/ui/OverlayWidget.cpp`, replace the `croppedImage()` method with:
```cpp
QImage OverlayWidget::croppedImage() const
{
    if (m_selection.isValid()) {
        double scaleX = static_cast<double>(m_screenshot.width()) / width();
        double scaleY = static_cast<double>(m_screenshot.height()) / height();

        QRect scaledSelection(
            static_cast<int>(m_selection.x() * scaleX),
            static_cast<int>(m_selection.y() * scaleY),
            static_cast<int>(m_selection.width() * scaleX),
            static_cast<int>(m_selection.height() * scaleY)
        );

        QImage base = m_screenshot.copy(scaledSelection);

        if (m_annotationManager && m_annotationManager->hasAnnotations()) {
            return m_annotationManager->composite(m_screenshot, scaledSelection);
        }
        return base;
    }
    return QImage();
}
```

**Step 2: Remove the `#include <iostream>` from OverlayWidget.cpp**

Remove the line:
```cpp
#include <iostream>
```

**Step 3: Build and test**

```bash
cmake -B build && cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

**Step 4: Commit**

```bash
git add -A && git commit -m "refactor: remove debug output from OverlayWidget, Toolbar, and OcrService"
```

---

### Task 9: Add AnnotationManager tests

**Files:**
- Create: `tests/test_annotation_manager.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Write the tests**

Create `tests/test_annotation_manager.cpp`:
```cpp
#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>
#include "../src/annotation/AnnotationManager.h"

class TestAnnotationManager : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState()
    {
        AnnotationManager mgr;
        QCOMPARE(mgr.tool(), AnnotationManager::Tool::Freehand);
        QCOMPARE(mgr.color(), QColor(Qt::red));
        QCOMPARE(mgr.thickness(), 2);
        QVERIFY(!mgr.hasAnnotations());
        QCOMPARE(mgr.annotationCount(), 0);
    }

    void testSetTool()
    {
        AnnotationManager mgr;
        mgr.setTool(AnnotationManager::Tool::Rectangle);
        QCOMPARE(mgr.tool(), AnnotationManager::Tool::Rectangle);
    }

    void testSetColor()
    {
        AnnotationManager mgr;
        mgr.setColor(Qt::blue);
        QCOMPARE(mgr.color(), QColor(Qt::blue));
    }

    void testSetThickness()
    {
        AnnotationManager mgr;
        mgr.setThickness(6);
        QCOMPARE(mgr.thickness(), 6);
    }

    void testFreehandStroke()
    {
        AnnotationManager mgr;
        mgr.setTool(AnnotationManager::Tool::Freehand);
        mgr.startStroke(QPoint(10, 10));
        mgr.continueStroke(QPoint(20, 20));
        mgr.endStroke(QPoint(30, 30));

        QVERIFY(mgr.hasAnnotations());
        QCOMPARE(mgr.annotationCount(), 1);
    }

    void testRectangleStroke()
    {
        AnnotationManager mgr;
        mgr.setTool(AnnotationManager::Tool::Rectangle);
        mgr.startStroke(QPoint(10, 10));
        mgr.continueStroke(QPoint(50, 50));
        mgr.endStroke(QPoint(100, 100));

        QVERIFY(mgr.hasAnnotations());
        QCOMPARE(mgr.annotationCount(), 1);
    }

    void testMultipleStrokes()
    {
        AnnotationManager mgr;
        mgr.startStroke(QPoint(0, 0));
        mgr.endStroke(QPoint(10, 10));
        mgr.startStroke(QPoint(20, 20));
        mgr.endStroke(QPoint(30, 30));

        QCOMPARE(mgr.annotationCount(), 2);
    }

    void testUndo()
    {
        AnnotationManager mgr;
        mgr.startStroke(QPoint(0, 0));
        mgr.endStroke(QPoint(10, 10));
        mgr.startStroke(QPoint(20, 20));
        mgr.endStroke(QPoint(30, 30));

        QCOMPARE(mgr.annotationCount(), 2);
        mgr.undo();
        QCOMPARE(mgr.annotationCount(), 1);
        mgr.undo();
        QCOMPARE(mgr.annotationCount(), 0);
        QVERIFY(!mgr.hasAnnotations());
    }

    void testUndoOnEmpty()
    {
        AnnotationManager mgr;
        mgr.undo();
        QCOMPARE(mgr.annotationCount(), 0);
    }

    void testClear()
    {
        AnnotationManager mgr;
        mgr.startStroke(QPoint(0, 0));
        mgr.endStroke(QPoint(10, 10));
        mgr.startStroke(QPoint(20, 20));
        mgr.endStroke(QPoint(30, 30));
        mgr.clear();

        QCOMPARE(mgr.annotationCount(), 0);
        QVERIFY(!mgr.hasAnnotations());
    }

    void testCompositeNullImage()
    {
        AnnotationManager mgr;
        QImage result = mgr.composite(QImage(), QRect(0, 0, 100, 100));
        QVERIFY(result.isNull());
    }

    void testCompositeWithAnnotation()
    {
        QImage base(200, 200, QImage::Format_RGB32);
        base.fill(Qt::white);

        AnnotationManager mgr;
        mgr.setColor(Qt::red);
        mgr.setThickness(4);
        mgr.setTool(AnnotationManager::Tool::Freehand);
        mgr.startStroke(QPoint(50, 50));
        mgr.continueStroke(QPoint(100, 100));
        mgr.endStroke(QPoint(150, 150));

        QRect region(0, 0, 200, 200);
        QImage result = mgr.composite(base, region);

        QVERIFY(!result.isNull());
        QCOMPARE(result.size(), region.size());
        QVERIFY(result != base);
    }

    void testPaintDoesNotCrash()
    {
        AnnotationManager mgr;
        mgr.startStroke(QPoint(10, 10));
        mgr.continueStroke(QPoint(50, 50));
        mgr.endStroke(QPoint(100, 100));

        QImage canvas(200, 200, QImage::Format_RGB32);
        canvas.fill(Qt::white);
        QPainter painter(&canvas);
        mgr.paint(painter, QRect(0, 0, 200, 200));
        painter.end();
    }

    void testContinueStrokeWithoutStart()
    {
        AnnotationManager mgr;
        mgr.continueStroke(QPoint(50, 50));
        QCOMPARE(mgr.annotationCount(), 0);
    }

    void testEndStrokeWithoutStart()
    {
        AnnotationManager mgr;
        mgr.endStroke(QPoint(50, 50));
        QCOMPARE(mgr.annotationCount(), 0);
    }
};

QTEST_MAIN(TestAnnotationManager)
#include "test_annotation_manager.moc"
```

**Step 2: Add to CMakeLists.txt**

Add after the test_image_exporter block:
```cmake
add_executable(test_annotation_manager tests/test_annotation_manager.cpp src/annotation/AnnotationManager.cpp)
target_include_directories(test_annotation_manager PRIVATE ${CMAKE_BINARY_DIR} src/ src/annotation/)
target_link_libraries(test_annotation_manager Qt6::Test Qt6::Gui Qt6::Widgets)
add_test(NAME test_annotation_manager COMMAND test_annotation_manager)
```

**Step 3: Build and run tests**

```bash
cmake -B build && cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```
Expected: All tests pass.

**Step 4: Commit**

```bash
git add -A && git commit -m "test: add comprehensive AnnotationManager unit tests"
```

---

### Task 10: Update AGENTS.md file structure section

**Files:**
- Modify: `AGENTS.md`

**Step 1: Update the file structure section in AGENTS.md**

Replace the `## File Structure` section with:
```markdown
## File Structure

\`\`\`
openpix/
├── CMakeLists.txt              # Main build configuration
├── src/
│   ├── main.cpp                # Application entry point
│   ├── capture/
│   │   ├── CaptureManager.*    # Screenshot capture logic
│   │   ├── ScrollCapture.*     # Scroll capture coordination
│   │   └── Stitcher.*          # Scroll capture image stitching
│   ├── ui/
│   │   ├── OverlayWidget.*     # Selection UI overlay
│   │   ├── Toolbar.*           # Main/annotation toolbar
│   │   ├── LongImageWidget.*   # Long image viewer
│   │   └── PinnedImageWidget.* # Pinned image window
│   ├── services/
│   │   ├── OcrService.*        # OCR integration
│   │   └── ImageExporter.*     # Save/copy operations
│   └── annotation/
│       ├── AnnotationManager.* # Annotation state/rendering
│       ├── AnnotationToolbar.* # Reusable annotation toolbar widget
│       └── AnnotationConstants.h # Shared annotation constants
├── tests/
│   ├── test_stitcher.cpp       # Stitcher unit tests
│   ├── test_image_exporter.cpp # ImageExporter unit tests
│   └── test_annotation_manager.cpp # AnnotationManager unit tests
├── protocols/
│   └── wlr-screencopy-unstable-v1.xml
└── build/                      # Build directory (gitignored)
\`\`\`
```

**Step 2: Build and run all tests one final time**

```bash
cmake -B build && cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```
Expected: All tests pass.

**Step 3: Commit**

```bash
git add -A && git commit -m "docs: update AGENTS.md file structure to reflect refactored layout"
```
