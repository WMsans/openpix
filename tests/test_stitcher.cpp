#include <QtTest/QtTest>
#include <QPainter>
#include "../src/Stitcher.h"

class TestStitcher : public QObject
{
    Q_OBJECT

private slots:
    void testTwoIdenticalFrames()
    {
        QImage frame1(200, 100, QImage::Format_RGB32);
        frame1.fill(Qt::white);
        QPainter p1(&frame1);
        p1.setPen(Qt::black);
        p1.drawText(10, 50, "Hello");
        p1.drawText(10, 90, "World");
        
        QImage frame2(200, 100, QImage::Format_RGB32);
        frame2.fill(Qt::white);
        QPainter p2(&frame2);
        p2.setPen(Qt::black);
        p2.drawText(10, 10, "World");
        p2.drawText(10, 50, "Foo");
        
        QVector<QImage> frames = {frame1, frame2};
        QImage result = Stitcher::stitch(frames);
        
        QVERIFY(!result.isNull());
        QVERIFY(result.height() > 0);
        QCOMPARE(result.width(), 200);
    }

    void testSingleFrame()
    {
        QImage frame(100, 100, QImage::Format_RGB32);
        frame.fill(Qt::red);
        QVector<QImage> frames = {frame};
        QImage result = Stitcher::stitch(frames);
        QCOMPARE(result, frame);
    }

    void testEmptyInput()
    {
        QVector<QImage> frames;
        QImage result = Stitcher::stitch(frames);
        QVERIFY(result.isNull());
    }

    void testReverseScrolling()
    {
        QImage frame1(200, 100, QImage::Format_RGB32);
        frame1.fill(Qt::white);
        QPainter p1(&frame1);
        p1.setPen(Qt::black);
        p1.drawText(10, 10, "Line1");
        p1.drawText(10, 50, "Line2");
        p1.drawText(10, 90, "Line3");
        
        QImage frame2(200, 100, QImage::Format_RGB32);
        frame2.fill(Qt::white);
        QPainter p2(&frame2);
        p2.setPen(Qt::black);
        p2.drawText(10, 10, "Line0");
        p2.drawText(10, 50, "Line1");
        p2.drawText(10, 90, "Line2");
        
        QVector<QImage> frames = {frame1, frame2};
        QImage result = Stitcher::stitch(frames);
        
        QVERIFY(!result.isNull());
        QVERIFY(result.height() > 100);
        QCOMPARE(result.width(), 200);
    }
};

QTEST_MAIN(TestStitcher)
#include "test_stitcher.moc"
