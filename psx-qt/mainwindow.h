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

    void setBiosPath(const QString &biosPath);

public slots:
    void startEmulation();

private:
    Ui::MainWindow *ui;

    QString biosPath;

    OpenGLWindow *openGLWindow;
    EmuThread *emuthread;
};

#endif

