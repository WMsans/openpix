# Long Image Viewer for Scroll Capture

## Problem

After scroll capture, the stitched image (e.g., 800x8000 pixels) is displayed in the same OverlayWidget designed for screen-sized screenshots. OverlayWidget stretches the image to fill the screen via `painter.drawImage(rect(), m_screenshot)`, producing a distorted, unreadable result. The aspect ratio mismatch makes the image useless for inspection, and there is no way to scroll, pan, or zoom.

## Approach

Create a new `LongImageWidget` class -- a dedicated fullscreen widget for viewing and interacting with tall stitched images. This replaces the current behavior of stuffing the stitched image back into OverlayWidget.

Alternatives considered:
- **Refactor OverlayWidget with mode system**: Rejected because OverlayWidget is already complex (5 states, selection, annotations) and the two modes share almost no rendering logic.
- **QScrollArea wrapping a QLabel/QPixmap**: Rejected due to memory concerns with very large pixmaps, limited rendering control, and visual inconsistency (scrollbars).

## Architecture

### LongImageWidget

A top-level fullscreen `QWidget` that owns:
- The stitched `QImage`
- Viewport state: zoom factor, offset (top-left of visible area in image coordinates)
- An `AnnotationManager` instance (reused from existing codebase)
- A simple toolbar (Save, Copy, Annotate, Quit)

### Core State

| Member | Type | Purpose |
|--------|------|---------|
| `m_image` | `QImage` | Full stitched image |
| `m_zoom` | `double` | Current zoom factor (1.0 = fit-width) |
| `m_offset` | `QPointF` | Viewport offset in image coordinates |
| `m_annotating` | `bool` | Whether annotation mode is active |
| `m_dragging` | `bool` | Whether user is panning |
| `m_dragStart` | `QPoint` | Mouse position at drag start |
| `m_annotations` | `AnnotationManager` | Annotation state and rendering |

### Coordinate Mapping

Two helper methods handle all translation between widget (screen) and image coordinate spaces:
- `widgetToImage(QPointF)` -- accounts for zoom and offset
- `imageToWidget(QPointF)` -- inverse transform

Used for: painting, annotation point storage, hit testing.

### Rendering

`paintEvent` computes the visible rectangle in image coordinates from `m_offset` and `m_zoom`, then calls `painter.drawImage(targetRect, m_image, sourceRect)` to draw only the visible portion. Annotations are rendered on top, with image-space coordinates converted to widget-space.

## Navigation

### Initial View

Zoom is set so the image width fits the widget width. Offset starts at (0, 0) -- top of image visible.

### Scroll Wheel

Adjusts `m_offset.y()` for vertical scrolling. Scroll amount is scaled by zoom for consistent feel. Offset is clamped to image bounds.

### Ctrl+Scroll Zoom

Adjusts `m_zoom`, anchored to cursor position (the image point under the cursor stays fixed). Zoom is clamped between fit-width level and 5x native resolution.

### Drag to Pan

Left-click drag adjusts `m_offset` by the mouse delta (scaled by zoom). Cursor changes to grab hand. Disabled during annotation mode.

## Annotations

### Image-Space Storage

All annotation coordinates are stored in image space (converted via `widgetToImage()` at input time). This makes annotations zoom/pan-independent and correct at any viewport position.

### Drawing

When annotating, left-click draws instead of panning. Scroll wheel still works for navigation. Cursor shows crosshair.

### Rendering

In `paintEvent`, annotation points are converted from image-space to widget-space via `imageToWidget()` before drawing. Line thickness is stored in image-space pixels.

### Compositing

`AnnotationManager::composite()` works without modification since annotations are already in image coordinates.

### Toolbar Controls

Annotation mode shows: Freehand, Rectangle, Color, Thickness, Undo, Done.

## Save, Copy & Exit

### Save As

Composites annotations onto full image, opens `QFileDialog`, saves via `QImage::save()`.

### Copy

Composites annotations, saves to temp file, pipes through `wl-copy --type image/png`. App exits after copy.

### Quit

Escape key or toolbar button. During annotation mode, first Escape exits annotation mode; second Escape quits.

### Toolbar

Simple toolbar (QPushButtons in QHBoxLayout) positioned at top-center of screen. Does not reuse the existing `Toolbar` class -- the long image toolbar is simpler and avoids coupling to OverlayWidget.

## Integration

### Changed Files

| File | Change |
|------|--------|
| **New: `LongImageWidget.h/.cpp`** | New widget with viewport, navigation, annotations, export |
| **`OverlayWidget.cpp`** | `startScrollCapture()` lambda creates LongImageWidget instead of re-showing overlay |
| **`CMakeLists.txt`** | Add `LongImageWidget.cpp` to sources |

### Unchanged

- `ScrollCapture`, `Stitcher`, `CaptureManager`, `Toolbar`, `AnnotationManager`, `OcrEngine`, `main.cpp`
- Normal screenshot flow is untouched

## Out of Scope

- OCR on long images
- Sub-region selection/cropping
- Kinetic/momentum scrolling
- Minimap or scroll position indicator
- Tiled rendering
