#pragma once
#include <QImage>
#include <QVector>
#include <QString>
#include <opencv2/core.hpp>

class Stitcher
{
public:
    static QImage stitch(const QVector<QImage> &frames, QString *errorOut = nullptr);
private:
    enum class ScrollDirection { Down, Up, None };
    struct OverlapResult {
        int overlap;
        ScrollDirection direction;
    };
    static OverlapResult findOverlap(const QImage &imgA, const QImage &imgB, double threshold = 0.8);
    static cv::Mat qimageToCvMat(const QImage &image);
};
