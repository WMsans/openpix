# OpenPix Screenshot Tool -- Design Document

## Overview

OpenPix is a Wayland-native screenshot tool built with C++ and Qt 6. It captures the current screen on launch, presents a fullscreen overlay for region selection, and offers actions: save as PNG, copy to clipboard, OCR text extraction, annotation, and scrolling screenshot.

## Decisions

| Decision | Choice |
|---|---|
| Framework | Qt 6, C++17, CMake |
| OCR engine | RapidOCR (RapidOcrOnnx, ONNX Runtime backend) |
| Annotations | Minimal: freehand draw + rectangle |
| Scrolling screenshot | Manual scroll, template matching stitch via OpenCV |
| Save format | PNG only |
| Launch model | Instant capture on invocation, process exits after action |
| Screen capture | xdg-desktop-portal first, wlr-screencopy-unstable-v1 fallback |
| Multi-monitor | Single output (the monitor the cursor is on) |
| Architecture | Single fullscreen QWidget overlay (Approach A) |

## 1. Application Lifecycle & Screen Capture

### Launch Flow

```
User invokes `openpix` (e.g. via WM keybind)
  -> main() creates QApplication
  -> CaptureManager tries xdg-desktop-portal Screenshot D-Bus call
     -> If portal unavailable/fails: fall back to wlr-screencopy-unstable-v1
  -> Captured frame arrives as QImage
  -> OverlayWidget is created fullscreen with the QImage as background
  -> User interacts with overlay
  -> User completes action (save/copy/OCR/etc.) or presses Escape
  -> Application exits
```

### Screen Capture: Dual Backend

CaptureManager abstracts two capture methods behind a common interface returning a `QImage`.

**Portal path (primary):**
- Calls `org.freedesktop.portal.Screenshot.Screenshot` over D-Bus using `QDBusInterface`
- Sets `interactive: false` to skip the portal's own UI
- Receives a `Response` signal with a file URI to the captured image
- Loads the URI into a `QImage` and deletes the temp file

**wlr-screencopy path (fallback):**
- Connects to the Wayland display, binds `zwlr_screencopy_manager_v1`
- Requests a frame for the output the cursor is on (determined via `wl_output` enumeration and pointer position)
- Receives the frame buffer, converts to `QImage`
- Uses Qt's `QGuiApplication::platformNativeInterface()` to get the `wl_display*`

Portal is tried first because it is compositor-agnostic and handles permissions correctly. wlr-screencopy is the fallback for wlroots compositors without a portal daemon.

## 2. Overlay Widget & Region Selection

### OverlayWidget

A single fullscreen frameless `QWidget` with flags: `Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::BypassWindowManagerHint`. Holds all state and handles all rendering in `paintEvent`.

### Visual Layers (painted in order)

1. **Screenshot background** -- the captured `QImage` at full resolution
2. **Dimming mask** -- semi-transparent dark overlay (`rgba(0,0,0,0.5)`) covering everything outside the selected region, using a QPainterPath with the selection rect subtracted
3. **Selection border** -- dashed rectangle with resize handles at corners and edge midpoints
4. **Annotations** -- freehand paths and rectangles drawn within the selection region
5. **Toolbar** -- QPushButtons positioned near the selection rectangle

### Region Selection State Machine

```
IDLE  -->  DRAWING   (mouse press on empty area starts new selection)
IDLE  -->  MOVING    (mouse press inside existing selection)
IDLE  -->  RESIZING  (mouse press on a handle)

DRAWING  -->  SELECTED  (mouse release finalizes the rectangle)
MOVING   -->  SELECTED  (mouse release)
RESIZING -->  SELECTED  (mouse release)

SELECTED -->  MOVING    (mouse press inside selection)
SELECTED -->  RESIZING  (mouse press on handle)
SELECTED -->  DRAWING   (mouse press outside selection -- resets and starts new)
```

### Resize Handles

8 handles: 4 corners + 4 edge midpoints. Each is 8x8 logical pixels. Hit-testing checks mouse proximity. Cursor shape changes to the appropriate resize arrow on hover.

### Moving the Selection

Mouse press inside selection (not on handle) drags the entire rectangle. Offset between mouse and selection top-left is tracked. Selection is clamped to screenshot bounds.

### Keyboard Shortcuts

- `Escape` -- cancel and quit (or exit annotation mode if active)
- `Enter` -- confirm (equivalent to "Copy" button)
- Arrow keys -- nudge selection by 1px (Shift: 10px)

## 3. Toolbar Actions & Feature Modules

### Toolbar

Horizontal row of `QPushButton`s anchored to the bottom-right edge of the selection rectangle. Repositioned above if near screen bottom. Child widget of OverlayWidget.

| Button | Action |
|---|---|
| Save As | Opens `QFileDialog`, saves selection as PNG |
| Copy | Copies selection to clipboard, exits |
| OCR | Runs RapidOCR on selection, copies text to clipboard, exits |
| Annotate | Enters annotation mode |
| Scroll Capture | Enters scrolling screenshot mode |

### Save As

Crops screenshot `QImage` to selection rect with annotations composited. Opens `QFileDialog::getSaveFileName` with PNG filter. Saves via `QImage::save()`. Exits.

### Copy

Same crop logic. Writes `QImage` to `QClipboard` using `QMimeData` with `image/png`. Exits.

### OCR Module

RapidOcrOnnx is compiled as a static library (CLIB output mode) and linked into the project. ONNX models (det, cls, rec) and keys.txt are bundled.

Flow:
1. Crop selection to `QImage`
2. Convert to `cv::Mat` (format conversion, no deep copy)
3. Call `OcrLite::detect()`
4. Extract recognized text
5. Copy to clipboard
6. Exit

### Annotation Mode

Toolbar swaps to annotation-specific buttons:

| Button | Action |
|---|---|
| Freehand | Draw with mouse (default) |
| Rectangle | Click-drag rectangle outline |
| Color | Preset swatches: red, green, blue, yellow, white, black |
| Thickness | Cycle 2px / 4px / 6px |
| Undo | Remove last annotation |
| Done | Return to main toolbar |

Annotations stored as `QVector<Annotation>`:

```cpp
struct Annotation {
    enum Type { Freehand, Rectangle };
    Type type;
    QColor color;
    int thickness;
    QPainterPath path;    // for Freehand
    QRect rect;           // for Rectangle
};
```

Annotations are painted onto a copy of the cropped screenshot at export time. The original QImage is never modified.

### Scrolling Screenshot Mode

Flow:
1. User clicks "Scroll Capture" -- overlay hides
2. Small floating control panel appears with "Capture Frame" and "Finish" buttons
3. User scrolls manually, clicks "Capture Frame" after each scroll
4. Each click re-captures via CaptureManager and crops to original selection rect
5. "Finish" runs the stitcher
6. Stitched image shown in overlay for save/copy

Stitching algorithm:
- For each consecutive pair of frames, `cv::matchTemplate` with `TM_CCOEFF_NORMED` on bottom strip of frame N vs top of frame N+1 to find overlap offset
- Composite vertically, removing overlap
- Result is one tall `QImage`

## 4. Project Structure, Dependencies & Build System

### Directory Structure

```
openpix/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── CaptureManager.h/cpp
│   ├── OverlayWidget.h/cpp
│   ├── Toolbar.h/cpp
│   ├── AnnotationManager.h/cpp
│   ├── ScrollCapture.h/cpp
│   ├── Stitcher.h/cpp
│   ├── OcrEngine.h/cpp
│   └── protocols/
│       ├── wlr-screencopy-unstable-v1.h/c  (generated)
│       └── ...
├── resources/
│   └── models/
│       ├── ch_PP-OCRv3_det_infer.onnx
│       ├── ch_PP-OCRv3_cls_infer.onnx
│       ├── ch_PP-OCRv3_rec_infer.onnx
│       └── keys.txt
└── protocols/
    └── wlr-screencopy-unstable-v1.xml
```

### Dependencies

| Dependency | Purpose | Source |
|---|---|---|
| Qt 6 (Widgets, DBus, Gui, WaylandClient) | UI, D-Bus, Wayland | System package |
| OpenCV (core, imgproc) | Template matching, QImage/Mat conversion | System package |
| ONNX Runtime | RapidOCR inference | System package or bundled |
| RapidOcrOnnx | OCR detection + recognition | FetchContent, built as static lib |
| wayland-client, wayland-protocols | wlr-screencopy fallback | System package |
| wlr-protocols | Protocol XML | FetchContent or bundled |

### CMake Sketch

```cmake
cmake_minimum_required(VERSION 3.20)
project(openpix LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui DBus WaylandClient)
find_package(OpenCV REQUIRED COMPONENTS core imgproc)
find_package(onnxruntime REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED wayland-client)

# wayland-scanner for protocol code generation
# FetchContent for RapidOcrOnnx

add_executable(openpix ...)
target_link_libraries(openpix
    Qt6::Widgets Qt6::Gui Qt6::DBus Qt6::WaylandClient
    ${OpenCV_LIBS} onnxruntime RapidOcrOnnx ${WAYLAND_LIBRARIES}
)
```

### Error Handling

- **Capture failure (both backends):** `QMessageBox::critical`, exit.
- **OCR failure:** `QMessageBox::warning`. Don't exit -- other features still work.
- **Stitch failure (no overlap):** Fall back to simple vertical concatenation with visible seam. Warn via tooltip.
- **Save failure:** Show error if `QImage::save()` returns false.
- **Escape:** Clean exit at any point.

### Testing

- **Stitcher:** Unit test with known overlapping images
- **OcrEngine:** Unit test with known test image
- **CaptureManager:** Manual testing (requires live Wayland session)
- **Selection state machine:** Unit-testable with synthetic mouse events
- Framework: Qt Test (`QTest`)
