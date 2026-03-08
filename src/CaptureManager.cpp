#include "CaptureManager.h"

#include <wayland-client.h>
#include "wlr-screencopy-unstable-v1-client-protocol.h"

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <algorithm>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
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

    QString senderName = bus.baseService().mid(1).replace('.', '_');
    QString token = QStringLiteral("openpix_%1").arg(QCoreApplication::applicationPid());
    QString handlePath = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2")
                             .arg(senderName, token);

    bool connected = bus.connect(
        QString(),
        handlePath,
        "org.freedesktop.portal.Request",
        "Response",
        this,
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
}

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

    QFile::remove(filePath);

    emit captured(image);
}

void CaptureManager::registryGlobal(void *data, wl_registry *registry,
                                     uint32_t name, const char *interface, uint32_t version)
{
    auto *self = static_cast<CaptureManager *>(data);
    if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        self->m_wlr.screencopyManager = static_cast<zwlr_screencopy_manager_v1 *>(
            wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, std::min(version, 3u)));
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        self->m_wlr.shm = static_cast<wl_shm *>(
            wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        if (!self->m_wlr.output) {
            self->m_wlr.output = static_cast<wl_output *>(
                wl_registry_bind(registry, name, &wl_output_interface, 1));
        }
    }
}

void CaptureManager::registryGlobalRemove(void *, wl_registry *, uint32_t) {}

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

    self->m_wlr.shmSize = self->m_wlr.stride * self->m_wlr.height;
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
    if (!self->m_wlr.shmPool) {
        munmap(self->m_wlr.shmData, self->m_wlr.shmSize);
        close(self->m_wlr.shmFd);
        self->m_wlr.copyFailed = true;
        return;
    }
    self->m_wlr.buffer = wl_shm_pool_create_buffer(self->m_wlr.shmPool, 0,
                                                     self->m_wlr.width,
                                                     self->m_wlr.height,
                                                     self->m_wlr.stride,
                                                     self->m_wlr.format);
    if (!self->m_wlr.buffer) {
        wl_shm_pool_destroy(self->m_wlr.shmPool);
        munmap(self->m_wlr.shmData, self->m_wlr.shmSize);
        close(self->m_wlr.shmFd);
        self->m_wlr.copyFailed = true;
        return;
    }

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
    static const struct wl_registry_listener registryListener = {
        .global = CaptureManager::registryGlobal,
        .global_remove = CaptureManager::registryGlobalRemove,
    };

    static const struct zwlr_screencopy_frame_v1_listener frameListener = {
        .buffer = CaptureManager::frameBuffer,
        .flags = CaptureManager::frameFlags,
        .ready = CaptureManager::frameReady,
        .failed = CaptureManager::frameFailed,
        .damage = nullptr,
        .linux_dmabuf = nullptr,
        .buffer_done = CaptureManager::frameBufferDone,
    };

    m_wlr = WlrState{};

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

    auto *frame = zwlr_screencopy_manager_v1_capture_output(
        m_wlr.screencopyManager, 0, m_wlr.output);
    zwlr_screencopy_frame_v1_add_listener(frame, &frameListener, this);

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

    QImage::Format qFormat;
    switch (m_wlr.format) {
    case WL_SHM_FORMAT_ARGB8888:
        qFormat = QImage::Format_ARGB32;
        break;
    case WL_SHM_FORMAT_XRGB8888:
        qFormat = QImage::Format_RGB32;
        break;
    default:
        qFormat = QImage::Format_ARGB32;
        break;
    }

    QImage image(static_cast<const uchar *>(m_wlr.shmData),
                 m_wlr.width, m_wlr.height, m_wlr.stride, qFormat);

    QImage result = image.copy();

    if (m_wlr.flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT) {
        result = result.flipped(Qt::Vertical);
    }

    wl_buffer_destroy(m_wlr.buffer);
    wl_shm_pool_destroy(m_wlr.shmPool);
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
