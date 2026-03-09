#include "Stitcher.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <QPainter>
#include <algorithm>

cv::Mat Stitcher::qimageToCvMat(const QImage &image)
{
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3,
                const_cast<uchar *>(rgb.bits()),
                static_cast<size_t>(rgb.bytesPerLine()));
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
    return bgr.clone();
}

Stitcher::OverlapResult Stitcher::findOverlap(const QImage &imgA, const QImage &imgB, double threshold)
{
    cv::Mat matA = qimageToCvMat(imgA);
    cv::Mat matB = qimageToCvMat(imgB);

    int stripHeight = matA.rows / 3;
    if (stripHeight < 10) stripHeight = 10;

    cv::Mat result;
    double maxVal;
    cv::Point maxLoc;

    cv::Mat templDown = matA(cv::Rect(0, matA.rows - stripHeight, matA.cols, stripHeight));
    cv::matchTemplate(matB, templDown, result, cv::TM_CCOEFF_NORMED);
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

    if (maxVal >= threshold) {
        return {maxLoc.y + stripHeight, ScrollDirection::Down};
    }

    cv::Mat templUp = matA(cv::Rect(0, 0, matA.cols, stripHeight));
    cv::matchTemplate(matB, templUp, result, cv::TM_CCOEFF_NORMED);
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

    if (maxVal >= threshold) {
        int overlap = matB.rows - maxLoc.y;
        return {overlap, ScrollDirection::Up};
    }

    return {-1, ScrollDirection::None};
}

QImage Stitcher::stitch(const QVector<QImage> &frames, QString *errorOut)
{
    if (frames.isEmpty())
        return QImage();
    if (frames.size() == 1)
        return frames.first();
    
    if (frames.first().isNull()) {
        if (errorOut) *errorOut = "Frame 0 is null or invalid";
        return QImage();
    }
    
    int width = frames.first().width();
    
    for (int i = 1; i < frames.size(); ++i) {
        if (frames[i].width() != width) {
            if (errorOut) {
                *errorOut = QString("Frame %1 has width %2, expected %3")
                    .arg(i).arg(frames[i].width()).arg(width);
            }
            return QImage();
        }
        if (frames[i].isNull()) {
            if (errorOut) {
                *errorOut = QString("Frame %1 is null or invalid").arg(i);
            }
            return QImage();
        }
    }
    
    struct Segment {
        const QImage *image;
        int startRow;
        int endRow;
    };
    QVector<Segment> segments;
    segments.append({&frames[0], 0, frames[0].height()});

    static const int MIN_NEW_CONTENT = 4;

    bool anyFailed = false;
    bool scrollingUp = false;
    int lastKept = 0;
    for (int i = 1; i < frames.size(); ++i) {
        OverlapResult overlapResult = findOverlap(frames[lastKept], frames[i]);
        if (overlapResult.overlap < 0) {
            anyFailed = true;
            segments.append({&frames[i], 0, frames[i].height()});
            lastKept = i;
        } else {
            int newContent;
            if (overlapResult.direction == ScrollDirection::Down) {
                newContent = frames[i].height() - overlapResult.overlap;
                if (newContent >= MIN_NEW_CONTENT) {
                    segments.append({&frames[i], overlapResult.overlap, frames[i].height()});
                    lastKept = i;
                }
            } else {
                newContent = frames[i].height() - overlapResult.overlap;
                if (newContent >= MIN_NEW_CONTENT) {
                    scrollingUp = true;
                    segments.append({&frames[i], 0, frames[i].height() - overlapResult.overlap});
                    lastKept = i;
                }
            }
        }
    }
    
    if (scrollingUp) {
        std::reverse(segments.begin(), segments.end());
    }
    
    if (anyFailed && errorOut) {
        *errorOut = "Some frames had no detectable overlap; result may have visible seams.";
    }
    
    int totalHeight = 0;
    for (int i = 0; i < segments.size(); ++i) {
        totalHeight += segments[i].endRow - segments[i].startRow;
    }

    QImage result(width, totalHeight, QImage::Format_RGB32);
    QPainter painter(&result);
    int y = 0;
    for (auto &seg : segments) {
        int h = seg.endRow - seg.startRow;
        painter.drawImage(0, y,
                          seg.image->copy(0, seg.startRow, width, h));
        y += h;
    }
    return result;
}
