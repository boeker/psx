#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QFileSystemModel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      biosPath("") {
    ui->setupUi(this);

    QDir dir = QDir::currentPath();
    dir.cd("BIOS");

    QFileSystemModel *fsModel = new QFileSystemModel(ui->treeView);
    fsModel->setRootPath(dir.path());
    ui->treeView->setModel(fsModel);
    ui->treeView->setRootIndex(fsModel->index(dir.path()));

    connect(ui->actionToolbarStart, &QAction::triggered,
            this, &MainWindow::startEmulation);

    connect(ui->actionExit, &QAction::triggered,
            QCoreApplication::instance(), &QCoreApplication::quit);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::setBiosPath(const QString &biosPath) {
    this->biosPath = biosPath;
}

void MainWindow::startEmulation() {
    emuthread = new EmuThread(this);
    emuthread->setBiosPath(biosPath);
    emuthread->start();
}
