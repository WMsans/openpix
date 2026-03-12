#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QClipboard>
#include <QStandardPaths>
#include "CaptureManager.h"
#include "OverlayWidget.h"
#include "OcrEngine.h"
#include "PinnedImageWidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("openpix");
    app.setApplicationVersion("0.1.0");

    OcrEngine *ocrEngine = new OcrEngine(&app);
    QString modelsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
    if (!ocrEngine->init(modelsDir)) {
        qWarning() << "OCR initialization failed:" << ocrEngine->lastError();
    }

    CaptureManager *captureManager = new CaptureManager(&app);

    QObject::connect(captureManager, &CaptureManager::captured,
        &app, [&](const QImage &image) {
            qDebug() << "Captured screenshot:" << image.size();
            auto overlay = new OverlayWidget(image, captureManager);
            auto shouldQuit = []() {
    const auto &widgets = QApplication::topLevelWidgets();
    for (QWidget *w : widgets) {
        if (qobject_cast<PinnedImageWidget*>(w)) {
            return false;
        }
    }
    return true;
};

QObject::connect(overlay, &OverlayWidget::cancelled, [=]() {
    if (shouldQuit()) {
        qApp->quit();
    }
});
            QObject::connect(overlay, &OverlayWidget::regionSelected, [&](const QRect &region) {
                qDebug() << "Region selected:" << region;
                app.quit();
            });
            QObject::connect(overlay, &OverlayWidget::ocrRequested, [&]() {
                QImage cropped = overlay->croppedImage();
                if (cropped.isNull()) {
                    QMessageBox::information(nullptr, "OpenPix", "No region selected");
                    return;
                }
                if (!ocrEngine->isInitialized()) {
                    QMessageBox::warning(nullptr, "OpenPix", "OCR not initialized: " + ocrEngine->lastError());
                    return;
                }
                QString text = ocrEngine->recognize(cropped);
                if (text.isEmpty()) {
                    QMessageBox::information(nullptr, "OpenPix", "No text found");
                    return;
                }
                QApplication::clipboard()->setText(text);
                QMessageBox::information(nullptr, "OpenPix", "Text copied to clipboard");
                overlay->close();
                app.quit();
            });
        }, Qt::SingleShotConnection);

    QObject::connect(captureManager, &CaptureManager::failed,
        [&](const QString &error) {
            QMessageBox::critical(nullptr, "OpenPix", "Screenshot capture failed: " + error);
            app.quit();
        });

    captureManager->capture();
    return app.exec();
}
