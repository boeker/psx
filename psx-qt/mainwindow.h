#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <iostream>
#include <memory>
#include <sstream>

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPlainTextEdit>
#include <QString>

#include "psx/util/log.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
class QFileSystemModel;
QT_END_NAMESPACE

class DebuggerWindow;
class EmuThread;
class OpenGLWindow;
class VRAMViewerWindow;

namespace PSX {
class Core;
class OpenGLRenderer;
}

// Global emulation variables
extern bool running;
extern PSX::Core *core;
extern PSX::OpenGLRenderer *renderer;
extern EmuThread *emuThread;

class PlainTextEditLog : public QObject, public util::Log {
    Q_OBJECT

public:
    PlainTextEditLog(QPlainTextEdit *plainTextEdit);
    bool print(const std::string &message) override;

signals:
    void logString(QString string);
    void moveScrollBar(int value);

private:
    QPlainTextEdit *plainTextEdit;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(const QString &biosPath, QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void startPauseEmulation();
    void continueEmulation();
    void pauseEmulation();
    void stopEmulation();

    void triggerVRAMViewerWindow();
    void triggerDebuggerWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void makeConnections();

private:
    // Main window
    Ui::MainWindow *ui;
    QFileSystemModel *biosFSModel;

    // Debugger window
    DebuggerWindow *debuggerWindow;

    // VRAM viewer window
    VRAMViewerWindow *vramViewerWindow;

    // OpenGL windows
    OpenGLWindow *openGLWindow;
    QWidget *openGLWindowWidget;
};

#endif

