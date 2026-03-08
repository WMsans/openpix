#include "CaptureManager.h"

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

void CaptureManager::captureViaWlrScreencopy()
{
    emit failed("wlr-screencopy not yet implemented");
}
