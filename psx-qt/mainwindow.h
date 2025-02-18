#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>

#include <QtWidgets/QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
class QFileSystemModel;
QT_END_NAMESPACE

class EmuThread;
class OpenGLWindow;
class VRAMViewerWindow;

namespace PSX {
class Core;
class OpenGLRenderer;
}

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

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void makeConnections();

private:
    // Main Window stuff
    Ui::MainWindow *ui;
    QFileSystemModel *biosFSModel;

    // Emulation stuff
    bool running;
    PSX::Core *core;
    EmuThread *emuThread;

    // VRAM Viewer Window
    VRAMViewerWindow *vramViewerWindow;
};

#endif

