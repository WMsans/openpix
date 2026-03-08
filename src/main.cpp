#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("openpix");
    app.setApplicationVersion("0.1.0");

    qDebug() << "openpix starting...";

    // TODO: CaptureManager -> OverlayWidget
    return 0;
}
