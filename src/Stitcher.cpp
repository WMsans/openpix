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

    static const int MIN_NEW_CONTENT = 4;

    QVector<PlacedFrame> placed;
    placed.append({0, 0, frames[0].height()});

    bool anyFailed = false;

    for (int i = 1; i < frames.size(); ++i) {
        bool matched = false;
        int newY = 0;
        bool redundant = false;

        int searchStart = placed.size() - 1;
        int searchEnd = std::max(0, static_cast<int>(placed.size()) - MATCH_SEARCH_DEPTH);

        for (int j = searchStart; j >= searchEnd; --j) {
            OverlapResult result = findOverlap(frames[placed[j].frameIndex], frames[i]);
            if (result.overlap < 0)
                continue;

            int newContent = frames[i].height() - result.overlap;
            if (newContent < MIN_NEW_CONTENT) {
                matched = true;
                redundant = true;
                break;
            }

            int candidateY;
            if (result.direction == ScrollDirection::Down) {
                candidateY = placed[j].y + placed[j].height - result.overlap;
            } else {
                candidateY = placed[j].y - (frames[i].height() - result.overlap);
            }

            bool candidateRedundant = false;
            for (int k = 0; k < placed.size(); ++k) {
                int existingTop = placed[k].y;
                int existingBottom = placed[k].y + placed[k].height;
                int newTop = candidateY;
                int newBottom = candidateY + frames[i].height();

                int overlapTop = std::max(existingTop, newTop);
                int overlapBottom = std::min(existingBottom, newBottom);
                int overlapHeight = overlapBottom - overlapTop;

                if (overlapHeight >= frames[i].height() - MIN_NEW_CONTENT) {
                    candidateRedundant = true;
                    break;
                }
            }

            if (!matched || candidateRedundant) {
                newY = candidateY;
                matched = true;
                redundant = candidateRedundant;
            }

            if (candidateRedundant)
                break;
        }

        if (!matched) {
            anyFailed = true;
            newY = placed.last().y + placed.last().height;
        }

        if (!redundant && matched) {
            placed.append({i, newY, frames[i].height()});
        } else if (!matched) {
            placed.append({i, newY, frames[i].height()});
        }
    }

    if (anyFailed && errorOut) {
        *errorOut = "Some frames had no detectable overlap; result may have visible seams.";
    }

    std::sort(placed.begin(), placed.end(),
              [](const PlacedFrame &a, const PlacedFrame &b) { return a.y < b.y; });

    int minY = placed.first().y;
    int maxY = placed.first().y + placed.first().height;
    for (int i = 1; i < placed.size(); ++i) {
        minY = std::min(minY, placed[i].y);
        maxY = std::max(maxY, placed[i].y + placed[i].height);
    }

    int totalHeight = maxY - minY;
    QImage result(width, totalHeight, QImage::Format_RGB32);
    result.fill(Qt::white);
    QPainter painter(&result);

    int canvasBottom = minY;

    for (int i = 0; i < placed.size(); ++i) {
        int frameTop = placed[i].y;
        int frameBottom = placed[i].y + placed[i].height;

        int visibleTop = std::max(frameTop, canvasBottom);
        int visibleBottom = frameBottom;
        if (visibleTop >= visibleBottom)
            continue;

        int cropStart = visibleTop - frameTop;
        int cropHeight = visibleBottom - visibleTop;
        int drawY = visibleTop - minY;

        painter.drawImage(0, drawY,
                          frames[placed[i].frameIndex].copy(0, cropStart, width, cropHeight));

        canvasBottom = std::max(canvasBottom, visibleBottom);
    }

    return result;
}
