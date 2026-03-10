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
        QImage frame1(200, 120, QImage::Format_RGB32);
        frame1.fill(Qt::white);
        QPainter p1(&frame1);
        p1.setPen(Qt::black);
        p1.setFont(QFont("monospace", 14));
        p1.drawText(10, 30, "XXXX");
        p1.drawText(10, 70, "YYYY");
        p1.drawText(10, 110, "ZZZZ");

        QImage frame2(200, 120, QImage::Format_RGB32);
        frame2.fill(Qt::white);
        QPainter p2(&frame2);
        p2.setPen(Qt::black);
        p2.setFont(QFont("monospace", 14));
        p2.drawText(10, 30, "WWWW");
        p2.drawText(10, 70, "XXXX");
        p2.drawText(10, 110, "YYYY");

        QVector<QImage> frames = {frame1, frame2};
        QImage result = Stitcher::stitch(frames);

        QVERIFY(!result.isNull());
        QVERIFY(result.height() > 120);
        QCOMPARE(result.width(), 200);
    }

    void testDownThenUp()
    {
        QImage frame1(200, 120, QImage::Format_RGB32);
        frame1.fill(Qt::white);
        QPainter p1(&frame1);
        p1.setPen(Qt::black);
        p1.setFont(QFont("monospace", 14));
        p1.drawText(10, 30, "AAAA");
        p1.drawText(10, 70, "BBBB");
        p1.drawText(10, 110, "CCCC");

        QImage frame2(200, 120, QImage::Format_RGB32);
        frame2.fill(Qt::white);
        QPainter p2(&frame2);
        p2.setPen(Qt::black);
        p2.setFont(QFont("monospace", 14));
        p2.drawText(10, 30, "BBBB");
        p2.drawText(10, 70, "CCCC");
        p2.drawText(10, 110, "DDDD");

        QImage frame3(200, 120, QImage::Format_RGB32);
        frame3.fill(Qt::white);
        QPainter p3(&frame3);
        p3.setPen(Qt::black);
        p3.setFont(QFont("monospace", 14));
        p3.drawText(10, 30, "AAAA");
        p3.drawText(10, 70, "BBBB");
        p3.drawText(10, 110, "CCCC");

        QVector<QImage> frames = {frame1, frame2, frame3};
        QString error;
        QImage result = Stitcher::stitch(frames, &error);

        QVERIFY(!result.isNull());
        QVERIFY(result.height() > 120);
        QVERIFY(result.height() < 240);
        QCOMPARE(result.width(), 200);
    }

    void testMultipleDirectionChanges()
    {
        QImage f1(200, 120, QImage::Format_RGB32);
        f1.fill(Qt::white);
        QPainter p1(&f1);
        p1.setPen(Qt::black);
        p1.setFont(QFont("monospace", 14));
        p1.drawText(10, 30, "AAAA");
        p1.drawText(10, 70, "BBBB");
        p1.drawText(10, 110, "CCCC");

        QImage f2(200, 120, QImage::Format_RGB32);
        f2.fill(Qt::white);
        QPainter p2(&f2);
        p2.setPen(Qt::black);
        p2.setFont(QFont("monospace", 14));
        p2.drawText(10, 30, "BBBB");
        p2.drawText(10, 70, "CCCC");
        p2.drawText(10, 110, "DDDD");

        QImage f3(200, 120, QImage::Format_RGB32);
        f3.fill(Qt::white);
        QPainter p3(&f3);
        p3.setPen(Qt::black);
        p3.setFont(QFont("monospace", 14));
        p3.drawText(10, 30, "CCCC");
        p3.drawText(10, 70, "DDDD");
        p3.drawText(10, 110, "EEEE");

        QImage f4(200, 120, QImage::Format_RGB32);
        f4.fill(Qt::white);
        QPainter p4(&f4);
        p4.setPen(Qt::black);
        p4.setFont(QFont("monospace", 14));
        p4.drawText(10, 30, "BBBB");
        p4.drawText(10, 70, "CCCC");
        p4.drawText(10, 110, "DDDD");

        QImage f5(200, 120, QImage::Format_RGB32);
        f5.fill(Qt::white);
        QPainter p5(&f5);
        p5.setPen(Qt::black);
        p5.setFont(QFont("monospace", 14));
        p5.drawText(10, 30, "CCCC");
        p5.drawText(10, 70, "DDDD");
        p5.drawText(10, 110, "EEEE");

        QVector<QImage> frames = {f1, f2, f3, f4, f5};
        QString error;
        QImage result = Stitcher::stitch(frames, &error);

        QVERIFY(!result.isNull());
        QVERIFY(result.height() > 120);
        QVERIFY(result.height() < 300);
        QCOMPARE(result.width(), 200);
    }

    void testRevisitedContent()
    {
        QImage frame1(200, 120, QImage::Format_RGB32);
        frame1.fill(Qt::white);
        QPainter p1(&frame1);
        p1.setPen(Qt::black);
        p1.setFont(QFont("monospace", 14));
        p1.drawText(10, 30, "AAAA");
        p1.drawText(10, 70, "BBBB");
        p1.drawText(10, 110, "CCCC");

        QImage frame2(200, 120, QImage::Format_RGB32);
        frame2.fill(Qt::white);
        QPainter p2(&frame2);
        p2.setPen(Qt::black);
        p2.setFont(QFont("monospace", 14));
        p2.drawText(10, 30, "BBBB");
        p2.drawText(10, 70, "CCCC");
        p2.drawText(10, 110, "DDDD");

        QImage frame3 = frame1.copy();

        QVector<QImage> frames = {frame1, frame2, frame3};
        QString error;
        QImage result = Stitcher::stitch(frames, &error);

        QVERIFY(!result.isNull());
        QVERIFY(result.height() > 120);
        QVERIFY(result.height() < 240);
        QCOMPARE(result.width(), 200);
    }
};

QTEST_MAIN(TestStitcher)
#include "test_stitcher.moc"
