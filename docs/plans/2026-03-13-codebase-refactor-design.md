# Codebase Refactor Design

## Problem

The codebase has grown to ~3,100 lines across 19 source files with several maintainability issues:

- **God objects**: `OverlayWidget` (630 lines) and `Toolbar` (453 lines) have too many responsibilities
- **Code duplication**: `LongImageWidget` duplicates save/copy/annotation-toolbar logic from `Toolbar`
- **Duplicate OcrEngine instances**: `main.cpp` and `Toolbar::onOcr()` each create independent instances
- **Scattered lifecycle management**: quit logic spread across 4+ files inconsistently
- **Debug noise**: `std::cout` statements and temp file writes throughout
- **Tight coupling**: `Toolbar` and `OverlayWidget` call each other's methods directly

## Approach

Layered architecture with subdirectories, service extraction, signal-based decoupling, debug cleanup, and tests for extracted logic.

## Directory Structure

```
src/
  main.cpp

  capture/
    CaptureManager.h/cpp      # Screenshot capture (Portal + wlr) -- largely unchanged
    ScrollCapture.h/cpp        # Scroll capture coordination -- largely unchanged
    Stitcher.h/cpp             # Image stitching -- unchanged

  ui/
    OverlayWidget.h/cpp        # Region selection + annotation overlay (slimmed down)
    Toolbar.h/cpp              # Toolbar widget (UI only, no business logic)
    LongImageWidget.h/cpp      # Long image viewer (uses shared services)
    PinnedImageWidget.h/cpp    # Pinned window -- unchanged

  services/
    ImageExporter.h/cpp        # NEW: consolidated save-to-file + copy-to-clipboard
    OcrService.h/cpp           # Renamed from OcrEngine, single instance via DI

  annotation/
    AnnotationManager.h/cpp    # Annotation state/rendering -- unchanged
    AnnotationConstants.h      # NEW: shared Colors[], Thicknesses[], type aliases
    AnnotationToolbar.h/cpp    # NEW: reusable annotation toolbar widget
```

## Component Designs

### ImageExporter

Consolidates duplicated save and copy logic from `Toolbar` and `LongImageWidget`.

```cpp
class ImageExporter {
public:
    static bool saveToFile(const QImage &image, QWidget *parent);
    static bool copyToClipboard(const QImage &image);
};
```

- `saveToFile` opens a `QFileDialog`, saves the image, returns success/failure.
- `copyToClipboard` writes the image as PNG to `wl-copy --type image/png`.
- Both methods are static since they need no state.

Eliminates ~110 lines of duplicated save/copy code.

### OcrService

Single instance created in `main.cpp`, passed to `Toolbar` via dependency injection.

```cpp
class OcrService {
public:
    bool init(const QString &modelsDir);
    QString recognize(const QImage &image);
    QString lastError() const;
};
```

- Renamed from `OcrEngine` to reflect its role.
- Eliminates the duplicate static `OcrEngine` instance in `Toolbar::onOcr()`.
- Background-thread execution stays in `Toolbar::onOcr()` (UI concern).

### AnnotationConstants & AnnotationToolbar

**AnnotationConstants.h** extracts shared constants:

```cpp
namespace Annotation {
    constexpr int ColorCount = 6;
    inline const QColor Colors[ColorCount] = { ... };
    constexpr int ThicknessCount = 3;
    constexpr int Thicknesses[ThicknessCount] = { 3, 6, 12 };
}
```

**AnnotationToolbar** extracts the duplicated toolbar setup:

```cpp
class AnnotationToolbar : public QWidget {
    Q_OBJECT
public:
    explicit AnnotationToolbar(QWidget *parent = nullptr);
    QColor currentColor() const;
    int currentThickness() const;

signals:
    void freehandSelected();
    void rectangleSelected();
    void colorChanged(QColor color);
    void thicknessChanged(int thickness);
    void undoRequested();
    void doneRequested();
};
```

Both `Toolbar` and `LongImageWidget` embed an `AnnotationToolbar` instead of building their own. Eliminates ~110 lines of duplicated toolbar setup and state management.

### Toolbar/OverlayWidget Decoupling

**Before:** Bidirectional -- `Toolbar` holds `OverlayWidget*`, `OverlayWidget` calls Toolbar methods directly.

**After:** Unidirectional -- `OverlayWidget` creates `Toolbar` and connects to its signals.

- Remove `OverlayWidget*` from Toolbar. Toolbar emits signals only: `saveRequested()`, `copyRequested()`, `ocrRequested()`, `annotateRequested()`, `scrollRequested()`, `pinRequested()`, `quitRequested()`.
- OverlayWidget handles hotkeys by calling its own methods (using `ImageExporter` and `OcrService` directly) instead of reaching into Toolbar.
- Remove `QCoreApplication::sendEvent()` key forwarding from Toolbar -- let Qt's event propagation handle it naturally.

### Debug Cleanup

- Remove all `std::cout` statements from `OverlayWidget`, `Toolbar`, and `OcrService`.
- Remove temp file writes (`/tmp/selection_debug.png`, `/tmp/ocr_debug_*.png`).
- Replace with `qDebug()` only where genuinely useful (model path resolution, Wayland errors).
- Use `qWarning()` for actual error conditions.

### Application Lifecycle

Route all quit decisions through `main.cpp`:

- `OverlayWidget` emits `finished()` when done (after save, copy, cancel).
- `LongImageWidget` emits `finished()` instead of calling `qApp->quit()`.
- `main.cpp` connects to `finished()` and applies pinned-window check in one place.

## Testing

New test targets alongside existing `test_stitcher`:

- **test_annotation_manager**: stroke lifecycle, undo, color/thickness state, composite rendering.
- **test_image_exporter**: PNG encoding, file write operations.

All tests use QTest framework following the existing `test_stitcher` pattern.

## What Does Not Change

- `CaptureManager` internals (both capture strategies are related concerns)
- `Stitcher` (already clean and well-tested)
- `PinnedImageWidget` (already focused)
- `AnnotationManager` logic (already a clean model, just moves to a subdirectory)
- The overall signal-driven architecture pattern
