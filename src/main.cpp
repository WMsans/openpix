#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include "CaptureManager.h"
#include "OverlayWidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("openpix");
    app.setApplicationVersion("0.1.0");

    CaptureManager captureManager;

    QObject::connect(&captureManager, &CaptureManager::captured,
        [&](const QImage &image) {
            qDebug() << "Captured screenshot:" << image.size();
            auto overlay = new OverlayWidget(image);
            QObject::connect(overlay, &OverlayWidget::cancelled, &app, &QApplication::quit);
            QObject::connect(overlay, &OverlayWidget::regionSelected, [&](const QRect &region) {
                qDebug() << "Region selected:" << region;
                app.quit();
            });
        });

    QObject::connect(&captureManager, &CaptureManager::failed,
        [&](const QString &error) {
            QMessageBox::critical(nullptr, "OpenPix", "Screenshot capture failed: " + error);
            app.quit();
        });

    captureManager.capture();
    return app.exec();
}
