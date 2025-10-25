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

    QCommandLineOption biosOption(QStringList() << "B" << "bios",
                                  "Select bios file <bios>.",
                                  "bios");
    parser.addOption(biosOption);

    QCommandLineOption exeOption(QStringList() << "E" << "exe",
                                  "Load executable file <exe>.",
                                  "exe");
    parser.addOption(exeOption);

    QCommandLineOption startOption(QStringList() << "S" << "start",
                                   "Start emulation on startup.");
    parser.addOption(startOption);

    QCommandLineOption vRAMOption(QStringList() << "V" << "vram",
                                  "Open VRAM viewer window on startup.");
    parser.addOption(vRAMOption);

    QCommandLineOption debuggerOption(QStringList() << "D" << "debugger",
                                      "Open debugger window on startup.");
    parser.addOption(debuggerOption);

    parser.process(app);


    QString biosPath;

    if (parser.isSet(biosOption)) {
        biosPath = parser.value(biosOption);
    }

    MainWindow mainWindow(biosPath, nullptr);

    if (parser.isSet(exeOption)) {
        mainWindow.loadExecutable(parser.value(exeOption));
    }

    mainWindow.show();

    if (parser.isSet(startOption)) {
        // start emulation on start up
        // calling this function directly causes a crash
        // "xdg_surface has never been configured"
        QTimer::singleShot(0, &mainWindow, &MainWindow::startPauseEmulation);
    }

    if (parser.isSet(vRAMOption)) {
        QTimer::singleShot(0, &mainWindow, &MainWindow::triggerVRAMViewerWindow);
    }

    if (parser.isSet(debuggerOption)) {
        QTimer::singleShot(0, &mainWindow, &MainWindow::triggerDebuggerWindow);
    }

    return app.exec();
}

