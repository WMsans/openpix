#pragma once

#include <QImage>
#include <QObject>
#include <functional>

struct wl_display;
struct wl_registry;
struct wl_shm;
struct wl_shm_pool;
struct wl_buffer;
struct wl_output;
struct zwlr_screencopy_manager_v1;
struct zwlr_screencopy_frame_v1;

class CaptureManager : public QObject
{
    Q_OBJECT

public:
    explicit CaptureManager(QObject *parent = nullptr);

    void capture();

signals:
    void captured(const QImage &image);
    void failed(const QString &error);

private:
    void captureViaPortal();
    void captureViaWlrScreencopy();

    bool m_portalAttempted = false;

    struct WlrState {
        wl_display *display = nullptr;
        wl_registry *registry = nullptr;
        wl_shm *shm = nullptr;
        zwlr_screencopy_manager_v1 *screencopyManager = nullptr;
        wl_output *output = nullptr;

        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t stride = 0;
        uint32_t format = 0;
        bool bufferDone = false;
        bool copyDone = false;
        bool copyFailed = false;
        uint32_t flags = 0;

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
    static void frameDamage(void *data, zwlr_screencopy_frame_v1 *frame,
                            uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    static void frameLinuxDmabuf(void *data, zwlr_screencopy_frame_v1 *frame,
                                  uint32_t format, uint32_t width, uint32_t height);

private slots:
    void onPortalResponse(uint response, const QVariantMap &results);
};
