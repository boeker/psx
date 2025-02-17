#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>

#include <QtWidgets/QMainWindow>

#include "emuthread.h"
#include "openglwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
class QFileSystemModel;
QT_END_NAMESPACE


class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(const QString &biosPath, QWidget *parent = nullptr);
    ~MainWindow();

    void initializeEmuThread();

public slots:
    void startPauseEmulation();
    void continueEmulation();
    void pauseEmulation();
    void stopEmulation();

private:
    void createConnections();

private:
    Ui::MainWindow *ui;
    QFileSystemModel *biosFSModel;

    OpenGLWindow *openGLWindow;
    EmuThread *emuThread;
};

#endif

