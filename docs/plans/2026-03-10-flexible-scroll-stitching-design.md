# Flexible Scroll Stitching Design

## Problem

The current `Stitcher::stitch()` assumes all frames scroll in a single direction. It uses a global `scrollingUp` boolean and reverses the entire segment array when any frame pair scrolls upward. This breaks when the user scrolls down, then back up, then down again -- producing garbled output.

## Goal

Support arbitrary scroll direction changes during capture. The user scrolls freely through a page and the stitcher assembles a correct, deduplicated result regardless of direction reversals.

## Approach: Spatial Canvas with Relative Y-Offsets

Replace the linear segment list with absolute Y-position tracking for each frame.

### Data Model

```cpp
struct PlacedFrame {
    int frameIndex;  // index into original frames array
    int y;           // absolute Y-position on virtual canvas
    int height;      // frame height
};
```

Frame 0 is placed at `y=0`. Each subsequent frame's Y is computed relative to the frame it matched against:

- Scroll Down: `newY = matched.y + matched.height - overlap`
- Scroll Up: `newY = matched.y - (newFrame.height - overlap)`

### Frame Placement Algorithm

For each new frame:

1. Try `findOverlap` against the last placed frame.
2. If no match, search backward through up to 10 previously placed frames.
3. If a match is found, compute absolute Y-position from the matched frame.
4. If the new frame's Y-range is mostly covered by already-placed frames (less than `MIN_NEW_CONTENT` new pixels), skip it (deduplication).
5. If no match at all, append with a seam (current fallback behavior).

### Rendering

1. Sort placed frames by Y-position.
2. Walk through sorted frames. For each frame, compute the visible rows (crop the top if overlapping with the previous frame's bottom edge).
3. Paint each frame's visible rows onto the output image sequentially.

### What Changes

| Component | Change |
|-----------|--------|
| `Stitcher.h` | Add `PlacedFrame` struct |
| `Stitcher.cpp` `stitch()` | Rewrite orchestration to use placement + Y-sort + dedup |
| `Stitcher.cpp` `findOverlap()` | No change |
| `Stitcher.cpp` `qimageToCvMat()` | No change |
| `ScrollCapture.*` | No change |
| Public API | No change — `stitch(QVector<QImage>, QString*)` signature is identical |

### What Stays the Same

- `findOverlap()` algorithm (template matching with bottom/top 1/3 strips)
- `qimageToCvMat()` conversion
- Validation (null checks, width consistency)
- `MIN_NEW_CONTENT = 4` threshold
- Error reporting pattern
- Pure scroll-down and pure scroll-up produce identical results to current implementation

## Test Plan

Existing tests unchanged:
- `testTwoIdenticalFrames`
- `testSingleFrame`
- `testEmptyInput`
- `testReverseScrolling`

New tests:
- `testDownThenUp` -- 3 frames, direction reversal mid-capture
- `testUpThenDown` -- 3 frames, opposite reversal
- `testRevisitedContent` -- 3 frames where third revisits first frame's content, verify dedup
- `testMultipleDirectionChanges` -- 5 frames with multiple reversals
