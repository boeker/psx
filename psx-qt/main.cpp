#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("PSX");
    QApplication::setApplicationVersion("wip");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption biosOption(QStringList() << "b" << "bios",
                                  "Load bios file <bios>.",
                                  "bios");
    parser.addOption(biosOption);

    QCommandLineOption startOption(QStringList() << "s" << "start",
                                   "Immediately start emulation.");
    parser.addOption(startOption);

    parser.process(app);

    MainWindow mainWindow;
    mainWindow.show();

    if (parser.isSet(biosOption)) {
        QString biosPath = parser.value(biosOption);
        mainWindow.setBiosPath(biosPath);
    }

    if (parser.isSet(startOption)) {
        // start emulation on start up
        // calling this function directly causes a crash
        // "xdg_surface has never been configured"
        QTimer::singleShot(0, &mainWindow, &MainWindow::startEmulation);
    }

    return app.exec();
}

