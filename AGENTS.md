# AGENTS.md - OpenPix Codebase Guide

This document provides guidance for AI coding agents working in this repository.

## Project Overview

OpenPix is a Linux screenshot tool with OCR capabilities, built with C++17, Qt6, OpenCV, Wayland, and ONNX Runtime. It supports screenshot capture via XDG Portal or wlr-screencopy protocol.

## Build Commands

```bash
# Configure and build
cmake -B build
cmake --build build

# Build with parallel jobs
cmake --build build -j$(nproc)

# Clean build
rm -rf build && cmake -B build && cmake --build build
```

## Test Commands

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run all tests (verbose)
cd build && ctest -V

# Run a single test by name
cd build && ctest -R test_stitcher

# Run a single test executable directly
cd build && ./test_stitcher
```

## Lint/Format Commands

This project does not currently have a configured linter or formatter. Consider using:
- `clang-format` for code formatting
- `clang-tidy` for static analysis

## Code Style Guidelines

### Header Guards
Always use `#pragma once` instead of traditional `#ifndef` guards.

### Include Order
Organize includes in this order, separated by blank lines:
1. The corresponding header (for .cpp files)
2. Qt headers
3. Third-party library headers (OpenCV, wayland, etc.)
4. Standard library headers

```cpp
#include "MyClass.h"

#include <QImage>
#include <QObject>

#include <opencv2/core.hpp>

#include <memory>
#include <vector>
```

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Classes | PascalCase | `CaptureManager`, `OverlayWidget` |
| Functions/Methods | camelCase | `captureViaPortal()`, `findOverlap()` |
| Member variables | m_ prefix + camelCase | `m_screenshot`, `m_captureManager` |
| Local variables | camelCase | `frameWidth`, `resultCode` |
| Constants | UPPER_SNAKE_CASE or PascalCase | `HandleSize`, `ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT` |
| Enums | PascalCase for enum and values | `State::Idle`, `Handle::TopLeft` |
| Namespaces | lowercase | Not currently used |

### Formatting

- **Indentation**: 4 spaces (no tabs)
- **Braces**: Opening brace on same line as statement
- **Line length**: Aim for 100-120 characters max
- **Pointer/reference**: Asterisk/ampersand attached to type

```cpp
// Correct
void process(const QImage &image);
QString *errorOut = nullptr;
auto *self = static_cast<CaptureManager *>(data);

// Incorrect
void process(const QImage& image);
QString * errorOut = nullptr;
```

### Types

- Use Qt types for strings and containers: `QString`, `QVector`, `QImage`
- Use `int` for loop indices and sizes that fit in 32 bits
- Use `std::unique_ptr` for owned heap allocations
- Use `std::shared_ptr` sparingly; prefer unique ownership
- Use `explicit` for single-argument constructors

### Error Handling

- Return `bool` for success/failure, with error details via output parameter or member variable
- Use `QString *errorOut` parameter pattern for optional error reporting
- Store last error in member variable `m_error` accessible via `lastError()` method

```cpp
bool init(const QString &modelsDir);
QString lastError() const { return m_error; }

// Usage with output parameter
QImage stitch(const QVector<QImage> &frames, QString *errorOut = nullptr);
```

### Qt Conventions

- Always include `Q_OBJECT` macro in QObject-derived classes
- Use signals/slots for event communication
- Prefer Qt5/6 style signal-slot connections (function pointers over `SIGNAL()`/`SLOT()` macros)
- Use lambdas for simple signal handlers

```cpp
// Preferred
QObject::connect(captureManager, &CaptureManager::captured,
    [&](const QImage &image) { /* ... */ });

// Avoid
QObject::connect(captureManager, SIGNAL(captured(QImage)), this, SLOT(handleCapture(QImage)));
```

### Class Structure

Organize class declarations in this order:
1. `public` section
   - Constructors/destructor
   - Public methods
2. `signals` section (if applicable)
3. `public slots` section (if applicable)
4. `protected` section
5. `private slots` section
6. `private` section
   - Private methods
   - Static callback functions
   - Member variables

### Static Callbacks

For C-style callbacks (e.g., Wayland), use static member functions with `void *data` pattern:

```cpp
static void registryGlobal(void *data, wl_registry *registry,
                           uint32_t name, const char *interface, uint32_t version);

// Implementation
void CaptureManager::registryGlobal(void *data, wl_registry *registry,
                                     uint32_t name, const char *interface, uint32_t version)
{
    auto *self = static_cast<CaptureManager *>(data);
    // Use self-> to access instance members
}
```

### Testing

- Use Qt Test framework (`QTest` macros)
- Test class inherits from `QObject` with `Q_OBJECT` macro
- Test methods go in `private slots:` section
- Use `QTEST_MAIN` and include the moc file

```cpp
class TestStitcher : public QObject
{
    Q_OBJECT

private slots:
    void testSingleFrame();
    void testEmptyInput();
};

QTEST_MAIN(TestStitcher)
#include "test_stitcher.moc"
```

### CMake Guidelines

- Use `target_include_directories` with `PRIVATE` unless headers expose the dependency
- Use `target_link_libraries` with `PRIVATE` by default
- Group related sources logically
- Use `FetchContent` for external dependencies

## File Structure

```
openpix/
├── CMakeLists.txt          # Main build configuration
├── src/
│   ├── main.cpp            # Application entry point
│   ├── CaptureManager.*    # Screenshot capture logic
│   ├── OverlayWidget.*     # Selection UI overlay
│   ├── Toolbar.*           # Annotation toolbar
│   ├── AnnotationManager.* # Annotation state/rendering
│   ├── OcrEngine.*         # OCR integration
│   ├── Stitcher.*          # Scroll capture image stitching
│   └── ScrollCapture.*     # Scroll capture coordination
├── tests/
│   └── test_stitcher.cpp   # Unit tests
├── protocols/
│   └── wlr-screencopy-unstable-v1.xml
└── build/                  # Build directory (gitignored)
```

## Dependencies

- **Qt6**: Widgets, Gui, DBus, Concurrent, Test
- **OpenCV**: core, imgproc, imgcodecs
- **ONNX Runtime**: For OCR inference
- **Wayland**: Client protocol
- **RapidOcrOnnx**: Fetched via CMake FetchContent

## Important Notes

- Do not add comments unless explicitly requested
- Follow existing patterns when extending functionality
- Keep methods focused; extract helper functions when logic becomes complex
- Prefer stack allocation over heap when lifetime is scoped
- Initialize member variables in-class when possible
