# Toolbar Hotkey System Design

## Overview

Add keyboard shortcuts for main toolbar actions that work in both main and annotation modes.

## Hotkey Mappings

| Action | Shortcut |
|--------|----------|
| Copy image | Ctrl+C |
| Save image | Ctrl+S |
| OCR image | Shift+C |
| Pin image | Ctrl+T |

## Implementation

Add key handling in `OverlayWidget::keyPressEvent()`. The existing slot methods already implement the action logic, so we just invoke them.

### Code Changes

**File:** `src/OverlayWidget.cpp`

Add cases to `keyPressEvent()` switch statement:

```cpp
case Qt::Key_C:
    if (event->modifiers() & Qt::ControlModifier && m_selection.isValid()) {
        onCopyRequested();
    } else if (event->modifiers() & Qt::ShiftModifier && m_selection.isValid()) {
        onOcrRequested();
    }
    break;
case Qt::Key_S:
    if (event->modifiers() & Qt::ControlModifier && m_selection.isValid()) {
        onSaveRequested();
    }
    break;
case Qt::Key_T:
    if (event->modifiers() & Qt::ControlModifier && m_selection.isValid()) {
        onPinClicked();
    }
    break;
```

## Behavior

- All actions require a valid selection (consistent with button clicks)
- All actions close the overlay after completion (existing behavior)
- No changes to toolbar buttons or UI
- Hotkeys work in both main and annotation modes
