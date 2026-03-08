#pragma once

#include <QString>
#include <QImage>
#include <memory>
#include <opencv2/core.hpp>

class OcrLite;

class OcrEngine
{
public:
    OcrEngine();
    ~OcrEngine();

    bool init(const QString &modelsDir);
    bool isInitialized() const { return m_initialized; }

    QString recognize(const QImage &image);

    QString lastError() const { return m_lastError; }

private:
    cv::Mat qImageToCvMat(const QImage &image) const;

    std::unique_ptr<OcrLite> m_ocr;
    bool m_initialized = false;
    QString m_lastError;
};
