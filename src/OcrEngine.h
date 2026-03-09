#pragma once

#include <QString>
#include <QImage>
#include <QObject>
#include <memory>

class OcrLite;

class OcrEngine : public QObject
{
    Q_OBJECT

public:
    explicit OcrEngine(QObject *parent = nullptr);
    ~OcrEngine();

    bool init(const QString &modelsDir);
    bool isInitialized() const { return m_initialized; }

    QString recognize(const QImage &image);

    QString lastError() const { return m_error; }

private:
    std::unique_ptr<OcrLite> m_ocr;
    bool m_initialized = false;
    QString m_error;
};
