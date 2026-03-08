#include "OcrEngine.h"
#include "OcrLite.h"
#include <QDir>
#include <QDebug>
#include <opencv2/imgproc.hpp>

OcrEngine::OcrEngine()
    : m_ocr(std::make_unique<OcrLite>())
{
}

OcrEngine::~OcrEngine() = default;

bool OcrEngine::init(const QString &modelsDir)
{
    QDir dir(modelsDir);
    if (!dir.exists()) {
        m_lastError = QString("Models directory not found: %1").arg(modelsDir);
        qWarning() << m_lastError;
        return false;
    }

    QString detPath = dir.filePath("dbnet.onnx");
    QString clsPath = dir.filePath("angle_net.onnx");
    QString recPath = dir.filePath("crnn_lite_lstm.onnx");
    QString keysPath = dir.filePath("keys.txt");

    if (!QFile::exists(detPath)) {
        m_lastError = QString("Detection model not found: %1").arg(detPath);
        qWarning() << m_lastError;
        return false;
    }
    if (!QFile::exists(clsPath)) {
        m_lastError = QString("Classification model not found: %1").arg(clsPath);
        qWarning() << m_lastError;
        return false;
    }
    if (!QFile::exists(recPath)) {
        m_lastError = QString("Recognition model not found: %1").arg(recPath);
        qWarning() << m_lastError;
        return false;
    }
    if (!QFile::exists(keysPath)) {
        m_lastError = QString("Keys file not found: %1").arg(keysPath);
        qWarning() << m_lastError;
        return false;
    }

    m_ocr->setNumThread(2);
    m_ocr->initLogger(false, false, false);

    bool success = m_ocr->initModels(
        detPath.toStdString(),
        clsPath.toStdString(),
        recPath.toStdString(),
        keysPath.toStdString()
    );

    if (!success) {
        m_lastError = "Failed to initialize OCR models";
        qWarning() << m_lastError;
        return false;
    }

    m_initialized = true;
    qDebug() << "OcrEngine initialized successfully";
    return true;
}

QString OcrEngine::recognize(const QImage &image)
{
    if (!m_initialized) {
        m_lastError = "OcrEngine not initialized";
        return QString();
    }

    if (image.isNull()) {
        m_lastError = "Invalid image";
        return QString();
    }

    cv::Mat mat = qImageToCvMat(image);
    if (mat.empty()) {
        m_lastError = "Failed to convert image";
        return QString();
    }

    constexpr int padding = 50;
    constexpr int maxSideLen = 1024;
    constexpr float boxScoreThresh = 0.6f;
    constexpr float boxThresh = 0.3f;
    constexpr float unClipRatio = 2.0f;
    constexpr bool doAngle = true;
    constexpr bool mostAngle = true;

    OcrResult result = m_ocr->detect(
        mat,
        padding,
        maxSideLen,
        boxScoreThresh,
        boxThresh,
        unClipRatio,
        doAngle,
        mostAngle
    );

    return QString::fromStdString(result.strRes);
}

cv::Mat OcrEngine::qImageToCvMat(const QImage &image) const
{
    QImage converted = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(
        converted.height(),
        converted.width(),
        CV_8UC3,
        const_cast<uchar*>(converted.bits()),
        converted.bytesPerLine()
    );
    cv::Mat result;
    cv::cvtColor(mat, result, cv::COLOR_RGB2BGR);
    return result.clone();
}
