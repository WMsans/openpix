#include "OcrEngine.h"
#include "OcrLite.h"
#include <QDir>
#include <QDebug>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>

static cv::Mat qImageToCvMat(const QImage &image)
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

OcrEngine::OcrEngine(QObject *parent)
    : QObject(parent)
    , m_ocr(std::make_unique<OcrLite>())
{
}

OcrEngine::~OcrEngine() = default;

bool OcrEngine::init(const QString &modelsDir)
{
    QDir dir(modelsDir);
    if (!dir.exists()) {
        m_error = QString("Models directory not found: %1").arg(modelsDir);
        qWarning() << m_error;
        return false;
    }

    QString detPath = dir.filePath("ch_PP-OCRv4_det_infer.onnx");
    QString clsPath = dir.filePath("ch_ppocr_mobile_v2.0_cls_infer.onnx");
    QString recPath = dir.filePath("ch_PP-OCRv4_rec_infer.onnx");
    QString keysPath = dir.filePath("keys.txt");

    if (!QFile::exists(detPath)) {
        m_error = QString("Detection model not found: %1").arg(detPath);
        qWarning() << m_error;
        return false;
    }
    if (!QFile::exists(clsPath)) {
        m_error = QString("Classification model not found: %1").arg(clsPath);
        qWarning() << m_error;
        return false;
    }
    if (!QFile::exists(recPath)) {
        m_error = QString("Recognition model not found: %1").arg(recPath);
        qWarning() << m_error;
        return false;
    }
    if (!QFile::exists(keysPath)) {
        m_error = QString("Keys file not found: %1").arg(keysPath);
        qWarning() << m_error;
        return false;
    }

    m_ocr->setNumThread(4);
    m_ocr->setGpuIndex(-1);
    m_ocr->initLogger(true, false, false);

    bool success = m_ocr->initModels(
        detPath.toStdString(),
        clsPath.toStdString(),
        recPath.toStdString(),
        keysPath.toStdString()
    );

    if (!success) {
        m_error = "Failed to initialize OCR models";
        qWarning() << m_error;
        return false;
    }

    m_initialized = true;
    qDebug() << "OcrEngine initialized successfully";
    return true;
}

QString OcrEngine::recognize(const QImage &image)
{
    std::cout << "=== OcrEngine::recognize called ===" << std::endl;
    std::cout << "Initialized: " << m_initialized << std::endl;
    std::cout << "Image null: " << image.isNull() << std::endl;
    std::cout << "Image size: " << image.width() << "x" << image.height() << std::endl;
    std::cout << "Image format: " << image.format() << std::endl;
    
    if (!m_initialized) {
        m_error = "OcrEngine not initialized";
        std::cout << "ERROR: Not initialized" << std::endl;
        return QString();
    }

    if (image.isNull()) {
        m_error = "Invalid image";
        std::cout << "ERROR: Image is null" << std::endl;
        return QString();
    }

    std::cout << "Converting QImage to cv::Mat..." << std::endl;
    
    QString debugPath = "/tmp/ocr_debug_input.png";
    image.save(debugPath);
    std::cout << "Saved debug image to: " << debugPath.toStdString() << std::endl;
    
    cv::Mat mat = qImageToCvMat(image);
    std::cout << "Mat empty: " << mat.empty() << std::endl;
    std::cout << "Mat size: " << mat.cols << "x" << mat.rows << std::endl;
    std::cout << "Mat channels: " << mat.channels() << std::endl;
    std::cout << "Mat type: " << mat.type() << std::endl;
    
    cv::imwrite("/tmp/ocr_debug_mat.png", mat);
    std::cout << "Saved mat image to: /tmp/ocr_debug_mat.png" << std::endl;
    
    if (mat.empty()) {
        m_error = "Failed to convert image";
        std::cout << "ERROR: Mat conversion failed" << std::endl;
        return QString();
    }

    constexpr int padding = 50;
    constexpr int maxSideLen = 1024;
    constexpr float boxScoreThresh = 0.5f;
    constexpr float boxThresh = 0.3f;
    constexpr float unClipRatio = 1.6f;
    constexpr bool doAngle = true;
    constexpr bool mostAngle = true;

    std::cout << "Calling OCR detect..." << std::endl;
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

    qDebug() << "OCR detect completed. TextBlocks count:" << result.textBlocks.size();
    qDebug() << "OCR strRes:" << QString::fromStdString(result.strRes);

    QString text = QString::fromStdString(result.strRes).trimmed();
    
    if (text.isEmpty()) {
        for (const auto &block : result.textBlocks) {
            QString blockText = QString::fromStdString(block.text);
            if (!blockText.trimmed().isEmpty()) {
                qDebug() << "Block text:" << blockText;
                if (!text.isEmpty())
                    text += '\n';
                text += blockText;
            }
        }
    }
    
    qDebug() << "Final OCR text:" << text;
    return text;
}
