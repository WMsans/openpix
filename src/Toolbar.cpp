#include "Toolbar.h"
#include "OverlayWidget.h"
#include "OcrEngine.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QMessageBox>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QtConcurrent>

const QColor Toolbar::Colors[6] = {Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::white, Qt::black};
constexpr int Toolbar::Thicknesses[];

Toolbar::Toolbar(OverlayWidget *overlay, QWidget *parent)
    : QWidget(parent)
    , m_overlay(overlay)
    , m_annotationColor(Colors[0])
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating);

    setupAnnotationToolbar();
    setupMainToolbar();
    applyStyles();

    setMode(Mode::Main);
}

void Toolbar::setupMainToolbar()
{
    m_mainWidget = new QWidget(this);
    m_mainLayout = new QHBoxLayout(m_mainWidget);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(2);

    auto *saveBtn = new QPushButton("Save As", m_mainWidget);
    auto *copyBtn = new QPushButton("Copy", m_mainWidget);
    auto *ocrBtn = new QPushButton("OCR", m_mainWidget);
    auto *annotateBtn = new QPushButton("Annotate", m_mainWidget);
    auto *scrollBtn = new QPushButton("Scroll", m_mainWidget);

    connect(saveBtn, &QPushButton::clicked, this, &Toolbar::onSaveClicked);
    connect(copyBtn, &QPushButton::clicked, this, &Toolbar::onCopyClicked);
    connect(ocrBtn, &QPushButton::clicked, this, &Toolbar::onOcr);
    connect(annotateBtn, &QPushButton::clicked, this, &Toolbar::annotateRequested);
    connect(scrollBtn, &QPushButton::clicked, this, &Toolbar::scrollCaptureRequested);

    m_mainLayout->addWidget(saveBtn);
    m_mainLayout->addWidget(copyBtn);
    m_mainLayout->addWidget(ocrBtn);
    m_mainLayout->addWidget(annotateBtn);
    m_mainLayout->addWidget(scrollBtn);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_mainWidget);
    layout->addWidget(m_annotationWidget);
}

void Toolbar::setupAnnotationToolbar()
{
    m_annotationWidget = new QWidget(this);
    m_annotationLayout = new QHBoxLayout(m_annotationWidget);
    m_annotationLayout->setContentsMargins(4, 4, 4, 4);
    m_annotationLayout->setSpacing(2);

    m_freehandBtn = new QPushButton("Freehand", m_annotationWidget);
    m_rectBtn = new QPushButton("Rectangle", m_annotationWidget);
    m_colorBtn = new QPushButton("Color", m_annotationWidget);
    m_thicknessBtn = new QPushButton("2px", m_annotationWidget);
    m_undoBtn = new QPushButton("Undo", m_annotationWidget);
    m_doneBtn = new QPushButton("Done", m_annotationWidget);

    connect(m_freehandBtn, &QPushButton::clicked, this, &Toolbar::onFreehandClicked);
    connect(m_rectBtn, &QPushButton::clicked, this, &Toolbar::onRectClicked);
    connect(m_colorBtn, &QPushButton::clicked, this, &Toolbar::cycleColor);
    connect(m_thicknessBtn, &QPushButton::clicked, this, &Toolbar::cycleThickness);
    connect(m_undoBtn, &QPushButton::clicked, this, &Toolbar::onUndoClicked);
    connect(m_doneBtn, &QPushButton::clicked, this, &Toolbar::onDoneClicked);

    m_annotationLayout->addWidget(m_freehandBtn);
    m_annotationLayout->addWidget(m_rectBtn);
    m_annotationLayout->addWidget(m_colorBtn);
    m_annotationLayout->addWidget(m_thicknessBtn);
    m_annotationLayout->addWidget(m_undoBtn);
    m_annotationLayout->addWidget(m_doneBtn);

    updateColorButton();
}

void Toolbar::applyStyles()
{
    setStyleSheet(R"(
        Toolbar {
            background-color: #2d2d2d;
            border-radius: 6px;
            border: 1px solid #444;
        }
        QPushButton {
            background-color: #3d3d3d;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 6px 12px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #4d4d4d;
        }
        QPushButton:pressed {
            background-color: #5d5d5d;
        }
    )");
}

void Toolbar::setMode(Mode mode)
{
    m_mode = mode;
    if (mode == Mode::Main) {
        m_mainWidget->show();
        m_annotationWidget->hide();
    } else {
        m_mainWidget->hide();
        m_annotationWidget->show();
    }
    adjustSize();
}

void Toolbar::showMainToolbar()
{
    setMode(Mode::Main);
}

void Toolbar::showAnnotationToolbar()
{
    setMode(Mode::Annotation);
}

void Toolbar::cycleColor()
{
    m_colorIndex = (m_colorIndex + 1) % ColorCount;
    m_annotationColor = Colors[m_colorIndex];
    updateColorButton();
}

void Toolbar::cycleThickness()
{
    m_thicknessIndex = (m_thicknessIndex + 1) % ThicknessCount;
    m_annotationThickness = Thicknesses[m_thicknessIndex];
    updateThicknessButton();
}

void Toolbar::updateColorButton()
{
    if (m_colorBtn) {
        QString colorName = m_annotationColor.name();
        m_colorBtn->setStyleSheet(QString("background-color: %1; color: %2;")
            .arg(colorName)
            .arg(m_annotationColor.lightness() > 128 ? "#000000" : "#ffffff"));
    }
}

void Toolbar::updateThicknessButton()
{
    if (m_thicknessBtn) {
        m_thicknessBtn->setText(QString("%1px").arg(m_annotationThickness));
    }
}

void Toolbar::onFreehandClicked()
{
    m_annotationTool = AnnotationTool::Freehand;
    emit annotationToolChanged(m_annotationTool);
}

void Toolbar::onRectClicked()
{
    m_annotationTool = AnnotationTool::Rectangle;
    emit annotationToolChanged(m_annotationTool);
}

void Toolbar::onUndoClicked()
{
    emit undoRequested();
}

void Toolbar::onDoneClicked()
{
    emit annotationDone();
}

void Toolbar::onSaveClicked()
{
    if (!m_overlay) return;

    QImage image = m_overlay->croppedImage();
    if (image.isNull()) return;

    QString defaultPath = QDir::homePath() + "/Pictures/screenshot_" +
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";

    QString fileName = QFileDialog::getSaveFileName(
        nullptr,
        "Save Screenshot",
        defaultPath,
        "PNG Images (*.png);;All Files (*)"
    );

    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".png", Qt::CaseInsensitive)) {
            fileName += ".png";
        }
        if (!image.save(fileName, "PNG")) {
            qWarning() << "Failed to save screenshot to" << fileName;
            return;
        }
        emit quitRequested();
    }
}

void Toolbar::onCopyClicked()
{
    if (!m_overlay) return;

    QImage image = m_overlay->croppedImage();
    if (image.isNull()) return;

    auto *mimeData = new QMimeData();
    mimeData->setImageData(image);

    QApplication::clipboard()->setMimeData(mimeData);
    emit quitRequested();
}

void Toolbar::onOcr()
{
    QImage img = m_overlay->croppedImage();
    if (img.isNull()) return;

    setEnabled(false);
    setCursor(Qt::WaitCursor);

    QtConcurrent::run([this, img]() {
        static OcrEngine ocrEngine;
        if (!ocrEngine.isInitialized()) {
            QString modelsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
            if (!ocrEngine.init(modelsDir)) {
                modelsDir = QCoreApplication::applicationDirPath() + "/../share/openpix/models";
                if (!ocrEngine.init(modelsDir)) {
                    modelsDir = "/usr/share/openpix/models";
                    if (!ocrEngine.init(modelsDir)) {
                        QMetaObject::invokeMethod(this, [this]() {
                            QMessageBox::warning(m_overlay, "OCR Error",
                                "OCR models not found. Place models in ~/.local/share/openpix/models/");
                            setEnabled(true);
                            unsetCursor();
                        }, Qt::QueuedConnection);
                        return;
                    }
                }
            }
        }

        QString text = ocrEngine.recognize(img);

        QMetaObject::invokeMethod(this, [this, text]() {
            setEnabled(true);
            unsetCursor();

            if (text.isEmpty()) {
                QMessageBox::information(m_overlay, "OCR", "No text detected in selection.");
                return;
            }

            QApplication::clipboard()->setText(text);
            qApp->quit();
        }, Qt::QueuedConnection);
    });
}
