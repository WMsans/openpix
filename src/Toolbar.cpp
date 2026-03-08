#include "Toolbar.h"
#include "OverlayWidget.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QDateTime>
#include <QDir>

const QColor Toolbar::Colors[6] = {Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::white, Qt::black};
constexpr int Toolbar::Thicknesses[];

Toolbar::Toolbar(OverlayWidget *overlay, QWidget *parent)
    : QWidget(parent)
    , m_overlay(overlay)
    , m_annotationColor(Colors[0])
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating);

    setupMainToolbar();
    setupAnnotationToolbar();
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
    connect(ocrBtn, &QPushButton::clicked, this, &Toolbar::ocrRequested);
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

    auto *freehandBtn = new QPushButton("Freehand", m_annotationWidget);
    auto *rectBtn = new QPushButton("Rectangle", m_annotationWidget);
    auto *colorBtn = new QPushButton("Color", m_annotationWidget);
    auto *thicknessBtn = new QPushButton("2px", m_annotationWidget);
    auto *undoBtn = new QPushButton("Undo", m_annotationWidget);
    auto *doneBtn = new QPushButton("Done", m_annotationWidget);

    colorBtn->setObjectName("colorBtn");
    thicknessBtn->setObjectName("thicknessBtn");

    connect(colorBtn, &QPushButton::clicked, this, &Toolbar::cycleColor);
    connect(thicknessBtn, &QPushButton::clicked, this, &Toolbar::cycleThickness);

    m_annotationLayout->addWidget(freehandBtn);
    m_annotationLayout->addWidget(rectBtn);
    m_annotationLayout->addWidget(colorBtn);
    m_annotationLayout->addWidget(thicknessBtn);
    m_annotationLayout->addWidget(undoBtn);
    m_annotationLayout->addWidget(doneBtn);

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
    auto *colorBtn = m_annotationWidget->findChild<QPushButton*>("colorBtn");
    if (colorBtn) {
        QString colorName = m_annotationColor.name();
        colorBtn->setStyleSheet(QString("background-color: %1; color: %2;")
            .arg(colorName)
            .arg(m_annotationColor.lightness() > 128 ? "#000000" : "#ffffff"));
    }
}

void Toolbar::updateThicknessButton()
{
    auto *thicknessBtn = m_annotationWidget->findChild<QPushButton*>("thicknessBtn");
    if (thicknessBtn) {
        thicknessBtn->setText(QString("%1px").arg(m_annotationThickness));
    }
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
        image.save(fileName, "PNG");
        qApp->quit();
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
    qApp->quit();
}
