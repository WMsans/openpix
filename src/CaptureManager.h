#pragma once

#include <QImage>
#include <QObject>
#include <functional>

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

private slots:
    void onPortalResponse(uint response, const QVariantMap &results);
};
