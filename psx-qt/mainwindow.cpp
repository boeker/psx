#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <limits>
#include <QDir>
#include <QFileSystemModel>
#include <QOpenGLContext>
#include <QScrollBar>

#include "emuthread.h"
#include "openglwindow.h"
#include "vramviewerwindow.h"

#include "psx/core.h"
#include "psx/util/log.h"

PlainTextEditLog::PlainTextEditLog(QPlainTextEdit *plainTextEdit)
    : Log(true),
      plainTextEdit(plainTextEdit) {
}

bool PlainTextEditLog::print(const std::string &message) {
        QString qString = QString::fromStdString(message);
        emit logString(qString);
        emit moveScrollBar(std::numeric_limits<int>::max());

        return false;
}

MainWindow::MainWindow(const QString &biosPath, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      biosFSModel(nullptr),
      running(false),
      core(nullptr),
      emuThread(nullptr),
      vramViewerWindow(nullptr) {
    ui->setupUi(this);

    // Bios Picker
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

    // Core
    core = new PSX::Core();

    // Emulation thread
    emuThread = new EmuThread(this, core);

    // Windows
    vramViewerWindow = new VRAMViewerWindow(this, emuThread);
    vramViewerWindow->show();

    // Logging
    std::shared_ptr<PlainTextEditLog> plainTextEditLog = std::make_shared<PlainTextEditLog>(ui->plainTextEditLog);
    connect(plainTextEditLog.get(), &PlainTextEditLog::logString, ui->plainTextEditLog, &QPlainTextEdit::appendPlainText);
    connect(plainTextEditLog.get(), &PlainTextEditLog::moveScrollBar, ui->plainTextEditLog->verticalScrollBar(), &QScrollBar::setValue);
    util::logPack.installAdditionalLog(plainTextEditLog);

    // Connections
    makeConnections();

    LOG_MISC("Application startup");
}

MainWindow::~MainWindow() {
    stopEmulation();

    delete ui;
    delete core;
}

void MainWindow::makeConnections() {
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

    connect(emuThread, &EmuThread::emulationShouldStop,
            this, &MainWindow::stopEmulation);

    connect(emuThread, &EmuThread::initializedWindows,
            vramViewerWindow, &VRAMViewerWindow::grabWindowFromEmuThread);

    connect(ui->actionVRAMViewer, &QAction::toggled,
            vramViewerWindow, &QWidget::setVisible);
    connect(vramViewerWindow, &VRAMViewerWindow::closed,
            ui->actionVRAMViewer, &QAction::toggle);
}

void MainWindow::startPauseEmulation() {
    if (!running) {
        core->reset();

        QString selectedBios = biosFSModel->filePath(ui->treeView->currentIndex());
        core->bus.bios.readFromFile(selectedBios.toStdString());

        running = true;
    }

    if (emuThread->emulationIsPaused()) {
        continueEmulation();

    } else {
        LOG_MISC("Pausing emulation");
        pauseEmulation();
    }
}

void MainWindow::continueEmulation() {
    LOG_MISC("Starting emulation");
    ui->actionToolbarStart->setText("Pause");
    ui->actionToolbarStop->setEnabled(true);

    ui->actionStart->setEnabled(false);
    ui->actionPause->setEnabled(true);
    ui->actionStop->setEnabled(true);

    emuThread->start();
}

void MainWindow::pauseEmulation() {
    ui->actionToolbarStart->setText("Start");
    ui->actionStart->setEnabled(true);
    ui->actionPause->setEnabled(false);

    emuThread->pauseEmulation();
}

void MainWindow::stopEmulation() {
    if (running) {
        ui->actionToolbarStart->setText("Start");
        ui->actionToolbarStop->setEnabled(false);

        ui->actionStart->setEnabled(true);
        ui->actionPause->setEnabled(false);
        ui->actionStop->setEnabled(false);

        emuThread->pauseEmulation();
        emuThread->wait();
        running = false;

        emuThread->getOpenGLWindow()->hide();
        LOG_MISC("Stopped emulation");
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    stopEmulation();
}

