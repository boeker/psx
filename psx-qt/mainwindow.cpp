#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QFileSystemModel>

MainWindow::MainWindow(const QString &biosPath, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      emuThread(nullptr) {
    ui->setupUi(this);

    QDir dir = QDir::currentPath();

    biosFSModel = new QFileSystemModel(ui->treeView);
    biosFSModel->setRootPath(dir.path());
    ui->treeView->setModel(biosFSModel);
    ui->treeView->setRootIndex(biosFSModel->index(dir.path()));
    ui->treeView->hideColumn(2);
    ui->treeView->hideColumn(3);
    ui->treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    std::vector<QString> biosLocations;
    if (!biosPath.isEmpty()) {
        biosLocations.push_back(biosPath);
    }
    biosLocations.push_back("SCPH1001.BIN");
    biosLocations.push_back("BIOS/SCPH1001.BIN");

    for (const QString &bios: biosLocations) {
        if (dir.exists(bios)) {
            ui->treeView->setCurrentIndex(biosFSModel->index(dir.path() + "/" + bios));
            break;
        }
    }

    createConnections();
}

MainWindow::~MainWindow() {
    if (emuThread != nullptr) {
        pauseEmulation();
        emuThread->wait();
    }

    delete ui;
}

void MainWindow::createConnections() {
    connect(ui->actionExit, &QAction::triggered,
            QCoreApplication::instance(), &QCoreApplication::quit);

    connect(ui->actionStart, &QAction::triggered,
            this, &MainWindow::startPauseEmulation);
    connect(ui->actionToolbarStart, &QAction::triggered,
            this, &MainWindow::startPauseEmulation);

    connect(ui->actionPause, &QAction::triggered,
            this, &MainWindow::startPauseEmulation);

    connect(ui->actionStop, &QAction::triggered,
            this, &MainWindow::stopEmulation);
    connect(ui->actionToolbarStop, &QAction::triggered,
            this, &MainWindow::stopEmulation);
}

void MainWindow::initializeEmuThread() {
    emuThread = new EmuThread(this);

    QString selectedBios = biosFSModel->filePath(ui->treeView->currentIndex());
    emuThread->setBiosPath(selectedBios);
}

void MainWindow::startPauseEmulation() {
    if (emuThread == nullptr) {
        initializeEmuThread();
    }

    if (emuThread->emulationIsPaused()) {
        continueEmulation();

    } else {
        pauseEmulation();
    }
}

void MainWindow::continueEmulation() {
    ui->actionToolbarStart->setText("Pause");
    ui->actionToolbarStop->setEnabled(true);

    ui->actionStart->setEnabled(false);
    ui->actionPause->setEnabled(true);

    emuThread->start();
}

void MainWindow::pauseEmulation() {
    ui->actionToolbarStart->setText("Start");
    ui->actionStart->setEnabled(true);
    ui->actionPause->setEnabled(false);

    emuThread->pauseEmulation();
}

void MainWindow::stopEmulation() {
}

