#pragma once
#include <QWidget>
#include <QImage>
#include <QRect>
#include <QVector>

class QPushButton;
class QLabel;
class CaptureManager;

class ScrollCapture : public QWidget
{
    Q_OBJECT

public:
    ScrollCapture(CaptureManager *captureManager, const QRect &captureRegion,
                  QWidget *parent = nullptr);
    ~ScrollCapture();

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
    QMetaObject::Connection m_captureConnection;
};
