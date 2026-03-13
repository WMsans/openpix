# Toolbar Hotkey System Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add keyboard shortcuts (Ctrl+C, Ctrl+S, Shift+C, Ctrl+T) for toolbar actions.

**Architecture:** Extend the existing `keyPressEvent()` handler in OverlayWidget to intercept key combinations and invoke the existing slot methods.

**Tech Stack:** Qt6 key handling, existing signal/slot infrastructure

---

### Task 1: Add Hotkey Handling to OverlayWidget

**Files:**
- Modify: `src/OverlayWidget.cpp:372-416` (keyPressEvent method)

**Step 1: Add key cases for C, S, T**

In `OverlayWidget::keyPressEvent()`, add new cases before the `default:` case:

```cpp
case Qt::Key_C:
    if (event->modifiers() & Qt::ControlModifier && m_selection.isValid()) {
        onCopyRequested();
    } else if (event->modifiers() & Qt::ShiftModifier && m_selection.isValid()) {
        onOcrRequested();
    } else {
        QWidget::keyPressEvent(event);
    }
    break;
case Qt::Key_S:
    if (event->modifiers() & Qt::ControlModifier && m_selection.isValid()) {
        onSaveRequested();
    } else {
        QWidget::keyPressEvent(event);
    }
    break;
case Qt::Key_T:
    if (event->modifiers() & Qt::ControlModifier && m_selection.isValid()) {
        onPinClicked();
    } else {
        QWidget::keyPressEvent(event);
    }
    break;
```

**Step 2: Build the project**

Run: `cmake --build build`
Expected: Build succeeds with no errors

**Step 3: Test manually**

1. Run `./build/src/openpix`
2. Make a selection
3. Press Ctrl+C - should copy image and quit
4. Run again, make selection, press Ctrl+S - should open save dialog
5. Run again, make selection, press Shift+C - should run OCR
6. Run again, make selection, press Ctrl+T - should pin image

**Step 4: Commit**

```bash
git add src/OverlayWidget.cpp
git commit -m "feat: add toolbar hotkeys (Ctrl+C, Ctrl+S, Shift+C, Ctrl+T)"
```

---

## Summary

Single task modifying one method. The existing slot methods handle all the logic - we only add key bindings to invoke them.
