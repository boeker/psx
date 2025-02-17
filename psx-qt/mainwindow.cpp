#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QFileSystemModel>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow) {
    ui->setupUi(this);

    QDir dir = QDir::currentPath();
    dir.cd("BIOS");

    QFileSystemModel *fsModel = new QFileSystemModel(ui->treeView);
    fsModel->setRootPath(dir.path());
    ui->treeView->setModel(fsModel);
    ui->treeView->setRootIndex(fsModel->index(dir.path()));

    QObject::connect(ui->actionToolbarStart, &QAction::triggered,
                     this, &MainWindow::startEmulation);

    // start emulation on start up
    // calling this function directly causes a crash
    // "xdg_surface has never been configured"
    QTimer::singleShot(0, this, SLOT(startEmulation()));
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::startEmulation() {
    emuthread = new EmuThread(this);
    emuthread->start();
}
