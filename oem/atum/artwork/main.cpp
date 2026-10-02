// SPDX-License-Identifier: CC0-1.0
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 3)
        return 2;
    QSvgRenderer renderer(QString::fromLocal8Bit(argv[1]));
    if (!renderer.isValid())
        return 3;
    for (int size : {16, 32, 48, 64, 128, 256, 512, 1024}) {
        QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.render(&painter);
        painter.end();
        if (!image.save(QStringLiteral("%1/%2-atum-icon.png").arg(QString::fromLocal8Bit(argv[2])).arg(size)))
            return 4;
    }
    return 0;
}
