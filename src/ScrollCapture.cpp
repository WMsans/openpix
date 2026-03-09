#include "ScrollCapture.h"
#include "CaptureManager.h"
#include "Stitcher.h"

#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>

static const int CAPTURE_INTERVAL_MS = 500;

ScrollCapture::ScrollCapture(CaptureManager *captureManager, const QRect &captureRegion,
                             QWidget *parent)
    : QWidget(parent)
    , m_captureManager(captureManager)
    , m_captureRegion(captureRegion)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);

    QWidget *panel = new QWidget(this);
    panel->setStyleSheet(R"(
        QWidget {
            background-color: rgba(30, 30, 30, 220);
            border-radius: 6px;
        }
        QPushButton {
            background-color: #3a3a3a;
            color: white;
            border: 1px solid #555;
            padding: 6px 12px;
            border-radius: 4px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
        }
        QPushButton:disabled {
            background-color: #2a2a2a;
            color: #666;
        }
        QLabel {
            color: white;
            font-size: 12px;
            padding: 0 8px;
        }
    )");

    QHBoxLayout *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(8);

    m_countLabel = new QLabel("Frames: 0", panel);
    layout->addWidget(m_countLabel);

    m_finishBtn = new QPushButton("Finish", panel);
    connect(m_finishBtn, &QPushButton::clicked, this, &ScrollCapture::finish);
    layout->addWidget(m_finishBtn);

    m_cancelBtn = new QPushButton("Cancel", panel);
    connect(m_cancelBtn, &QPushButton::clicked, this, &ScrollCapture::cancelled);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QWidget::close);
    layout->addWidget(m_cancelBtn);

    panel->adjustSize();
    setFixedSize(panel->size());

    adjustSize();
    auto *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeom = screen->geometry();
        move(screenGeom.center().x() - width() / 2, screenGeom.top() + 20);
    } else {
        move(100, 20);
    }

    m_captureTimer = new QTimer(this);
    m_captureTimer->setInterval(CAPTURE_INTERVAL_MS);
    connect(m_captureTimer, &QTimer::timeout, this, &ScrollCapture::captureFrame);

    show();

    QTimer::singleShot(600, this, [this]() {
        captureFrame();
        m_captureTimer->start();
    });
}

ScrollCapture::~ScrollCapture()
{
    if (m_captureTimer) {
        m_captureTimer->stop();
    }
    if (m_captureConnection) {
        disconnect(m_captureConnection);
    }
}

void ScrollCapture::captureFrame()
{
    if (!m_captureManager || m_capturing) {
        return;
    }

    m_capturing = true;
    m_captureConnection = connect(m_captureManager, &CaptureManager::captured,
        this, &ScrollCapture::onFrameCaptured);
    m_captureManager->capture();
}

void ScrollCapture::onFrameCaptured(const QImage &image)
{
    if (m_captureConnection) {
        disconnect(m_captureConnection);
        m_captureConnection = QMetaObject::Connection();
    }

    m_capturing = false;

    QImage cropped;
    if (m_captureRegion.isValid() && m_captureRegion.intersects(image.rect())) {
        QRect intersected = m_captureRegion.intersected(image.rect());
        cropped = image.copy(intersected);
    } else {
        cropped = image;
    }

    m_frames.append(cropped);
    m_countLabel->setText(QString("Frames: %1").arg(m_frames.size()));
}

void ScrollCapture::finish()
{
    m_captureTimer->stop();

    if (m_frames.size() < 2) {
        QMessageBox::warning(this, "Scroll Capture",
                             "At least 2 frames are required for stitching.");
        return;
    }

    QString error;
    QImage result = Stitcher::stitch(m_frames, &error);

    if (result.isNull()) {
        QMessageBox::critical(this, "Stitching Failed",
                              QString("Failed to stitch frames: %1").arg(error));
        return;
    }

    emit finished(result);
    close();
}
