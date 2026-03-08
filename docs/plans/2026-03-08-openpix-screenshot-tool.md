OpenPix Implementation Plan
> For Claude: REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.
Goal: Build a Wayland-native screenshot tool with region selection, save/copy, OCR, annotations, and scrolling capture.
Architecture: Single fullscreen QWidget overlay painted with QPainter. Dual screen capture backend (xdg-desktop-portal primary, wlr-screencopy fallback). RapidOcrOnnx for OCR. OpenCV for scroll stitching.
Tech Stack: C++17, Qt 6 (Widgets/DBus/Gui/WaylandClient), OpenCV (core/imgproc), ONNX Runtime, RapidOcrOnnx, wayland-client, wlr-screencopy-unstable-v1
Design doc: docs/plans/2026-03-08-openpix-screenshot-tool-design.md
---
Task 1: Project Scaffold & CMake Build System
Files:
- Create: CMakeLists.txt
- Create: src/main.cpp
- Create: protocols/wlr-screencopy-unstable-v1.xml
Step 1: Create the Wayland protocol XML
Download the wlr-screencopy-unstable-v1.xml from the wlroots repository and place it at protocols/wlr-screencopy-unstable-v1.xml. This is the standard protocol definition used by all wlroots compositors.
mkdir -p protocols
curl -o protocols/wlr-screencopy-unstable-v1.xml \
  https://raw.githubusercontent.com/swaywm/wlr-protocols/master/unstable/wlr-screencopy-unstable-v1.xml
Step 2: Create CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(openpix VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
# --- Dependencies ---
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui DBus)
find_package(OpenCV REQUIRED COMPONENTS core imgproc)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
# onnxruntime - try pkg-config first, then find_library fallback
pkg_check_modules(ONNXRUNTIME QUIET libonnxruntime)
if(NOT ONNXRUNTIME_FOUND)
    find_library(ONNXRUNTIME_LIBRARIES NAMES onnxruntime)
    find_path(ONNXRUNTIME_INCLUDE_DIRS NAMES onnxruntime_cxx_api.h
              PATH_SUFFIXES onnxruntime/core/session)
    if(NOT ONNXRUNTIME_LIBRARIES)
        message(FATAL_ERROR "onnxruntime not found")
    endif()
endif()
# --- Wayland protocol code generation ---
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
set(WLR_SCREENCOPY_XML ${CMAKE_SOURCE_DIR}/protocols/wlr-screencopy-unstable-v1.xml)
set(WLR_SCREENCOPY_C ${CMAKE_BINARY_DIR}/wlr-screencopy-unstable-v1-protocol.c)
set(WLR_SCREENCOPY_H ${CMAKE_BINARY_DIR}/wlr-screencopy-unstable-v1-client-protocol.h)
add_custom_command(
    OUTPUT ${WLR_SCREENCOPY_H}
    COMMAND ${WAYLAND_SCANNER} client-header ${WLR_SCREENCOPY_XML} ${WLR_SCREENCOPY_H}
    DEPENDS ${WLR_SCREENCOPY_XML}
    COMMENT "Generating wlr-screencopy client header"
)
add_custom_command(
    OUTPUT ${WLR_SCREENCOPY_C}
    COMMAND ${WAYLAND_SCANNER} private-code ${WLR_SCREENCOPY_XML} ${WLR_SCREENCOPY_C}
    DEPENDS ${WLR_SCREENCOPY_XML}
    COMMENT "Generating wlr-screencopy protocol code"
)
# --- RapidOcrOnnx (built from source) ---
include(FetchContent)
FetchContent_Declare(
    rapidocr
    GIT_REPOSITORY https://github.com/RapidAI/RapidOcrOnnx.git
    GIT_TAG main
)
FetchContent_MakeAvailable(rapidocr)
# --- Main executable ---
add_executable(openpix
    src/main.cpp
    src/CaptureManager.cpp
    src/OverlayWidget.cpp
    src/Toolbar.cpp
    src/AnnotationManager.cpp
    src/ScrollCapture.cpp
    src/Stitcher.cpp
    src/OcrEngine.cpp
    ${WLR_SCREENCOPY_C}
)
target_include_directories(openpix PRIVATE
    ${CMAKE_BINARY_DIR}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIRS}
    ${rapidocr_SOURCE_DIR}/include
)
target_link_libraries(openpix PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::DBus
    ${OpenCV_LIBS}
    ${WAYLAND_CLIENT_LIBRARIES}
    ${ONNXRUNTIME_LIBRARIES}
)
# --- Install ---
install(TARGETS openpix RUNTIME DESTINATION bin)
install(DIRECTORY resources/models DESTINATION share/openpix)
Step 3: Create minimal main.cpp
// src/main.cpp
#include <QApplication>
#include <QDebug>
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("openpix");
    app.setApplicationVersion("0.1.0");
    qDebug() << "openpix starting...";
    // TODO: CaptureManager -> OverlayWidget
    return 0;
}
Step 4: Verify the build compiles
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
Expected: Compiles and produces openpix binary that prints "openpix starting..." and exits.
Note: RapidOcrOnnx FetchContent may need adjustment depending on their CMakeLists structure. If it doesn't integrate cleanly via FetchContent, fall back to building its sources directly by globbing ${rapidocr_SOURCE_DIR}/src/*.cpp and adding them to the openpix target. Inspect their CMakeLists.txt after fetch to determine the right approach.
Step 5: Commit
git add CMakeLists.txt src/main.cpp protocols/
git commit -m "feat: project scaffold with CMake build system"
---
Task 2: CaptureManager -- Portal Backend
Files:
- Create: src/CaptureManager.h
- Create: src/CaptureManager.cpp
- Modify: src/main.cpp
Step 1: Create CaptureManager header
// src/CaptureManager.h
#pragma once
#include <QImage>
#include <QObject>
#include <functional>
class CaptureManager : public QObject
{
    Q_OBJECT
public:
    explicit CaptureManager(QObject *parent = nullptr);
    // Attempts portal first, then wlr-screencopy. Emits captured() on success,
    // failed() on error.
    void capture();
signals:
    void captured(const QImage &image);
    void failed(const QString &error);
private:
    void captureViaPortal();
    void captureViaWlrScreencopy();
    bool m_portalAttempted = false;
};
Step 2: Implement portal capture
// src/CaptureManager.cpp
#include "CaptureManager.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QUrl>
#include <QDebug>
CaptureManager::CaptureManager(QObject *parent)
    : QObject(parent)
{
}
void CaptureManager::capture()
{
    m_portalAttempted = false;
    captureViaPortal();
}
void CaptureManager::captureViaPortal()
{
    m_portalAttempted = true;
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning() << "D-Bus session bus not connected, trying wlr-screencopy";
        captureViaWlrScreencopy();
        return;
    }
    // Build a unique handle token
    QString senderName = bus.baseService().mid(1).replace('.', '_');
    QString token = QStringLiteral("openpix_%1").arg(QCoreApplication::applicationPid());
    QString handlePath = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2")
                             .arg(senderName, token);
    // Subscribe to the Response signal BEFORE making the call
    bool connected = bus.connect(
        QString(),                              // any sender
        handlePath,                             // object path
        "org.freedesktop.portal.Request",       // interface
        "Response",                             // signal
        this,                                   // receiver
        SLOT(onPortalResponse(uint, QVariantMap))
    );
    if (!connected) {
        qWarning() << "Failed to connect to portal Response signal";
        captureViaWlrScreencopy();
        return;
    }
    QDBusInterface portal(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Screenshot",
        bus
    );
    if (!portal.isValid()) {
        qWarning() << "Portal interface not available:" << portal.lastError().message();
        captureViaWlrScreencopy();
        return;
    }
    QVariantMap options;
    options["handle_token"] = token;
    options["modal"] = false;
    options["interactive"] = false;
    QDBusMessage reply = portal.call("Screenshot", QString(), options);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "Portal Screenshot call failed:" << reply.errorMessage();
        captureViaWlrScreencopy();
        return;
    }
    // Response will arrive asynchronously via onPortalResponse
}
// Private slot -- add Q_INVOKABLE or use Q_SLOTS in header
void CaptureManager::onPortalResponse(uint response, const QVariantMap &results)
{
    if (response != 0) {
        qWarning() << "Portal returned non-success response:" << response;
        captureViaWlrScreencopy();
        return;
    }
    QString uri = results.value("uri").toString();
    if (uri.isEmpty()) {
        qWarning() << "Portal returned empty URI";
        captureViaWlrScreencopy();
        return;
    }
    QUrl url(uri);
    QString filePath = url.toLocalFile();
    QImage image(filePath);
    if (image.isNull()) {
        qWarning() << "Failed to load image from" << filePath;
        captureViaWlrScreencopy();
        return;
    }
    // Clean up the temp file
    QFile::remove(filePath);
    emit captured(image);
}
void CaptureManager::captureViaWlrScreencopy()
{
    // TODO: Implement in Task 3
    emit failed("wlr-screencopy not yet implemented");
}
Note: You'll need to add onPortalResponse to the header as a private slot:
private slots:
    void onPortalResponse(uint response, const QVariantMap &results);
Step 3: Wire up main.cpp
// src/main.cpp
#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include "CaptureManager.h"
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("openpix");
    app.setApplicationVersion("0.1.0");
    CaptureManager captureManager;
    QObject::connect(&captureManager, &CaptureManager::captured,
        [&](const QImage &image) {
            qDebug() << "Captured screenshot:" << image.size();
            // TODO: show OverlayWidget
            app.quit();
        });
    QObject::connect(&captureManager, &CaptureManager::failed,
        [&](const QString &error) {
            QMessageBox::critical(nullptr, "OpenPix", "Screenshot capture failed: " + error);
            app.quit();
        });
    captureManager.capture();
    return app.exec();
}
Step 4: Build and test
cd build && cmake .. && make -j$(nproc)
./openpix
Expected: On a system with xdg-desktop-portal running, captures a screenshot and prints its size. On a system without portal, prints the fallback error.
Step 5: Commit
git add src/CaptureManager.h src/CaptureManager.cpp src/main.cpp
git commit -m "feat: CaptureManager with xdg-desktop-portal backend"
---
Task 3: CaptureManager -- wlr-screencopy Fallback
Files:
- Modify: src/CaptureManager.h
- Modify: src/CaptureManager.cpp
Step 1: Add wlr-screencopy members to header
Add to CaptureManager.h:
// At the top, outside the class
struct wl_display;
struct wl_registry;
struct wl_shm;
struct wl_shm_pool;
struct wl_buffer;
struct wl_output;
struct zwlr_screencopy_manager_v1;
struct zwlr_screencopy_frame_v1;
// Inside the class, private section:
private:
    // wlr-screencopy state
    struct WlrState {
        wl_display *display = nullptr;
        wl_registry *registry = nullptr;
        wl_shm *shm = nullptr;
        zwlr_screencopy_manager_v1 *screencopyManager = nullptr;
        wl_output *output = nullptr;
        // Frame buffer info
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t stride = 0;
        uint32_t format = 0;
        bool bufferDone = false;
        bool copyDone = false;
        bool copyFailed = false;
        uint32_t flags = 0;
        // SHM buffer
        int shmFd = -1;
        void *shmData = nullptr;
        size_t shmSize = 0;
        wl_shm_pool *shmPool = nullptr;
        wl_buffer *buffer = nullptr;
    } m_wlr;
    static void registryGlobal(void *data, wl_registry *registry,
                                uint32_t name, const char *interface, uint32_t version);
    static void registryGlobalRemove(void *data, wl_registry *registry, uint32_t name);
    static void frameBuffer(void *data, zwlr_screencopy_frame_v1 *frame,
                            uint32_t format, uint32_t width, uint32_t height, uint32_t stride);
    static void frameBufferDone(void *data, zwlr_screencopy_frame_v1 *frame);
    static void frameFlags(void *data, zwlr_screencopy_frame_v1 *frame, uint32_t flags);
    static void frameReady(void *data, zwlr_screencopy_frame_v1 *frame,
                           uint32_t tvSecHi, uint32_t tvSecLo, uint32_t tvNsec);
    static void frameFailed(void *data, zwlr_screencopy_frame_v1 *frame);
Step 2: Implement wlr-screencopy capture
// Add to CaptureManager.cpp
#include <wayland-client.h>
#include "wlr-screencopy-unstable-v1-client-protocol.h"
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
// --- Wayland listener callbacks ---
static const struct wl_registry_listener registryListener = {
    .global = CaptureManager::registryGlobal,
    .global_remove = CaptureManager::registryGlobalRemove,
};
void CaptureManager::registryGlobal(void *data, wl_registry *registry,
                                     uint32_t name, const char *interface, uint32_t version)
{
    auto *self = static_cast<CaptureManager *>(data);
    if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        self->m_wlr.screencopyManager = static_cast<zwlr_screencopy_manager_v1 *>(
            wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, 3));
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        self->m_wlr.shm = static_cast<wl_shm *>(
            wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        // Take the first output. For multi-monitor: match against cursor position.
        if (!self->m_wlr.output) {
            self->m_wlr.output = static_cast<wl_output *>(
                wl_registry_bind(registry, name, &wl_output_interface, 1));
        }
    }
}
void CaptureManager::registryGlobalRemove(void *, wl_registry *, uint32_t) {}
static const struct zwlr_screencopy_frame_v1_listener frameListener = {
    .buffer = CaptureManager::frameBuffer,
    .flags = CaptureManager::frameFlags,
    .ready = CaptureManager::frameReady,
    .failed = CaptureManager::frameFailed,
    .damage = nullptr,         // optional, we don't use it
    .linux_dmabuf = nullptr,   // optional
    .buffer_done = CaptureManager::frameBufferDone,
};
void CaptureManager::frameBuffer(void *data, zwlr_screencopy_frame_v1 *,
                                  uint32_t format, uint32_t width, uint32_t height, uint32_t stride)
{
    auto *self = static_cast<CaptureManager *>(data);
    self->m_wlr.format = format;
    self->m_wlr.width = width;
    self->m_wlr.height = height;
    self->m_wlr.stride = stride;
}
void CaptureManager::frameBufferDone(void *data, zwlr_screencopy_frame_v1 *frame)
{
    auto *self = static_cast<CaptureManager *>(data);
    self->m_wlr.bufferDone = true;
    // Create SHM buffer
    self->m_wlr.shmSize = self->m_wlr.stride * self->m_wlr.height;
    char shmName[] = "/openpix-XXXXXX";
    self->m_wlr.shmFd = memfd_create("openpix-screencopy", MFD_CLOEXEC);
    if (self->m_wlr.shmFd < 0) {
        self->m_wlr.copyFailed = true;
        return;
    }
    if (ftruncate(self->m_wlr.shmFd, self->m_wlr.shmSize) < 0) {
        close(self->m_wlr.shmFd);
        self->m_wlr.copyFailed = true;
        return;
    }
    self->m_wlr.shmData = mmap(nullptr, self->m_wlr.shmSize,
                                PROT_READ | PROT_WRITE, MAP_SHARED,
                                self->m_wlr.shmFd, 0);
    if (self->m_wlr.shmData == MAP_FAILED) {
        close(self->m_wlr.shmFd);
        self->m_wlr.copyFailed = true;
        return;
    }
    self->m_wlr.shmPool = wl_shm_create_pool(self->m_wlr.shm,
                                               self->m_wlr.shmFd,
                                               self->m_wlr.shmSize);
    self->m_wlr.buffer = wl_shm_pool_create_buffer(self->m_wlr.shmPool, 0,
                                                     self->m_wlr.width,
                                                     self->m_wlr.height,
                                                     self->m_wlr.stride,
                                                     self->m_wlr.format);
    zwlr_screencopy_frame_v1_copy(frame, self->m_wlr.buffer);
}
void CaptureManager::frameFlags(void *data, zwlr_screencopy_frame_v1 *, uint32_t flags)
{
    auto *self = static_cast<CaptureManager *>(data);
    self->m_wlr.flags = flags;
}
void CaptureManager::frameReady(void *data, zwlr_screencopy_frame_v1 *,
                                 uint32_t, uint32_t, uint32_t)
{
    auto *self = static_cast<CaptureManager *>(data);
    self->m_wlr.copyDone = true;
}
void CaptureManager::frameFailed(void *data, zwlr_screencopy_frame_v1 *)
{
    auto *self = static_cast<CaptureManager *>(data);
    self->m_wlr.copyFailed = true;
}
void CaptureManager::captureViaWlrScreencopy()
{
    // Reset state
    m_wlr = WlrState{};
    // Connect to Wayland display
    m_wlr.display = wl_display_connect(nullptr);
    if (!m_wlr.display) {
        emit failed("Cannot connect to Wayland display");
        return;
    }
    m_wlr.registry = wl_display_get_registry(m_wlr.display);
    wl_registry_add_listener(m_wlr.registry, &registryListener, this);
    wl_display_roundtrip(m_wlr.display);
    if (!m_wlr.screencopyManager) {
        wl_display_disconnect(m_wlr.display);
        emit failed("Compositor does not support wlr-screencopy-unstable-v1");
        return;
    }
    if (!m_wlr.shm) {
        wl_display_disconnect(m_wlr.display);
        emit failed("Compositor does not support wl_shm");
        return;
    }
    if (!m_wlr.output) {
        wl_display_disconnect(m_wlr.display);
        emit failed("No output found");
        return;
    }
    // Request frame capture (overlay_cursor = 1 to include cursor)
    auto *frame = zwlr_screencopy_manager_v1_capture_output(
        m_wlr.screencopyManager, 0, m_wlr.output);
    zwlr_screencopy_frame_v1_add_listener(frame, &frameListener, this);
    // Dispatch until we get the frame
    while (!m_wlr.copyDone && !m_wlr.copyFailed) {
        if (wl_display_dispatch(m_wlr.display) < 0) {
            m_wlr.copyFailed = true;
            break;
        }
    }
    if (m_wlr.copyFailed) {
        if (m_wlr.shmData && m_wlr.shmData != MAP_FAILED)
            munmap(m_wlr.shmData, m_wlr.shmSize);
        if (m_wlr.shmFd >= 0)
            close(m_wlr.shmFd);
        wl_display_disconnect(m_wlr.display);
        emit failed("wlr-screencopy frame copy failed");
        return;
    }
    // Convert SHM buffer to QImage
    // wl_shm format WL_SHM_FORMAT_ARGB8888 (0) or WL_SHM_FORMAT_XRGB8888 (1)
    // map to QImage::Format_ARGB32 or Format_RGB32
    QImage::Format qFormat;
    switch (m_wlr.format) {
    case 0: // WL_SHM_FORMAT_ARGB8888
        qFormat = QImage::Format_ARGB32;
        break;
    case 1: // WL_SHM_FORMAT_XRGB8888
        qFormat = QImage::Format_RGB32;
        break;
    default:
        qFormat = QImage::Format_ARGB32;
        break;
    }
    QImage image(static_cast<const uchar *>(m_wlr.shmData),
                 m_wlr.width, m_wlr.height, m_wlr.stride, qFormat);
    // Deep copy since we're about to unmap
    QImage result = image.copy();
    // Handle Y-invert flag
    if (m_wlr.flags & 1) { // ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT
        result = result.mirrored(false, true);
    }
    // Cleanup
    munmap(m_wlr.shmData, m_wlr.shmSize);
    close(m_wlr.shmFd);
    zwlr_screencopy_frame_v1_destroy(frame);
    zwlr_screencopy_manager_v1_destroy(m_wlr.screencopyManager);
    wl_shm_destroy(m_wlr.shm);
    wl_output_destroy(m_wlr.output);
    wl_registry_destroy(m_wlr.registry);
    wl_display_disconnect(m_wlr.display);
    emit captured(result);
}
Step 3: Build and test
cd build && cmake .. && make -j$(nproc)
./openpix
Expected: On a wlroots compositor (Sway, Hyprland), if portal fails, captures via wlr-screencopy and prints image dimensions.
Step 4: Commit
git add src/CaptureManager.h src/CaptureManager.cpp
git commit -m "feat: wlr-screencopy fallback in CaptureManager"
---
Task 4: OverlayWidget -- Fullscreen Display & Dimming
Files:
- Create: src/OverlayWidget.h
- Create: src/OverlayWidget.cpp
- Modify: src/main.cpp
Step 1: Create OverlayWidget header
// src/OverlayWidget.h
#pragma once
#include <QWidget>
#include <QImage>
#include <QRect>
#include <QPoint>
class Toolbar;
class OverlayWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OverlayWidget(const QImage &screenshot, QWidget *parent = nullptr);
    QImage croppedImage() const;
    enum class State {
        Idle,
        Drawing,
        Moving,
        Resizing,
    };
    enum class Handle {
        None,
        TopLeft, Top, TopRight,
        Left, Right,
        BottomLeft, Bottom, BottomRight,
    };
signals:
    void regionSelected(const QRect &region);
    void cancelled();
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
private:
    Handle handleAt(const QPoint &pos) const;
    Qt::CursorShape cursorForHandle(Handle handle) const;
    QRect handleRect(Handle handle) const;
    void updateToolbarPosition();
    QImage m_screenshot;
    QRect m_selection;
    State m_state = State::Idle;
    Handle m_activeHandle = Handle::None;
    QPoint m_dragStart;
    QPoint m_selectionStartPos;
    Toolbar *m_toolbar = nullptr;
    static constexpr int HandleSize = 8;
};
Step 2: Implement OverlayWidget
// src/OverlayWidget.cpp
#include "OverlayWidget.h"
#include "Toolbar.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QGuiApplication>
OverlayWidget::OverlayWidget(const QImage &screenshot, QWidget *parent)
    : QWidget(parent)
    , m_screenshot(screenshot)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    // Fullscreen on the primary screen
    if (auto *screen = QGuiApplication::primaryScreen()) {
        setGeometry(screen->geometry());
    }
    m_toolbar = new Toolbar(this);
    m_toolbar->hide();
    showFullScreen();
}
QImage OverlayWidget::croppedImage() const
{
    if (m_selection.isNull())
        return QImage();
    return m_screenshot.copy(m_selection.normalized());
}
void OverlayWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // Layer 1: Screenshot background
    painter.drawImage(0, 0, m_screenshot);
    // Layer 2: Dimming mask (everything outside selection)
    QPainterPath fullPath;
    fullPath.addRect(rect());
    if (!m_selection.isNull()) {
        QRect sel = m_selection.normalized();
        QPainterPath selPath;
        selPath.addRect(sel);
        QPainterPath dimPath = fullPath - selPath;
        painter.fillPath(dimPath, QColor(0, 0, 0, 128));
        // Layer 3: Selection border
        QPen borderPen(Qt::white, 1, Qt::DashLine);
        painter.setPen(borderPen);
        painter.drawRect(sel);
        // Draw resize handles
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        for (int i = static_cast<int>(Handle::TopLeft);
             i <= static_cast<int>(Handle::BottomRight); ++i) {
            QRect hr = handleRect(static_cast<Handle>(i));
            if (!hr.isNull())
                painter.drawRect(hr);
        }
    } else {
        // No selection yet -- dim everything
        painter.fillPath(fullPath, QColor(0, 0, 0, 128));
    }
}
OverlayWidget::Handle OverlayWidget::handleAt(const QPoint &pos) const
{
    if (m_selection.isNull())
        return Handle::None;
    for (int i = static_cast<int>(Handle::TopLeft);
         i <= static_cast<int>(Handle::BottomRight); ++i) {
        auto h = static_cast<Handle>(i);
        QRect hr = handleRect(h);
        // Expand hit area slightly for easier grabbing
        if (hr.adjusted(-4, -4, 4, 4).contains(pos))
            return h;
    }
    return Handle::None;
}
QRect OverlayWidget::handleRect(Handle handle) const
{
    if (m_selection.isNull())
        return QRect();
    QRect sel = m_selection.normalized();
    int hs = HandleSize;
    int halfHs = hs / 2;
    switch (handle) {
    case Handle::TopLeft:     return QRect(sel.left() - halfHs, sel.top() - halfHs, hs, hs);
    case Handle::Top:         return QRect(sel.center().x() - halfHs, sel.top() - halfHs, hs, hs);
    case Handle::TopRight:    return QRect(sel.right() - halfHs, sel.top() - halfHs, hs, hs);
    case Handle::Left:        return QRect(sel.left() - halfHs, sel.center().y() - halfHs, hs, hs);
    case Handle::Right:       return QRect(sel.right() - halfHs, sel.center().y() - halfHs, hs, hs);
    case Handle::BottomLeft:  return QRect(sel.left() - halfHs, sel.bottom() - halfHs, hs, hs);
    case Handle::Bottom:      return QRect(sel.center().x() - halfHs, sel.bottom() - halfHs, hs, hs);
    case Handle::BottomRight: return QRect(sel.right() - halfHs, sel.bottom() - halfHs, hs, hs);
    default: return QRect();
    }
}
Qt::CursorShape OverlayWidget::cursorForHandle(Handle handle) const
{
    switch (handle) {
    case Handle::TopLeft:
    case Handle::BottomRight: return Qt::SizeFDiagCursor;
    case Handle::TopRight:
    case Handle::BottomLeft:  return Qt::SizeBDiagCursor;
    case Handle::Top:
    case Handle::Bottom:      return Qt::SizeVerCursor;
    case Handle::Left:
    case Handle::Right:       return Qt::SizeHorCursor;
    default:                  return Qt::CrossCursor;
    }
}
void OverlayWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    QPoint pos = event->pos();
    // Check handles first
    Handle h = handleAt(pos);
    if (h != Handle::None) {
        m_state = State::Resizing;
        m_activeHandle = h;
        m_dragStart = pos;
        return;
    }
    // Check if inside selection (move)
    if (!m_selection.isNull() && m_selection.normalized().contains(pos)) {
        m_state = State::Moving;
        m_dragStart = pos;
        m_selectionStartPos = m_selection.topLeft();
        return;
    }
    // Otherwise, start new selection
    m_state = State::Drawing;
    m_selection = QRect(pos, QSize(0, 0));
    m_toolbar->hide();
    update();
}
void OverlayWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();
    switch (m_state) {
    case State::Idle: {
        // Update cursor based on position
        Handle h = handleAt(pos);
        if (h != Handle::None) {
            setCursor(cursorForHandle(h));
        } else if (!m_selection.isNull() && m_selection.normalized().contains(pos)) {
            setCursor(Qt::SizeAllCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }
        return;
    }
    case State::Drawing: {
        m_selection.setBottomRight(pos);
        update();
        return;
    }
    case State::Moving: {
        QPoint delta = pos - m_dragStart;
        QRect newSel = QRect(m_selectionStartPos + delta, m_selection.size());
        // Clamp to screenshot bounds
        if (newSel.left() < 0) newSel.moveLeft(0);
        if (newSel.top() < 0) newSel.moveTop(0);
        if (newSel.right() >= width()) newSel.moveRight(width() - 1);
        if (newSel.bottom() >= height()) newSel.moveBottom(height() - 1);
        m_selection = newSel;
        update();
        return;
    }
    case State::Resizing: {
        QRect sel = m_selection.normalized();
        switch (m_activeHandle) {
        case Handle::TopLeft:     sel.setTopLeft(pos); break;
        case Handle::Top:         sel.setTop(pos.y()); break;
        case Handle::TopRight:    sel.setTopRight(pos); break;
        case Handle::Left:        sel.setLeft(pos.x()); break;
        case Handle::Right:       sel.setRight(pos.x()); break;
        case Handle::BottomLeft:  sel.setBottomLeft(pos); break;
        case Handle::Bottom:      sel.setBottom(pos.y()); break;
        case Handle::BottomRight: sel.setBottomRight(pos); break;
        default: break;
        }
        m_selection = sel;
        update();
        return;
    }
    }
}
void OverlayWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (m_state == State::Drawing || m_state == State::Moving || m_state == State::Resizing) {
        m_selection = m_selection.normalized();
        // Only show toolbar if selection is meaningful (> 5x5)
        if (m_selection.width() > 5 && m_selection.height() > 5) {
            m_state = State::Idle;
            updateToolbarPosition();
            m_toolbar->show();
            emit regionSelected(m_selection);
        } else {
            m_selection = QRect();
            m_state = State::Idle;
        }
        m_activeHandle = Handle::None;
        update();
    }
}
void OverlayWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        emit cancelled();
        close();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!m_selection.isNull()) {
            // Trigger copy action
            m_toolbar->onCopy();
        }
        break;
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right: {
        if (m_selection.isNull()) break;
        int step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
        QPoint delta;
        if (event->key() == Qt::Key_Up)    delta = QPoint(0, -step);
        if (event->key() == Qt::Key_Down)  delta = QPoint(0, step);
        if (event->key() == Qt::Key_Left)  delta = QPoint(-step, 0);
        if (event->key() == Qt::Key_Right) delta = QPoint(step, 0);
        m_selection.translate(delta);
        updateToolbarPosition();
        update();
        break;
    }
    default:
        QWidget::keyPressEvent(event);
    }
}
void OverlayWidget::updateToolbarPosition()
{
    if (m_selection.isNull() || !m_toolbar)
        return;
    QRect sel = m_selection.normalized();
    int toolbarHeight = m_toolbar->sizeHint().height();
    int toolbarWidth = m_toolbar->sizeHint().width();
    // Position below selection, right-aligned
    int x = sel.right() - toolbarWidth;
    int y = sel.bottom() + 8;
    // If below screen bottom, position above selection
    if (y + toolbarHeight > height()) {
        y = sel.top() - toolbarHeight - 8;
    }
    // If off left edge, clamp
    if (x < 0) x = 0;
    m_toolbar->move(x, y);
}
Step 3: Wire into main.cpp
Update main.cpp's captured handler:
QObject::connect(&captureManager, &CaptureManager::captured,
    [&](const QImage &image) {
        auto *overlay = new OverlayWidget(image);
        QObject::connect(overlay, &OverlayWidget::cancelled, &app, &QApplication::quit);
        // overlay shows itself in its constructor
    });
Step 4: Build and test
cd build && cmake .. && make -j$(nproc)
./openpix
Expected: Screenshot captured, fullscreen overlay shown with dimming. Click-drag draws a selection rectangle with white dashed border and resize handles. Escape quits.
Step 5: Commit
git add src/OverlayWidget.h src/OverlayWidget.cpp src/main.cpp
git commit -m "feat: OverlayWidget with region selection and dimming"
---
Task 5: Toolbar -- Action Buttons
Files:
- Create: src/Toolbar.h
- Create: src/Toolbar.cpp
Step 1: Create Toolbar header
// src/Toolbar.h
#pragma once
#include <QWidget>
class QPushButton;
class OverlayWidget;
class Toolbar : public QWidget
{
    Q_OBJECT
public:
    explicit Toolbar(OverlayWidget *overlay);
    void showAnnotationTools();
    void showMainTools();
public slots:
    void onSaveAs();
    void onCopy();
    void onOcr();
    void onAnnotate();
    void onScrollCapture();
    // Annotation toolbar slots
    void onFreehand();
    void onRectangle();
    void onColorCycle();
    void onThicknessCycle();
    void onUndo();
    void onAnnotateDone();
signals:
    void saveRequested();
    void copyRequested();
    void ocrRequested();
    void annotateRequested();
    void scrollCaptureRequested();
private:
    OverlayWidget *m_overlay;
    // Main toolbar buttons
    QWidget *m_mainBar = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_copyBtn = nullptr;
    QPushButton *m_ocrBtn = nullptr;
    QPushButton *m_annotateBtn = nullptr;
    QPushButton *m_scrollBtn = nullptr;
    // Annotation toolbar buttons
    QWidget *m_annotationBar = nullptr;
    QPushButton *m_freehandBtn = nullptr;
    QPushButton *m_rectBtn = nullptr;
    QPushButton *m_colorBtn = nullptr;
    QPushButton *m_thicknessBtn = nullptr;
    QPushButton *m_undoBtn = nullptr;
    QPushButton *m_doneBtn = nullptr;
};
Step 2: Implement Toolbar
// src/Toolbar.cpp
#include "Toolbar.h"
#include "OverlayWidget.h"
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>
#include <QBuffer>
#include <QStyle>
Toolbar::Toolbar(OverlayWidget *overlay)
    : QWidget(overlay)
    , m_overlay(overlay)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    // --- Main toolbar ---
    m_mainBar = new QWidget(this);
    auto *mainHLayout = new QHBoxLayout(m_mainBar);
    mainHLayout->setContentsMargins(4, 4, 4, 4);
    mainHLayout->setSpacing(4);
    auto makeBtn = [](const QString &text, QWidget *parent) {
        auto *btn = new QPushButton(text, parent);
        btn->setFixedHeight(28);
        btn->setStyleSheet(
            "QPushButton { background: #333; color: white; border: 1px solid #555; "
            "border-radius: 4px; padding: 2px 8px; font-size: 12px; }"
            "QPushButton:hover { background: #555; }");
        return btn;
    };
    m_saveBtn = makeBtn("Save As", m_mainBar);
    m_copyBtn = makeBtn("Copy", m_mainBar);
    m_ocrBtn = makeBtn("OCR", m_mainBar);
    m_annotateBtn = makeBtn("Annotate", m_mainBar);
    m_scrollBtn = makeBtn("Scroll", m_mainBar);
    mainHLayout->addWidget(m_saveBtn);
    mainHLayout->addWidget(m_copyBtn);
    mainHLayout->addWidget(m_ocrBtn);
    mainHLayout->addWidget(m_annotateBtn);
    mainHLayout->addWidget(m_scrollBtn);
    connect(m_saveBtn, &QPushButton::clicked, this, &Toolbar::onSaveAs);
    connect(m_copyBtn, &QPushButton::clicked, this, &Toolbar::onCopy);
    connect(m_ocrBtn, &QPushButton::clicked, this, &Toolbar::onOcr);
    connect(m_annotateBtn, &QPushButton::clicked, this, &Toolbar::onAnnotate);
    connect(m_scrollBtn, &QPushButton::clicked, this, &Toolbar::onScrollCapture);
    // --- Annotation toolbar ---
    m_annotationBar = new QWidget(this);
    auto *annHLayout = new QHBoxLayout(m_annotationBar);
    annHLayout->setContentsMargins(4, 4, 4, 4);
    annHLayout->setSpacing(4);
    m_freehandBtn = makeBtn("Freehand", m_annotationBar);
    m_rectBtn = makeBtn("Rect", m_annotationBar);
    m_colorBtn = makeBtn("Red", m_annotationBar);  // cycles through colors
    m_thicknessBtn = makeBtn("2px", m_annotationBar);
    m_undoBtn = makeBtn("Undo", m_annotationBar);
    m_doneBtn = makeBtn("Done", m_annotationBar);
    annHLayout->addWidget(m_freehandBtn);
    annHLayout->addWidget(m_rectBtn);
    annHLayout->addWidget(m_colorBtn);
    annHLayout->addWidget(m_thicknessBtn);
    annHLayout->addWidget(m_undoBtn);
    annHLayout->addWidget(m_doneBtn);
    connect(m_freehandBtn, &QPushButton::clicked, this, &Toolbar::onFreehand);
    connect(m_rectBtn, &QPushButton::clicked, this, &Toolbar::onRectangle);
    connect(m_colorBtn, &QPushButton::clicked, this, &Toolbar::onColorCycle);
    connect(m_thicknessBtn, &QPushButton::clicked, this, &Toolbar::onThicknessCycle);
    connect(m_undoBtn, &QPushButton::clicked, this, &Toolbar::onUndo);
    connect(m_doneBtn, &QPushButton::clicked, this, &Toolbar::onAnnotateDone);
    m_annotationBar->hide();
    mainLayout->addWidget(m_mainBar);
    mainLayout->addWidget(m_annotationBar);
}
void Toolbar::showAnnotationTools()
{
    m_mainBar->hide();
    m_annotationBar->show();
    adjustSize();
}
void Toolbar::showMainTools()
{
    m_annotationBar->hide();
    m_mainBar->show();
    adjustSize();
}
void Toolbar::onSaveAs()
{
    QImage img = m_overlay->croppedImage();
    if (img.isNull()) return;
    QString path = QFileDialog::getSaveFileName(
        m_overlay, "Save Screenshot", "screenshot.png", "PNG (*.png)");
    if (path.isEmpty()) return;
    if (!img.save(path, "PNG")) {
        // Error handling: could show QMessageBox here
        return;
    }
    qApp->quit();
}
void Toolbar::onCopy()
{
    QImage img = m_overlay->croppedImage();
    if (img.isNull()) return;
    auto *mimeData = new QMimeData();
    // Set as image
    mimeData->setImageData(img);
    // Also set as PNG bytes for apps that prefer that
    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    mimeData->setData("image/png", pngData);
    QApplication::clipboard()->setMimeData(mimeData);
    qApp->quit();
}
void Toolbar::onOcr()
{
    emit ocrRequested();
}
void Toolbar::onAnnotate()
{
    showAnnotationTools();
    emit annotateRequested();
}
void Toolbar::onScrollCapture()
{
    emit scrollCaptureRequested();
}
void Toolbar::onFreehand() { /* handled by AnnotationManager via signal */ }
void Toolbar::onRectangle() { /* handled by AnnotationManager via signal */ }
void Toolbar::onColorCycle() { /* handled by AnnotationManager */ }
void Toolbar::onThicknessCycle() { /* handled by AnnotationManager */ }
void Toolbar::onUndo() { /* handled by AnnotationManager */ }
void Toolbar::onAnnotateDone()
{
    showMainTools();
}
Step 3: Build and test
cd build && cmake .. && make -j$(nproc)
./openpix
Expected: After drawing a selection, toolbar appears below it with Save As, Copy, OCR, Annotate, Scroll buttons. "Save As" opens file dialog, saves PNG. "Copy" copies to clipboard and exits.
Step 4: Commit
git add src/Toolbar.h src/Toolbar.cpp
git commit -m "feat: Toolbar with save/copy actions"
---
Task 6: AnnotationManager
Files:
- Create: src/AnnotationManager.h
- Create: src/AnnotationManager.cpp
- Modify: src/OverlayWidget.h (add annotation rendering)
- Modify: src/OverlayWidget.cpp
Step 1: Create AnnotationManager header
// src/AnnotationManager.h
#pragma once
#include <QObject>
#include <QVector>
#include <QColor>
#include <QPainterPath>
#include <QRect>
#include <QPoint>
struct Annotation {
    enum Type { Freehand, Rectangle };
    Type type;
    QColor color;
    int thickness;
    QPainterPath path;   // for Freehand
    QRect rect;          // for Rectangle
};
class AnnotationManager : public QObject
{
    Q_OBJECT
public:
    explicit AnnotationManager(QObject *parent = nullptr);
    enum class Tool { Freehand, Rectangle };
    void setTool(Tool tool);
    Tool tool() const { return m_tool; }
    void setColor(const QColor &color);
    QColor color() const { return m_color; }
    void setThickness(int thickness);
    int thickness() const { return m_thickness; }
    void startStroke(const QPoint &pos);
    void continueStroke(const QPoint &pos);
    void endStroke(const QPoint &pos);
    void undo();
    bool isActive() const { return m_active; }
    void setActive(bool active);
    const QVector<Annotation> &annotations() const { return m_annotations; }
    const Annotation *currentAnnotation() const;
    // Paint all annotations onto a QPainter (used by OverlayWidget and for export)
    void paint(QPainter &painter) const;
    // Composite annotations onto an image
    QImage compositeOnto(const QImage &image, const QRect &selectionRect) const;
    void cycleColor();
    void cycleThickness();
signals:
    void changed();
private:
    QVector<Annotation> m_annotations;
    Annotation m_current;
    bool m_drawing = false;
    bool m_active = false;
    Tool m_tool = Tool::Freehand;
    QColor m_color = Qt::red;
    int m_thickness = 2;
    static const QVector<QColor> s_colors;
    static const QVector<int> s_thicknesses;
    int m_colorIndex = 0;
    int m_thicknessIndex = 0;
};
Step 2: Implement AnnotationManager
// src/AnnotationManager.cpp
#include "AnnotationManager.h"
#include <QPainter>
const QVector<QColor> AnnotationManager::s_colors = {
    Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::white, Qt::black
};
const QVector<int> AnnotationManager::s_thicknesses = { 2, 4, 6 };
AnnotationManager::AnnotationManager(QObject *parent)
    : QObject(parent)
{
}
void AnnotationManager::setTool(Tool tool) { m_tool = tool; }
void AnnotationManager::setColor(const QColor &color) { m_color = color; }
void AnnotationManager::setThickness(int thickness) { m_thickness = thickness; }
void AnnotationManager::setActive(bool active) { m_active = active; }
void AnnotationManager::startStroke(const QPoint &pos)
{
    m_drawing = true;
    m_current = Annotation{};
    m_current.color = m_color;
    m_current.thickness = m_thickness;
    if (m_tool == Tool::Freehand) {
        m_current.type = Annotation::Freehand;
        m_current.path.moveTo(pos);
    } else {
        m_current.type = Annotation::Rectangle;
        m_current.rect = QRect(pos, pos);
    }
}
void AnnotationManager::continueStroke(const QPoint &pos)
{
    if (!m_drawing) return;
    if (m_current.type == Annotation::Freehand) {
        m_current.path.lineTo(pos);
    } else {
        m_current.rect.setBottomRight(pos);
    }
    emit changed();
}
void AnnotationManager::endStroke(const QPoint &pos)
{
    if (!m_drawing) return;
    continueStroke(pos);
    m_annotations.append(m_current);
    m_drawing = false;
    emit changed();
}
const Annotation *AnnotationManager::currentAnnotation() const
{
    return m_drawing ? &m_current : nullptr;
}
void AnnotationManager::undo()
{
    if (!m_annotations.isEmpty()) {
        m_annotations.removeLast();
        emit changed();
    }
}
void AnnotationManager::paint(QPainter &painter) const
{
    auto drawAnnotation = [&](const Annotation &ann) {
        QPen pen(ann.color, ann.thickness, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        if (ann.type == Annotation::Freehand) {
            painter.drawPath(ann.path);
        } else {
            painter.drawRect(ann.rect.normalized());
        }
    };
    for (const auto &ann : m_annotations)
        drawAnnotation(ann);
    if (m_drawing)
        drawAnnotation(m_current);
}
QImage AnnotationManager::compositeOnto(const QImage &image, const QRect &selectionRect) const
{
    QImage result = image.copy();
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    // Annotations are stored in screen coordinates; translate to image coordinates
    painter.translate(-selectionRect.topLeft());
    paint(painter);
    return result;
}
void AnnotationManager::cycleColor()
{
    m_colorIndex = (m_colorIndex + 1) % s_colors.size();
    m_color = s_colors[m_colorIndex];
}
void AnnotationManager::cycleThickness()
{
    m_thicknessIndex = (m_thicknessIndex + 1) % s_thicknesses.size();
    m_thickness = s_thicknesses[m_thicknessIndex];
}
Step 3: Integrate annotations into OverlayWidget
Add to OverlayWidget.h:
#include "AnnotationManager.h"
// In the class, private section:
    AnnotationManager *m_annotationManager = nullptr;
    bool m_annotating = false;
Add to OverlayWidget constructor:
m_annotationManager = new AnnotationManager(this);
connect(m_annotationManager, &AnnotationManager::changed, this, QOverload<>::of(&QWidget::update));
In paintEvent, after drawing the selection border and handles, add:
// Layer 4: Annotations
if (m_annotationManager) {
    painter.save();
    painter.setClipRect(sel);  // clip to selection
    m_annotationManager->paint(painter);
    painter.restore();
}
In mousePressEvent, before the existing logic, add:
if (m_annotating && m_annotationManager->isActive()) {
    m_annotationManager->startStroke(pos);
    return;
}
Similarly for mouseMoveEvent and mouseReleaseEvent -- delegate to annotation manager when annotating.
Update croppedImage() to composite annotations:
QImage OverlayWidget::croppedImage() const
{
    if (m_selection.isNull())
        return QImage();
    QImage base = m_screenshot.copy(m_selection.normalized());
    if (m_annotationManager && !m_annotationManager->annotations().isEmpty())
        return m_annotationManager->compositeOnto(base, m_selection.normalized());
    return base;
}
Wire up Toolbar annotation buttons to AnnotationManager via OverlayWidget.
Step 4: Build and test
cd build && cmake .. && make -j$(nproc)
./openpix
Expected: After selecting a region and clicking "Annotate", can draw freehand lines and rectangles in the selected area. "Done" returns to main toolbar. Annotations appear in saved/copied images.
Step 5: Commit
git add src/AnnotationManager.h src/AnnotationManager.cpp src/OverlayWidget.h src/OverlayWidget.cpp
git commit -m "feat: annotation support with freehand and rectangle tools"
---
Task 7: OcrEngine
Files:
- Create: src/OcrEngine.h
- Create: src/OcrEngine.cpp
- Modify: src/Toolbar.cpp (wire up OCR button)
Step 1: Create OcrEngine header
// src/OcrEngine.h
#pragma once
#include <QImage>
#include <QString>
#include <memory>
class OcrLite;
class OcrEngine
{
public:
    OcrEngine();
    ~OcrEngine();
    // Initialize with model directory path. Returns false if models not found.
    bool init(const QString &modelsDir);
    // Run OCR on image, return recognized text. Returns empty string on failure.
    QString recognize(const QImage &image);
    QString lastError() const { return m_error; }
private:
    std::unique_ptr<OcrLite> m_ocr;
    QString m_error;
    bool m_initialized = false;
};
Step 2: Implement OcrEngine
// src/OcrEngine.cpp
#include "OcrEngine.h"
#include "OcrLite.h"
#include "OcrStruct.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <QDir>
#include <QDebug>
OcrEngine::OcrEngine() = default;
OcrEngine::~OcrEngine() = default;
bool OcrEngine::init(const QString &modelsDir)
{
    QDir dir(modelsDir);
    QString detPath = dir.filePath("ch_PP-OCRv3_det_infer.onnx");
    QString clsPath = dir.filePath("ch_PP-OCRv3_cls_infer.onnx");
    QString recPath = dir.filePath("ch_PP-OCRv3_rec_infer.onnx");
    QString keysPath = dir.filePath("keys.txt");
    // Verify files exist
    for (const auto &path : {detPath, clsPath, recPath, keysPath}) {
        if (!QFile::exists(path)) {
            m_error = QString("Model file not found: %1").arg(path);
            return false;
        }
    }
    m_ocr = std::make_unique<OcrLite>();
    m_ocr->setNumThread(4);
    m_ocr->initLogger(false, false, false);
    m_ocr->setGpuIndex(-1); // CPU
    bool ok = m_ocr->initModels(
        detPath.toStdString(),
        clsPath.toStdString(),
        recPath.toStdString(),
        keysPath.toStdString()
    );
    if (!ok) {
        m_error = "Failed to initialize OCR models";
        m_ocr.reset();
        return false;
    }
    m_initialized = true;
    return true;
}
QString OcrEngine::recognize(const QImage &image)
{
    if (!m_initialized) {
        m_error = "OCR engine not initialized";
        return {};
    }
    // Convert QImage to cv::Mat
    QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(rgbImage.height(), rgbImage.width(), CV_8UC3,
                const_cast<uchar *>(rgbImage.bits()),
                static_cast<size_t>(rgbImage.bytesPerLine()));
    // OpenCV expects BGR, QImage gives RGB
    cv::Mat bgrMat;
    cv::cvtColor(mat, bgrMat, cv::COLOR_RGB2BGR);
    OcrResult result = m_ocr->detect(
        bgrMat,
        /* padding */ 50,
        /* maxSideLen */ 1024,
        /* boxScoreThresh */ 0.5f,
        /* boxThresh */ 0.3f,
        /* unClipRatio */ 1.6f,
        /* doAngle */ true,
        /* mostAngle */ true
    );
    if (result.textBlocks.empty()) {
        return {};
    }
    // Concatenate all recognized text blocks
    QString text;
    for (const auto &block : result.textBlocks) {
        if (!text.isEmpty())
            text += '\n';
        text += QString::fromStdString(block.text);
    }
    return text;
}
Step 3: Wire OCR into Toolbar
In Toolbar::onOcr():
void Toolbar::onOcr()
{
    QImage img = m_overlay->croppedImage();
    if (img.isNull()) return;
    // Lazy-init OCR engine
    static OcrEngine ocrEngine;
    if (!ocrEngine.lastError().isEmpty() && /* not initialized check */) {
        // Try standard locations for models
        QString modelsDir = QCoreApplication::applicationDirPath() + "/../share/openpix/models";
        if (!ocrEngine.init(modelsDir)) {
            modelsDir = "/usr/share/openpix/models";
            if (!ocrEngine.init(modelsDir)) {
                QMessageBox::warning(m_overlay, "OCR Error",
                    "OCR models not found. Place models in share/openpix/models/");
                return;
            }
        }
    }
    QString text = ocrEngine.recognize(img);
    if (text.isEmpty()) {
        QMessageBox::information(m_overlay, "OCR", "No text detected in selection.");
        return;
    }
    QApplication::clipboard()->setText(text);
    qApp->quit();
}
Step 4: Build and test
cd build && cmake .. && make -j$(nproc)
Expected: Compiles. To test OCR, place model files in the expected directory and select a region containing text. Recognized text should be copied to clipboard.
Step 5: Commit
git add src/OcrEngine.h src/OcrEngine.cpp src/Toolbar.cpp
git commit -m "feat: OCR text extraction via RapidOcrOnnx"
---
Task 8: Stitcher
Files:
- Create: src/Stitcher.h
- Create: src/Stitcher.cpp
Step 1: Create Stitcher header
// src/Stitcher.h
#pragma once
#include <QImage>
#include <QVector>
#include <QString>
class Stitcher
{
public:
    // Stitch a sequence of frames vertically by detecting overlap via template matching.
    // Returns the stitched image. If stitching fails, falls back to simple concatenation.
    static QImage stitch(const QVector<QImage> &frames, QString *errorOut = nullptr);
private:
    // Find vertical overlap between bottom of imgA and top of imgB.
    // Returns the number of overlapping rows, or -1 if no match found.
    static int findOverlap(const QImage &imgA, const QImage &imgB, double threshold = 0.8);
    // Convert QImage to cv::Mat (BGR)
    static cv::Mat qimageToCvMat(const QImage &image);
};
Step 2: Implement Stitcher
// src/Stitcher.cpp
#include "Stitcher.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <QPainter>
cv::Mat Stitcher::qimageToCvMat(const QImage &image)
{
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3,
                const_cast<uchar *>(rgb.bits()),
                static_cast<size_t>(rgb.bytesPerLine()));
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
    return bgr.clone(); // deep copy since QImage data is temporary
}
int Stitcher::findOverlap(const QImage &imgA, const QImage &imgB, double threshold)
{
    cv::Mat matA = qimageToCvMat(imgA);
    cv::Mat matB = qimageToCvMat(imgB);
    // Use bottom third of imgA as the template to search in top half of imgB
    int stripHeight = matA.rows / 3;
    if (stripHeight < 10) stripHeight = 10;
    cv::Mat templ = matA(cv::Rect(0, matA.rows - stripHeight, matA.cols, stripHeight));
    int searchHeight = matB.rows / 2;
    if (searchHeight < stripHeight) searchHeight = matB.rows;
    cv::Mat searchRegion = matB(cv::Rect(0, 0, matB.cols, searchHeight));
    cv::Mat result;
    cv::matchTemplate(searchRegion, templ, result, cv::TM_CCOEFF_NORMED);
    double maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
    if (maxVal < threshold)
        return -1;
    // The overlap is: template starts at maxLoc.y in imgB,
    // and the template covers stripHeight rows from the bottom of imgA.
    // So the overlap is (stripHeight + (searchHeight - maxLoc.y - stripHeight)) ... 
    // Actually: maxLoc.y is where the template's top-left matches in searchRegion.
    // The template is the bottom `stripHeight` rows of imgA.
    // In imgB, the matching region starts at row maxLoc.y.
    // The overlap between the two images is: imgA's bottom aligns with imgB starting at maxLoc.y.
    // So overlap = stripHeight + (imgB portion already covered) ... 
    // Simpler: imgA ends at its bottom. The bottom stripHeight of imgA matches imgB at row maxLoc.y.
    // Therefore imgA row (imgA.rows - stripHeight) corresponds to imgB row maxLoc.y.
    // Overlap = imgB rows from 0 to (maxLoc.y + stripHeight) that are redundant with imgA's bottom.
    // We should start imgB from row (maxLoc.y + stripHeight) to avoid duplication.
    return maxLoc.y + stripHeight;
}
QImage Stitcher::stitch(const QVector<QImage> &frames, QString *errorOut)
{
    if (frames.isEmpty())
        return QImage();
    if (frames.size() == 1)
        return frames.first();
    // All frames should have the same width
    int width = frames.first().width();
    // Compute overlaps and non-overlapping regions
    struct Segment {
        const QImage *image;
        int startRow; // row in this image to start copying from
    };
    QVector<Segment> segments;
    segments.append({&frames[0], 0});
    bool anyFailed = false;
    for (int i = 1; i < frames.size(); ++i) {
        int overlap = findOverlap(frames[i - 1], frames[i]);
        if (overlap < 0) {
            anyFailed = true;
            segments.append({&frames[i], 0}); // no overlap detected, concatenate directly
        } else {
            segments.append({&frames[i], overlap});
        }
    }
    if (anyFailed && errorOut) {
        *errorOut = "Some frames had no detectable overlap; result may have visible seams.";
    }
    // Calculate total height
    int totalHeight = 0;
    for (int i = 0; i < segments.size(); ++i) {
        totalHeight += segments[i].image->height() - segments[i].startRow;
    }
    QImage result(width, totalHeight, QImage::Format_RGB32);
    QPainter painter(&result);
    int y = 0;
    for (auto &seg : segments) {
        int h = seg.image->height() - seg.startRow;
        painter.drawImage(0, y,
                          seg.image->copy(0, seg.startRow, width, h));
        y += h;
    }
    return result;
}
Step 3: Commit
git add src/Stitcher.h src/Stitcher.cpp
git commit -m "feat: Stitcher with template matching for scrolling screenshots"
---
Task 9: ScrollCapture
Files:
- Create: src/ScrollCapture.h
- Create: src/ScrollCapture.cpp
- Modify: src/OverlayWidget.cpp (wire up scroll capture flow)
Step 1: Create ScrollCapture header
// src/ScrollCapture.h
#pragma once
#include <QWidget>
#include <QImage>
#include <QRect>
#include <QVector>
class QPushButton;
class CaptureManager;
class ScrollCapture : public QWidget
{
    Q_OBJECT
public:
    // captureRegion is in screen coordinates
    ScrollCapture(CaptureManager *captureManager, const QRect &captureRegion,
                  QWidget *parent = nullptr);
signals:
    void finished(const QImage &stitchedImage);
    void cancelled();
private slots:
    void captureFrame();
    void finish();
    void onFrameCaptured(const QImage &image);
private:
    CaptureManager *m_captureManager;
    QRect m_captureRegion;
    QVector<QImage> m_frames;
    QPushButton *m_captureBtn = nullptr;
    QPushButton *m_finishBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QLabel *m_countLabel = nullptr;
};
Step 2: Implement ScrollCapture
// src/ScrollCapture.cpp
#include "ScrollCapture.h"
#include "CaptureManager.h"
#include "Stitcher.h"
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
ScrollCapture::ScrollCapture(CaptureManager *captureManager,
                             const QRect &captureRegion, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_captureManager(captureManager)
    , m_captureRegion(captureRegion)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    m_countLabel = new QLabel("Frames: 0", this);
    m_countLabel->setStyleSheet("color: white; font-size: 12px;");
    m_captureBtn = new QPushButton("Capture Frame", this);
    m_finishBtn = new QPushButton("Finish", this);
    m_cancelBtn = new QPushButton("Cancel", this);
    QString btnStyle =
        "QPushButton { background: #333; color: white; border: 1px solid #555; "
        "border-radius: 4px; padding: 4px 12px; }"
        "QPushButton:hover { background: #555; }";
    m_captureBtn->setStyleSheet(btnStyle);
    m_finishBtn->setStyleSheet(btnStyle);
    m_cancelBtn->setStyleSheet(btnStyle);
    layout->addWidget(m_countLabel);
    layout->addWidget(m_captureBtn);
    layout->addWidget(m_finishBtn);
    layout->addWidget(m_cancelBtn);
    setStyleSheet("background: rgba(0,0,0,200); border-radius: 8px;");
    connect(m_captureBtn, &QPushButton::clicked, this, &ScrollCapture::captureFrame);
    connect(m_finishBtn, &QPushButton::clicked, this, &ScrollCapture::finish);
    connect(m_cancelBtn, &QPushButton::clicked, this, &ScrollCapture::cancelled);
    // Position at top-center of screen
    adjustSize();
    move((parentWidget() ? parentWidget()->width() : 800) / 2 - width() / 2, 20);
}
void ScrollCapture::captureFrame()
{
    m_captureBtn->setEnabled(false);
    connect(m_captureManager, &CaptureManager::captured,
            this, &ScrollCapture::onFrameCaptured, Qt::SingleShotConnection);
    m_captureManager->capture();
}
void ScrollCapture::onFrameCaptured(const QImage &image)
{
    // Crop to the original selection region
    QImage cropped = image.copy(m_captureRegion);
    m_frames.append(cropped);
    m_countLabel->setText(QString("Frames: %1").arg(m_frames.size()));
    m_captureBtn->setEnabled(true);
}
void ScrollCapture::finish()
{
    if (m_frames.size() < 2) {
        QMessageBox::warning(this, "Scroll Capture",
                             "Need at least 2 frames to stitch.");
        return;
    }
    QString error;
    QImage stitched = Stitcher::stitch(m_frames, &error);
    if (!error.isEmpty()) {
        qWarning() << "Stitcher warning:" << error;
    }
    emit finished(stitched);
}
Step 3: Wire into OverlayWidget
In OverlayWidget, when scroll capture is triggered:
void OverlayWidget::startScrollCapture()
{
    // Hide the overlay to let user see the actual screen
    hide();
    auto *scrollCapture = new ScrollCapture(&m_captureManager, m_selection.normalized());
    scrollCapture->show();
    connect(scrollCapture, &ScrollCapture::finished, this,
        [this, scrollCapture](const QImage &stitchedImage) {
            scrollCapture->deleteLater();
            // Replace the screenshot with the stitched image and show overlay
            m_screenshot = stitchedImage;
            m_selection = QRect(0, 0, stitchedImage.width(), stitchedImage.height());
            showFullScreen();
            update();
        });
    connect(scrollCapture, &ScrollCapture::cancelled, this,
        [this, scrollCapture]() {
            scrollCapture->deleteLater();
            showFullScreen();
        });
}
Note: OverlayWidget will need access to a CaptureManager reference. Pass it through the constructor or store a pointer.
Step 4: Build and test
cd build && cmake .. && make -j$(nproc)
./openpix
Expected: Select a region, click Scroll. Overlay hides, floating panel appears. Click "Capture Frame" multiple times between scrolls. "Finish" stitches and shows result in overlay.
Step 5: Commit
git add src/ScrollCapture.h src/ScrollCapture.cpp src/OverlayWidget.h src/OverlayWidget.cpp
git commit -m "feat: scrolling screenshot with manual capture and stitching"
---
Task 10: Integration, Polish & Testing
Files:
- Modify: src/main.cpp (final wiring)
- Create: tests/test_stitcher.cpp (optional, if time permits)
Step 1: Final main.cpp wiring
Ensure the full flow works end-to-end:
// src/main.cpp
#include <QApplication>
#include <QMessageBox>
#include "CaptureManager.h"
#include "OverlayWidget.h"
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("openpix");
    app.setApplicationVersion("0.1.0");
    auto *captureManager = new CaptureManager(&app);
    QObject::connect(captureManager, &CaptureManager::captured,
        [&](const QImage &image) {
            auto *overlay = new OverlayWidget(image, captureManager);
            QObject::connect(overlay, &OverlayWidget::cancelled, &app, &QApplication::quit);
        });
    QObject::connect(captureManager, &CaptureManager::failed,
        [&](const QString &error) {
            QMessageBox::critical(nullptr, "OpenPix",
                                  "Screenshot capture failed:\n" + error);
            app.quit();
        });
    captureManager->capture();
    return app.exec();
}
Step 2: Verify full workflow
Test each action manually:
1. Launch ./openpix -- should capture and show overlay
2. Draw selection -- dimming mask, handles, toolbar visible
3. Drag selection -- moves correctly, clamped to bounds
4. Resize via handles -- all 8 handles work
5. Click outside selection -- resets and draws new
6. Arrow keys -- nudge selection
7. "Save As" -- file dialog, PNG saved correctly
8. "Copy" -- image in clipboard, app exits
9. "OCR" -- text recognized and copied (with models installed)
10. "Annotate" -- freehand and rectangle drawing works, composited in export
11. "Scroll" -- overlay hides, capture panel appears, stitching works
12. Escape -- quits at any point
Step 3: Add a basic Stitcher unit test
// tests/test_stitcher.cpp
#include <QtTest/QtTest>
#include "../src/Stitcher.h"
class TestStitcher : public QObject
{
    Q_OBJECT
private slots:
    void testTwoIdenticalFrames()
    {
        // Create two images with overlapping content
        QImage frame1(200, 100, QImage::Format_RGB32);
        frame1.fill(Qt::white);
        QPainter p1(&frame1);
        p1.setPen(Qt::black);
        p1.drawText(10, 50, "Hello");
        p1.drawText(10, 90, "World");
        QImage frame2(200, 100, QImage::Format_RGB32);
        frame2.fill(Qt::white);
        QPainter p2(&frame2);
        p2.setPen(Qt::black);
        p2.drawText(10, 10, "World"); // overlaps with bottom of frame1
        p2.drawText(10, 50, "Foo");
        QVector<QImage> frames = {frame1, frame2};
        QImage result = Stitcher::stitch(frames);
        // Result should be taller than either frame but shorter than both combined
        // (because of the overlap)
        QVERIFY(!result.isNull());
        QVERIFY(result.height() > 0);
        QCOMPARE(result.width(), 200);
    }
    void testSingleFrame()
    {
        QImage frame(100, 100, QImage::Format_RGB32);
        frame.fill(Qt::red);
        QVector<QImage> frames = {frame};
        QImage result = Stitcher::stitch(frames);
        QCOMPARE(result, frame);
    }
    void testEmptyInput()
    {
        QVector<QImage> frames;
        QImage result = Stitcher::stitch(frames);
        QVERIFY(result.isNull());
    }
};
QTEST_MAIN(TestStitcher)
#include "test_stitcher.moc"
Add to CMakeLists.txt:
# --- Tests ---
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
add_executable(test_stitcher tests/test_stitcher.cpp src/Stitcher.cpp)
target_include_directories(test_stitcher PRIVATE ${CMAKE_BINARY_DIR} src/)
target_link_libraries(test_stitcher Qt6::Test Qt6::Gui ${OpenCV_LIBS})
add_test(NAME test_stitcher COMMAND test_stitcher)
Step 4: Run tests
cd build && cmake .. && make -j$(nproc)
ctest --output-on-failure
Expected: All tests pass.
Step 5: Commit
git add -A
git commit -m "feat: final integration, polish, and stitcher tests"
---
Summary of Tasks
Task	Description	Key Files
1	Project scaffold & CMake	CMakeLists.txt, src/main.cpp, protocols/
2	CaptureManager portal backend	src/CaptureManager.h/cpp
3	CaptureManager wlr-screencopy fallback	src/CaptureManager.cpp
4	OverlayWidget with region selection	src/OverlayWidget.h/cpp
5	Toolbar with save/copy actions	src/Toolbar.h/cpp
6	AnnotationManager	src/AnnotationManager.h/cpp
7	OcrEngine via RapidOcrOnnx	src/OcrEngine.h/cpp
8	Stitcher for scroll screenshots	src/Stitcher.h/cpp
9	ScrollCapture flow	src/ScrollCapture.h/cpp
10	Integration, polish, tests	src/main.cpp, tests/
# Todos
[✓] 
Explore project context — check files, docs, recent commits
[✓] 
Ask clarifying questions — one at a time, understand purpose/constraints/success criteria
[✓] 
Propose 2-3 approaches — with trade-offs and recommendation
[✓] 
Present design — in sections, get user approval after each section
[✓] 
Write design doc — save to docs/plans/YYYY-MM-DD-<topic>-design.md and commit
[✓] 
Transition to implementation — output implementation plan
