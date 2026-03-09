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
    static int findOverlap(const QImage &imgA, const QImage &imgB, double threshold = 0.8);
    static cv::Mat qimageToCvMat(const QImage &image);
};
