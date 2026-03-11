# Pin Feature Design

## Overview

The "pin" feature allows users to spawn a floating, draggable, resizable window containing a cropped screenshot. This enables users to keep reference images visible while working in other applications.

## Requirements Summary

- **Trigger**: Toolbar button
- **Initial size**: Cropped selection size (actual pixels)
- **Resize**: Scroll wheel scales toward mouse cursor position
- **Drag**: Click and drag anywhere on the image
- **Close**: X button in top-right corner
- **Multiple pins**: Unlimited simultaneous pinned windows
- **App behavior**: Overlay closes after pin, app continues running

## Architecture

### New Class: `PinnedImageWidget`

- **Location**: `src/PinnedImageWidget.h`, `src/PinnedImageWidget.cpp`
- **Base class**: `QWidget`

**Window Properties:**
- `Qt::FramelessWindowHint` — No window decorations
- `Qt::WindowStaysOnTopHint` — Always visible above other windows
- `WA_DeleteOnClose` — Auto-cleanup when closed
- `WA_ShowWithoutActivating` — Doesn't steal focus from other apps (optional)

### Integration Points

| Component | Change |
|-----------|--------|
| `Toolbar` | Add "Pin" button and `pinClicked()` signal |
| `OverlayWidget` | Handle pin action, create `PinnedImageWidget`, close overlay |
| `main.cpp` | Modify quit logic to keep app running while pins exist |

## Component Design

### PinnedImageWidget

**Member Variables:**
```cpp
QImage m_image;           // Original image at full resolution
QPixmap m_scaledPixmap;   // Cache of currently displayed scaled image
qreal m_scale;            // Current scale factor (1.0 = original size)
QPoint m_dragStart;       // Mouse position when drag starts
QPoint m_windowStart;     // Window position when drag starts
bool m_dragging;          // Currently in drag operation
QRect m_closeButtonRect;  // Hit area for close button (16x16)
bool m_closeButtonHovered;// Close button hover state
```

**Key Methods:**
```cpp
explicit PinnedImageWidget(const QImage &image, QWidget *parent = nullptr);

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
```

### Close Button

- **Size**: 16x16 pixels
- **Position**: Top-right corner
- **Appearance**: "X" icon with circular background on hover
- **Behavior**: Closes widget on click

## Scaling Behavior

### Scroll-to-Resize

When the user scrolls while the mouse is over the pinned window:

1. Calculate scale delta (±0.1 per scroll tick)
2. Clamp new scale to bounds [0.1, 10.0]
3. Store mouse position relative to widget
4. Update scaled pixmap at new size
5. Resize window to match new scaled dimensions
6. Reposition window so the point under the mouse cursor stays fixed

**Formula for repositioning:**
```
newWindowPos = mouseScreenPos - (localMousePos * newScale / oldScale)
```

### Scale Bounds

| Property | Value |
|----------|-------|
| Minimum scale | 0.1 (10%) |
| Maximum scale | 10.0 (1000%) |
| Step per scroll | 0.1 |

### Initial Position

- Window appears centered on cursor position
- Clamped to screen bounds to avoid spawning off-screen
- Uses `QScreen::availableGeometry()` to account for panels/docks

## Data Flow

```
User clicks Pin button
    ↓
Toolbar::pinClicked() signal
    ↓
OverlayWidget::onPinClicked() slot
    ↓
QImage cropped = croppedImage()
    ↓
new PinnedImageWidget(cropped)
    ↓
OverlayWidget::close()
    ↓
PinnedImageWidget visible, app continues running
```

## App Lifecycle Changes

Current behavior: App quits when overlay closes.

New behavior:
- App quits when overlay closes AND no pinned windows exist
- Check `QApplication::topLevelWidgets()` for `PinnedImageWidget` instances
- Alternative: Track pinned window count with a static counter

## Error Handling

| Scenario | Handling |
|----------|----------|
| Empty/null image | Don't create pin, log warning |
| Selection too small | Enforce minimum window size of 32x32 |
| Selection too large | Scale down to 80% of screen if exceeds bounds |
| Off-screen position | Clamp to nearest screen edge |
| Rapid scroll events | Qt event system handles naturally |

## Memory Management

- `PinnedImageWidget` created with `new`, parentless
- `WA_DeleteOnClose` ensures automatic deletion when closed
- Original image held in memory until widget closes
- No explicit cleanup required from caller

## File Changes Summary

| File | Change |
|------|--------|
| `src/PinnedImageWidget.h` | New file |
| `src/PinnedImageWidget.cpp` | New file |
| `src/Toolbar.h` | Add `pinClicked()` signal |
| `src/Toolbar.cpp` | Add Pin button |
| `src/OverlayWidget.h` | Add `onPinClicked()` slot |
| `src/OverlayWidget.cpp` | Implement pin handler, connect signal |
| `src/main.cpp` | Modify quit logic for pinned windows |
| `src/CMakeLists.txt` | Add PinnedImageWidget sources |

## Testing Considerations

- Manual testing: Verify drag, scroll-resize, close functionality
- Test multiple simultaneous pins
- Test scale limits (min/max)
- Test multi-monitor scenarios
- Test edge cases: tiny selections, huge selections
