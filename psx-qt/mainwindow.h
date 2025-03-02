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

class EmuThread;
class OpenGLWindow;
class VRAMViewerWindow;

namespace PSX {
class Core;
class OpenGLRenderer;
}

class PlainTextEditLog : public QObject, public util::Log {
    Q_OBJECT

public:
    PlainTextEditLog(QPlainTextEdit *plainTextEdit);
    bool print(const std::string &message) override;

signals:
    void logString(QString string);

private:
    QPlainTextEdit *plainTextEdit;
};

//class LogBuffer : public QObject, public std::stringbuf {
//    Q_OBJECT
//
//public:
//    LogBuffer(QPlainTextEdit *plainTextEdit)
//    : plainTextEdit(plainTextEdit) {
//    }
//    virtual int sync() {
//        std::clog << this->str();
//        QString qString = QString::fromStdString(this->str());
//
//        emit logString(qString.left(qString.length() - 1));
//        this->str("");
//        return 0;
//    };
//
//signals:
//    void logString(QString string);
//
//private:
//    QPlainTextEdit *plainTextEdit;
//};

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

