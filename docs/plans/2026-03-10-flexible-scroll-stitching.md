# Flexible Scroll Stitching Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Support arbitrary scroll direction changes during capture by replacing the linear segment model with a spatial canvas that tracks absolute Y-positions.

**Architecture:** Each frame gets an absolute Y-position computed from pairwise overlap detection. When overlap fails against the last placed frame, search backward through recent frames. Frames covering already-captured content are deduplicated. Final output is rendered by sorting frames by Y-position.

**Tech Stack:** C++17, Qt6 (QImage, QTest), OpenCV (matchTemplate)

---

### Task 1: Verify existing tests pass

**Files:**
- None modified

**Step 1: Build and run tests**

Run:
```bash
cmake -B build && cmake --build build -j$(nproc)
```

Run:
```bash
cd build && ctest --output-on-failure
```

Expected: All 4 existing tests pass (testTwoIdenticalFrames, testSingleFrame, testEmptyInput, testReverseScrolling).

---

### Task 2: Write failing test for down-then-up scrolling

**Files:**
- Modify: `tests/test_stitcher.cpp`

**Step 1: Add the failing test**

Add this test method to the `TestStitcher` class `private slots:` section, before the closing `};`:

```cpp
    void testDownThenUp()
    {
        // Frame 1: lines A, B, C
        QImage frame1(200, 120, QImage::Format_RGB32);
        frame1.fill(Qt::white);
        QPainter p1(&frame1);
        p1.setPen(Qt::black);
        p1.setFont(QFont("monospace", 14));
        p1.drawText(10, 30, "AAAA");
        p1.drawText(10, 70, "BBBB");
        p1.drawText(10, 110, "CCCC");

        // Frame 2: scrolled down, lines B, C, D
        QImage frame2(200, 120, QImage::Format_RGB32);
        frame2.fill(Qt::white);
        QPainter p2(&frame2);
        p2.setPen(Qt::black);
        p2.setFont(QFont("monospace", 14));
        p2.drawText(10, 30, "BBBB");
        p2.drawText(10, 70, "CCCC");
        p2.drawText(10, 110, "DDDD");

        // Frame 3: scrolled back up, lines A, B, C (same as frame 1)
        QImage frame3(200, 120, QImage::Format_RGB32);
        frame3.fill(Qt::white);
        QPainter p3(&frame3);
        p3.setPen(Qt::black);
        p3.setFont(QFont("monospace", 14));
        p3.drawText(10, 30, "AAAA");
        p3.drawText(10, 70, "BBBB");
        p3.drawText(10, 110, "CCCC");

        QVector<QImage> frames = {frame1, frame2, frame3};
        QString error;
        QImage result = Stitcher::stitch(frames, &error);

        QVERIFY(!result.isNull());
        // Should contain A, B, C, D — total ~160px (120 + 40 new from frame2)
        // Frame 3 is deduplicated since it covers content already in frames 1+2
        QVERIFY(result.height() > 120);
        QVERIFY(result.height() < 240);  // Must not contain duplicate content
        QCOMPARE(result.width(), 200);
    }
```

**Step 2: Build and run to verify it fails**

Run:
```bash
cmake --build build -j$(nproc) && cd build && ctest -R test_stitcher --output-on-failure
```

Expected: testDownThenUp FAILS (current stitcher produces garbled output with direction change).

**Step 3: Commit**

```bash
git add tests/test_stitcher.cpp
git commit -m "test: add failing test for down-then-up scroll stitching"
```

---

### Task 3: Write failing test for multiple direction changes

**Files:**
- Modify: `tests/test_stitcher.cpp`

**Step 1: Add the failing test**

Add this test method to the `TestStitcher` class `private slots:` section:

```cpp
    void testMultipleDirectionChanges()
    {
        // 5 frames: down, down, up, down
        // Content lines: A B C D E, each frame shows 3 lines with 1-line scroll steps

        // Frame 1: A B C
        QImage f1(200, 120, QImage::Format_RGB32);
        f1.fill(Qt::white);
        QPainter p1(&f1);
        p1.setPen(Qt::black);
        p1.setFont(QFont("monospace", 14));
        p1.drawText(10, 30, "AAAA");
        p1.drawText(10, 70, "BBBB");
        p1.drawText(10, 110, "CCCC");

        // Frame 2: B C D (scrolled down)
        QImage f2(200, 120, QImage::Format_RGB32);
        f2.fill(Qt::white);
        QPainter p2(&f2);
        p2.setPen(Qt::black);
        p2.setFont(QFont("monospace", 14));
        p2.drawText(10, 30, "BBBB");
        p2.drawText(10, 70, "CCCC");
        p2.drawText(10, 110, "DDDD");

        // Frame 3: C D E (scrolled down)
        QImage f3(200, 120, QImage::Format_RGB32);
        f3.fill(Qt::white);
        QPainter p3(&f3);
        p3.setPen(Qt::black);
        p3.setFont(QFont("monospace", 14));
        p3.drawText(10, 30, "CCCC");
        p3.drawText(10, 70, "DDDD");
        p3.drawText(10, 110, "EEEE");

        // Frame 4: B C D (scrolled back up — revisit)
        QImage f4(200, 120, QImage::Format_RGB32);
        f4.fill(Qt::white);
        QPainter p4(&f4);
        p4.setPen(Qt::black);
        p4.setFont(QFont("monospace", 14));
        p4.drawText(10, 30, "BBBB");
        p4.drawText(10, 70, "CCCC");
        p4.drawText(10, 110, "DDDD");

        // Frame 5: C D E (scrolled back down — revisit)
        QImage f5(200, 120, QImage::Format_RGB32);
        f5.fill(Qt::white);
        QPainter p5(&f5);
        p5.setPen(Qt::black);
        p5.setFont(QFont("monospace", 14));
        p5.drawText(10, 30, "CCCC");
        p5.drawText(10, 70, "DDDD");
        p5.drawText(10, 110, "EEEE");

        QVector<QImage> frames = {f1, f2, f3, f4, f5};
        QString error;
        QImage result = Stitcher::stitch(frames, &error);

        QVERIFY(!result.isNull());
        // Should contain A, B, C, D, E — total ~200px (120 + 40 + 40)
        // Frames 4 and 5 are deduped (revisits)
        QVERIFY(result.height() > 120);
        QVERIFY(result.height() < 300);  // No duplicate content
        QCOMPARE(result.width(), 200);
    }
```

**Step 2: Build and run to verify it fails**

Run:
```bash
cmake --build build -j$(nproc) && cd build && ctest -R test_stitcher --output-on-failure
```

Expected: testMultipleDirectionChanges FAILS.

**Step 3: Commit**

```bash
git add tests/test_stitcher.cpp
git commit -m "test: add failing test for multiple scroll direction changes"
```

---

### Task 4: Write failing test for revisited content deduplication

**Files:**
- Modify: `tests/test_stitcher.cpp`

**Step 1: Add the failing test**

Add this test method to the `TestStitcher` class `private slots:` section:

```cpp
    void testRevisitedContent()
    {
        // Frame 1: A B C
        QImage frame1(200, 120, QImage::Format_RGB32);
        frame1.fill(Qt::white);
        QPainter p1(&frame1);
        p1.setPen(Qt::black);
        p1.setFont(QFont("monospace", 14));
        p1.drawText(10, 30, "AAAA");
        p1.drawText(10, 70, "BBBB");
        p1.drawText(10, 110, "CCCC");

        // Frame 2: B C D (scrolled down)
        QImage frame2(200, 120, QImage::Format_RGB32);
        frame2.fill(Qt::white);
        QPainter p2(&frame2);
        p2.setPen(Qt::black);
        p2.setFont(QFont("monospace", 14));
        p2.drawText(10, 30, "BBBB");
        p2.drawText(10, 70, "CCCC");
        p2.drawText(10, 110, "DDDD");

        // Frame 3: exact copy of frame 1 (user scrolled all the way back)
        QImage frame3 = frame1.copy();

        QVector<QImage> frames = {frame1, frame2, frame3};
        QString error;
        QImage result = Stitcher::stitch(frames, &error);

        QVERIFY(!result.isNull());
        // A+B+C+D = 160px. Frame 3 is fully redundant.
        QVERIFY(result.height() > 120);
        QVERIFY(result.height() < 240);
        QCOMPARE(result.width(), 200);
    }
```

**Step 2: Build and run to verify it fails**

Run:
```bash
cmake --build build -j$(nproc) && cd build && ctest -R test_stitcher --output-on-failure
```

Expected: testRevisitedContent FAILS.

**Step 3: Commit**

```bash
git add tests/test_stitcher.cpp
git commit -m "test: add failing test for revisited content deduplication"
```

---

### Task 5: Implement spatial canvas stitcher

**Files:**
- Modify: `src/Stitcher.h`
- Modify: `src/Stitcher.cpp`

**Step 1: Update Stitcher.h**

Replace the entire contents of `src/Stitcher.h` with:

```cpp
#pragma once
#include <QImage>
#include <QVector>
#include <QString>
#include <opencv2/core.hpp>

class Stitcher
{
public:
    static QImage stitch(const QVector<QImage> &frames, QString *errorOut = nullptr);
private:
    enum class ScrollDirection { Down, Up, None };
    struct OverlapResult {
        int overlap;
        ScrollDirection direction;
    };
    struct PlacedFrame {
        int frameIndex;
        int y;
        int height;
    };
    static const int MATCH_SEARCH_DEPTH = 10;
    static OverlapResult findOverlap(const QImage &imgA, const QImage &imgB, double threshold = 0.8);
    static cv::Mat qimageToCvMat(const QImage &image);
};
```

**Step 2: Rewrite stitch() in Stitcher.cpp**

Replace the `stitch()` function (lines 50-141 of `src/Stitcher.cpp`) with the spatial canvas implementation. Keep `qimageToCvMat()` and `findOverlap()` completely unchanged (lines 1-48).

The new `stitch()`:

```cpp
QImage Stitcher::stitch(const QVector<QImage> &frames, QString *errorOut)
{
    if (frames.isEmpty())
        return QImage();
    if (frames.size() == 1)
        return frames.first();

    if (frames.first().isNull()) {
        if (errorOut) *errorOut = "Frame 0 is null or invalid";
        return QImage();
    }

    int width = frames.first().width();

    for (int i = 1; i < frames.size(); ++i) {
        if (frames[i].width() != width) {
            if (errorOut) {
                *errorOut = QString("Frame %1 has width %2, expected %3")
                    .arg(i).arg(frames[i].width()).arg(width);
            }
            return QImage();
        }
        if (frames[i].isNull()) {
            if (errorOut) {
                *errorOut = QString("Frame %1 is null or invalid").arg(i);
            }
            return QImage();
        }
    }

    static const int MIN_NEW_CONTENT = 4;

    QVector<PlacedFrame> placed;
    placed.append({0, 0, frames[0].height()});

    bool anyFailed = false;

    for (int i = 1; i < frames.size(); ++i) {
        bool matched = false;
        int newY = 0;

        int searchStart = placed.size() - 1;
        int searchEnd = std::max(0, placed.size() - MATCH_SEARCH_DEPTH);

        for (int j = searchStart; j >= searchEnd; --j) {
            OverlapResult result = findOverlap(frames[placed[j].frameIndex], frames[i]);
            if (result.overlap < 0)
                continue;

            int newContent = frames[i].height() - result.overlap;
            if (newContent < MIN_NEW_CONTENT) {
                matched = true;
                break;
            }

            if (result.direction == ScrollDirection::Down) {
                newY = placed[j].y + placed[j].height - result.overlap;
            } else {
                newY = placed[j].y - (frames[i].height() - result.overlap);
            }
            matched = true;
            break;
        }

        if (!matched) {
            anyFailed = true;
            newY = placed.last().y + placed.last().height;
        }

        bool redundant = false;
        for (int j = 0; j < placed.size(); ++j) {
            int existingTop = placed[j].y;
            int existingBottom = placed[j].y + placed[j].height;
            int newTop = newY;
            int newBottom = newY + frames[i].height();

            int overlapTop = std::max(existingTop, newTop);
            int overlapBottom = std::min(existingBottom, newBottom);
            int overlapHeight = overlapBottom - overlapTop;

            if (overlapHeight >= frames[i].height() - MIN_NEW_CONTENT) {
                redundant = true;
                break;
            }
        }

        if (!redundant && matched) {
            placed.append({i, newY, frames[i].height()});
        } else if (!matched) {
            placed.append({i, newY, frames[i].height()});
        }
    }

    if (anyFailed && errorOut) {
        *errorOut = "Some frames had no detectable overlap; result may have visible seams.";
    }

    std::sort(placed.begin(), placed.end(),
              [](const PlacedFrame &a, const PlacedFrame &b) { return a.y < b.y; });

    int minY = placed.first().y;
    int maxY = placed.first().y + placed.first().height;
    for (int i = 1; i < placed.size(); ++i) {
        minY = std::min(minY, placed[i].y);
        maxY = std::max(maxY, placed[i].y + placed[i].height);
    }

    int totalHeight = maxY - minY;
    QImage result(width, totalHeight, QImage::Format_RGB32);
    result.fill(Qt::white);
    QPainter painter(&result);

    int canvasY = placed[0].y;
    int paintY = 0;

    for (int i = 0; i < placed.size(); ++i) {
        int frameTop = placed[i].y;
        int frameBottom = placed[i].y + placed[i].height;

        int startRow = 0;
        if (frameTop < canvasY) {
            startRow = 0;
            paintY = frameTop - minY;
            canvasY = frameTop;
        } else {
            startRow = canvasY - frameTop;
            if (startRow < 0) startRow = 0;
        }

        int visibleTop = std::max(frameTop, canvasY);
        int visibleBottom = frameBottom;
        if (visibleTop >= visibleBottom)
            continue;

        int cropStart = visibleTop - frameTop;
        int cropHeight = visibleBottom - visibleTop;
        int drawY = visibleTop - minY;

        painter.drawImage(0, drawY,
                          frames[placed[i].frameIndex].copy(0, cropStart, width, cropHeight));

        canvasY = std::max(canvasY, visibleBottom);
    }

    return result;
}
```

**Step 3: Build and run ALL tests**

Run:
```bash
cmake --build build -j$(nproc) && cd build && ctest --output-on-failure
```

Expected: All tests pass — the 4 original tests plus the 3 new tests.

**Step 4: Commit**

```bash
git add src/Stitcher.h src/Stitcher.cpp
git commit -m "feat: support flexible bidirectional scroll stitching

Replace linear segment model with spatial canvas that tracks absolute
Y-positions. Frames are matched against recent neighbors when direction
changes, and revisited content is deduplicated."
```

---

### Task 6: Final verification

**Step 1: Clean build from scratch**

Run:
```bash
rm -rf build && cmake -B build && cmake --build build -j$(nproc)
```

**Step 2: Run all tests**

Run:
```bash
cd build && ctest -V
```

Expected: All 7 tests pass. No warnings.

**Step 3: Verify test names**

Run:
```bash
cd build && ctest -N
```

Expected output lists: testTwoIdenticalFrames, testSingleFrame, testEmptyInput, testReverseScrolling, testDownThenUp, testMultipleDirectionChanges, testRevisitedContent (all within the single test_stitcher executable, so ctest shows 1 test).
