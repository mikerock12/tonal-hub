// Loads the Christmas images the way the installed app does: from compiled resources,
// through the image plugins, including Santa's animation. The QML harness reads from disk
// and cannot catch a missing resource or plugin. No window is created.
#include <QDirIterator>
#include <QGuiApplication>
#include <QImageReader>
#include <QQmlComponent>
#include <QQmlEngine>
#include <cstdio>
#include <memory>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    int images = 0;
    for (QDirIterator it(":/companion/resources/seasonal"); it.hasNext();) {
        const QString path = it.next();
        if (!path.endsWith(".webp"))
            continue;
        QImageReader reader(path);
        if (reader.read().isNull()) {
            std::printf("FAIL %s: %s\n", qPrintable(path), qPrintable(reader.errorString()));
            return 1;
        }
        ++images;
    }
    if (images < 16) {
        std::printf("FAIL only %d seasonal images\n", images);
        return 1;
    }
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick\nAnimatedImage { source: 'qrc:/companion/resources/seasonal/santa.webp'; cache: false; asynchronous: true }",
                      QUrl("qrc:/seasonal-check.qml"));
    std::unique_ptr<QObject> santa(component.create());
    if (!santa) {
        std::printf("FAIL %s\n", qPrintable(component.errorString()));
        return 1;
    }
    const int status = santa->property("status").toInt();
    const int frames = santa->property("frameCount").toInt();
    if (status != 1 || frames != 81) {
        std::printf("FAIL Santa status=%d frames=%d\n", status, frames);
        return 1;
    }
    std::printf("SEASONAL_RESOURCES_PASS %d images, Santa %d frames\n", images, frames);
    return 0;
}
