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
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    OpenGLWindow *openGLWindow;
    EmuThread *emuthread;
};

#endif

