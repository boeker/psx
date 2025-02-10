#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow) {
    ui->setupUi(this);

    emuthread = new EmuThread();
    emuthread->start();
}

MainWindow::~MainWindow() {
    delete ui;
}
