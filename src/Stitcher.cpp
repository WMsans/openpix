#include "Stitcher.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <QPainter>

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

int Stitcher::findOverlap(const QImage &imgA, const QImage &imgB, double threshold)
{
    cv::Mat matA = qimageToCvMat(imgA);
    cv::Mat matB = qimageToCvMat(imgB);
    
    int stripHeight = matA.rows / 3;
    if (stripHeight < 10) stripHeight = 10;
    cv::Mat templ = matA(cv::Rect(0, matA.rows - stripHeight, matA.cols, stripHeight));
    
    int searchHeight = matB.rows / 2;
    if (searchHeight < stripHeight) searchHeight = matB.rows;
    cv::Mat searchRegion = matB(cv::Rect(0, 0, matB.cols, searchHeight));
    
    cv::Mat result;
    cv::matchTemplate(searchRegion, templ, result, cv::TM_CCOEFF_NORMED);
    
    double maxVal;
    cv::Point maxLoc;
    cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
    
    if (maxVal < threshold)
        return -1;
    
    return maxLoc.y + stripHeight;
}

QImage Stitcher::stitch(const QVector<QImage> &frames, QString *errorOut)
{
    if (frames.isEmpty())
        return QImage();
    if (frames.size() == 1)
        return frames.first();
    
    int width = frames.first().width();
    
    struct Segment {
        const QImage *image;
        int startRow;
    };
    QVector<Segment> segments;
    segments.append({&frames[0], 0});
    
    bool anyFailed = false;
    for (int i = 1; i < frames.size(); ++i) {
        int overlap = findOverlap(frames[i - 1], frames[i]);
        if (overlap < 0) {
            anyFailed = true;
            segments.append({&frames[i], 0});
        } else {
            segments.append({&frames[i], overlap});
        }
    }
    
    if (anyFailed && errorOut) {
        *errorOut = "Some frames had no detectable overlap; result may have visible seams.";
    }
    
    int totalHeight = 0;
    for (int i = 0; i < segments.size(); ++i) {
        totalHeight += segments[i].image->height() - segments[i].startRow;
    }
    
    QImage result(width, totalHeight, QImage::Format_RGB32);
    QPainter painter(&result);
    int y = 0;
    for (auto &seg : segments) {
        int h = seg.image->height() - seg.startRow;
        painter.drawImage(0, y,
                          seg.image->copy(0, seg.startRow, width, h));
        y += h;
    }
    return result;
}
